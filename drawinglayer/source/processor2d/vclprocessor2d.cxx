/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include "vclprocessor2d.hxx"

#include "getdigitlanguage.hxx"
#include "vclhelperbufferdevice.hxx"
#include <cmath>
#include <comphelper/string.hxx>
#include <comphelper/lok.hxx>
#include <svtools/optionsdrawinglayer.hxx>
#include <tools/debug.hxx>
#include <tools/fract.hxx>
#include <utility>
#include <vcl/glyphitemcache.hxx>
#include <vcl/graph.hxx>
#include <vcl/kernarray.hxx>
#include <vcl/outdev.hxx>
#include <rtl/ustrbuf.hxx>
#include <sal/log.hxx>
#include <toolkit/helper/vclunohelper.hxx>
#include <basegfx/polygon/b2dpolygontools.hxx>
#include <basegfx/polygon/b2dpolypolygontools.hxx>
#include <basegfx/polygon/b2dpolygonclipper.hxx>
#include <basegfx/color/bcolor.hxx>
#include <basegfx/matrix/b2dhommatrixtools.hxx>
#include <drawinglayer/primitive2d/drawinglayer_primitivetypes2d.hxx>
#include <drawinglayer/primitive2d/textprimitive2d.hxx>
#include <drawinglayer/primitive2d/textdecoratedprimitive2d.hxx>
#include <drawinglayer/primitive2d/bitmapprimitive2d.hxx>
#include <drawinglayer/primitive2d/fillgraphicprimitive2d.hxx>
#include <drawinglayer/primitive2d/PolygonHairlinePrimitive2D.hxx>
#include <drawinglayer/primitive2d/PolygonStrokePrimitive2D.hxx>
#include <drawinglayer/primitive2d/PolyPolygonGraphicPrimitive2D.hxx>
#include <drawinglayer/primitive2d/maskprimitive2d.hxx>
#include <drawinglayer/primitive2d/modifiedcolorprimitive2d.hxx>
#include <drawinglayer/primitive2d/unifiedtransparenceprimitive2d.hxx>
#include <drawinglayer/primitive2d/transparenceprimitive2d.hxx>
#include <drawinglayer/primitive2d/transformprimitive2d.hxx>
#include <drawinglayer/primitive2d/markerarrayprimitive2d.hxx>
#include <drawinglayer/primitive2d/pagepreviewprimitive2d.hxx>
#include <drawinglayer/primitive2d/textenumsprimitive2d.hxx>
#include <drawinglayer/primitive2d/svggradientprimitive2d.hxx>
// for support of Title/Description in all apps when embedding pictures
#include <drawinglayer/primitive2d/objectinfoprimitive2d.hxx>
// control support
#include <drawinglayer/primitive2d/textlayoutdevice.hxx>

#include <drawinglayer/primitive2d/pointarrayprimitive2d.hxx>
#include <drawinglayer/primitive2d/epsprimitive2d.hxx>

using namespace com::sun::star;

namespace
{
sal_uInt32 calculateStepsForSvgGradient(const basegfx::BColor& rColorA,
                                        const basegfx::BColor& rColorB, double fDelta,
                                        double fDiscreteUnit)
{
    // use color distance, assume to do every color step
    sal_uInt32 nSteps(basegfx::fround(rColorA.getDistance(rColorB) * 255.0));

    if (nSteps)
    {
        // calc discrete length to change color each discrete unit (pixel)
        const sal_uInt32 nDistSteps(basegfx::fround(fDelta / fDiscreteUnit));

        nSteps = std::min(nSteps, nDistSteps);
    }

    // reduce quality to 3 discrete units or every 3rd color step for rendering
    nSteps /= 2;

    // roughly cut when too big or too small (not full quality, reduce complexity)
    nSteps = std::min(nSteps, sal_uInt32(255));
    nSteps = std::max(nSteps, sal_uInt32(1));

    return nSteps;
}
}

namespace
{
/** helper to convert a MapMode to a transformation */
basegfx::B2DHomMatrix getTransformFromMapMode(const MapMode& rMapMode)
{
    basegfx::B2DHomMatrix aMapping;
    const Fraction aNoScale(1, 1);
    const Point& rOrigin(rMapMode.GetOrigin());

    if (0 != rOrigin.X() || 0 != rOrigin.Y())
    {
        aMapping.translate(rOrigin.X(), rOrigin.Y());
    }

    if (rMapMode.GetScaleX() != aNoScale || rMapMode.GetScaleY() != aNoScale)
    {
        aMapping.scale(double(rMapMode.GetScaleX()), double(rMapMode.GetScaleY()));
    }

    return aMapping;
}
}

namespace drawinglayer::processor2d
{
// rendering support

// directdraw of text simple portion or decorated portion primitive. When decorated, all the extra
// information is translated to VCL parameters and set at the font.
// Acceptance is restricted to no shearing and positive scaling in X and Y (no font mirroring
// for VCL)
void VclProcessor2D::RenderTextSimpleOrDecoratedPortionPrimitive2D(
    const primitive2d::TextSimplePortionPrimitive2D& rTextCandidate)
{
    // decompose matrix to have position and size of text
    basegfx::B2DHomMatrix aLocalTransform(maCurrentTransformation
                                          * rTextCandidate.getTextTransform());
    basegfx::B2DVector aFontScaling, aTranslate;
    double fRotate, fShearX;
    aLocalTransform.decompose(aFontScaling, aTranslate, fRotate, fShearX);

    bool bPrimitiveAccepted(false);

    // tdf#95581: Assume tiny shears are rounding artefacts or whatever and can be ignored,
    // especially if the effect is less than a pixel.
    if (std::abs(aFontScaling.getY() * fShearX) < 1)
    {
        if (aFontScaling.getX() < 0.0 && aFontScaling.getY() < 0.0)
        {
            // handle special case: If scale is negative in (x,y) (3rd quadrant), it can
            // be expressed as rotation by PI. Use this since the Font rendering will not
            // apply the negative scales in any form
            aFontScaling = basegfx::absolute(aFontScaling);
            fRotate += M_PI;
        }

        if (aFontScaling.getX() > 0.0 && aFontScaling.getY() > 0.0)
        {
            double fIgnoreRotate, fIgnoreShearX;

            basegfx::B2DVector aFontSize, aTextTranslate;
            rTextCandidate.getTextTransform().decompose(aFontSize, aTextTranslate, fIgnoreRotate,
                                                        fIgnoreShearX);

            // tdf#153092 Ideally we don't have to scale the font and dxarray, but we might have
            // to nevertheless if dealing with non integer sizes
            const bool bScaleFont(aFontSize.getY() != std::round(aFontSize.getY())
                                  || comphelper::LibreOfficeKit::isActive());
            vcl::Font aFont;

            // Get the VCL font
            if (!bScaleFont)
            {
                aFont = primitive2d::getVclFontFromFontAttribute(
                    rTextCandidate.getFontAttribute(), aFontSize.getX(), aFontSize.getY(), fRotate,
                    rTextCandidate.getLocale());
            }
            else
            {
                aFont = primitive2d::getVclFontFromFontAttribute(
                    rTextCandidate.getFontAttribute(), aFontScaling.getX(), aFontScaling.getY(),
                    fRotate, rTextCandidate.getLocale());
            }

            // Don't draw fonts without height
            Size aResultFontSize = aFont.GetFontSize();
            if (aResultFontSize.Height() <= 0)
                return;

            // set FillColor Attribute
            const Color aFillColor(rTextCandidate.getTextFillColor());
            aFont.SetTransparent(aFillColor.IsTransparent());
            aFont.SetFillColor(aFillColor);

            // handle additional font attributes
            const primitive2d::TextDecoratedPortionPrimitive2D* pTCPP = nullptr;
            if (rTextCandidate.getPrimitive2DID() == PRIMITIVE2D_ID_TEXTDECORATEDPORTIONPRIMITIVE2D)
                pTCPP = static_cast<const primitive2d::TextDecoratedPortionPrimitive2D*>(
                    &rTextCandidate);

            if (pTCPP != nullptr)
            {
                // set the color of text decorations
                const basegfx::BColor aTextlineColor
                    = maBColorModifierStack.getModifiedColor(pTCPP->getTextlineColor());
                mpOutputDevice->SetTextLineColor(Color(aTextlineColor));

                // set Overline attribute
                const FontLineStyle eFontOverline(
                    primitive2d::mapTextLineToFontLineStyle(pTCPP->getFontOverline()));
                if (eFontOverline != LINESTYLE_NONE)
                {
                    aFont.SetOverline(eFontOverline);
                    const basegfx::BColor aOverlineColor
                        = maBColorModifierStack.getModifiedColor(pTCPP->getOverlineColor());
                    mpOutputDevice->SetOverlineColor(Color(aOverlineColor));
                    if (pTCPP->getWordLineMode())
                        aFont.SetWordLineMode(true);
                }

                // set Underline attribute
                const FontLineStyle eFontLineStyle(
                    primitive2d::mapTextLineToFontLineStyle(pTCPP->getFontUnderline()));
                if (eFontLineStyle != LINESTYLE_NONE)
                {
                    aFont.SetUnderline(eFontLineStyle);
                    if (pTCPP->getWordLineMode())
                        aFont.SetWordLineMode(true);
                }

                // set Strikeout attribute
                const FontStrikeout eFontStrikeout(
                    primitive2d::mapTextStrikeoutToFontStrikeout(pTCPP->getTextStrikeout()));

                if (eFontStrikeout != STRIKEOUT_NONE)
                    aFont.SetStrikeout(eFontStrikeout);

                // set EmphasisMark attribute
                FontEmphasisMark eFontEmphasisMark = FontEmphasisMark::NONE;
                switch (pTCPP->getTextEmphasisMark())
                {
                    default:
                        SAL_WARN("drawinglayer",
                                 "Unknown EmphasisMark style " << pTCPP->getTextEmphasisMark());
                        [[fallthrough]];
                    case primitive2d::TEXT_FONT_EMPHASIS_MARK_NONE:
                        eFontEmphasisMark = FontEmphasisMark::NONE;
                        break;
                    case primitive2d::TEXT_FONT_EMPHASIS_MARK_DOT:
                        eFontEmphasisMark = FontEmphasisMark::Dot;
                        break;
                    case primitive2d::TEXT_FONT_EMPHASIS_MARK_CIRCLE:
                        eFontEmphasisMark = FontEmphasisMark::Circle;
                        break;
                    case primitive2d::TEXT_FONT_EMPHASIS_MARK_DISC:
                        eFontEmphasisMark = FontEmphasisMark::Disc;
                        break;
                    case primitive2d::TEXT_FONT_EMPHASIS_MARK_ACCENT:
                        eFontEmphasisMark = FontEmphasisMark::Accent;
                        break;
                }

                if (eFontEmphasisMark != FontEmphasisMark::NONE)
                {
                    DBG_ASSERT((pTCPP->getEmphasisMarkAbove() != pTCPP->getEmphasisMarkBelow()),
                               "DrawingLayer: Bad EmphasisMark position!");
                    if (pTCPP->getEmphasisMarkAbove())
                        eFontEmphasisMark |= FontEmphasisMark::PosAbove;
                    else
                        eFontEmphasisMark |= FontEmphasisMark::PosBelow;
                    aFont.SetEmphasisMark(eFontEmphasisMark);
                }

                // set Relief attribute
                FontRelief eFontRelief = FontRelief::NONE;
                switch (pTCPP->getTextRelief())
                {
                    default:
                        SAL_WARN("drawinglayer", "Unknown Relief style " << pTCPP->getTextRelief());
                        [[fallthrough]];
                    case primitive2d::TEXT_RELIEF_NONE:
                        eFontRelief = FontRelief::NONE;
                        break;
                    case primitive2d::TEXT_RELIEF_EMBOSSED:
                        eFontRelief = FontRelief::Embossed;
                        break;
                    case primitive2d::TEXT_RELIEF_ENGRAVED:
                        eFontRelief = FontRelief::Engraved;
                        break;
                }

                if (eFontRelief != FontRelief::NONE)
                    aFont.SetRelief(eFontRelief);

                // set Shadow attribute
                if (pTCPP->getShadow())
                    aFont.SetShadow(true);
            }

            // create integer DXArray
            KernArray aDXArray;

            if (!rTextCandidate.getDXArray().empty())
            {
                double fPixelVectorFactor(1.0);
                if (bScaleFont)
                {
                    const basegfx::B2DVector aPixelVector(maCurrentTransformation
                                                          * basegfx::B2DVector(1.0, 0.0));
                    fPixelVectorFactor = aPixelVector.getLength();
                }

                aDXArray.reserve(rTextCandidate.getDXArray().size());
                for (auto const& elem : rTextCandidate.getDXArray())
                    aDXArray.push_back(elem * fPixelVectorFactor);
            }

            // set parameters and paint text snippet
            const basegfx::BColor aRGBFontColor(
                maBColorModifierStack.getModifiedColor(rTextCandidate.getFontColor()));

            // Store previous complex text layout state, to be restored after drawing
            const vcl::text::ComplexTextLayoutFlags nOldLayoutMode(mpOutputDevice->GetLayoutMode());

            if (rTextCandidate.getFontAttribute().getRTL())
            {
                vcl::text::ComplexTextLayoutFlags nRTLLayoutMode(
                    nOldLayoutMode & ~vcl::text::ComplexTextLayoutFlags::BiDiStrong);
                nRTLLayoutMode |= vcl::text::ComplexTextLayoutFlags::BiDiRtl
                                  | vcl::text::ComplexTextLayoutFlags::TextOriginLeft;
                mpOutputDevice->SetLayoutMode(nRTLLayoutMode);
            }
            else
            {
                // tdf#101686: This is LTR text, but the output device may have RTL state.
                vcl::text::ComplexTextLayoutFlags nLTRLayoutMode(nOldLayoutMode);
                nLTRLayoutMode = nLTRLayoutMode & ~vcl::text::ComplexTextLayoutFlags::BiDiRtl;
                nLTRLayoutMode = nLTRLayoutMode & ~vcl::text::ComplexTextLayoutFlags::BiDiStrong;
                mpOutputDevice->SetLayoutMode(nLTRLayoutMode);
            }

            Point aStartPoint;
            bool bChangeMapMode(false);
            if (!bScaleFont)
            {
                basegfx::B2DHomMatrix aCombinedTransform(
                    getTransformFromMapMode(mpOutputDevice->GetMapMode())
                    * maCurrentTransformation);

                basegfx::B2DVector aCurrentScaling, aCurrentTranslate;
                double fCurrentRotate;
                aCombinedTransform.decompose(aCurrentScaling, aCurrentTranslate, fCurrentRotate,
                                             fIgnoreShearX);

                const Point aOrigin(
                    basegfx::fround<tools::Long>(aCurrentTranslate.getX() / aCurrentScaling.getX()),
                    basegfx::fround<tools::Long>(aCurrentTranslate.getY()
                                                 / aCurrentScaling.getY()));

                Fraction aScaleX(aCurrentScaling.getX());
                if (!aScaleX.IsValid())
                {
                    SAL_WARN("drawinglayer", "invalid X Scale");
                    return;
                }

                Fraction aScaleY(aCurrentScaling.getY());
                if (!aScaleY.IsValid())
                {
                    SAL_WARN("drawinglayer", "invalid Y Scale");
                    return;
                }

                MapMode aMapMode(mpOutputDevice->GetMapMode().GetMapUnit(), aOrigin, aScaleX,
                                 aScaleY);

                if (fCurrentRotate)
                    aTextTranslate *= basegfx::utils::createRotateB2DHomMatrix(fCurrentRotate);
                aStartPoint = Point(basegfx::fround<tools::Long>(aTextTranslate.getX()),
                                    basegfx::fround<tools::Long>(aTextTranslate.getY()));

                bChangeMapMode = aMapMode != mpOutputDevice->GetMapMode();
                if (bChangeMapMode)
                {
                    mpOutputDevice->Push(vcl::PushFlags::MAPMODE);
                    mpOutputDevice->SetRelativeMapMode(aMapMode);
                }
            }
            else
            {
                const basegfx::B2DPoint aPoint(aLocalTransform * basegfx::B2DPoint(0.0, 0.0));
                double aPointX = aPoint.getX(), aPointY = aPoint.getY();

                if (!comphelper::LibreOfficeKit::isActive())
                {
                    // aFont has an integer size; we must scale a bit for precision
                    double nFontScalingFixY = aFontScaling.getY() / aResultFontSize.Height();
                    double nFontScalingFixX
                        = aFontScaling.getX()
                          / (aResultFontSize.Width() ? aResultFontSize.Width()
                                                     : aResultFontSize.Height());

#ifdef _WIN32
                    if (aResultFontSize.Width()
                        && aResultFontSize.Width() != aResultFontSize.Height())
                    {
                        // See getVclFontFromFontAttribute in drawinglayer/source/primitive2d/textlayoutdevice.cxx
                        vcl::Font aUnscaledTest(aFont);
                        aUnscaledTest.SetFontSize({ 0, aResultFontSize.Height() });
                        const FontMetric aUnscaledFontMetric(
                            Application::GetDefaultDevice()->GetFontMetric(aUnscaledTest));
                        if (aUnscaledFontMetric.GetAverageFontWidth() > 0)
                        {
                            double nExistingXScale = static_cast<double>(aResultFontSize.Width())
                                                     / aUnscaledFontMetric.GetAverageFontWidth();
                            nFontScalingFixX
                                = aFontScaling.getX() / aFontScaling.getY() / nExistingXScale;
                        }
                    }
#endif

                    if (!rtl_math_approxEqual(nFontScalingFixY, 1.0)
                        || !rtl_math_approxEqual(nFontScalingFixX, 1.0))
                    {
                        MapMode aMapMode = mpOutputDevice->GetMapMode();
                        aMapMode.SetScaleX(aMapMode.GetScaleX() * nFontScalingFixX);
                        aMapMode.SetScaleY(aMapMode.GetScaleY() * nFontScalingFixY);

                        const bool bValidScaling
                            = aMapMode.GetScaleX().IsValid() && aMapMode.GetScaleY().IsValid();
                        if (!bValidScaling)
                            SAL_WARN("drawinglayer", "skipping invalid scaling");
                        else
                        {
                            Point origin = aMapMode.GetOrigin();

                            mpOutputDevice->Push(vcl::PushFlags::MAPMODE);
                            mpOutputDevice->SetRelativeMapMode(aMapMode);
                            bChangeMapMode = true;

                            aPointX = (aPointX + origin.X()) / nFontScalingFixX - origin.X();
                            aPointY = (aPointY + origin.Y()) / nFontScalingFixY - origin.Y();
                        }
                    }
                }

                aStartPoint = Point(basegfx::fround<tools::Long>(aPointX),
                                    basegfx::fround<tools::Long>(aPointY));
            }

            // tdf#152990 set the font after the MapMode is (potentially) set so canvas uses the desired
            // font size
            mpOutputDevice->SetFont(aFont);
            mpOutputDevice->SetTextColor(Color(aRGBFontColor));

            if (!aDXArray.empty())
            {
                const SalLayoutGlyphs* pGlyphs = SalLayoutGlyphsCache::self()->GetLayoutGlyphs(
                    mpOutputDevice, rTextCandidate.getText(), rTextCandidate.getTextPosition(),
                    rTextCandidate.getTextLength());
                mpOutputDevice->DrawTextArray(
                    aStartPoint, rTextCandidate.getText(), aDXArray,
                    rTextCandidate.getKashidaArray(), rTextCandidate.getTextPosition(),
                    rTextCandidate.getTextLength(), SalLayoutFlags::NONE, pGlyphs);
            }
            else
            {
                mpOutputDevice->DrawText(aStartPoint, rTextCandidate.getText(),
                                         rTextCandidate.getTextPosition(),
                                         rTextCandidate.getTextLength());
            }

            // Restore previous layout mode
            mpOutputDevice->SetLayoutMode(nOldLayoutMode);

            if (bChangeMapMode)
                mpOutputDevice->Pop();

            bPrimitiveAccepted = true;
        }
    }

    if (!bPrimitiveAccepted)
    {
        // let break down
        process(rTextCandidate);
    }
}

// direct draw of hairline
void VclProcessor2D::RenderPolygonHairlinePrimitive2D(
    const primitive2d::PolygonHairlinePrimitive2D& rPolygonCandidate, bool bPixelBased)
{
    const basegfx::BColor aHairlineColor(
        maBColorModifierStack.getModifiedColor(rPolygonCandidate.getBColor()));
    mpOutputDevice->SetLineColor(Color(aHairlineColor));
    mpOutputDevice->SetFillColor();

    basegfx::B2DPolygon aLocalPolygon(rPolygonCandidate.getB2DPolygon());
    aLocalPolygon.transform(maCurrentTransformation);

    if (bPixelBased && getViewInformation2D().getPixelSnapHairline())
    {
        // #i98289#
        // when a Hairline is painted and AntiAliasing is on the option SnapHorVerLinesToDiscrete
        // allows to suppress AntiAliasing for pure horizontal or vertical lines. This is done since
        // not-AntiAliased such lines look more pleasing to the eye (e.g. 2D chart content). This
        // NEEDS to be done in discrete coordinates, so only useful for pixel based rendering.
        aLocalPolygon = basegfx::utils::snapPointsOfHorizontalOrVerticalEdges(aLocalPolygon);
    }

    mpOutputDevice->DrawPolyLine(aLocalPolygon, 0.0);
}

// direct draw of transformed BitmapEx primitive
void VclProcessor2D::RenderBitmapPrimitive2D(const primitive2d::BitmapPrimitive2D& rBitmapCandidate)
{
    BitmapEx aBitmapEx(rBitmapCandidate.getBitmap());
    const basegfx::B2DHomMatrix aLocalTransform(maCurrentTransformation
                                                * rBitmapCandidate.getTransform());

    if (maBColorModifierStack.count())
    {
        aBitmapEx = aBitmapEx.ModifyBitmapEx(maBColorModifierStack);

        if (aBitmapEx.IsEmpty())
        {
            // color gets completely replaced, get it
            const basegfx::BColor aModifiedColor(
                maBColorModifierStack.getModifiedColor(basegfx::BColor()));
            basegfx::B2DPolygon aPolygon(basegfx::utils::createUnitPolygon());
            aPolygon.transform(aLocalTransform);

            mpOutputDevice->SetFillColor(Color(aModifiedColor));
            mpOutputDevice->SetLineColor();
            mpOutputDevice->DrawPolygon(aPolygon);

            return;
        }
    }

    // #122923# do no longer add Alpha channel here; the right place to do this is when really
    // the own transformer is used (see OutputDevice::DrawTransformedBitmapEx).

    // draw using OutputDevice'sDrawTransformedBitmapEx
    mpOutputDevice->DrawTransformedBitmapEx(aLocalTransform, aBitmapEx);
}

void VclProcessor2D::RenderFillGraphicPrimitive2D(
    const primitive2d::FillGraphicPrimitive2D& rFillBitmapCandidate)
{
    if (rFillBitmapCandidate.getTransparency() < 0.0
        || rFillBitmapCandidate.getTransparency() > 1.0)
    {
        // invalid transparence, done
        return;
    }

    bool bPrimitiveAccepted = RenderFillGraphicPrimitive2DImpl(rFillBitmapCandidate);

    if (!bPrimitiveAccepted)
    {
        // do not accept, use decomposition
        process(rFillBitmapCandidate);
    }
}

bool VclProcessor2D::RenderFillGraphicPrimitive2DImpl(
    const primitive2d::FillGraphicPrimitive2D& rFillBitmapCandidate)
{
    const attribute::FillGraphicAttribute& rFillGraphicAttribute(
        rFillBitmapCandidate.getFillGraphic());

    // #121194# when tiling is used and content is bitmap-based, do direct tiling in the
    // renderer on pixel base to ensure tight fitting. Do not do this when
    // the fill is rotated or sheared.
    if (!rFillGraphicAttribute.getTiling())
        return false;

    // content is bitmap(ex)
    //
    // for Vector Graphic Data (SVG, EMF+) support, force decomposition when present. This will lead to use
    // the primitive representation of the vector data directly.
    //
    // when graphic is animated, force decomposition to use the correct graphic, else
    // fill style will not be animated
    if (GraphicType::Bitmap != rFillGraphicAttribute.getGraphic().GetType()
        || rFillGraphicAttribute.getGraphic().getVectorGraphicData()
        || rFillGraphicAttribute.getGraphic().IsAnimated())
        return false;

    // decompose matrix to check for shear, rotate and mirroring
    basegfx::B2DHomMatrix aLocalTransform(maCurrentTransformation
                                          * rFillBitmapCandidate.getTransformation());
    basegfx::B2DVector aScale, aTranslate;
    double fRotate, fShearX;
    aLocalTransform.decompose(aScale, aTranslate, fRotate, fShearX);

    // when nopt rotated/sheared
    if (!basegfx::fTools::equalZero(fRotate) || !basegfx::fTools::equalZero(fShearX))
        return false;

    // no shear or rotate, draw direct in pixel coordinates

    // transform object range to device coordinates (pixels). Use
    // the device transformation for better accuracy
    basegfx::B2DRange aObjectRange(aTranslate, aTranslate + aScale);
    aObjectRange.transform(mpOutputDevice->GetViewTransformation());

    // extract discrete size of object
    const sal_Int32 nOWidth(basegfx::fround(aObjectRange.getWidth()));
    const sal_Int32 nOHeight(basegfx::fround(aObjectRange.getHeight()));

    // only do something when object has a size in discrete units
    if (nOWidth <= 0 || nOHeight <= 0)
        return true;

    // transform graphic range to device coordinates (pixels). Use
    // the device transformation for better accuracy
    basegfx::B2DRange aGraphicRange(rFillGraphicAttribute.getGraphicRange());
    aGraphicRange.transform(mpOutputDevice->GetViewTransformation() * aLocalTransform);

    // extract discrete size of graphic
    // caution: when getting to zero, nothing would be painted; thus, do not allow this
    const sal_Int32 nBWidth(std::max(sal_Int32(1), basegfx::fround(aGraphicRange.getWidth())));
    const sal_Int32 nBHeight(std::max(sal_Int32(1), basegfx::fround(aGraphicRange.getHeight())));

    // nBWidth, nBHeight is the pixel size of the needed bitmap. To not need to scale it
    // in vcl many times, create a size-optimized version
    const Size aNeededBitmapSizePixel(nBWidth, nBHeight);
    BitmapEx aBitmapEx(rFillGraphicAttribute.getGraphic().GetBitmapEx());
    const bool bPreScaled(nBWidth * nBHeight < (250 * 250));

    // ... but only up to a maximum size, else it gets too expensive
    if (bPreScaled)
    {
        // if color depth is below 24bit, expand before scaling for better quality.
        // This is even needed for low colors, else the scale will produce
        // a bitmap in gray or Black/White (!)
        if (isPalettePixelFormat(aBitmapEx.getPixelFormat()))
        {
            aBitmapEx.Convert(BmpConversion::N24Bit);
        }

        aBitmapEx.Scale(aNeededBitmapSizePixel, BmpScaleFlag::Interpolate);
    }

    if (rFillBitmapCandidate.hasTransparency())
        aBitmapEx.BlendAlpha(
            static_cast<sal_uInt8>(255 - (rFillBitmapCandidate.getTransparency() * 255)));

    if (maBColorModifierStack.count())
    {
        // when color modifier, apply to bitmap
        aBitmapEx = aBitmapEx.ModifyBitmapEx(maBColorModifierStack);

        // ModifyBitmapEx uses empty bitmap as sign to return that
        // the content will be completely replaced to mono color, use shortcut
        if (aBitmapEx.IsEmpty())
        {
            // color gets completely replaced, get it
            const basegfx::BColor aModifiedColor(
                maBColorModifierStack.getModifiedColor(basegfx::BColor()));
            basegfx::B2DPolygon aPolygon(basegfx::utils::createUnitPolygon());
            aPolygon.transform(aLocalTransform);

            mpOutputDevice->SetFillColor(Color(aModifiedColor));
            mpOutputDevice->SetLineColor();
            mpOutputDevice->DrawPolygon(aPolygon);

            return true;
        }
    }

    sal_Int32 nBLeft(basegfx::fround(aGraphicRange.getMinX()));
    sal_Int32 nBTop(basegfx::fround(aGraphicRange.getMinY()));
    const sal_Int32 nOLeft(basegfx::fround(aObjectRange.getMinX()));
    const sal_Int32 nOTop(basegfx::fround(aObjectRange.getMinY()));
    sal_Int32 nPosX(0);
    sal_Int32 nPosY(0);

    if (nBLeft > nOLeft)
    {
        const sal_Int32 nDiff((nBLeft / nBWidth) + 1);

        nPosX -= nDiff;
        nBLeft -= nDiff * nBWidth;
    }

    if (nBLeft + nBWidth <= nOLeft)
    {
        const sal_Int32 nDiff(-nBLeft / nBWidth);

        nPosX += nDiff;
        nBLeft += nDiff * nBWidth;
    }

    if (nBTop > nOTop)
    {
        const sal_Int32 nDiff((nBTop / nBHeight) + 1);

        nPosY -= nDiff;
        nBTop -= nDiff * nBHeight;
    }

    if (nBTop + nBHeight <= nOTop)
    {
        const sal_Int32 nDiff(-nBTop / nBHeight);

        nPosY += nDiff;
        nBTop += nDiff * nBHeight;
    }

    // prepare OutDev
    const Point aEmptyPoint(0, 0);
    // the visible rect, in pixels
    const ::tools::Rectangle aVisiblePixel(aEmptyPoint, mpOutputDevice->GetOutputSizePixel());
    const bool bWasEnabled(mpOutputDevice->IsMapModeEnabled());
    mpOutputDevice->EnableMapMode(false);

    // check if offset is used
    const sal_Int32 nOffsetX(basegfx::fround(rFillGraphicAttribute.getOffsetX() * nBWidth));
    const sal_Int32 nOffsetY(basegfx::fround(rFillGraphicAttribute.getOffsetY() * nBHeight));

    // if the tile is a single pixel big, just flood fill with that pixel color
    if (nOffsetX == 0 && nOffsetY == 0 && aNeededBitmapSizePixel.getWidth() == 1
        && aNeededBitmapSizePixel.getHeight() == 1)
    {
        Color col = aBitmapEx.GetPixelColor(0, 0);
        mpOutputDevice->SetLineColor(col);
        mpOutputDevice->SetFillColor(col);
        mpOutputDevice->DrawRect(aVisiblePixel);
    }
    else if (nOffsetX)
    {
        // offset in X, so iterate over Y first and draw lines
        for (sal_Int32 nYPos(nBTop); nYPos < nOTop + nOHeight; nYPos += nBHeight, nPosY++)
        {
            for (sal_Int32 nXPos((nPosY % 2) ? nBLeft - nBWidth + nOffsetX : nBLeft);
                 nXPos < nOLeft + nOWidth; nXPos += nBWidth)
            {
                const ::tools::Rectangle aOutRectPixel(Point(nXPos, nYPos), aNeededBitmapSizePixel);

                if (aOutRectPixel.Overlaps(aVisiblePixel))
                {
                    if (bPreScaled)
                    {
                        mpOutputDevice->DrawBitmapEx(aOutRectPixel.TopLeft(), aBitmapEx);
                    }
                    else
                    {
                        mpOutputDevice->DrawBitmapEx(aOutRectPixel.TopLeft(),
                                                     aNeededBitmapSizePixel, aBitmapEx);
                    }
                }
            }
        }
    }
    else // nOffsetY is used
    {
        // possible offset in Y, so iterate over X first and draw columns
        for (sal_Int32 nXPos(nBLeft); nXPos < nOLeft + nOWidth; nXPos += nBWidth, nPosX++)
        {
            for (sal_Int32 nYPos((nPosX % 2) ? nBTop - nBHeight + nOffsetY : nBTop);
                 nYPos < nOTop + nOHeight; nYPos += nBHeight)
            {
                const ::tools::Rectangle aOutRectPixel(Point(nXPos, nYPos), aNeededBitmapSizePixel);

                if (aOutRectPixel.Overlaps(aVisiblePixel))
                {
                    if (bPreScaled)
                    {
                        mpOutputDevice->DrawBitmapEx(aOutRectPixel.TopLeft(), aBitmapEx);
                    }
                    else
                    {
                        mpOutputDevice->DrawBitmapEx(aOutRectPixel.TopLeft(),
                                                     aNeededBitmapSizePixel, aBitmapEx);
                    }
                }
            }
        }
    }

    // restore OutDev
    mpOutputDevice->EnableMapMode(bWasEnabled);
    return true;
}

// direct draw of Graphic
void VclProcessor2D::RenderPolyPolygonGraphicPrimitive2D(
    const primitive2d::PolyPolygonGraphicPrimitive2D& rPolygonCandidate)
{
    bool bDone(false);
    const basegfx::B2DPolyPolygon& rPolyPolygon = rPolygonCandidate.getB2DPolyPolygon();

    // #121194# Todo: check if this works
    if (!rPolyPolygon.count())
    {
        // empty polyPolygon, done
        bDone = true;
    }
    else
    {
        const attribute::FillGraphicAttribute& rFillGraphicAttribute
            = rPolygonCandidate.getFillGraphic();

        // try to catch cases where the graphic will be color-modified to a single
        // color (e.g. shadow)
        switch (rFillGraphicAttribute.getGraphic().GetType())
        {
            case GraphicType::GdiMetafile:
            {
                // metafiles are potentially transparent, cannot optimize, not done
                break;
            }
            case GraphicType::Bitmap:
            {
                if (!rFillGraphicAttribute.getGraphic().IsTransparent()
                    && !rFillGraphicAttribute.getGraphic().IsAlpha()
                    && !rPolygonCandidate.hasTransparency())
                {
                    // bitmap is not transparent and has no alpha
                    const sal_uInt32 nBColorModifierStackCount(maBColorModifierStack.count());

                    if (nBColorModifierStackCount)
                    {
                        const basegfx::BColorModifierSharedPtr& rTopmostModifier
                            = maBColorModifierStack.getBColorModifier(nBColorModifierStackCount
                                                                      - 1);
                        const basegfx::BColorModifier_replace* pReplacer
                            = dynamic_cast<const basegfx::BColorModifier_replace*>(
                                rTopmostModifier.get());

                        if (pReplacer)
                        {
                            // the bitmap fill is in unified color, so we can replace it with
                            // a single polygon fill. The form of the fill depends on tiling
                            if (rFillGraphicAttribute.getTiling())
                            {
                                // with tiling, fill the whole tools::PolyPolygon with the modifier color
                                basegfx::B2DPolyPolygon aLocalPolyPolygon(rPolyPolygon);

                                aLocalPolyPolygon.transform(maCurrentTransformation);
                                mpOutputDevice->SetLineColor();
                                mpOutputDevice->SetFillColor(Color(pReplacer->getBColor()));
                                mpOutputDevice->DrawPolyPolygon(aLocalPolyPolygon);
                            }
                            else
                            {
                                // without tiling, only the area common to the bitmap tile and the
                                // tools::PolyPolygon is filled. Create the bitmap tile area in object
                                // coordinates. For this, the object transformation needs to be created
                                // from the already scaled PolyPolygon. The tile area in object
                                // coordinates will always be non-rotated, so it's not necessary to
                                // work with a polygon here
                                basegfx::B2DRange aTileRange(
                                    rFillGraphicAttribute.getGraphicRange());
                                const basegfx::B2DRange aPolyPolygonRange(
                                    rPolyPolygon.getB2DRange());
                                const basegfx::B2DHomMatrix aNewObjectTransform(
                                    basegfx::utils::createScaleTranslateB2DHomMatrix(
                                        aPolyPolygonRange.getRange(),
                                        aPolyPolygonRange.getMinimum()));

                                aTileRange.transform(aNewObjectTransform);

                                // now clip the object polyPolygon against the tile range
                                // to get the common area
                                basegfx::B2DPolyPolygon aTarget
                                    = basegfx::utils::clipPolyPolygonOnRange(
                                        rPolyPolygon, aTileRange, true, false);

                                if (aTarget.count())
                                {
                                    aTarget.transform(maCurrentTransformation);
                                    mpOutputDevice->SetLineColor();
                                    mpOutputDevice->SetFillColor(Color(pReplacer->getBColor()));
                                    mpOutputDevice->DrawPolyPolygon(aTarget);
                                }
                            }

                            // simplified output executed, we are done
                            bDone = true;
                        }
                    }
                }
                break;
            }
            default: //GraphicType::NONE, GraphicType::Default
            {
                // empty graphic, we are done
                bDone = true;
                break;
            }
        }
    }

    if (!bDone)
    {
        // use default decomposition
        process(rPolygonCandidate);
    }
}

// mask group
void VclProcessor2D::RenderMaskPrimitive2DPixel(const primitive2d::MaskPrimitive2D& rMaskCandidate)
{
    if (rMaskCandidate.getChildren().empty())
        return;

    basegfx::B2DPolyPolygon aMask(rMaskCandidate.getMask());

    if (!aMask.count())
        return;

    aMask.transform(maCurrentTransformation);

    // Unless smooth edges are needed, simply use clipping.
    if (basegfx::utils::isRectangle(aMask) || !getViewInformation2D().getUseAntiAliasing())
    {
        mpOutputDevice->Push(vcl::PushFlags::CLIPREGION);
        mpOutputDevice->IntersectClipRegion(vcl::Region(aMask));
        process(rMaskCandidate.getChildren());
        mpOutputDevice->Pop();
        return;
    }

    const basegfx::B2DRange aRange(basegfx::utils::getRange(aMask));
    impBufferDevice aBufferDevice(*mpOutputDevice, aRange);

    if (!aBufferDevice.isVisible())
        return;

    // remember last OutDev and set to content
    OutputDevice* pLastOutputDevice = mpOutputDevice;
    mpOutputDevice = &aBufferDevice.getContent();

    // paint to it
    process(rMaskCandidate.getChildren());

    // back to old OutDev
    mpOutputDevice = pLastOutputDevice;

    // draw mask
    VirtualDevice& rMask = aBufferDevice.getTransparence();
    rMask.SetLineColor();
    rMask.SetFillColor(COL_BLACK);
    rMask.DrawPolyPolygon(aMask);

    // dump buffer to outdev
    aBufferDevice.paint();
}

// modified color group. Force output to unified color.
void VclProcessor2D::RenderModifiedColorPrimitive2D(
    const primitive2d::ModifiedColorPrimitive2D& rModifiedCandidate)
{
    if (!rModifiedCandidate.getChildren().empty())
    {
        maBColorModifierStack.push(rModifiedCandidate.getColorModifier());
        process(rModifiedCandidate.getChildren());
        maBColorModifierStack.pop();
    }
}

// unified sub-transparence. Draw to VDev first.
void VclProcessor2D::RenderUnifiedTransparencePrimitive2D(
    const primitive2d::UnifiedTransparencePrimitive2D& rTransCandidate)
{
    if (rTransCandidate.getChildren().empty())
        return;

    if (0.0 == rTransCandidate.getTransparence())
    {
        // no transparence used, so just use the content
        process(rTransCandidate.getChildren());
    }
    else if (rTransCandidate.getTransparence() > 0.0 && rTransCandidate.getTransparence() < 1.0)
    {
        // transparence is in visible range
        basegfx::B2DRange aRange(rTransCandidate.getChildren().getB2DRange(getViewInformation2D()));
        aRange.transform(maCurrentTransformation);
        impBufferDevice aBufferDevice(*mpOutputDevice, aRange);

        if (aBufferDevice.isVisible())
        {
            // remember last OutDev and set to content
            OutputDevice* pLastOutputDevice = mpOutputDevice;
            mpOutputDevice = &aBufferDevice.getContent();

            // paint content to it
            process(rTransCandidate.getChildren());

            // back to old OutDev
            mpOutputDevice = pLastOutputDevice;

            // dump buffer to outdev using given transparence
            aBufferDevice.paint(rTransCandidate.getTransparence());
        }
    }
}

// sub-transparence group. Draw to VDev first.
void VclProcessor2D::RenderTransparencePrimitive2D(
    const primitive2d::TransparencePrimitive2D& rTransCandidate)
{
    if (rTransCandidate.getChildren().empty())
        return;

    basegfx::B2DRange aRange(rTransCandidate.getChildren().getB2DRange(getViewInformation2D()));
    aRange.transform(maCurrentTransformation);
    impBufferDevice aBufferDevice(*mpOutputDevice, aRange);

    if (!aBufferDevice.isVisible())
        return;

    // remember last OutDev and set to content
    OutputDevice* pLastOutputDevice = mpOutputDevice;
    mpOutputDevice = &aBufferDevice.getContent();

    // paint content to it
    process(rTransCandidate.getChildren());

    // set to mask
    mpOutputDevice = &aBufferDevice.getTransparence();

    // when painting transparence masks, reset the color stack
    basegfx::BColorModifierStack aLastBColorModifierStack(maBColorModifierStack);
    maBColorModifierStack = basegfx::BColorModifierStack();

    // paint mask to it (always with transparence intensities, evtl. with AA)
    process(rTransCandidate.getTransparence());

    // back to old color stack
    maBColorModifierStack = std::move(aLastBColorModifierStack);

    // back to old OutDev
    mpOutputDevice = pLastOutputDevice;

    // dump buffer to outdev
    aBufferDevice.paint();
}

// transform group.
void VclProcessor2D::RenderTransformPrimitive2D(
    const primitive2d::TransformPrimitive2D& rTransformCandidate)
{
    // remember current transformation and ViewInformation
    const basegfx::B2DHomMatrix aLastCurrentTransformation(maCurrentTransformation);
    const geometry::ViewInformation2D aLastViewInformation2D(getViewInformation2D());

    // create new transformations for CurrentTransformation
    // and for local ViewInformation2D
    maCurrentTransformation = maCurrentTransformation * rTransformCandidate.getTransformation();
    geometry::ViewInformation2D aViewInformation2D(getViewInformation2D());
    aViewInformation2D.setObjectTransformation(getViewInformation2D().getObjectTransformation()
                                               * rTransformCandidate.getTransformation());
    updateViewInformation(aViewInformation2D);

    // process content
    process(rTransformCandidate.getChildren());

    // restore transformations
    maCurrentTransformation = aLastCurrentTransformation;
    updateViewInformation(aLastViewInformation2D);
}

// new XDrawPage for ViewInformation2D
void VclProcessor2D::RenderPagePreviewPrimitive2D(
    const primitive2d::PagePreviewPrimitive2D& rPagePreviewCandidate)
{
    // remember current transformation and ViewInformation
    const geometry::ViewInformation2D aLastViewInformation2D(getViewInformation2D());

    // create new local ViewInformation2D
    geometry::ViewInformation2D aViewInformation2D(getViewInformation2D());
    aViewInformation2D.setVisualizedPage(rPagePreviewCandidate.getXDrawPage());
    updateViewInformation(aViewInformation2D);

    // process decomposed content
    process(rPagePreviewCandidate);

    // restore transformations
    updateViewInformation(aLastViewInformation2D);
}

// marker
void VclProcessor2D::RenderMarkerArrayPrimitive2D(
    const primitive2d::MarkerArrayPrimitive2D& rMarkArrayCandidate)
{
    // get data
    const std::vector<basegfx::B2DPoint>& rPositions = rMarkArrayCandidate.getPositions();
    const sal_uInt32 nCount(rPositions.size());

    if (!nCount || rMarkArrayCandidate.getMarker().IsEmpty())
        return;

    // get pixel size
    const BitmapEx& rMarker(rMarkArrayCandidate.getMarker());
    const Size aBitmapSize(rMarker.GetSizePixel());

    if (!(aBitmapSize.Width() && aBitmapSize.Height()))
        return;

    // get discrete half size
    const basegfx::B2DVector aDiscreteHalfSize((aBitmapSize.getWidth() - 1.0) * 0.5,
                                               (aBitmapSize.getHeight() - 1.0) * 0.5);
    const bool bWasEnabled(mpOutputDevice->IsMapModeEnabled());

    // do not forget evtl. moved origin in target device MapMode when
    // switching it off; it would be missing and lead to wrong positions.
    // All his could be done using logic sizes and coordinates, too, but
    // we want a 1:1 bitmap rendering here, so it's more safe and faster
    // to work with switching off MapMode usage completely.
    const Point aOrigin(mpOutputDevice->GetMapMode().GetOrigin());

    mpOutputDevice->EnableMapMode(false);

    for (auto const& pos : rPositions)
    {
        const basegfx::B2DPoint aDiscreteTopLeft((maCurrentTransformation * pos)
                                                 - aDiscreteHalfSize);
        const Point aDiscretePoint(basegfx::fround<tools::Long>(aDiscreteTopLeft.getX()),
                                   basegfx::fround<tools::Long>(aDiscreteTopLeft.getY()));

        mpOutputDevice->DrawBitmapEx(aDiscretePoint + aOrigin, rMarker);
    }

    mpOutputDevice->EnableMapMode(bWasEnabled);
}

// point
void VclProcessor2D::RenderPointArrayPrimitive2D(
    const primitive2d::PointArrayPrimitive2D& rPointArrayCandidate)
{
    const std::vector<basegfx::B2DPoint>& rPositions = rPointArrayCandidate.getPositions();
    const basegfx::BColor aRGBColor(
        maBColorModifierStack.getModifiedColor(rPointArrayCandidate.getRGBColor()));
    const Color aVCLColor(aRGBColor);

    for (auto const& pos : rPositions)
    {
        const basegfx::B2DPoint aViewPosition(maCurrentTransformation * pos);
        const Point aPos(basegfx::fround<tools::Long>(aViewPosition.getX()),
                         basegfx::fround<tools::Long>(aViewPosition.getY()));

        mpOutputDevice->DrawPixel(aPos, aVCLColor);
    }
}

void VclProcessor2D::RenderPolygonStrokePrimitive2D(
    const primitive2d::PolygonStrokePrimitive2D& rPolygonStrokeCandidate)
{
    // #i101491# method restructured to clearly use the DrawPolyLine
    // calls starting from a defined line width
    const attribute::LineAttribute& rLineAttribute = rPolygonStrokeCandidate.getLineAttribute();
    const double fLineWidth(rLineAttribute.getWidth());
    bool bDone(false);

    if (fLineWidth > 0.0)
    {
        const basegfx::B2DVector aDiscreteUnit(maCurrentTransformation
                                               * basegfx::B2DVector(fLineWidth, 0.0));
        const double fDiscreteLineWidth(aDiscreteUnit.getLength());
        const attribute::StrokeAttribute& rStrokeAttribute
            = rPolygonStrokeCandidate.getStrokeAttribute();
        const basegfx::BColor aHairlineColor(
            maBColorModifierStack.getModifiedColor(rLineAttribute.getColor()));
        basegfx::B2DPolyPolygon aHairlinePolyPolygon;

        mpOutputDevice->SetLineColor(Color(aHairlineColor));
        mpOutputDevice->SetFillColor();

        if (0.0 == rStrokeAttribute.getFullDotDashLen())
        {
            // no line dashing, just copy
            aHairlinePolyPolygon.append(rPolygonStrokeCandidate.getB2DPolygon());
        }
        else
        {
            // else apply LineStyle
            basegfx::utils::applyLineDashing(
                rPolygonStrokeCandidate.getB2DPolygon(), rStrokeAttribute.getDotDashArray(),
                &aHairlinePolyPolygon, nullptr, rStrokeAttribute.getFullDotDashLen());
        }

        const sal_uInt32 nCount(aHairlinePolyPolygon.count());

        if (nCount)
        {
            const bool bAntiAliased(getViewInformation2D().getUseAntiAliasing());
            aHairlinePolyPolygon.transform(maCurrentTransformation);

            if (bAntiAliased)
            {
                if (basegfx::fTools::lessOrEqual(fDiscreteLineWidth, 1.0))
                {
                    // line in range ]0.0 .. 1.0[
                    // paint as simple hairline
                    for (sal_uInt32 a(0); a < nCount; a++)
                    {
                        mpOutputDevice->DrawPolyLine(aHairlinePolyPolygon.getB2DPolygon(a), 0.0);
                    }

                    bDone = true;
                }
                else if (basegfx::fTools::lessOrEqual(fDiscreteLineWidth, 2.0))
                {
                    // line in range [1.0 .. 2.0[
                    // paint as 2x2 with dynamic line distance
                    basegfx::B2DHomMatrix aMat;
                    const double fDistance(fDiscreteLineWidth - 1.0);
                    const double fHalfDistance(fDistance * 0.5);

                    for (sal_uInt32 a(0); a < nCount; a++)
                    {
                        basegfx::B2DPolygon aCandidate(aHairlinePolyPolygon.getB2DPolygon(a));

                        aMat.set(0, 2, -fHalfDistance);
                        aMat.set(1, 2, -fHalfDistance);
                        aCandidate.transform(aMat);
                        mpOutputDevice->DrawPolyLine(aCandidate, 0.0);

                        aMat.set(0, 2, fDistance);
                        aMat.set(1, 2, 0.0);
                        aCandidate.transform(aMat);
                        mpOutputDevice->DrawPolyLine(aCandidate, 0.0);

                        aMat.set(0, 2, 0.0);
                        aMat.set(1, 2, fDistance);
                        aCandidate.transform(aMat);
                        mpOutputDevice->DrawPolyLine(aCandidate, 0.0);

                        aMat.set(0, 2, -fDistance);
                        aMat.set(1, 2, 0.0);
                        aCandidate.transform(aMat);
                        mpOutputDevice->DrawPolyLine(aCandidate, 0.0);
                    }

                    bDone = true;
                }
                else if (basegfx::fTools::lessOrEqual(fDiscreteLineWidth, 3.0))
                {
                    // line in range [2.0 .. 3.0]
                    // paint as cross in a 3x3  with dynamic line distance
                    basegfx::B2DHomMatrix aMat;
                    const double fDistance((fDiscreteLineWidth - 1.0) * 0.5);

                    for (sal_uInt32 a(0); a < nCount; a++)
                    {
                        basegfx::B2DPolygon aCandidate(aHairlinePolyPolygon.getB2DPolygon(a));

                        mpOutputDevice->DrawPolyLine(aCandidate, 0.0);

                        aMat.set(0, 2, -fDistance);
                        aMat.set(1, 2, 0.0);
                        aCandidate.transform(aMat);
                        mpOutputDevice->DrawPolyLine(aCandidate, 0.0);

                        aMat.set(0, 2, fDistance);
                        aMat.set(1, 2, -fDistance);
                        aCandidate.transform(aMat);
                        mpOutputDevice->DrawPolyLine(aCandidate, 0.0);

                        aMat.set(0, 2, fDistance);
                        aMat.set(1, 2, fDistance);
                        aCandidate.transform(aMat);
                        mpOutputDevice->DrawPolyLine(aCandidate, 0.0);

                        aMat.set(0, 2, -fDistance);
                        aMat.set(1, 2, fDistance);
                        aCandidate.transform(aMat);
                        mpOutputDevice->DrawPolyLine(aCandidate, 0.0);
                    }

                    bDone = true;
                }
                else
                {
                    // #i101491# line width above 3.0
                }
            }
            else
            {
                if (basegfx::fTools::lessOrEqual(fDiscreteLineWidth, 1.5))
                {
                    // line width below 1.5, draw the basic hairline polygon
                    for (sal_uInt32 a(0); a < nCount; a++)
                    {
                        mpOutputDevice->DrawPolyLine(aHairlinePolyPolygon.getB2DPolygon(a), 0.0);
                    }

                    bDone = true;
                }
                else if (basegfx::fTools::lessOrEqual(fDiscreteLineWidth, 2.5))
                {
                    // line width is in range ]1.5 .. 2.5], use four hairlines
                    // drawn in a square
                    for (sal_uInt32 a(0); a < nCount; a++)
                    {
                        basegfx::B2DPolygon aCandidate(aHairlinePolyPolygon.getB2DPolygon(a));
                        basegfx::B2DHomMatrix aMat;

                        mpOutputDevice->DrawPolyLine(aCandidate, 0.0);

                        aMat.set(0, 2, 1.0);
                        aMat.set(1, 2, 0.0);
                        aCandidate.transform(aMat);

                        mpOutputDevice->DrawPolyLine(aCandidate, 0.0);

                        aMat.set(0, 2, 0.0);
                        aMat.set(1, 2, 1.0);
                        aCandidate.transform(aMat);

                        mpOutputDevice->DrawPolyLine(aCandidate, 0.0);

                        aMat.set(0, 2, -1.0);
                        aMat.set(1, 2, 0.0);
                        aCandidate.transform(aMat);

                        mpOutputDevice->DrawPolyLine(aCandidate, 0.0);
                    }

                    bDone = true;
                }
                else
                {
                    // #i101491# line width is above 2.5
                }
            }

            if (!bDone && rPolygonStrokeCandidate.getB2DPolygon().count() > 1000)
            {
                // #i101491# If the polygon complexity uses more than a given amount, do
                // use OutputDevice::DrawPolyLine directly; this will avoid buffering all
                // decompositions in primitives (memory) and fallback to old line painting
                // for very complex polygons, too
                for (sal_uInt32 a(0); a < nCount; a++)
                {
                    mpOutputDevice->DrawPolyLine(aHairlinePolyPolygon.getB2DPolygon(a),
                                                 fDiscreteLineWidth, rLineAttribute.getLineJoin(),
                                                 rLineAttribute.getLineCap(),
                                                 rLineAttribute.getMiterMinimumAngle());
                }

                bDone = true;
            }
        }
    }

    if (!bDone)
    {
        // remember that we enter a PolygonStrokePrimitive2D decomposition,
        // used for AA thick line drawing
        mnPolygonStrokePrimitive2D++;

        // line width is big enough for standard filled polygon visualisation or zero
        process(rPolygonStrokeCandidate);

        // leave PolygonStrokePrimitive2D
        mnPolygonStrokePrimitive2D--;
    }
}

void VclProcessor2D::RenderEpsPrimitive2D(const primitive2d::EpsPrimitive2D& rEpsPrimitive2D)
{
    // The new decomposition of Metafiles made it necessary to add an Eps
    // primitive to handle embedded Eps data. On some devices, this can be
    // painted directly (mac, printer).
    // To be able to handle the replacement correctly, i need to handle it myself
    // since DrawEPS will not be able e.g. to rotate the replacement. To be able
    // to do that, i added a boolean return to OutputDevice::DrawEPS(..)
    // to know when EPS was handled directly already.
    basegfx::B2DRange aRange(0.0, 0.0, 1.0, 1.0);
    aRange.transform(maCurrentTransformation * rEpsPrimitive2D.getEpsTransform());

    if (aRange.isEmpty())
        return;

    const ::tools::Rectangle aRectangle(static_cast<sal_Int32>(floor(aRange.getMinX())),
                                        static_cast<sal_Int32>(floor(aRange.getMinY())),
                                        static_cast<sal_Int32>(ceil(aRange.getMaxX())),
                                        static_cast<sal_Int32>(ceil(aRange.getMaxY())));

    if (aRectangle.IsEmpty())
        return;

    bool bWillReallyRender = mpOutputDevice->IsDeviceOutputNecessary();
    // try to paint EPS directly without fallback visualisation
    const bool bEPSPaintedDirectly
        = bWillReallyRender
          && mpOutputDevice->DrawEPS(aRectangle.TopLeft(), aRectangle.GetSize(),
                                     rEpsPrimitive2D.getGfxLink());

    if (!bEPSPaintedDirectly)
    {
        // use the decomposition which will correctly handle the
        // fallback visualisation using full transformation (e.g. rotation)
        process(rEpsPrimitive2D);
    }
}

void VclProcessor2D::RenderSvgLinearAtomPrimitive2D(
    const primitive2d::SvgLinearAtomPrimitive2D& rCandidate)
{
    const double fDelta(rCandidate.getOffsetB() - rCandidate.getOffsetA());

    if (fDelta <= 0.0)
        return;

    const basegfx::BColor aColorA(maBColorModifierStack.getModifiedColor(rCandidate.getColorA()));
    const basegfx::BColor aColorB(maBColorModifierStack.getModifiedColor(rCandidate.getColorB()));

    // calculate discrete unit in WorldCoordinates; use diagonal (1.0, 1.0) and divide by sqrt(2)
    const basegfx::B2DVector aDiscreteVector(
        getViewInformation2D().getInverseObjectToViewTransformation()
        * basegfx::B2DVector(1.0, 1.0));
    const double fDiscreteUnit(aDiscreteVector.getLength() * (1.0 / M_SQRT2));

    // use color distance and discrete lengths to calculate step count
    const sal_uInt32 nSteps(calculateStepsForSvgGradient(aColorA, aColorB, fDelta, fDiscreteUnit));

    // switch off line painting
    mpOutputDevice->SetLineColor();

    // prepare polygon in needed width at start position (with discrete overlap)
    const basegfx::B2DPolygon aPolygon(basegfx::utils::createPolygonFromRect(
        basegfx::B2DRange(rCandidate.getOffsetA() - fDiscreteUnit, 0.0,
                          rCandidate.getOffsetA() + (fDelta / nSteps) + fDiscreteUnit, 1.0)));

    // prepare loop ([0.0 .. 1.0[)
    double fUnitScale(0.0);
    const double fUnitStep(1.0 / nSteps);

    // loop and paint
    for (sal_uInt32 a(0); a < nSteps; a++, fUnitScale += fUnitStep)
    {
        basegfx::B2DPolygon aNew(aPolygon);

        aNew.transform(maCurrentTransformation
                       * basegfx::utils::createTranslateB2DHomMatrix(fDelta * fUnitScale, 0.0));
        mpOutputDevice->SetFillColor(Color(basegfx::interpolate(aColorA, aColorB, fUnitScale)));
        mpOutputDevice->DrawPolyPolygon(basegfx::B2DPolyPolygon(aNew));
    }
}

void VclProcessor2D::RenderSvgRadialAtomPrimitive2D(
    const primitive2d::SvgRadialAtomPrimitive2D& rCandidate)
{
    const double fDeltaScale(rCandidate.getScaleB() - rCandidate.getScaleA());

    if (fDeltaScale <= 0.0)
        return;

    const basegfx::BColor aColorA(maBColorModifierStack.getModifiedColor(rCandidate.getColorA()));
    const basegfx::BColor aColorB(maBColorModifierStack.getModifiedColor(rCandidate.getColorB()));

    // calculate discrete unit in WorldCoordinates; use diagonal (1.0, 1.0) and divide by sqrt(2)
    const basegfx::B2DVector aDiscreteVector(
        getViewInformation2D().getInverseObjectToViewTransformation()
        * basegfx::B2DVector(1.0, 1.0));
    const double fDiscreteUnit(aDiscreteVector.getLength() * (1.0 / M_SQRT2));

    // use color distance and discrete lengths to calculate step count
    const sal_uInt32 nSteps(
        calculateStepsForSvgGradient(aColorA, aColorB, fDeltaScale, fDiscreteUnit));

    // switch off line painting
    mpOutputDevice->SetLineColor();

    // prepare loop ([0.0 .. 1.0[, full polygons, no polypolygons with holes)
    double fUnitScale(0.0);
    const double fUnitStep(1.0 / nSteps);

    for (sal_uInt32 a(0); a < nSteps; a++, fUnitScale += fUnitStep)
    {
        basegfx::B2DHomMatrix aTransform;
        const double fEndScale(rCandidate.getScaleB() - (fDeltaScale * fUnitScale));

        if (rCandidate.isTranslateSet())
        {
            const basegfx::B2DVector aTranslate(basegfx::interpolate(
                rCandidate.getTranslateB(), rCandidate.getTranslateA(), fUnitScale));

            aTransform = basegfx::utils::createScaleTranslateB2DHomMatrix(
                fEndScale, fEndScale, aTranslate.getX(), aTranslate.getY());
        }
        else
        {
            aTransform = basegfx::utils::createScaleB2DHomMatrix(fEndScale, fEndScale);
        }

        basegfx::B2DPolygon aNew(basegfx::utils::createPolygonFromUnitCircle());

        aNew.transform(maCurrentTransformation * aTransform);
        mpOutputDevice->SetFillColor(Color(basegfx::interpolate(aColorB, aColorA, fUnitScale)));
        mpOutputDevice->DrawPolyPolygon(basegfx::B2DPolyPolygon(aNew));
    }
}

void VclProcessor2D::adaptLineToFillDrawMode() const
{
    const DrawModeFlags nOriginalDrawMode(mpOutputDevice->GetDrawMode());

    if (!(nOriginalDrawMode
          & (DrawModeFlags::BlackLine | DrawModeFlags::GrayLine | DrawModeFlags::WhiteLine
             | DrawModeFlags::SettingsLine)))
        return;

    DrawModeFlags nAdaptedDrawMode(nOriginalDrawMode);

    if (nOriginalDrawMode & DrawModeFlags::BlackLine)
    {
        nAdaptedDrawMode |= DrawModeFlags::BlackFill;
    }
    else
    {
        nAdaptedDrawMode &= ~DrawModeFlags::BlackFill;
    }

    if (nOriginalDrawMode & DrawModeFlags::GrayLine)
    {
        nAdaptedDrawMode |= DrawModeFlags::GrayFill;
    }
    else
    {
        nAdaptedDrawMode &= ~DrawModeFlags::GrayFill;
    }

    if (nOriginalDrawMode & DrawModeFlags::WhiteLine)
    {
        nAdaptedDrawMode |= DrawModeFlags::WhiteFill;
    }
    else
    {
        nAdaptedDrawMode &= ~DrawModeFlags::WhiteFill;
    }

    if (nOriginalDrawMode & DrawModeFlags::SettingsLine)
    {
        nAdaptedDrawMode |= DrawModeFlags::SettingsFill;
    }
    else
    {
        nAdaptedDrawMode &= ~DrawModeFlags::SettingsFill;
    }

    mpOutputDevice->SetDrawMode(nAdaptedDrawMode);
}

void VclProcessor2D::adaptTextToFillDrawMode() const
{
    const DrawModeFlags nOriginalDrawMode(mpOutputDevice->GetDrawMode());
    if (!(nOriginalDrawMode
          & (DrawModeFlags::BlackText | DrawModeFlags::GrayText | DrawModeFlags::WhiteText
             | DrawModeFlags::SettingsText)))
        return;

    DrawModeFlags nAdaptedDrawMode(nOriginalDrawMode);

    if (nOriginalDrawMode & DrawModeFlags::BlackText)
    {
        nAdaptedDrawMode |= DrawModeFlags::BlackFill;
    }
    else
    {
        nAdaptedDrawMode &= ~DrawModeFlags::BlackFill;
    }

    if (nOriginalDrawMode & DrawModeFlags::GrayText)
    {
        nAdaptedDrawMode |= DrawModeFlags::GrayFill;
    }
    else
    {
        nAdaptedDrawMode &= ~DrawModeFlags::GrayFill;
    }

    if (nOriginalDrawMode & DrawModeFlags::WhiteText)
    {
        nAdaptedDrawMode |= DrawModeFlags::WhiteFill;
    }
    else
    {
        nAdaptedDrawMode &= ~DrawModeFlags::WhiteFill;
    }

    if (nOriginalDrawMode & DrawModeFlags::SettingsText)
    {
        nAdaptedDrawMode |= DrawModeFlags::SettingsFill;
    }
    else
    {
        nAdaptedDrawMode &= ~DrawModeFlags::SettingsFill;
    }

    mpOutputDevice->SetDrawMode(nAdaptedDrawMode);
}

void VclProcessor2D::updateViewInformation(const geometry::ViewInformation2D& rViewInformation2D)
{
    if (rViewInformation2D == getViewInformation2D())
        return;

    // call parent to actually set
    BaseProcessor2D::updateViewInformation(rViewInformation2D);

    // apply AntiAlias information to target device
    if (getViewInformation2D().getUseAntiAliasing())
        mpOutputDevice->SetAntialiasing(mpOutputDevice->GetAntialiasing()
                                        | AntialiasingFlags::Enable);
    else
        mpOutputDevice->SetAntialiasing(mpOutputDevice->GetAntialiasing()
                                        & ~AntialiasingFlags::Enable);
}

// process support
VclProcessor2D::VclProcessor2D(const geometry::ViewInformation2D& rViewInformation,
                               OutputDevice& rOutDev)
    : BaseProcessor2D(rViewInformation)
    , mpOutputDevice(&rOutDev)
    , maBColorModifierStack()
    , mnPolygonStrokePrimitive2D(0)
    , mnOriginalAA(rOutDev.GetAntialiasing())
{
    // set digit language, derived from SvtCTLOptions to have the correct
    // number display for arabic/hindi numerals
    rOutDev.SetDigitLanguage(drawinglayer::detail::getDigitLanguage());

    // NOTE: to save/restore original AntiAliasing mode we need
    // to use mnOriginalAA here - OutputDevice::Push/Pop does not
    // offer that
}

VclProcessor2D::~VclProcessor2D() { mpOutputDevice->SetAntialiasing(mnOriginalAA); }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
