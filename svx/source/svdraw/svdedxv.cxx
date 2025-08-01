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

#include <com/sun/star/i18n/WordType.hpp>
#include <editeng/editdata.hxx>
#include <editeng/editeng.hxx>
#include <editeng/editobj.hxx>
#include <editeng/editstat.hxx>
#include <editeng/outlobj.hxx>
#include <editeng/unotext.hxx>
#include <o3tl/deleter.hxx>
#include <officecfg/Office/Common.hxx>
#include <svl/itemiter.hxx>
#include <svl/style.hxx>
#include <svl/whiter.hxx>
#include <svx/sdtfchim.hxx>
#include <svx/selectioncontroller.hxx>
#include <svx/svdedxv.hxx>
#include <svx/svdetc.hxx>
#include <svx/svdotable.hxx>
#include <svx/svdotext.hxx>
#include <svx/svdoutl.hxx>
#include <svx/svdpage.hxx>
#include <svx/svdpagv.hxx>
#include <svx/svdundo.hxx>
#include <vcl/canvastools.hxx>
#include <vcl/commandevent.hxx>
#include <vcl/cursor.hxx>
#include <vcl/dndlistenercontainer.hxx>
#include <vcl/weld.hxx>
#include <vcl/window.hxx>
#include <comphelper/lok.hxx>
#include <basegfx/polygon/b2dpolygontools.hxx>
#include <drawinglayer/processor2d/baseprocessor2d.hxx>
#include <drawinglayer/primitive2d/maskprimitive2d.hxx>
#include <drawinglayer/processor2d/processor2dtools.hxx>
#include <drawinglayer/primitive2d/PolyPolygonColorPrimitive2D.hxx>
#include <editeng/outliner.hxx>
#include <sal/log.hxx>
#include <sdr/overlay/overlaytools.hxx>
#include <sfx2/viewsh.hxx>
#include <svx/dialmgr.hxx>
#include <svx/sdr/overlay/overlaymanager.hxx>
#include <svx/sdr/overlay/overlayselection.hxx>
#include <svx/sdr/table/tablecontroller.hxx>
#include <svx/sdrpagewindow.hxx>
#include <svx/sdrpaintwindow.hxx>
#include <svx/sdrundomanager.hxx>
#include <svx/strings.hrc>
#include <svx/svdviter.hxx>
#include <svtools/optionsdrawinglayer.hxx>
#include <textchain.hxx>
#include <textchaincursor.hxx>
#include <tools/debug.hxx>
#include <vcl/svapp.hxx>
#include <svx/sdr/contact/viewcontact.hxx>

#include <memory>

SdrObjEditView::SdrObjEditView(SdrModel& rSdrModel, OutputDevice* pOut)
    : SdrGlueEditView(rSdrModel, pOut)
    , maTEOverlayGroup()
    , maTextEditUpdateTimer("TextEditUpdateTimer")
    , mxWeakTextEditObj()
    , mpTextEditPV(nullptr)
    , mpTextEditOutlinerView(nullptr)
    , mpTextEditWin(nullptr)
    , m_pTextEditCursorBuffer(nullptr)
    , m_pMacroObj(nullptr)
    , m_pMacroPV(nullptr)
    , m_pMacroWin(nullptr)
    , m_aTextEditArea()
    , m_aMinTextEditArea()
    , m_aOldCalcFieldValueLink()
    , m_aMacroDownPos()
    , m_nMacroTol(0)
    , mbTextEditDontDelete(false)
    , mbTextEditOnlyOneView(false)
    , mbTextEditNewObj(false)
    , mbQuickTextEditMode(true)
    , mbMacroDown(false)
    , mbInteractiveSlideShow(false)
    , mxSelectionController()
    , mxLastSelectionController()
    , mpOldTextEditUndoManager(nullptr)
    , mpLocalTextEditUndoManager()
{
    // init some timer settings (not starting it of course)
    maTextEditUpdateTimer.SetTimeout(EDIT_UPDATEDATA_TIMEOUT);
    maTextEditUpdateTimer.SetInvokeHandler(LINK(this, SdrObjEditView, TextEditUpdate));
}

IMPL_LINK_NOARG(SdrObjEditView, ImpModifyHdl, LinkParamNone*, void)
{
    // IASS: active TextEdit had a model change. Check and react.
    if (nullptr == mpTextEditOutliner)
        // no Outliner, no TextEdit
        return;

    if (!mxWeakTextEditObj.get().is())
        // no TextObject, no TextEdit
        return;

    // reset & restart the timer
    maTextEditUpdateTimer.SetTimeout(EDIT_UPDATEDATA_TIMEOUT);
    maTextEditUpdateTimer.Start();
}

IMPL_LINK_NOARG(SdrObjEditView, TextEditUpdate, Timer*, void)
{
    // IASS: text was changed and EDIT_UPDATEDATA_TIMEOUT has passed
    // since last user input
    maTextEditUpdateTimer.Stop();

    // be safe: still in TextEdit?
    if (nullptr == mpTextEditOutliner)
        // no Outliner, no TextEdit
        return;

    if (!mxWeakTextEditObj.get().is())
        // no TextObject, no TextEdit
        return;

    // launch an ObjectChange: This is the straightforward method
    // to get this broadcasted. We do not risk to set the model
    // unwantedly to changed, we had a text edit going on already.
    // This is needed for SlideShow since it is not (yet) using the
    // standard schema with VC/VOC/OC
    if (isInteractiveSlideShow())
        mxWeakTextEditObj.get()->BroadcastObjectChange();

    // force repaint for objects with changed text in all views
    // that are VC/VOC/OC based (SlideShow is not yet)
    sdr::contact::ViewContact& rVC(mxWeakTextEditObj.get()->GetViewContact());

    if (!rVC.hasMultipleViewObjectContacts())
        // only one VOC -> this is us
        return;

    if (nullptr == mpTextEditPV)
        // should not happen, just invalidate all visualizations
        rVC.ActionChanged();
    else
        // invalidate only visualizations in different views:
        // this is important to not cause evtl. high repaint costs
        // in the EditView -> we avoid this by running the TextEdit
        // on the overlay. NOTE: This is only for better performance,
        // any repaint will just work fine and do the right thing
        rVC.ActionChangedIfDifferentPageView(*mpTextEditPV);
}

SdrObjEditView::~SdrObjEditView()
{
    maTextEditUpdateTimer.Stop();
    mpTextEditWin = nullptr; // so there's no ShowCursor in SdrEndTextEdit
    assert(!IsTextEdit());
    if (IsTextEdit())
        suppress_fun_call_w_exception(SdrEndTextEdit());
    mpTextEditOutliner.reset();
    assert(nullptr == mpOldTextEditUndoManager); // should have been reset
}

bool SdrObjEditView::IsAction() const { return IsMacroObj() || SdrGlueEditView::IsAction(); }

void SdrObjEditView::MovAction(const Point& rPnt)
{
    if (IsMacroObj())
        MovMacroObj(rPnt);
    SdrGlueEditView::MovAction(rPnt);
}

void SdrObjEditView::EndAction()
{
    if (IsMacroObj())
        EndMacroObj();
    SdrGlueEditView::EndAction();
}

void SdrObjEditView::BckAction()
{
    BrkMacroObj();
    SdrGlueEditView::BckAction();
}

void SdrObjEditView::BrkAction()
{
    BrkMacroObj();
    SdrGlueEditView::BrkAction();
}

SdrPageView* SdrObjEditView::ShowSdrPage(SdrPage* pPage)
{
    SdrPageView* pPageView = SdrGlueEditView::ShowSdrPage(pPage);

    if (comphelper::LibreOfficeKit::isActive() && pPageView)
    {
        // Check if other views have an active text edit on the same page as
        // this one.
        SdrViewIter::ForAllViews(pPageView->GetPage(), [this](SdrView* pView) {
            if (pView == this || !pView->IsTextEdit())
                return;

            OutputDevice* pOutDev = GetFirstOutputDevice();
            if (!pOutDev || pOutDev->GetOutDevType() != OUTDEV_WINDOW)
                return;

            // Found one, so create an outliner view, to get invalidations when
            // the text edit changes.
            // Call GetSfxViewShell() to make sure ImpMakeOutlinerView()
            // registers the view shell of this draw view, and not the view
            // shell of pView.
            OutlinerView* pOutlinerView
                = pView->ImpMakeOutlinerView(pOutDev->GetOwnerWindow(), nullptr, GetSfxViewShell());
            pOutlinerView->HideCursor();
            pView->GetTextEditOutliner()->InsertView(pOutlinerView);
        });
    }

    return pPageView;
}

namespace
{
/// Removes outliner views registered in other draw views that use pOutputDevice.
void lcl_RemoveTextEditOutlinerViews(SdrObjEditView const* pThis, SdrPageView const* pPageView,
                                     OutputDevice const* pOutputDevice)
{
    if (!comphelper::LibreOfficeKit::isActive())
        return;

    if (!pPageView)
        return;

    if (!pOutputDevice || pOutputDevice->GetOutDevType() != OUTDEV_WINDOW)
        return;

    SdrViewIter::ForAllViews(pPageView->GetPage(), [&pThis, &pOutputDevice](SdrView* pView) {
        if (pView == pThis || !pView->IsTextEdit())
            return;

        SdrOutliner* pOutliner = pView->GetTextEditOutliner();
        for (size_t nView = 0; nView < pOutliner->GetViewCount(); ++nView)
        {
            OutlinerView* pOutlinerView = pOutliner->GetView(nView);
            if (pOutlinerView->GetWindow()->GetOutDev() != pOutputDevice)
                continue;

            pOutliner->RemoveView(pOutlinerView);
            delete pOutlinerView;
        }
    });
}
}

void SdrObjEditView::HideSdrPage()
{
    lcl_RemoveTextEditOutlinerViews(this, GetSdrPageView(), GetFirstOutputDevice());

    if (mpTextEditPV == GetSdrPageView())
    {
        // HideSdrPage() will clear mpPageView, avoid a dangling pointer.
        mpTextEditPV = nullptr;
    }

    SdrGlueEditView::HideSdrPage();
}

void SdrObjEditView::TakeActionRect(tools::Rectangle& rRect) const
{
    if (IsMacroObj())
    {
        rRect = m_pMacroObj->GetCurrentBoundRect();
    }
    else
    {
        SdrGlueEditView::TakeActionRect(rRect);
    }
}

void SdrObjEditView::Notify(SfxBroadcaster& rBC, const SfxHint& rHint)
{
    SdrGlueEditView::Notify(rBC, rHint);
    if (mpTextEditOutliner == nullptr)
        return;

    // change of printer while editing
    if (rHint.GetId() != SfxHintId::ThisIsAnSdrHint)
        return;

    const SdrHint* pSdrHint = static_cast<const SdrHint*>(&rHint);
    SdrHintKind eKind = pSdrHint->GetKind();
    if (eKind == SdrHintKind::RefDeviceChange)
    {
        mpTextEditOutliner->SetRefDevice(GetModel().GetRefDevice());
    }
    if (eKind == SdrHintKind::DefaultTabChange)
    {
        mpTextEditOutliner->SetDefTab(GetModel().GetDefaultTabulator());
    }
}

void SdrObjEditView::ModelHasChanged()
{
    SdrGlueEditView::ModelHasChanged();
    rtl::Reference<SdrTextObj> pTextObj = mxWeakTextEditObj.get();
    if (pTextObj && !pTextObj->IsInserted())
        SdrEndTextEdit(); // object deleted
    // TextEditObj changed?
    if (!IsTextEdit())
        return;

    if (pTextObj != nullptr)
    {
        size_t nOutlViewCnt = mpTextEditOutliner->GetViewCount();
        bool bAreaChg = false;
        bool bAnchorChg = false;
        bool bColorChg = false;
        bool bContourFrame = pTextObj->IsContourTextFrame();
        EEAnchorMode eNewAnchor(EEAnchorMode::VCenterHCenter);
        tools::Rectangle aOldArea(m_aMinTextEditArea);
        aOldArea.Union(m_aTextEditArea);
        Color aNewColor;
        { // check area
            Size aPaperMin1;
            Size aPaperMax1;
            tools::Rectangle aEditArea1;
            tools::Rectangle aMinArea1;
            pTextObj->TakeTextEditArea(&aPaperMin1, &aPaperMax1, &aEditArea1, &aMinArea1);
            Point aPvOfs(pTextObj->GetTextEditOffset());

            // add possible GridOffset to up-to-now view-independent EditAreas
            basegfx::B2DVector aGridOffset(0.0, 0.0);
            if (getPossibleGridOffsetForSdrObject(aGridOffset, pTextObj.get(), GetSdrPageView()))
            {
                const Point aOffset(basegfx::fround<tools::Long>(aGridOffset.getX()),
                                    basegfx::fround<tools::Long>(aGridOffset.getY()));

                aEditArea1 += aOffset;
                aMinArea1 += aOffset;
            }

            aEditArea1.Move(aPvOfs.X(), aPvOfs.Y());
            aMinArea1.Move(aPvOfs.X(), aPvOfs.Y());
            tools::Rectangle aNewArea(aMinArea1);
            aNewArea.Union(aEditArea1);

            if (aNewArea != aOldArea || aEditArea1 != m_aTextEditArea
                || aMinArea1 != m_aMinTextEditArea
                || mpTextEditOutliner->GetMinAutoPaperSize() != aPaperMin1
                || mpTextEditOutliner->GetMaxAutoPaperSize() != aPaperMax1)
            {
                m_aTextEditArea = aEditArea1;
                m_aMinTextEditArea = aMinArea1;

                const bool bPrevUpdateLayout = mpTextEditOutliner->SetUpdateLayout(false);
                mpTextEditOutliner->SetMinAutoPaperSize(aPaperMin1);
                mpTextEditOutliner->SetMaxAutoPaperSize(aPaperMax1);
                mpTextEditOutliner->SetPaperSize(Size(0, 0)); // re-format Outliner

                if (!bContourFrame)
                {
                    mpTextEditOutliner->ClearPolygon();
                    EEControlBits nStat = mpTextEditOutliner->GetControlWord();
                    nStat |= EEControlBits::AUTOPAGESIZE;
                    mpTextEditOutliner->SetControlWord(nStat);
                }
                else
                {
                    EEControlBits nStat = mpTextEditOutliner->GetControlWord();
                    nStat &= ~EEControlBits::AUTOPAGESIZE;
                    mpTextEditOutliner->SetControlWord(nStat);
                    tools::Rectangle aAnchorRect;
                    pTextObj->TakeTextAnchorRect(aAnchorRect);
                    pTextObj->ImpSetContourPolygon(*mpTextEditOutliner, aAnchorRect, true);
                }
                for (size_t nOV = 0; nOV < nOutlViewCnt; nOV++)
                {
                    OutlinerView* pOLV = mpTextEditOutliner->GetView(nOV);
                    EVControlBits nStat0 = pOLV->GetControlWord();
                    EVControlBits nStat = nStat0;
                    // AutoViewSize only if not ContourFrame.
                    if (!bContourFrame)
                        nStat |= EVControlBits::AUTOSIZE;
                    else
                        nStat &= ~EVControlBits::AUTOSIZE;
                    if (nStat != nStat0)
                        pOLV->SetControlWord(nStat);
                }

                mpTextEditOutliner->SetUpdateLayout(bPrevUpdateLayout);
                bAreaChg = true;
            }
        }
        if (mpTextEditOutlinerView != nullptr)
        { // check fill and anchor
            EEAnchorMode eOldAnchor = mpTextEditOutlinerView->GetAnchorMode();
            eNewAnchor = pTextObj->GetOutlinerViewAnchorMode();
            bAnchorChg = eOldAnchor != eNewAnchor;
            Color aOldColor(mpTextEditOutlinerView->GetBackgroundColor());
            aNewColor = GetTextEditBackgroundColor(*this);
            bColorChg = aOldColor != aNewColor;
        }
        // refresh always when it's a contour frame. That
        // refresh is necessary since it triggers the repaint
        // which makes the Handles visible. Changes at TakeTextRect()
        // seem to have resulted in a case where no refresh is executed.
        // Before that, a refresh must have been always executed
        // (else this error would have happened earlier), thus I
        // even think here a refresh should be done always.
        // Since follow-up problems cannot even be guessed I only
        // add this one more case to the if below.
        // BTW: It's VERY bad style that here, inside ModelHasChanged()
        // the outliner is again massively changed for the text object
        // in text edit mode. Normally, all necessary data should be
        // set at SdrBeginTextEdit(). Some changes and value assigns in
        // SdrBeginTextEdit() are completely useless since they are set here
        // again on ModelHasChanged().
        if (bContourFrame || bAreaChg || bAnchorChg || bColorChg)
        {
            for (size_t nOV = 0; nOV < nOutlViewCnt; nOV++)
            {
                OutlinerView* pOLV = mpTextEditOutliner->GetView(nOV);
                vcl::Window* pWin = pOLV->GetWindow();
                { // invalidate old OutlinerView area
                    tools::Rectangle aTmpRect(aOldArea);
                    sal_uInt16 nPixSiz = pOLV->GetInvalidateMore() + 1;
                    Size aMore(pWin->PixelToLogic(Size(nPixSiz, nPixSiz)));
                    aTmpRect.AdjustLeft(-(aMore.Width()));
                    aTmpRect.AdjustRight(aMore.Width());
                    aTmpRect.AdjustTop(-(aMore.Height()));
                    aTmpRect.AdjustBottom(aMore.Height());
                    InvalidateOneWin(*pWin->GetOutDev(), aTmpRect);
                }
                if (bAnchorChg)
                    pOLV->SetAnchorMode(eNewAnchor);
                if (bColorChg)
                    pOLV->SetBackgroundColor(aNewColor);

                bool bWasCoursorVisible = pOLV->IsCursorVisible();
                vcl::Cursor* pOldCursor = pWin->GetCursor();
                pOLV->SetOutputArea(
                    m_aTextEditArea); // because otherwise, we're not re-anchoring correctly
                ImpInvalidateOutlinerView(*pOLV);
                // Undo SetOutputArea setting and showing the cursor
                if (!bWasCoursorVisible)
                    pOLV->HideCursor();
                pWin->SetCursor(pOldCursor);
            }
            mpTextEditOutlinerView->ShowCursor();
        }
    }
    ImpMakeTextCursorAreaVisible();
}

namespace
{
class TextEditFrameOverlayObject;
class TextEditHighContrastOverlaySelection;

/**
        Helper class to visualize the content of an active EditView as an
        OverlayObject. These objects work with Primitives and are handled
        from the OverlayManager(s) in place as needed.

        It allows complete visualization of the content of the active
        EditView without the need of Invalidates triggered by the EditView
        and thus avoiding potentially expensive repaints by using the
        automatically buffered Overlay mechanism.

        It buffers as much as possible locally and *only* triggers a real
        change (see call to objectChange()) when really needed.
     */
class TextEditOverlayObject : public sdr::overlay::OverlayObject
{
protected:
    /// local access to associated sdr::overlay::OverlaySelection
    std::unique_ptr<sdr::overlay::OverlaySelection> mxOverlayTransparentSelection;
    std::unique_ptr<TextEditHighContrastOverlaySelection> mxOverlayHighContrastSelection;
    std::unique_ptr<TextEditFrameOverlayObject> mxOverlayFrame;

    /// local definition depends on active OutlinerView
    OutlinerView& mrOutlinerView;

    /// geometry definitions with buffering
    basegfx::B2DRange maLastRange;
    basegfx::B2DRange maRange;

    /// text content definitions with buffering
    drawinglayer::primitive2d::Primitive2DContainer maTextPrimitives;
    drawinglayer::primitive2d::Primitive2DContainer maLastTextPrimitives;

    // geometry creation for OverlayObject, can use local *Last* values
    virtual drawinglayer::primitive2d::Primitive2DContainer
    createOverlayObjectPrimitive2DSequence() override;

public:
    TextEditOverlayObject(const Color& rColor, OutlinerView& rOutlinerView);
    virtual ~TextEditOverlayObject() override;

    sdr::overlay::OverlayObject* getOverlaySelection();
    sdr::overlay::OverlayObject* getOverlayFrame();

    const OutlinerView& getOutlinerView() const { return mrOutlinerView; }

    /// override to check conditions for last createOverlayObjectPrimitive2DSequence
    virtual drawinglayer::primitive2d::Primitive2DContainer
    getOverlayObjectPrimitive2DSequence() const override;

    // data write access. In this OverlayObject we only have the
    // callback that triggers detecting if something *has* changed
    void checkDataChange(const basegfx::B2DRange& rMinTextEditArea);
    void checkSelectionChange();

    const basegfx::B2DRange& getRange() const { return maRange; }
    const drawinglayer::primitive2d::Primitive2DContainer& getTextPrimitives() const
    {
        return maTextPrimitives;
    }
};

class TextEditFrameOverlayObject : public sdr::overlay::OverlayObject
{
private:
    const TextEditOverlayObject& mrTextEditOverlayObject;

    // geometry creation for OverlayObject, can use local *Last* values
    virtual drawinglayer::primitive2d::Primitive2DContainer
    createOverlayObjectPrimitive2DSequence() override;

public:
    TextEditFrameOverlayObject(const TextEditOverlayObject& rTextEditOverlayObject);
    using sdr::overlay::OverlayObject::objectChange;
    virtual ~TextEditFrameOverlayObject() override;
};

class TextEditHighContrastOverlaySelection : public sdr::overlay::OverlayObject
{
private:
    const TextEditOverlayObject& mrTextEditOverlayObject;
    std::vector<basegfx::B2DRange> maRanges;

    // geometry creation for OverlayObject, can use local *Last* values
    virtual drawinglayer::primitive2d::Primitive2DContainer
    createOverlayObjectPrimitive2DSequence() override;

public:
    TextEditHighContrastOverlaySelection(const TextEditOverlayObject& rTextEditOverlayObject);
    void setRanges(std::vector<basegfx::B2DRange>&& rNew);
    virtual ~TextEditHighContrastOverlaySelection() override;
};

TextEditHighContrastOverlaySelection::TextEditHighContrastOverlaySelection(
    const TextEditOverlayObject& rTextEditOverlayObject)
    : OverlayObject(rTextEditOverlayObject.getBaseColor())
    , mrTextEditOverlayObject(rTextEditOverlayObject)
{
    allowAntiAliase(rTextEditOverlayObject.allowsAntiAliase());
    // use selection colors in HighContrast mode
    mbHighContrastSelection = true;
}

void TextEditHighContrastOverlaySelection::setRanges(std::vector<basegfx::B2DRange>&& rNew)
{
    if (rNew != maRanges)
    {
        maRanges = std::move(rNew);
        objectChange();
    }
}

drawinglayer::primitive2d::Primitive2DContainer
TextEditHighContrastOverlaySelection::createOverlayObjectPrimitive2DSequence()
{
    drawinglayer::primitive2d::Primitive2DContainer aRetval;

    size_t nCount = maRanges.size();

    if (nCount)
    {
        basegfx::B2DPolyPolygon aClipPolyPolygon;

        basegfx::BColor aRGBColor(getBaseColor().getBColor());

        for (size_t a = 0; a < nCount; ++a)
            aClipPolyPolygon.append(basegfx::utils::createPolygonFromRect(maRanges[a]));

        // This is used in high contrast mode, we will render the selection
        // with the bg forced to the selection Highlight color and the fg color
        // forced to the HighlightText color
        aRetval.append(new drawinglayer::primitive2d::PolyPolygonColorPrimitive2D(
            basegfx::B2DPolyPolygon(
                basegfx::utils::createPolygonFromRect(aClipPolyPolygon.getB2DRange())),
            aRGBColor));
        aRetval.append(mrTextEditOverlayObject.getTextPrimitives());
        aRetval.append(new drawinglayer::primitive2d::MaskPrimitive2D(std::move(aClipPolyPolygon),
                                                                      std::move(aRetval)));
    }

    return aRetval;
}

TextEditHighContrastOverlaySelection::~TextEditHighContrastOverlaySelection()
{
    if (getOverlayManager())
    {
        getOverlayManager()->remove(*this);
    }
}

sdr::overlay::OverlayObject* TextEditOverlayObject::getOverlaySelection()
{
    if (mxOverlayTransparentSelection)
        return mxOverlayTransparentSelection.get();
    return mxOverlayHighContrastSelection.get();
}

sdr::overlay::OverlayObject* TextEditOverlayObject::getOverlayFrame()
{
    if (!mxOverlayFrame)
        mxOverlayFrame.reset(new TextEditFrameOverlayObject(*this));
    return mxOverlayFrame.get();
}

drawinglayer::primitive2d::Primitive2DContainer
TextEditOverlayObject::createOverlayObjectPrimitive2DSequence()
{
    drawinglayer::primitive2d::Primitive2DContainer aRetval;

    // add buffered TextPrimitives
    aRetval.append(maTextPrimitives);

    return aRetval;
}

drawinglayer::primitive2d::Primitive2DContainer
TextEditFrameOverlayObject::createOverlayObjectPrimitive2DSequence()
{
    drawinglayer::primitive2d::Primitive2DContainer aRetval;

    /// outer frame visualization
    const double fTransparence(SvtOptionsDrawinglayer::GetTransparentSelectionPercent() * 0.01);
    const sal_uInt16 nPixSiz(mrTextEditOverlayObject.getOutlinerView().GetInvalidateMore() - 1);

    aRetval.push_back(new drawinglayer::primitive2d::OverlayRectanglePrimitive(
        mrTextEditOverlayObject.getRange(), getBaseColor().getBColor(), fTransparence,
        std::max(6, nPixSiz - 2), // grow
        0.0, // shrink
        0.0));

    return aRetval;
}

TextEditOverlayObject::TextEditOverlayObject(const Color& rColor, OutlinerView& rOutlinerView)
    : OverlayObject(rColor)
    , mrOutlinerView(rOutlinerView)
{
    // no AA for TextEdit overlay
    allowAntiAliase(false);

    // create local OverlaySelection - this is an integral part of EditText
    // visualization
    if (Application::GetSettings().GetStyleSettings().GetHighContrastMode())
    {
        mxOverlayHighContrastSelection.reset(new TextEditHighContrastOverlaySelection(*this));
    }
    else
    {
        std::vector<basegfx::B2DRange> aEmptySelection{};
        mxOverlayTransparentSelection.reset(new sdr::overlay::OverlaySelection(
            sdr::overlay::OverlayType::Transparent, rColor, std::move(aEmptySelection), true));
    }
}

TextEditOverlayObject::~TextEditOverlayObject()
{
    mxOverlayTransparentSelection.reset();
    mxOverlayHighContrastSelection.reset();

    if (getOverlayManager())
    {
        getOverlayManager()->remove(*this);
    }
}

TextEditFrameOverlayObject::TextEditFrameOverlayObject(
    const TextEditOverlayObject& rTextEditOverlayObject)
    : OverlayObject(rTextEditOverlayObject.getBaseColor())
    , mrTextEditOverlayObject(rTextEditOverlayObject)
{
    allowAntiAliase(rTextEditOverlayObject.allowsAntiAliase());
    // use selection colors in HighContrast mode
    mbHighContrastSelection = true;
}

TextEditFrameOverlayObject::~TextEditFrameOverlayObject()
{
    if (getOverlayManager())
    {
        getOverlayManager()->remove(*this);
    }
}

drawinglayer::primitive2d::Primitive2DContainer
TextEditOverlayObject::getOverlayObjectPrimitive2DSequence() const
{
    if (!getPrimitive2DSequence().empty())
    {
        if (!maRange.equal(maLastRange) || maLastTextPrimitives != maTextPrimitives)
        {
            // conditions of last local decomposition have changed, delete to force new evaluation
            const_cast<TextEditOverlayObject*>(this)->resetPrimitive2DSequence();
        }
    }

    if (getPrimitive2DSequence().empty())
    {
        // remember new buffered values
        const_cast<TextEditOverlayObject*>(this)->maLastRange = maRange;
        const_cast<TextEditOverlayObject*>(this)->maLastTextPrimitives = maTextPrimitives;
    }

    // call base implementation
    return OverlayObject::getOverlayObjectPrimitive2DSequence();
}

void TextEditOverlayObject::checkDataChange(const basegfx::B2DRange& rMinTextEditArea)
{
    bool bObjectChange(false);

    // check current range
    const tools::Rectangle aOutArea(mrOutlinerView.GetOutputArea());
    basegfx::B2DRange aNewRange = vcl::unotools::b2DRectangleFromRectangle(aOutArea);
    aNewRange.expand(rMinTextEditArea);

    if (aNewRange != maRange)
    {
        maRange = aNewRange;
        bObjectChange = true;
    }

    // check if text primitives did change
    SdrOutliner* pSdrOutliner = dynamic_cast<SdrOutliner*>(&getOutlinerView().GetOutliner());

    if (pSdrOutliner)
    {
        // get TextPrimitives directly from active Outliner
        basegfx::B2DHomMatrix aNewTransformA;
        basegfx::B2DHomMatrix aNewTransformB;
        basegfx::B2DRange aClipRange;
        drawinglayer::primitive2d::Primitive2DContainer aNewTextPrimitives;

        // active Outliner is always in unified oriented coordinate system (currently)
        // so just translate to TopLeft of visible Range. Keep in mind that top-left
        // depends on vertical text and top-to-bottom text attributes
        const tools::Rectangle aVisArea(mrOutlinerView.GetVisArea());
        const bool bVerticalWriting(pSdrOutliner->IsVertical());
        const bool bTopToBottom(pSdrOutliner->IsTopToBottom());
        const double fStartInX(bVerticalWriting && bTopToBottom
                                   ? aOutArea.Right() - aVisArea.Left()
                                   : aOutArea.Left() - aVisArea.Left());
        const double fStartInY(bVerticalWriting && !bTopToBottom
                                   ? aOutArea.Bottom() - aVisArea.Top()
                                   : aOutArea.Top() - aVisArea.Top());

        aNewTransformB.translate(fStartInX, fStartInY);

        // get the current TextPrimitives. This is the most expensive part
        // of this mechanism, it *may* be possible to buffer layouted
        // primitives per ParaPortion with/in/dependent on the EditEngine
        // content if needed. For now, get and compare
        TextHierarchyBreakupBlockText aBreakup(*pSdrOutliner, aNewTransformA, aNewTransformB,
                                               aClipRange);
        pSdrOutliner->StripPortions(aBreakup);
        aNewTextPrimitives.append(aBreakup.getTextPortionPrimitives());

        if (aNewTextPrimitives != maTextPrimitives)
        {
            maTextPrimitives = std::move(aNewTextPrimitives);
            bObjectChange = true;
        }
    }

    if (bObjectChange)
    {
        // if there really *was* a change signal the OverlayManager to
        // refresh this object's visualization
        objectChange();

        if (mxOverlayFrame)
            mxOverlayFrame->objectChange();

        // on data change, always do a SelectionChange, too
        // since the selection is an integral part of text visualization
        checkSelectionChange();
    }
}

void TextEditOverlayObject::checkSelectionChange()
{
    if (!(getOverlaySelection() && getOverlayManager()))
        return;

    std::vector<tools::Rectangle> aLogicRects;
    std::vector<basegfx::B2DRange> aLogicRanges;
    const Size aLogicPixel(getOverlayManager()->getOutputDevice().PixelToLogic(Size(1, 1)));

    // get logic selection
    getOutlinerView().GetSelectionRectangles(aLogicRects);

    aLogicRanges.reserve(aLogicRects.size());
    for (const auto& aRect : aLogicRects)
    {
        // convert from logic Rectangles to logic Ranges, do not forget to add
        // one Unit (in this case logical units for one pixel, pre-calculated)
        aLogicRanges.emplace_back(
            aRect.Left() - aLogicPixel.Width(), aRect.Top() - aLogicPixel.Height(),
            aRect.Right() + aLogicPixel.Width(), aRect.Bottom() + aLogicPixel.Height());
    }

    if (mxOverlayTransparentSelection)
        mxOverlayTransparentSelection->setRanges(std::move(aLogicRanges));
    else
        mxOverlayHighContrastSelection->setRanges(std::move(aLogicRanges));
}
} // end of anonymous namespace

// TextEdit

// callback from the active EditView, forward to evtl. existing instances of the
// TextEditOverlayObject(s). This will additionally update the selection which
// is an integral part of the text visualization
void SdrObjEditView::EditViewInvalidate(const tools::Rectangle&)
{
    if (!IsTextEdit())
        return;

    // MinTextRange may have changed. Forward it, too
    const basegfx::B2DRange aMinTextRange
        = vcl::unotools::b2DRectangleFromRectangle(m_aMinTextEditArea);

    for (sal_uInt32 a(0); a < maTEOverlayGroup.count(); a++)
    {
        TextEditOverlayObject* pCandidate
            = dynamic_cast<TextEditOverlayObject*>(&maTEOverlayGroup.getOverlayObject(a));

        if (pCandidate)
        {
            pCandidate->checkDataChange(aMinTextRange);
        }
    }
}

// callback from the active EditView, forward to evtl. existing instances of the
// TextEditOverlayObject(s). This cvall *only* updates the selection visualization
// which is e.g. used when only the selection is changed, but not the text
void SdrObjEditView::EditViewSelectionChange()
{
    if (!IsTextEdit())
        return;

    for (sal_uInt32 a(0); a < maTEOverlayGroup.count(); a++)
    {
        TextEditOverlayObject* pCandidate
            = dynamic_cast<TextEditOverlayObject*>(&maTEOverlayGroup.getOverlayObject(a));

        if (pCandidate)
        {
            pCandidate->checkSelectionChange();
        }
    }
}

OutputDevice& SdrObjEditView::EditViewOutputDevice() const { return *mpTextEditWin->GetOutDev(); }

Point SdrObjEditView::EditViewPointerPosPixel() const
{
    return mpTextEditWin->GetPointerPosPixel();
}

css::uno::Reference<css::datatransfer::clipboard::XClipboard> SdrObjEditView::GetClipboard() const
{
    if (!mpTextEditWin)
        return nullptr;
    return mpTextEditWin->GetClipboard();
}

css::uno::Reference<css::datatransfer::dnd::XDropTarget> SdrObjEditView::GetDropTarget()
{
    if (!mpTextEditWin)
        return nullptr;
    return mpTextEditWin->GetDropTarget();
}

void SdrObjEditView::EditViewInputContext(const InputContext& rInputContext)
{
    if (!mpTextEditWin)
        return;
    mpTextEditWin->SetInputContext(rInputContext);
}

void SdrObjEditView::EditViewCursorRect(const tools::Rectangle& rRect, int nExtTextInputWidth)
{
    if (!mpTextEditWin)
        return;
    mpTextEditWin->SetCursorRect(&rRect, nExtTextInputWidth);
}

void SdrObjEditView::TextEditDrawing(SdrPaintWindow& rPaintWindow)
{
    if (!comphelper::LibreOfficeKit::isActive())
    {
        // adapt all TextEditOverlayObject(s), so call EditViewInvalidate()
        // to update accordingly (will update selection, too). Suppress new
        // stuff when LibreOfficeKit is active
        EditViewInvalidate(tools::Rectangle());
    }
    else
    {
        // draw old text edit stuff
        if (IsTextEdit())
        {
            const SdrOutliner* pActiveOutliner = GetTextEditOutliner();

            if (pActiveOutliner)
            {
                const sal_uInt32 nViewCount(pActiveOutliner->GetViewCount());

                if (nViewCount)
                {
                    const vcl::Region& rRedrawRegion = rPaintWindow.GetRedrawRegion();
                    const tools::Rectangle aCheckRect(rRedrawRegion.GetBoundRect());

                    for (sal_uInt32 i(0); i < nViewCount; i++)
                    {
                        OutlinerView* pOLV = pActiveOutliner->GetView(i);

                        // If rPaintWindow knows that the output device is a render
                        // context and is aware of the underlying vcl::Window,
                        // compare against that; that's how double-buffering can
                        // still find the matching OutlinerView.
                        OutputDevice* pOutputDevice = rPaintWindow.GetWindow()
                                                          ? rPaintWindow.GetWindow()->GetOutDev()
                                                          : &rPaintWindow.GetOutputDevice();
                        if (pOLV->GetWindow()->GetOutDev() == pOutputDevice
                            || comphelper::LibreOfficeKit::isActive())
                        {
                            ImpPaintOutlinerView(*pOLV, aCheckRect,
                                                 rPaintWindow.GetTargetOutputDevice());
                            return;
                        }
                    }
                }
            }
        }
    }
}

void SdrObjEditView::ImpPaintOutlinerView(OutlinerView& rOutlView, const tools::Rectangle& rRect,
                                          OutputDevice& rTargetDevice) const
{
    const SdrTextObj* pText = GetTextEditObject();
    bool bTextFrame(pText && pText->IsTextFrame());
    bool bFitToSize(mpTextEditOutliner->GetControlWord() & EEControlBits::STRETCHING);
    bool bModified(mpTextEditOutliner->IsModified());
    tools::Rectangle aBlankRect(rOutlView.GetOutputArea());
    aBlankRect.Union(m_aMinTextEditArea);
    tools::Rectangle aPixRect(rTargetDevice.LogicToPixel(aBlankRect));

    // in the tiled rendering case, the setup is incomplete, and we very
    // easily get an empty rRect on input - that will cause that everything is
    // clipped; happens in case of editing text inside a shape in Calc.
    // FIXME would be better to complete the setup so that we don't get an
    // empty rRect here
    if (!comphelper::LibreOfficeKit::isActive() || !rRect.IsEmpty())
        aBlankRect.Intersection(rRect);

    rOutlView.GetOutliner().SetUpdateLayout(true); // Bugfix #22596#
    rOutlView.DrawText_ToEditView(aBlankRect, &rTargetDevice);

    if (!bModified)
    {
        mpTextEditOutliner->ClearModifyFlag();
    }

    if (bTextFrame && !bFitToSize)
    {
        // completely reworked to use primitives; this ensures same look and functionality
        const drawinglayer::geometry::ViewInformation2D aViewInformation2D;
        std::unique_ptr<drawinglayer::processor2d::BaseProcessor2D> xProcessor(
            drawinglayer::processor2d::createProcessor2DFromOutputDevice(rTargetDevice,
                                                                         aViewInformation2D));

        const bool bMapModeEnabled(rTargetDevice.IsMapModeEnabled());
        const basegfx::B2DRange aRange = vcl::unotools::b2DRectangleFromRectangle(aPixRect);
        const Color aHilightColor(SvtOptionsDrawinglayer::getHilightColor());
        const double fTransparence(SvtOptionsDrawinglayer::GetTransparentSelectionPercent() * 0.01);
        const sal_uInt16 nPixSiz(rOutlView.GetInvalidateMore() - 1);
        const drawinglayer::primitive2d::Primitive2DReference xReference(
            new drawinglayer::primitive2d::OverlayRectanglePrimitive(
                aRange, aHilightColor.getBColor(), fTransparence, std::max(6, nPixSiz - 2), // grow
                0.0, // shrink
                0.0));
        const drawinglayer::primitive2d::Primitive2DContainer aSequence{ xReference };

        rTargetDevice.EnableMapMode(false);
        xProcessor->process(aSequence);
        rTargetDevice.EnableMapMode(bMapModeEnabled);
    }

    rOutlView.ShowCursor(/*bGotoCursor=*/true, /*bActivate=*/true);
}

void SdrObjEditView::ImpInvalidateOutlinerView(OutlinerView const& rOutlView) const
{
    vcl::Window* pWin = rOutlView.GetWindow();

    if (!pWin)
        return;

    const SdrTextObj* pText = GetTextEditObject();
    bool bTextFrame(pText && pText->IsTextFrame());
    bool bFitToSize(pText && pText->IsFitToSize());

    if (!bTextFrame || bFitToSize)
        return;

    tools::Rectangle aBlankRect(rOutlView.GetOutputArea());
    aBlankRect.Union(m_aMinTextEditArea);
    tools::Rectangle aPixRect(pWin->LogicToPixel(aBlankRect));
    sal_uInt16 nPixSiz(rOutlView.GetInvalidateMore() - 1);

    aPixRect.AdjustLeft(-1);
    aPixRect.AdjustTop(-1);
    aPixRect.AdjustRight(1);
    aPixRect.AdjustBottom(1);

    {
        // limit xPixRect because of driver problems when pixel coordinates are too far out
        Size aMaxXY(pWin->GetOutputSizePixel());
        tools::Long a(2 * nPixSiz);
        tools::Long nMaxX(aMaxXY.Width() + a);
        tools::Long nMaxY(aMaxXY.Height() + a);

        if (aPixRect.Left() < -a)
            aPixRect.SetLeft(-a);
        if (aPixRect.Top() < -a)
            aPixRect.SetTop(-a);
        if (aPixRect.Right() > nMaxX)
            aPixRect.SetRight(nMaxX);
        if (aPixRect.Bottom() > nMaxY)
            aPixRect.SetBottom(nMaxY);
    }

    tools::Rectangle aOuterPix(aPixRect);
    aOuterPix.AdjustLeft(-nPixSiz);
    aOuterPix.AdjustTop(-nPixSiz);
    aOuterPix.AdjustRight(nPixSiz);
    aOuterPix.AdjustBottom(nPixSiz);

    bool bMapModeEnabled(pWin->IsMapModeEnabled());
    pWin->EnableMapMode(false);
    pWin->Invalidate(aOuterPix);
    pWin->EnableMapMode(bMapModeEnabled);
}

OutlinerView* SdrObjEditView::ImpMakeOutlinerView(vcl::Window* pWin, OutlinerView* pGivenView,
                                                  SfxViewShell* pViewShell) const
{
    // background
    Color aBackground(GetTextEditBackgroundColor(*this));
    rtl::Reference<SdrTextObj> pText = mxWeakTextEditObj.get();
    bool bTextFrame = pText != nullptr && pText->IsTextFrame();
    bool bContourFrame = pText != nullptr && pText->IsContourTextFrame();
    // create OutlinerView
    OutlinerView* pOutlView = pGivenView;
    mpTextEditOutliner->SetUpdateLayout(false);

    if (pOutlView == nullptr)
    {
        pOutlView = new OutlinerView(*mpTextEditOutliner, pWin);
    }
    else
    {
        pOutlView->SetWindow(pWin);
    }

    if (mbNegativeX)
        pOutlView->GetEditView().SetNegativeX(mbNegativeX);

    // disallow scrolling
    EVControlBits nStat = pOutlView->GetControlWord();
    nStat &= ~EVControlBits::AUTOSCROLL;
    // AutoViewSize only if not ContourFrame.
    if (!bContourFrame)
        nStat |= EVControlBits::AUTOSIZE;
    if (bTextFrame)
    {
        sal_uInt16 nPixSiz = maHdlList.GetHdlSize() * 2 + 1;
        nStat |= EVControlBits::INVONEMORE;
        pOutlView->SetInvalidateMore(nPixSiz);
    }
    pOutlView->SetControlWord(nStat);
    pOutlView->SetBackgroundColor(aBackground);

    // In case we're in the process of constructing a new view shell,
    // SfxViewShell::Current() may still point to the old one. So if possible,
    // depend on the application owning this draw view to provide the view
    // shell.
    SfxViewShell* pSfxViewShell = pViewShell ? pViewShell : GetSfxViewShell();
    pOutlView->RegisterViewShell(pSfxViewShell ? pSfxViewShell : SfxViewShell::Current());

    if (pText != nullptr)
    {
        pOutlView->SetAnchorMode(pText->GetOutlinerViewAnchorMode());
        mpTextEditOutliner->SetFixedCellHeight(
            pText->GetMergedItem(SDRATTR_TEXT_USEFIXEDCELLHEIGHT).GetValue());
    }
    // do update before setting output area so that aTextEditArea can be recalculated
    mpTextEditOutliner->SetUpdateLayout(true);
    pOutlView->SetOutputArea(m_aTextEditArea);
    ImpInvalidateOutlinerView(*pOutlView);
    return pOutlView;
}

IMPL_LINK(SdrObjEditView, ImpOutlinerStatusEventHdl, EditStatus&, rEditStat, void)
{
    if (mpTextEditOutliner)
    {
        rtl::Reference<SdrTextObj> pTextObj = mxWeakTextEditObj.get();
        if (pTextObj)
        {
            pTextObj->onEditOutlinerStatusEvent(&rEditStat);
        }
    }
}

void SdrObjEditView::ImpChainingEventHdl()
{
    if (!mpTextEditOutliner)
        return;

    rtl::Reference<SdrTextObj> pTextObj = mxWeakTextEditObj.get();
    OutlinerView* pOLV = GetTextEditOutlinerView();
    if (pTextObj && pOLV)
    {
        TextChain* pTextChain = pTextObj->GetTextChain();

        // XXX: IsChainable and GetNilChainingEvent are a bit mixed up atm
        if (!pTextObj->IsChainable())
        {
            return;
        }
        // This is true during an underflow-caused overflow (with pEdtOutl->SetText())
        if (pTextChain->GetNilChainingEvent(pTextObj.get()))
        {
            return;
        }

        // We prevent to trigger further handling of overflow/underflow for pTextObj
        pTextChain->SetNilChainingEvent(pTextObj.get(), true); // XXX

        // Save previous selection pos // NOTE: It must be done to have the right CursorEvent in KeyInput
        pTextChain->SetPreChainingSel(pTextObj.get(), pOLV->GetSelection());
        //maPreChainingSel = new ESelection(pOLV->GetSelection());

        // Handling Undo
        const int nText = 0; // XXX: hardcoded index (SdrTextObj::getText handles only 0)

        const bool bUndoEnabled = IsUndoEnabled();
        std::unique_ptr<SdrUndoObjSetText> pTxtUndo;
        if (bUndoEnabled)
            pTxtUndo.reset(
                dynamic_cast<SdrUndoObjSetText*>(GetModel()
                                                     .GetSdrUndoFactory()
                                                     .CreateUndoObjectSetText(*pTextObj, nText)
                                                     .release()));

        // trigger actual chaining
        pTextObj->onChainingEvent();

        if (pTxtUndo)
        {
            pTxtUndo->AfterSetText();
            if (!pTxtUndo->IsDifferent())
            {
                pTxtUndo.reset();
            }
        }

        if (pTxtUndo)
            AddUndo(std::move(pTxtUndo));

        //maCursorEvent = new CursorChainingEvent(pTextChain->GetCursorEvent(pTextObj));
        //SdrTextObj *pNextLink = pTextObj->GetNextLinkInChain();

        // NOTE: Must be called. Don't let the function return if you set it to true and not reset it
        pTextChain->SetNilChainingEvent(pTextObj.get(), false);
    }
    else
    {
        // XXX
        SAL_INFO("svx.chaining", "[OnChaining] No Edit Outliner View");
    }
}

IMPL_LINK_NOARG(SdrObjEditView, ImpAfterCutOrPasteChainingEventHdl, LinkParamNone*, void)
{
    SdrTextObj* pTextObj = GetTextEditObject();
    if (!pTextObj)
        return;
    ImpChainingEventHdl();
    TextChainCursorManager aCursorManager(this, pTextObj);
    ImpMoveCursorAfterChainingEvent(&aCursorManager);
}

void SdrObjEditView::ImpMoveCursorAfterChainingEvent(TextChainCursorManager* pCursorManager)
{
    rtl::Reference<SdrTextObj> pTextObj = mxWeakTextEditObj.get();

    if (!pTextObj || !pCursorManager)
        return;

    // Check if it has links to move it to
    if (!pTextObj || !pTextObj->IsChainable())
        return;

    TextChain* pTextChain = pTextObj->GetTextChain();
    ESelection aNewSel = pTextChain->GetPostChainingSel(pTextObj.get());

    pCursorManager->HandleCursorEventAfterChaining(pTextChain->GetCursorEvent(pTextObj.get()),
                                                   aNewSel);

    // Reset event
    pTextChain->SetCursorEvent(pTextObj.get(), CursorChainingEvent::NULL_EVENT);
}

IMPL_LINK(SdrObjEditView, ImpOutlinerCalcFieldValueHdl, EditFieldInfo*, pFI, void)
{
    bool bOk = false;
    OUString& rStr = pFI->GetRepresentation();
    rStr.clear();
    rtl::Reference<SdrTextObj> pTextObj = mxWeakTextEditObj.get();
    if (pTextObj != nullptr)
    {
        std::optional<Color> pTxtCol;
        std::optional<Color> pFldCol;
        std::optional<FontLineStyle> pFldLineStyle;
        bOk = pTextObj->CalcFieldValue(pFI->GetField(), pFI->GetPara(), pFI->GetPos(), true,
                                       pTxtCol, pFldCol, pFldLineStyle, rStr);
        if (bOk)
        {
            if (pTxtCol)
            {
                pFI->SetTextColor(*pTxtCol);
            }
            if (pFldLineStyle)
            {
                pFI->SetFontLineStyle(*pFldLineStyle);
            }
            if (pFldCol)
            {
                pFI->SetFieldColor(*pFldCol);
            }
            else
            {
                pFI->SetFieldColor(COL_LIGHTGRAY); // TODO: remove this later on (357)
            }
        }
    }
    Outliner& rDrawOutl = GetModel().GetDrawOutliner(pTextObj.get());
    Link<EditFieldInfo*, void> aDrawOutlLink = rDrawOutl.GetCalcFieldValueHdl();
    if (!bOk && aDrawOutlLink.IsSet())
    {
        aDrawOutlLink.Call(pFI);
        bOk = !rStr.isEmpty();
    }
    if (!bOk)
    {
        m_aOldCalcFieldValueLink.Call(pFI);
    }
}

IMPL_LINK_NOARG(SdrObjEditView, EndTextEditHdl, SdrUndoManager*, void) { SdrEndTextEdit(); }

// Default implementation - null UndoManager
std::unique_ptr<SdrUndoManager> SdrObjEditView::createLocalTextUndoManager()
{
    SAL_WARN("svx", "SdrObjEditView::createLocalTextUndoManager needs to be overridden");
    return std::unique_ptr<SdrUndoManager>();
}

bool SdrObjEditView::SdrBeginTextEdit(SdrObject* pObj_, SdrPageView* pPV, vcl::Window* pWin,
                                      bool bIsNewObj, SdrOutliner* pGivenOutliner,
                                      OutlinerView* pGivenOutlinerView, bool bDontDeleteOutliner,
                                      bool bOnlyOneView, bool bGrabFocus)
{
    // FIXME cannot be an assert() yet, the code is not ready for that;
    // eg. press F7 in Impress when you are inside a text object with spelling
    // mistakes => boom; and it is unclear how to avoid that
    SAL_WARN_IF(IsTextEdit(), "svx", "SdrBeginTextEdit called when IsTextEdit() is already true.");
    // FIXME this encourages all sorts of bad habits and should be removed
    SdrEndTextEdit();

    SdrTextObj* pObj = DynCastSdrTextObj(pObj_);
    if (!pObj)
        return false; // currently only possible with text objects

    if (bGrabFocus && pWin)
    {
        // attention, this call may cause an EndTextEdit() call to this view
        pWin->GrabFocus(); // to force the cursor into the edit view
    }

    mbTextEditDontDelete = bDontDeleteOutliner && pGivenOutliner != nullptr;
    mbTextEditOnlyOneView = bOnlyOneView;
    mbTextEditNewObj = bIsNewObj;
    const sal_uInt32 nWinCount(PaintWindowCount());

    bool bBrk(false);

    if (!pWin)
    {
        for (sal_uInt32 i = 0; i < nWinCount && !pWin; i++)
        {
            SdrPaintWindow* pPaintWindow = GetPaintWindow(i);

            if (OUTDEV_WINDOW == pPaintWindow->GetOutputDevice().GetOutDevType())
            {
                pWin = pPaintWindow->GetOutputDevice().GetOwnerWindow();
            }
        }

        // break, when no window exists
        if (!pWin)
        {
            bBrk = true;
        }
    }

    if (!bBrk && !pPV)
    {
        pPV = GetSdrPageView();

        // break, when no PageView for the object exists
        if (!pPV)
        {
            bBrk = true;
        }
    }

    // no TextEdit on objects in locked Layer
    if (pPV && pPV->GetLockedLayers().IsSet(pObj->GetLayer()))
    {
        bBrk = true;
    }

    if (mpTextEditOutliner)
    {
        OSL_FAIL("SdrObjEditView::SdrBeginTextEdit(): Old Outliner still exists.");
        mpTextEditOutliner.reset();
    }

    if (!bBrk)
    {
        mpTextEditWin = pWin;
        mpTextEditPV = pPV;
        mxWeakTextEditObj = pObj;
        if (pGivenOutliner)
        {
            mpTextEditOutliner.reset(pGivenOutliner);
            pGivenOutliner = nullptr; // so we don't delete it on the error path
        }
        else
            mpTextEditOutliner
                = SdrMakeOutliner(OutlinerMode::TextObject, pObj->getSdrModelFromSdrObject());

        {
            mpTextEditOutliner->ForceAutoColor(
                officecfg::Office::Common::Accessibility::IsAutomaticFontColor::get());
        }

        m_aOldCalcFieldValueLink = mpTextEditOutliner->GetCalcFieldValueHdl();
        // FieldHdl has to be set by SdrBeginTextEdit, because this call an UpdateFields
        mpTextEditOutliner->SetCalcFieldValueHdl(
            LINK(this, SdrObjEditView, ImpOutlinerCalcFieldValueHdl));
        mpTextEditOutliner->SetBeginPasteOrDropHdl(LINK(this, SdrObjEditView, BeginPasteOrDropHdl));
        mpTextEditOutliner->SetEndPasteOrDropHdl(LINK(this, SdrObjEditView, EndPasteOrDropHdl));

        // It is just necessary to make the visualized page known. Set it.
        mpTextEditOutliner->setVisualizedPage(pPV->GetPage());

        rtl::Reference<SdrTextObj> pTextObj = mxWeakTextEditObj.get();
        mpTextEditOutliner->SetTextObjNoInit(pTextObj.get());

        if (pTextObj->BegTextEdit(*mpTextEditOutliner))
        {
            // switch off any running TextAnimations
            pTextObj->SetTextAnimationAllowed(false);

            // remember old cursor
            if (mpTextEditOutliner->GetViewCount() != 0)
            {
                mpTextEditOutliner->RemoveView(static_cast<size_t>(0));
            }

            // Determine EditArea via TakeTextEditArea.
            // TODO: This could theoretically be left out, because TakeTextRect() calculates the aTextEditArea,
            // but aMinTextEditArea has to happen, too (therefore leaving this in right now)
            pTextObj->TakeTextEditArea(nullptr, nullptr, &m_aTextEditArea, &m_aMinTextEditArea);

            tools::Rectangle aTextRect;
            tools::Rectangle aAnchorRect;
            pTextObj->TakeTextRect(*mpTextEditOutliner, aTextRect, true,
                                   &aAnchorRect /* Give true here, not false */);

            if (!pTextObj->IsContourTextFrame())
            {
                // FitToSize not together with ContourFrame, for now
                if (pTextObj->IsFitToSize())
                    aTextRect = aAnchorRect;
            }

            m_aTextEditArea = aTextRect;

            // add possible GridOffset to up-to-now view-independent EditAreas
            basegfx::B2DVector aGridOffset(0.0, 0.0);
            if (getPossibleGridOffsetForSdrObject(aGridOffset, pTextObj.get(), pPV))
            {
                const Point aOffset(basegfx::fround<tools::Long>(aGridOffset.getX()),
                                    basegfx::fround<tools::Long>(aGridOffset.getY()));

                m_aTextEditArea += aOffset;
                m_aMinTextEditArea += aOffset;
            }

            Point aPvOfs(pTextObj->GetTextEditOffset());
            m_aTextEditArea.Move(aPvOfs.X(), aPvOfs.Y());
            m_aMinTextEditArea.Move(aPvOfs.X(), aPvOfs.Y());
            m_pTextEditCursorBuffer = pWin->GetCursor();

            maHdlList.SetMoveOutside(true);

            // Since IsMarkHdlWhenTextEdit() is ignored, it is necessary
            // to call AdjustMarkHdl() always.
            AdjustMarkHdl();

            mpTextEditOutlinerView = ImpMakeOutlinerView(pWin, pGivenOutlinerView);

            if (!comphelper::LibreOfficeKit::isActive() && mpTextEditOutlinerView)
            {
                // activate visualization of EditView on Overlay, suppress when
                // LibreOfficeKit is active
                mpTextEditOutlinerView->GetEditView().setEditViewCallbacks(this);

                const Color aHilightColor(SvtOptionsDrawinglayer::getHilightColor());
                const SdrTextObj* pText = GetTextEditObject();
                // show for cases like tdf#94223 but not for table cells like tdf#151311
                const bool bVisualizeSurroundingFrame(
                    pText && pText->GetObjIdentifier() != SdrObjKind::Table);
                SdrPageView* pPageView = GetSdrPageView();

                if (pPageView)
                {
                    for (sal_uInt32 b(0); b < pPageView->PageWindowCount(); b++)
                    {
                        const SdrPageWindow& rPageWindow = *pPageView->GetPageWindow(b);

                        if (rPageWindow.GetPaintWindow().OutputToWindow())
                        {
                            const rtl::Reference<sdr::overlay::OverlayManager>& xManager
                                = rPageWindow.GetOverlayManager();
                            if (xManager.is())
                            {
                                std::unique_ptr<TextEditOverlayObject> pNewTextEditOverlayObject(
                                    new TextEditOverlayObject(aHilightColor,
                                                              *mpTextEditOutlinerView));

                                xManager->add(*pNewTextEditOverlayObject);
                                if (bVisualizeSurroundingFrame)
                                    xManager->add(*pNewTextEditOverlayObject->getOverlayFrame());
                                xManager->add(*pNewTextEditOverlayObject->getOverlaySelection());

                                maTEOverlayGroup.append(std::move(pNewTextEditOverlayObject));
                            }
                        }
                    }
                }
            }

            // check if this view is already inserted
            size_t i2, nCount = mpTextEditOutliner->GetViewCount();
            for (i2 = 0; i2 < nCount; i2++)
            {
                if (mpTextEditOutliner->GetView(i2) == mpTextEditOutlinerView)
                    break;
            }

            if (i2 == nCount)
                mpTextEditOutliner->InsertView(mpTextEditOutlinerView, 0);

            maHdlList.SetMoveOutside(false);
            maHdlList.SetMoveOutside(true);

            // register all windows as OutlinerViews with the Outliner
            if (!bOnlyOneView)
            {
                for (sal_uInt32 i = 0; i < nWinCount; i++)
                {
                    SdrPaintWindow* pPaintWindow = GetPaintWindow(i);
                    OutputDevice& rOutDev = pPaintWindow->GetOutputDevice();

                    if (&rOutDev != pWin->GetOutDev() && OUTDEV_WINDOW == rOutDev.GetOutDevType())
                    {
                        OutlinerView* pOutlView
                            = ImpMakeOutlinerView(rOutDev.GetOwnerWindow(), nullptr);
                        mpTextEditOutliner->InsertView(pOutlView, static_cast<sal_uInt16>(i));
                    }
                }

                if (comphelper::LibreOfficeKit::isActive())
                {
                    // Register an outliner view for all other sdr views that
                    // show the same page, so that when the text edit changes,
                    // all interested windows get an invalidation.
                    SdrViewIter::ForAllViews(pObj->getSdrPageFromSdrObject(), [this, &pWin](
                                                                                  SdrView* pView) {
                        if (pView == this)
                            return;

                        for (sal_uInt32 nViewPaintWindow = 0;
                             nViewPaintWindow < pView->PaintWindowCount(); ++nViewPaintWindow)
                        {
                            SdrPaintWindow* pPaintWindow = pView->GetPaintWindow(nViewPaintWindow);
                            OutputDevice& rOutDev = pPaintWindow->GetOutputDevice();

                            if (&rOutDev != pWin->GetOutDev()
                                && OUTDEV_WINDOW == rOutDev.GetOutDevType())
                            {
                                OutlinerView* pOutlView
                                    = ImpMakeOutlinerView(rOutDev.GetOwnerWindow(), nullptr);
                                pOutlView->HideCursor();
                                rOutDev.GetOwnerWindow()->SetCursor(nullptr);
                                mpTextEditOutliner->InsertView(pOutlView);
                            }
                        }
                    });
                }
            }

            mpTextEditOutlinerView->ShowCursor();
            mpTextEditOutliner->SetStatusEventHdl(
                LINK(this, SdrObjEditView, ImpOutlinerStatusEventHdl));

            // IASS: start listening to ModelChanges of TextEdit
            if (isInteractiveSlideShow()
                || pTextObj->GetViewContact().hasMultipleViewObjectContacts())
                mpTextEditOutliner->SetModifyHdl(LINK(this, SdrObjEditView, ImpModifyHdl));

            if (pTextObj->IsChainable())
            {
                mpTextEditOutlinerView->SetEndCutPasteLinkHdl(
                    LINK(this, SdrObjEditView, ImpAfterCutOrPasteChainingEventHdl));
            }

            mpTextEditOutliner->ClearModifyFlag();

            if (pTextObj->IsFitToSize())
            {
                pWin->Invalidate(m_aTextEditArea);
            }

            SdrHint aHint(SdrHintKind::BeginEdit, *pTextObj);
            GetModel().Broadcast(aHint);
            if (auto pBroadcaster = pTextObj->GetBroadcaster())
                pBroadcaster->Broadcast(aHint);

            mpTextEditOutliner->setVisualizedPage(nullptr);

            if (mxSelectionController.is())
                mxSelectionController->onSelectionHasChanged();

            if (IsUndoEnabled() && !GetModel().GetDisableTextEditUsesCommonUndoManager())
            {
                SdrUndoManager* pSdrUndoManager = nullptr;
                mpLocalTextEditUndoManager = createLocalTextUndoManager();

                if (mpLocalTextEditUndoManager)
                    pSdrUndoManager = mpLocalTextEditUndoManager.get();

                if (pSdrUndoManager)
                {
                    // we have an outliner, undo manager and it's an EditUndoManager, exchange
                    // the document undo manager and the default one from the outliner and tell
                    // it that text edit starts by setting a callback if it needs to end text edit mode.
                    assert(nullptr == mpOldTextEditUndoManager);

                    mpOldTextEditUndoManager = mpTextEditOutliner->SetUndoManager(pSdrUndoManager);
                    pSdrUndoManager->SetEndTextEditHdl(LINK(this, SdrObjEditView, EndTextEditHdl));
                }
                else
                {
                    OSL_ENSURE(false,
                               "The document undo manager is not derived from SdrUndoManager (!)");
                }
            }

            return true; // ran fine, let TextEdit run now
        }
        else
        {
            mpTextEditOutliner->SetCalcFieldValueHdl(m_aOldCalcFieldValueLink);
            mpTextEditOutliner->SetBeginPasteOrDropHdl(Link<PasteOrDropInfos*, void>());
            mpTextEditOutliner->SetEndPasteOrDropHdl(Link<PasteOrDropInfos*, void>());
        }
    }
    if (mpTextEditOutliner != nullptr)
    {
        mpTextEditOutliner->setVisualizedPage(nullptr);
    }

    // something went wrong...
    if (!bDontDeleteOutliner)
    {
        delete pGivenOutliner;
        if (pGivenOutlinerView != nullptr)
        {
            delete pGivenOutlinerView;
            pGivenOutlinerView = nullptr;
        }
    }
    mpTextEditOutliner.reset();

    mpTextEditOutlinerView = nullptr;
    mxWeakTextEditObj.clear();
    mpTextEditPV = nullptr;
    mpTextEditWin = nullptr;
    maHdlList.SetMoveOutside(false);

    return false;
}

SdrEndTextEditKind SdrObjEditView::SdrEndTextEdit(bool bDontDeleteReally)
{
    // IASS: stop evtl. running timer immediately
    maTextEditUpdateTimer.Stop();

    SdrEndTextEditKind eRet = SdrEndTextEditKind::Unchanged;
    rtl::Reference<SdrTextObj> pTEObj = mxWeakTextEditObj.get();
    vcl::Window* pTEWin = mpTextEditWin;
    OutlinerView* pTEOutlinerView = mpTextEditOutlinerView;
    vcl::Cursor* pTECursorBuffer = m_pTextEditCursorBuffer;
    SdrUndoManager* pUndoEditUndoManager = nullptr;
    bool bNeedToUndoSavedRedoTextEdit(false);

    if (IsUndoEnabled() && pTEObj && mpTextEditOutliner
        && !GetModel().GetDisableTextEditUsesCommonUndoManager())
    {
        // change back the UndoManager to the remembered original one
        SfxUndoManager* pOriginal = mpTextEditOutliner->SetUndoManager(mpOldTextEditUndoManager);
        mpOldTextEditUndoManager = nullptr;

        if (pOriginal)
        {
            // check if we got back our document undo manager
            SdrUndoManager* pSdrUndoManager = mpLocalTextEditUndoManager.get();

            if (pSdrUndoManager && dynamic_cast<SdrUndoManager*>(pOriginal) == pSdrUndoManager)
            {
                if (pSdrUndoManager->isEndTextEditTriggeredFromUndo())
                {
                    // remember the UndoManager where missing Undos have to be triggered after end
                    // text edit. When the undo had triggered the end text edit, the original action
                    // which had to be undone originally is not yet undone.
                    pUndoEditUndoManager = pSdrUndoManager;

                    // We are ending text edit; if text edit was triggered from undo, execute all redos
                    // to create a complete text change undo action for the redo buffer. Also mark this
                    // state when at least one redo was executed; the created extra TextChange needs to
                    // be undone in addition to the first real undo outside the text edit changes
                    while (pSdrUndoManager->GetRedoActionCount()
                           > pSdrUndoManager->GetRedoActionCountBeforeTextEdit())
                    {
                        bNeedToUndoSavedRedoTextEdit = true;
                        pSdrUndoManager->Redo();
                    }
                }

                // reset the callback link and let the undo manager cleanup all text edit
                // undo actions to get the stack back to the form before the text edit
                pSdrUndoManager->SetEndTextEditHdl(Link<SdrUndoManager*, void>());
            }
            else
            {
                OSL_ENSURE(false, "Got UndoManager back in SdrEndTextEdit which is NOT the "
                                  "expected document UndoManager (!)");
                delete pOriginal;
            }

            // cid#1493241 - Wrapper object use after free
            if (pUndoEditUndoManager == mpLocalTextEditUndoManager.get())
                pUndoEditUndoManager = nullptr;
            mpLocalTextEditUndoManager.reset();
        }
    }
    else
    {
        assert(nullptr == mpOldTextEditUndoManager); // cannot be restored!
    }

    if (auto pTextEditObj = mxWeakTextEditObj.get())
    {
        SdrHint aHint(SdrHintKind::EndEdit, *pTextEditObj);
        GetModel().Broadcast(aHint);
        if (auto pBroadcaster = pTextEditObj->GetBroadcaster())
            pBroadcaster->Broadcast(aHint);
    }

    // if new mechanism was used, clean it up. At cleanup no need to check
    // for LibreOfficeKit
    if (mpTextEditOutlinerView)
    {
        mpTextEditOutlinerView->GetEditView().setEditViewCallbacks(nullptr);
        maTEOverlayGroup.clear();
    }

    mxWeakTextEditObj.clear();
    mpTextEditPV = nullptr;
    mpTextEditWin = nullptr;
    mpTextEditOutlinerView = nullptr;
    m_pTextEditCursorBuffer = nullptr;
    m_aTextEditArea = tools::Rectangle();

    if (SdrOutliner* pTEOutliner = mpTextEditOutliner.release())
    {
        bool bModified = pTEOutliner->IsModified();
        if (pTEOutlinerView != nullptr)
        {
            pTEOutlinerView->HideCursor();
        }
        if (pTEObj != nullptr)
        {
            pTEOutliner->CompleteOnlineSpelling();

            std::unique_ptr<SdrUndoObjSetText> pTxtUndo;

            if (bModified)
            {
                sal_Int32 nText;
                for (nText = 0; nText < pTEObj->getTextCount(); ++nText)
                    if (pTEObj->getText(nText) == pTEObj->getActiveText())
                        break;

                pTxtUndo.reset(
                    dynamic_cast<SdrUndoObjSetText*>(GetModel()
                                                         .GetSdrUndoFactory()
                                                         .CreateUndoObjectSetText(*pTEObj, nText)
                                                         .release()));
            }
            DBG_ASSERT(!bModified || pTxtUndo,
                       "svx::SdrObjEditView::EndTextEdit(), could not create undo action!");
            // Set old CalcFieldValue-Handler again, this
            // has to happen before Obj::EndTextEdit(), as this does UpdateFields().
            pTEOutliner->SetCalcFieldValueHdl(m_aOldCalcFieldValueLink);
            pTEOutliner->SetBeginPasteOrDropHdl(Link<PasteOrDropInfos*, void>());
            pTEOutliner->SetEndPasteOrDropHdl(Link<PasteOrDropInfos*, void>());

            // IASS: stop listening to ModelChanges of TextEdit
            pTEOutliner->SetModifyHdl(Link<LinkParamNone*, void>());

            const bool bUndo = IsUndoEnabled();
            if (bUndo)
            {
                OUString aObjName(pTEObj->TakeObjNameSingul());
                BegUndo(SvxResId(STR_UndoObjSetText), aObjName);
            }

            pTEObj->EndTextEdit(*pTEOutliner);

            if ((pTEObj->GetRotateAngle() != 0_deg100) || (pTEObj && pTEObj->IsFontwork()))
            {
                pTEObj->ActionChanged();
            }

            if (pTxtUndo != nullptr)
            {
                pTxtUndo->AfterSetText();
                if (!pTxtUndo->IsDifferent())
                {
                    pTxtUndo.reset();
                }
            }
            // check deletion of entire TextObj
            std::unique_ptr<SdrUndoAction> pDelUndo;
            bool bDelObj = false;
            if (mbTextEditNewObj)
            {
                bDelObj = pTEObj->IsTextFrame() && !pTEObj->HasText() && !pTEObj->IsEmptyPresObj()
                          && !pTEObj->HasFill() && !pTEObj->HasLine();

                if (pTEObj->IsInserted() && bDelObj
                    && pTEObj->GetObjInventor() == SdrInventor::Default && !bDontDeleteReally)
                {
                    SdrObjKind eIdent = pTEObj->GetObjIdentifier();
                    if (eIdent == SdrObjKind::Text)
                    {
                        pDelUndo = GetModel().GetSdrUndoFactory().CreateUndoDeleteObject(*pTEObj);
                    }
                }
            }
            if (pTxtUndo)
            {
                if (bUndo)
                    AddUndo(std::move(pTxtUndo));
                eRet = SdrEndTextEditKind::Changed;
            }
            if (pDelUndo != nullptr)
            {
                if (bUndo)
                {
                    AddUndo(std::move(pDelUndo));
                }
                eRet = SdrEndTextEditKind::Deleted;
                DBG_ASSERT(pTEObj->getParentSdrObjListFromSdrObject() != nullptr,
                           "SdrObjEditView::SdrEndTextEdit(): Fatal: Object edited doesn't have an "
                           "ObjList!");
                if (pTEObj->getParentSdrObjListFromSdrObject() != nullptr)
                {
                    pTEObj->getParentSdrObjListFromSdrObject()->RemoveObject(pTEObj->GetOrdNum());
                    CheckMarked(); // remove selection immediately...
                }
            }
            else if (bDelObj)
            { // for Writer: the app has to do the deletion itself.
                eRet = SdrEndTextEditKind::ShouldBeDeleted;
            }

            if (bUndo)
                EndUndo(); // EndUndo after Remove, in case UndoStack is deleted immediately

            // Switch on any TextAnimation again after TextEdit
            if (pTEObj)
            {
                pTEObj->SetTextAnimationAllowed(true);
            }

            // Since IsMarkHdlWhenTextEdit() is ignored, it is necessary
            // to call AdjustMarkHdl() always.
            AdjustMarkHdl();
        }
        if (pTEWin != nullptr)
        {
            pTEWin->SetCursor(pTECursorBuffer);
        }
        // delete all OutlinerViews
        for (size_t i = pTEOutliner->GetViewCount(); i > 0;)
        {
            i--;
            OutlinerView* pOLV = pTEOutliner->GetView(i);
            sal_uInt16 nMorePix = pOLV->GetInvalidateMore() + 10;
            VclPtr<vcl::Window> pWin = pOLV->GetWindow();
            tools::Rectangle aRect(pOLV->GetOutputArea());
            pTEOutliner->RemoveView(i);
            if (!mbTextEditDontDelete || i != 0)
            {
                // may not own the zeroth one
                delete pOLV;
            }
            aRect.Union(m_aTextEditArea);
            aRect.Union(m_aMinTextEditArea);
            aRect = pWin->LogicToPixel(aRect);
            aRect.AdjustLeft(-nMorePix);
            aRect.AdjustTop(-nMorePix);
            aRect.AdjustRight(nMorePix);
            aRect.AdjustBottom(nMorePix);
            aRect = pWin->PixelToLogic(aRect);
            InvalidateOneWin(*pWin->GetOutDev(), aRect);
            pWin->GetOutDev()->SetFillColor();
            pWin->GetOutDev()->SetLineColor(COL_BLACK);
        }
        // and now the Outliner itself
        if (!mbTextEditDontDelete)
            delete pTEOutliner;
        else
            pTEOutliner->Clear();
        maHdlList.SetMoveOutside(false);
        if (eRet != SdrEndTextEditKind::Unchanged)
        {
            GetMarkedObjectListWriteAccess().SetNameDirty();
        }
        // coverity[leaked_storage] - if pTEOutliner wasn't deleted it didn't really belong to us
    }

    if (pTEObj && !pTEObj->getSdrModelFromSdrObject().isLocked() && pTEObj->GetBroadcaster())
    {
        SdrHint aHint(SdrHintKind::EndEdit, *pTEObj);
        pTEObj->GetBroadcaster()->Broadcast(aHint);
    }

    if (pUndoEditUndoManager)
    {
        if (bNeedToUndoSavedRedoTextEdit)
        {
            // undo the text edit action since it was created as part of an EndTextEdit
            // callback from undo itself. This needs to be done after the call to
            // FmFormView::SdrEndTextEdit since it gets created there
            pUndoEditUndoManager->Undo();
        }

        // trigger the Undo which was not executed, but lead to this
        // end text edit
        pUndoEditUndoManager->Undo();
    }

    return eRet;
}

// info about TextEdit. Default is false.
bool SdrObjEditView::IsTextEdit() const { return mxWeakTextEditObj.get().is(); }

// info about TextEditPageView. Default is 0L.
SdrPageView* SdrObjEditView::GetTextEditPageView() const { return mpTextEditPV; }

OutlinerView* SdrObjEditView::ImpFindOutlinerView(vcl::Window const* pWin) const
{
    if (pWin == nullptr)
        return nullptr;
    if (mpTextEditOutliner == nullptr)
        return nullptr;
    OutlinerView* pNewView = nullptr;
    size_t nWinCount = mpTextEditOutliner->GetViewCount();
    for (size_t i = 0; i < nWinCount && pNewView == nullptr; i++)
    {
        OutlinerView* pView = mpTextEditOutliner->GetView(i);
        if (pView->GetWindow() == pWin)
            pNewView = pView;
    }
    return pNewView;
}

void SdrObjEditView::SetTextEditWin(vcl::Window* pWin)
{
    if (!(mxWeakTextEditObj.get() && pWin != nullptr && pWin != mpTextEditWin))
        return;

    OutlinerView* pNewView = ImpFindOutlinerView(pWin);
    if (pNewView != nullptr && pNewView != mpTextEditOutlinerView)
    {
        if (mpTextEditOutlinerView != nullptr)
        {
            mpTextEditOutlinerView->HideCursor();
        }
        mpTextEditOutlinerView = pNewView;
        mpTextEditWin = pWin;
        pWin->GrabFocus(); // Make the cursor blink here as well
        pNewView->ShowCursor();
        ImpMakeTextCursorAreaVisible();
    }
}

bool SdrObjEditView::IsTextEditHit(const Point& rHit) const
{
    bool bOk = false;
    if (mxWeakTextEditObj.get())
    {
        tools::Rectangle aEditArea;
        if (OutlinerView* pOLV = mpTextEditOutliner->GetView(0))
            aEditArea.Union(pOLV->GetOutputArea());

        if (aEditArea.Contains(rHit))
        { // check if any characters were actually hit
            const Point aPnt(rHit - aEditArea.TopLeft());
            tools::Long nHitTol = 2000;
            if (OutputDevice* pRef = mpTextEditOutliner->GetRefDevice())
                nHitTol = OutputDevice::LogicToLogic(nHitTol, MapUnit::Map100thMM,
                                                     pRef->GetMapMode().GetMapUnit());

            bOk = mpTextEditOutliner->IsTextPos(aPnt, static_cast<sal_uInt16>(nHitTol));
        }
    }
    return bOk;
}

bool SdrObjEditView::IsTextEditFrameHit(const Point& rHit) const
{
    bool bOk = false;
    if (rtl::Reference<SdrTextObj> pText = mxWeakTextEditObj.get())
    {
        OutlinerView* pOLV = mpTextEditOutliner->GetView(0);
        if (pOLV)
        {
            vcl::Window* pWin = pOLV->GetWindow();
            if (pText != nullptr && pText->IsTextFrame() && pWin != nullptr)
            {
                sal_uInt16 nPixSiz = pOLV->GetInvalidateMore();
                tools::Rectangle aEditArea(m_aMinTextEditArea);
                aEditArea.Union(pOLV->GetOutputArea());
                if (!aEditArea.Contains(rHit))
                {
                    Size aSiz(pWin->PixelToLogic(Size(nPixSiz, nPixSiz)));
                    aEditArea.AdjustLeft(-(aSiz.Width()));
                    aEditArea.AdjustTop(-(aSiz.Height()));
                    aEditArea.AdjustRight(aSiz.Width());
                    aEditArea.AdjustBottom(aSiz.Height());
                    bOk = aEditArea.Contains(rHit);
                }
            }
        }
    }
    return bOk;
}

std::unique_ptr<TextChainCursorManager>
SdrObjEditView::ImpHandleMotionThroughBoxesKeyInput(const KeyEvent& rKEvt, bool* bOutHandled)
{
    *bOutHandled = false;

    rtl::Reference<SdrTextObj> pTextObj = mxWeakTextEditObj.get();
    if (!pTextObj)
        return nullptr;

    if (!pTextObj->GetNextLinkInChain() && !pTextObj->GetPrevLinkInChain())
        return nullptr;

    std::unique_ptr<TextChainCursorManager> pCursorManager(
        new TextChainCursorManager(this, pTextObj.get()));
    if (pCursorManager->HandleKeyEvent(rKEvt))
    {
        // Possibly do other stuff here if necessary...
        // XXX: Careful with the checks below (in KeyInput) for pWin and co. You should do them here I guess.
        *bOutHandled = true;
    }

    return pCursorManager;
}

bool SdrObjEditView::KeyInput(const KeyEvent& rKEvt, vcl::Window* pWin)
{
    if (mpTextEditOutlinerView)
    {
        /* Start special handling of keys within a chain */
        // We possibly move to another box before any handling
        bool bHandled = false;
        std::unique_ptr<TextChainCursorManager> xCursorManager(
            ImpHandleMotionThroughBoxesKeyInput(rKEvt, &bHandled));
        if (bHandled)
            return true;
        /* End special handling of keys within a chain */

        if (mpTextEditOutlinerView->PostKeyEvent(rKEvt, pWin))
        {
            if (mpTextEditOutliner && mpTextEditOutliner->IsModified())
            {
                GetModel().SetChanged();
                SetInnerTextAreaForLOKit();
            }

            /* Start chaining processing */
            ImpChainingEventHdl();
            ImpMoveCursorAfterChainingEvent(xCursorManager.get());
            /* End chaining processing */

            if (pWin != nullptr && pWin != mpTextEditWin)
                SetTextEditWin(pWin);
            ImpMakeTextCursorAreaVisible();
            return true;
        }
    }
    return SdrGlueEditView::KeyInput(rKEvt, pWin);
}

bool SdrObjEditView::MouseButtonDown(const MouseEvent& rMEvt, OutputDevice* pWin)
{
    if (mpTextEditOutlinerView != nullptr)
    {
        bool bPostIt = mpTextEditOutliner->IsInSelectionMode();
        if (!bPostIt)
        {
            Point aPt(rMEvt.GetPosPixel());
            if (pWin != nullptr)
                aPt = pWin->PixelToLogic(aPt);
            else if (mpTextEditWin != nullptr)
                aPt = mpTextEditWin->PixelToLogic(aPt);
            bPostIt = IsTextEditHit(aPt);
        }
        if (bPostIt)
        {
            Point aPixPos(rMEvt.GetPosPixel());
            if (pWin)
            {
                tools::Rectangle aR(pWin->LogicToPixel(mpTextEditOutlinerView->GetOutputArea()));
                if (aPixPos.X() < aR.Left())
                    aPixPos.setX(aR.Left());
                if (aPixPos.X() > aR.Right())
                    aPixPos.setX(aR.Right());
                if (aPixPos.Y() < aR.Top())
                    aPixPos.setY(aR.Top());
                if (aPixPos.Y() > aR.Bottom())
                    aPixPos.setY(aR.Bottom());
            }
            MouseEvent aMEvt(aPixPos, rMEvt.GetClicks(), rMEvt.GetMode(), rMEvt.GetButtons(),
                             rMEvt.GetModifier());
            if (mpTextEditOutlinerView->MouseButtonDown(aMEvt))
            {
                if (pWin != nullptr && pWin != mpTextEditWin->GetOutDev()
                    && pWin->GetOutDevType() == OUTDEV_WINDOW)
                    SetTextEditWin(pWin->GetOwnerWindow());
                ImpMakeTextCursorAreaVisible();
                return true;
            }
        }
    }
    return SdrGlueEditView::MouseButtonDown(rMEvt, pWin);
}

bool SdrObjEditView::MouseButtonUp(const MouseEvent& rMEvt, OutputDevice* pWin)
{
    if (mpTextEditOutlinerView != nullptr)
    {
        bool bPostIt = mpTextEditOutliner->IsInSelectionMode();
        if (!bPostIt)
        {
            Point aPt(rMEvt.GetPosPixel());
            if (pWin != nullptr)
                aPt = pWin->PixelToLogic(aPt);
            else if (mpTextEditWin != nullptr)
                aPt = mpTextEditWin->PixelToLogic(aPt);
            bPostIt = IsTextEditHit(aPt);
        }
        if (bPostIt && pWin)
        {
            Point aPixPos(rMEvt.GetPosPixel());
            tools::Rectangle aR(pWin->LogicToPixel(mpTextEditOutlinerView->GetOutputArea()));
            if (aPixPos.X() < aR.Left())
                aPixPos.setX(aR.Left());
            if (aPixPos.X() > aR.Right())
                aPixPos.setX(aR.Right());
            if (aPixPos.Y() < aR.Top())
                aPixPos.setY(aR.Top());
            if (aPixPos.Y() > aR.Bottom())
                aPixPos.setY(aR.Bottom());
            MouseEvent aMEvt(aPixPos, rMEvt.GetClicks(), rMEvt.GetMode(), rMEvt.GetButtons(),
                             rMEvt.GetModifier());
            if (mpTextEditOutlinerView->MouseButtonUp(aMEvt))
            {
                ImpMakeTextCursorAreaVisible();
                return true;
            }
        }
    }
    return SdrGlueEditView::MouseButtonUp(rMEvt, pWin);
}

bool SdrObjEditView::MouseMove(const MouseEvent& rMEvt, OutputDevice* pWin)
{
    if (mpTextEditOutlinerView != nullptr)
    {
        bool bSelMode = mpTextEditOutliner->IsInSelectionMode();
        bool bPostIt = bSelMode;
        if (!bPostIt)
        {
            Point aPt(rMEvt.GetPosPixel());
            if (pWin)
                aPt = pWin->PixelToLogic(aPt);
            else if (mpTextEditWin)
                aPt = mpTextEditWin->PixelToLogic(aPt);
            bPostIt = IsTextEditHit(aPt);
        }
        if (bPostIt)
        {
            Point aPixPos(rMEvt.GetPosPixel());
            tools::Rectangle aR(mpTextEditOutlinerView->GetOutputArea());
            if (pWin)
                aR = pWin->LogicToPixel(aR);
            else if (mpTextEditWin)
                aR = mpTextEditWin->LogicToPixel(aR);
            if (aPixPos.X() < aR.Left())
                aPixPos.setX(aR.Left());
            if (aPixPos.X() > aR.Right())
                aPixPos.setX(aR.Right());
            if (aPixPos.Y() < aR.Top())
                aPixPos.setY(aR.Top());
            if (aPixPos.Y() > aR.Bottom())
                aPixPos.setY(aR.Bottom());
            MouseEvent aMEvt(aPixPos, rMEvt.GetClicks(), rMEvt.GetMode(), rMEvt.GetButtons(),
                             rMEvt.GetModifier());
            if (mpTextEditOutlinerView->MouseMove(aMEvt) && bSelMode)
            {
                ImpMakeTextCursorAreaVisible();
                return true;
            }
        }
    }
    return SdrGlueEditView::MouseMove(rMEvt, pWin);
}

bool SdrObjEditView::Command(const CommandEvent& rCEvt, vcl::Window* pWin)
{
    // as long as OutlinerView returns a sal_Bool, it only gets CommandEventId::StartDrag
    if (mpTextEditOutlinerView != nullptr)
    {
        if (rCEvt.GetCommand() == CommandEventId::StartDrag)
        {
            bool bPostIt = mpTextEditOutliner->IsInSelectionMode() || !rCEvt.IsMouseEvent();
            if (!bPostIt && rCEvt.IsMouseEvent())
            {
                Point aPt(rCEvt.GetMousePosPixel());
                if (pWin != nullptr)
                    aPt = pWin->PixelToLogic(aPt);
                else if (mpTextEditWin != nullptr)
                    aPt = mpTextEditWin->PixelToLogic(aPt);
                bPostIt = IsTextEditHit(aPt);
            }
            if (bPostIt)
            {
                Point aPixPos(rCEvt.GetMousePosPixel());
                if (rCEvt.IsMouseEvent() && pWin)
                {
                    tools::Rectangle aR(
                        pWin->LogicToPixel(mpTextEditOutlinerView->GetOutputArea()));
                    if (aPixPos.X() < aR.Left())
                        aPixPos.setX(aR.Left());
                    if (aPixPos.X() > aR.Right())
                        aPixPos.setX(aR.Right());
                    if (aPixPos.Y() < aR.Top())
                        aPixPos.setY(aR.Top());
                    if (aPixPos.Y() > aR.Bottom())
                        aPixPos.setY(aR.Bottom());
                }
                CommandEvent aCEvt(aPixPos, rCEvt.GetCommand(), rCEvt.IsMouseEvent());
                // Command is void at the OutlinerView, sadly
                mpTextEditOutlinerView->Command(aCEvt);
                if (pWin != nullptr && pWin != mpTextEditWin)
                    SetTextEditWin(pWin);
                ImpMakeTextCursorAreaVisible();
                return true;
            }
        }
        else
        {
            mpTextEditOutlinerView->Command(rCEvt);
            if (comphelper::LibreOfficeKit::isActive())
            {
                // It could execute CommandEventId::ExtTextInput, while SdrObjEditView::KeyInput
                // isn't called
                if (mpTextEditOutliner && mpTextEditOutliner->IsModified())
                {
                    GetModel().SetChanged();
                    SetInnerTextAreaForLOKit();
                }
            }
            return true;
        }
    }
    return SdrGlueEditView::Command(rCEvt, pWin);
}

bool SdrObjEditView::ImpIsTextEditAllSelected() const
{
    bool bRet = false;
    if (mpTextEditOutliner != nullptr && mpTextEditOutlinerView != nullptr)
    {
        if (SdrTextObj::HasTextImpl(mpTextEditOutliner.get()))
        {
            const sal_Int32 nParaCnt = mpTextEditOutliner->GetParagraphCount();
            Paragraph* pLastPara
                = mpTextEditOutliner->GetParagraph(nParaCnt > 1 ? nParaCnt - 1 : 0);

            ESelection aESel(mpTextEditOutlinerView->GetSelection());
            if (aESel.start.nPara == 0 && aESel.start.nIndex == 0
                && aESel.end.nPara == (nParaCnt - 1))
            {
                if (mpTextEditOutliner->GetText(pLastPara).getLength() == aESel.end.nIndex)
                    bRet = true;
            }
            // in case the selection was done backwards
            if (!bRet && aESel.end.nPara == 0 && aESel.end.nIndex == 0
                && aESel.start.nPara == (nParaCnt - 1))
            {
                if (mpTextEditOutliner->GetText(pLastPara).getLength() == aESel.start.nIndex)
                    bRet = true;
            }
        }
        else
        {
            bRet = true;
        }
    }
    return bRet;
}

void SdrObjEditView::ImpMakeTextCursorAreaVisible()
{
    if (mpTextEditOutlinerView != nullptr && mpTextEditWin != nullptr)
    {
        vcl::Cursor* pCsr = mpTextEditWin->GetCursor();
        if (pCsr != nullptr)
        {
            Size aSiz(pCsr->GetSize());
            if (!aSiz.IsEmpty())
            {
                MakeVisible(tools::Rectangle(pCsr->GetPos(), aSiz), *mpTextEditWin);
            }
        }
    }
}

SvtScriptType SdrObjEditView::GetScriptType() const
{
    SvtScriptType nScriptType = SvtScriptType::NONE;

    if (IsTextEdit())
    {
        auto pText = mxWeakTextEditObj.get();
        if (pText->GetOutlinerParaObject())
            nScriptType = pText->GetOutlinerParaObject()->GetTextObject().GetScriptType();

        if (mpTextEditOutlinerView)
            nScriptType = mpTextEditOutlinerView->GetSelectedScriptType();
    }
    else
    {
        const SdrMarkList& rMarkList = GetMarkedObjectList();
        const size_t nMarkCount(rMarkList.GetMarkCount());

        for (size_t i = 0; i < nMarkCount; ++i)
        {
            OutlinerParaObject* pParaObj
                = rMarkList.GetMark(i)->GetMarkedSdrObj()->GetOutlinerParaObject();

            if (pParaObj)
            {
                nScriptType |= pParaObj->GetTextObject().GetScriptType();
            }
        }
    }

    if (nScriptType == SvtScriptType::NONE)
        nScriptType = SvtScriptType::LATIN;

    return nScriptType;
}

void SdrObjEditView::GetAttributes(SfxItemSet& rTargetSet, bool bOnlyHardAttr) const
{
    if (mxSelectionController.is())
        if (mxSelectionController->GetAttributes(rTargetSet, bOnlyHardAttr))
            return;

    if (IsTextEdit())
    {
        DBG_ASSERT(mpTextEditOutlinerView != nullptr,
                   "SdrObjEditView::GetAttributes(): mpTextEditOutlinerView=NULL");
        DBG_ASSERT(mpTextEditOutliner != nullptr,
                   "SdrObjEditView::GetAttributes(): mpTextEditOutliner=NULL");

        auto pText = mxWeakTextEditObj.get();
        // take care of bOnlyHardAttr(!)
        if (!bOnlyHardAttr && pText->GetStyleSheet())
            rTargetSet.Put(pText->GetStyleSheet()->GetItemSet());

        // add object attributes
        rTargetSet.Put(pText->GetMergedItemSet());

        if (mpTextEditOutlinerView)
        {
            // FALSE= regard InvalidItems as "holes," not as Default
            rTargetSet.Put(mpTextEditOutlinerView->GetAttribs(), false);
        }

        const SdrMarkList& rMarkList = GetMarkedObjectList();
        if (rMarkList.GetMarkCount() == 1 && rMarkList.GetMark(0)->GetMarkedSdrObj() == pText.get())
        {
            MergeNotPersistAttrFromMarked(rTargetSet);
        }
    }
    else
    {
        SdrGlueEditView::GetAttributes(rTargetSet, bOnlyHardAttr);
    }
}

bool SdrObjEditView::SetAttributes(const SfxItemSet& rSet, bool bReplaceAll)
{
    bool bRet = false;
    rtl::Reference<SdrTextObj> pTextEditObj = mxWeakTextEditObj.get();
    bool bTextEdit = mpTextEditOutlinerView != nullptr && pTextEditObj != nullptr;
    bool bAllTextSelected = ImpIsTextEditAllSelected();
    const SfxItemSet* pSet = &rSet;

    if (!bTextEdit)
    {
        // no TextEdit active -> all Items to drawing object
        if (mxSelectionController.is())
            bRet = mxSelectionController->SetAttributes(*pSet, bReplaceAll);

        if (!bRet)
        {
            SdrGlueEditView::SetAttributes(*pSet, bReplaceAll);
            bRet = true;
        }
    }
    else
    {
#ifdef DBG_UTIL
        {
            bool bHasEEFeatureItems = false;
            SfxItemIter aIter(rSet);
            for (const SfxPoolItem* pItem = aIter.GetCurItem(); !bHasEEFeatureItems && pItem;
                 pItem = aIter.NextItem())
            {
                if (!IsInvalidItem(pItem))
                {
                    sal_uInt16 nW = pItem->Which();
                    if (nW >= EE_FEATURE_START && nW <= EE_FEATURE_END)
                        bHasEEFeatureItems = true;
                }
            }

            if (bHasEEFeatureItems)
            {
                std::unique_ptr<weld::MessageDialog> xInfoBox(Application::CreateMessageDialog(
                    nullptr, VclMessageType::Info, VclButtonsType::Ok,
                    u"SdrObjEditView::SetAttributes(): Setting EE_FEATURE items "
                    "at the SdrView does not make sense! It only leads to "
                    "overhead and unreadable documents."_ustr));
                xInfoBox->run();
            }
        }
#endif

        bool bOnlyEEItems;
        bool bNoEEItems = !SearchOutlinerItems(*pSet, bReplaceAll, &bOnlyEEItems);
        // everything selected? -> attributes to the border, too
        // if no EEItems, attributes to the border only
        if (bAllTextSelected || bNoEEItems)
        {
            if (mxSelectionController.is())
                bRet = mxSelectionController->SetAttributes(*pSet, bReplaceAll);

            if (!bRet)
            {
                const bool bUndo = IsUndoEnabled();

                if (bUndo)
                {
                    BegUndo(ImpGetDescriptionString(STR_EditSetAttributes));
                    AddUndo(GetModel().GetSdrUndoFactory().CreateUndoGeoObject(*pTextEditObj));

                    // If this is a text object also rescue the OutlinerParaObject since
                    // applying attributes to the object may change text layout when
                    // multiple portions exist with multiple formats. If an OutlinerParaObject
                    // really exists and needs to be rescued is evaluated in the undo
                    // implementation itself.
                    bool bRescueText(pTextEditObj);

                    AddUndo(GetModel().GetSdrUndoFactory().CreateUndoAttrObject(
                        *pTextEditObj, false, !bNoEEItems || bRescueText));
                    EndUndo();
                }

                pTextEditObj->SetMergedItemSetAndBroadcast(*pSet, bReplaceAll);

                FlushComeBackTimer(); // to set ModeHasChanged immediately
            }
        }
        else if (!bOnlyEEItems)
        {
            // Otherwise split Set, if necessary.
            // Now we build an ItemSet aSet that doesn't contain EE_Items from
            // *pSet (otherwise it would be a copy).
            WhichRangesContainer pNewWhichTable
                = RemoveWhichRange(pSet->GetRanges(), EE_ITEMS_START, EE_ITEMS_END);
            SfxItemSet aSet(GetModel().GetItemPool(), std::move(pNewWhichTable));
            SfxWhichIter aIter(aSet);
            sal_uInt16 nWhich = aIter.FirstWhich();
            while (nWhich != 0)
            {
                const SfxPoolItem* pItem;
                SfxItemState eState = pSet->GetItemState(nWhich, false, &pItem);
                if (eState == SfxItemState::SET)
                    aSet.Put(*pItem);
                nWhich = aIter.NextWhich();
            }

            if (mxSelectionController.is())
                bRet = mxSelectionController->SetAttributes(aSet, bReplaceAll);

            if (!bRet)
            {
                if (IsUndoEnabled())
                {
                    BegUndo(ImpGetDescriptionString(STR_EditSetAttributes));
                    AddUndo(GetModel().GetSdrUndoFactory().CreateUndoGeoObject(*pTextEditObj));
                    AddUndo(GetModel().GetSdrUndoFactory().CreateUndoAttrObject(*pTextEditObj));
                    EndUndo();
                }

                pTextEditObj->SetMergedItemSetAndBroadcast(aSet, bReplaceAll);

                const SdrMarkList& rMarkList = GetMarkedObjectList();
                if (rMarkList.GetMarkCount() == 1
                    && rMarkList.GetMark(0)->GetMarkedSdrObj() == pTextEditObj.get())
                {
                    SetNotPersistAttrToMarked(aSet);
                }
            }
            FlushComeBackTimer();
        }
        if (!bNoEEItems)
        {
            // and now the attributes to the EditEngine
            if (bReplaceAll)
            {
                mpTextEditOutlinerView->RemoveAttribs(true);
            }
            mpTextEditOutlinerView->SetAttribs(rSet);

            const Outliner& rTEOutliner = mpTextEditOutlinerView->GetOutliner();
            if (rTEOutliner.IsModified())
            {
                GetModel().SetChanged();
                SetInnerTextAreaForLOKit();
            }

            ImpMakeTextCursorAreaVisible();
        }
        bRet = true;
    }
    return bRet;
}

SfxStyleSheet* SdrObjEditView::GetStyleSheet() const
{
    SfxStyleSheet* pSheet = nullptr;

    if (mxSelectionController.is())
    {
        if (mxSelectionController->GetStyleSheet(pSheet))
            return pSheet;
    }

    if (mpTextEditOutlinerView)
    {
        pSheet = mpTextEditOutlinerView->GetStyleSheet();
    }
    else
    {
        pSheet = SdrGlueEditView::GetStyleSheet();
    }
    return pSheet;
}

void SdrObjEditView::SetStyleSheet(SfxStyleSheet* pStyleSheet, bool bDontRemoveHardAttr)
{
    if (mxSelectionController.is())
    {
        if (mxSelectionController->SetStyleSheet(pStyleSheet, bDontRemoveHardAttr))
            return;
    }

    // if we are currently in edit mode we must also set the stylesheet
    // on all paragraphs in the Outliner for the edit view
    if (nullptr != mpTextEditOutlinerView)
    {
        Outliner& rOutliner = mpTextEditOutlinerView->GetOutliner();

        const sal_Int32 nParaCount = rOutliner.GetParagraphCount();
        for (sal_Int32 nPara = 0; nPara < nParaCount; nPara++)
            rOutliner.SetStyleSheet(nPara, pStyleSheet);
    }

    SdrGlueEditView::SetStyleSheet(pStyleSheet, bDontRemoveHardAttr);
}

void SdrObjEditView::AddDeviceToPaintView(OutputDevice& rNewDev, vcl::Window* pWindow)
{
    SdrGlueEditView::AddDeviceToPaintView(rNewDev, pWindow);

    if (mxWeakTextEditObj.get() && !mbTextEditOnlyOneView
        && rNewDev.GetOutDevType() == OUTDEV_WINDOW)
    {
        OutlinerView* pOutlView = ImpMakeOutlinerView(rNewDev.GetOwnerWindow(), nullptr);
        mpTextEditOutliner->InsertView(pOutlView);
    }
}

void SdrObjEditView::DeleteDeviceFromPaintView(OutputDevice& rOldDev)
{
    SdrGlueEditView::DeleteDeviceFromPaintView(rOldDev);

    if (mxWeakTextEditObj.get() && !mbTextEditOnlyOneView
        && rOldDev.GetOutDevType() == OUTDEV_WINDOW)
    {
        for (size_t i = mpTextEditOutliner->GetViewCount(); i > 0;)
        {
            i--;
            OutlinerView* pOLV = mpTextEditOutliner->GetView(i);
            if (pOLV && pOLV->GetWindow() == rOldDev.GetOwnerWindow())
            {
                mpTextEditOutliner->RemoveView(i);
            }
        }
    }

    lcl_RemoveTextEditOutlinerViews(this, GetSdrPageView(), &rOldDev);
}

bool SdrObjEditView::IsTextEditInSelectionMode() const
{
    return mpTextEditOutliner != nullptr && mpTextEditOutliner->IsInSelectionMode();
}

// MacroMode

void SdrObjEditView::BegMacroObj(const Point& rPnt, short nTol, SdrObject* pObj, SdrPageView* pPV,
                                 vcl::Window* pWin)
{
    BrkMacroObj();
    if (pObj != nullptr && pPV != nullptr && pWin != nullptr && pObj->HasMacro())
    {
        nTol = ImpGetHitTolLogic(nTol, nullptr);
        m_pMacroObj = pObj;
        m_pMacroPV = pPV;
        m_pMacroWin = pWin;
        mbMacroDown = false;
        m_nMacroTol = sal_uInt16(nTol);
        m_aMacroDownPos = rPnt;
        MovMacroObj(rPnt);
    }
}

void SdrObjEditView::ImpMacroUp(const Point& rUpPos)
{
    if (m_pMacroObj != nullptr && mbMacroDown)
    {
        SdrObjMacroHitRec aHitRec;
        aHitRec.aPos = rUpPos;
        aHitRec.nTol = m_nMacroTol;
        aHitRec.pVisiLayer = &m_pMacroPV->GetVisibleLayers();
        aHitRec.pPageView = m_pMacroPV;
        m_pMacroObj->PaintMacro(*m_pMacroWin->GetOutDev(), tools::Rectangle(), aHitRec);
        mbMacroDown = false;
    }
}

void SdrObjEditView::ImpMacroDown(const Point& rDownPos)
{
    if (m_pMacroObj != nullptr && !mbMacroDown)
    {
        SdrObjMacroHitRec aHitRec;
        aHitRec.aPos = rDownPos;
        aHitRec.nTol = m_nMacroTol;
        aHitRec.pVisiLayer = &m_pMacroPV->GetVisibleLayers();
        aHitRec.pPageView = m_pMacroPV;
        m_pMacroObj->PaintMacro(*m_pMacroWin->GetOutDev(), tools::Rectangle(), aHitRec);
        mbMacroDown = true;
    }
}

void SdrObjEditView::MovMacroObj(const Point& rPnt)
{
    if (m_pMacroObj == nullptr)
        return;

    SdrObjMacroHitRec aHitRec;
    aHitRec.aPos = rPnt;
    aHitRec.nTol = m_nMacroTol;
    aHitRec.pVisiLayer = &m_pMacroPV->GetVisibleLayers();
    aHitRec.pPageView = m_pMacroPV;
    bool bDown = m_pMacroObj->IsMacroHit(aHitRec);
    if (bDown)
        ImpMacroDown(rPnt);
    else
        ImpMacroUp(rPnt);
}

void SdrObjEditView::BrkMacroObj()
{
    if (m_pMacroObj != nullptr)
    {
        ImpMacroUp(m_aMacroDownPos);
        m_pMacroObj = nullptr;
        m_pMacroPV = nullptr;
        m_pMacroWin = nullptr;
    }
}

bool SdrObjEditView::EndMacroObj()
{
    if (m_pMacroObj != nullptr && mbMacroDown)
    {
        ImpMacroUp(m_aMacroDownPos);
        SdrObjMacroHitRec aHitRec;
        aHitRec.aPos = m_aMacroDownPos;
        aHitRec.nTol = m_nMacroTol;
        aHitRec.pVisiLayer = &m_pMacroPV->GetVisibleLayers();
        aHitRec.pPageView = m_pMacroPV;
        bool bRet = m_pMacroObj->DoMacro(aHitRec);
        m_pMacroObj = nullptr;
        m_pMacroPV = nullptr;
        m_pMacroWin = nullptr;
        return bRet;
    }
    else
    {
        BrkMacroObj();
        return false;
    }
}

/** fills the given any with a XTextCursor for the current text selection.
    Leaves the any untouched if there currently is no text selected */
void SdrObjEditView::getTextSelection(css::uno::Any& rSelection)
{
    if (!IsTextEdit())
        return;

    OutlinerView* pOutlinerView = GetTextEditOutlinerView();
    if (!(pOutlinerView && pOutlinerView->HasSelection()))
        return;

    SdrObject* pObj = GetTextEditObject();

    if (!pObj)
        return;

    css::uno::Reference<css::text::XText> xText(pObj->getUnoShape(), css::uno::UNO_QUERY);
    if (xText.is())
    {
        SvxUnoTextBase* pRange = comphelper::getFromUnoTunnel<SvxUnoTextBase>(xText);
        if (pRange)
        {
            rSelection <<= pRange->createTextCursorBySelection(pOutlinerView->GetSelection());
        }
    }
}

/* check if we have a single selection and that single object likes
    to handle the mouse and keyboard events itself

    TODO: the selection controller should be queried from the
    object specific view contact. Currently this method only
    works for tables.
*/
void SdrObjEditView::MarkListHasChanged()
{
    SdrGlueEditView::MarkListHasChanged();

    if (mxSelectionController.is())
    {
        mxLastSelectionController = mxSelectionController;
        mxSelectionController->onSelectionHasChanged();
    }

    mxSelectionController.clear();

    const SdrMarkList& rMarkList = GetMarkedObjectList();
    if (rMarkList.GetMarkCount() != 1)
        return;

    const SdrObject* pObj(rMarkList.GetMark(0)->GetMarkedSdrObj());
    SdrView* pView(dynamic_cast<SdrView*>(this));

    // check for table
    if (pObj && pView && (pObj->GetObjInventor() == SdrInventor::Default)
        && (pObj->GetObjIdentifier() == SdrObjKind::Table))
    {
        mxSelectionController = sdr::table::CreateTableController(
            *pView, static_cast<const sdr::table::SdrTableObj&>(*pObj), mxLastSelectionController);

        if (mxSelectionController.is())
        {
            mxLastSelectionController.clear();
            mxSelectionController->onSelectionHasChanged();
        }
    }
}

IMPL_LINK(SdrObjEditView, EndPasteOrDropHdl, PasteOrDropInfos*, pInfo, void)
{
    OnEndPasteOrDrop(pInfo);
}

IMPL_LINK(SdrObjEditView, BeginPasteOrDropHdl, PasteOrDropInfos*, pInfo, void)
{
    OnBeginPasteOrDrop(pInfo);
}

void SdrObjEditView::OnBeginPasteOrDrop(PasteOrDropInfos*)
{
    // applications can derive from these virtual methods to do something before a drop or paste operation
}

void SdrObjEditView::OnEndPasteOrDrop(PasteOrDropInfos*)
{
    // applications can derive from these virtual methods to do something before a drop or paste operation
}

sal_uInt16 SdrObjEditView::GetSelectionLevel() const
{
    if (!IsTextEdit())
        return 0xFFFF;
    DBG_ASSERT(mpTextEditOutlinerView != nullptr,
               "SdrObjEditView::GetAttributes(): mpTextEditOutlinerView=NULL");
    DBG_ASSERT(mpTextEditOutliner != nullptr,
               "SdrObjEditView::GetAttributes(): mpTextEditOutliner=NULL");
    if (!mpTextEditOutlinerView)
        return 0xFFFF;
    //start and end position
    ESelection aSelect = mpTextEditOutlinerView->GetSelection();
    sal_Int32 nStartPara = ::std::min(aSelect.start.nPara, aSelect.end.nPara);
    sal_Int32 nEndPara = ::std::max(aSelect.start.nPara, aSelect.end.nPara);
    //get level from each paragraph
    sal_uInt16 nLevel = 0;
    for (sal_Int32 nPara = nStartPara; nPara <= nEndPara; nPara++)
    {
        sal_Int16 nDepth = mpTextEditOutliner->GetDepth(nPara);
        assert(nDepth <= 15);
        if (nDepth >= 0)
        {
            sal_uInt16 nParaDepth = 1 << static_cast<sal_uInt16>(nDepth);
            if (!(nLevel & nParaDepth))
                nLevel += nParaDepth;
        }
    }
    //reduce one level for Outliner Object
    //if( nLevel > 0 && GetTextEditObject()->GetObjIdentifier() == OBJ_OUTLINETEXT )
    //  nLevel = nLevel >> 1;
    //no bullet paragraph selected
    if (nLevel == 0)
        nLevel = 0xFFFF;
    return nLevel;
}

bool SdrObjEditView::SupportsFormatPaintbrush(SdrInventor nObjectInventor,
                                              SdrObjKind nObjectIdentifier)
{
    if (nObjectInventor != SdrInventor::Default && nObjectInventor != SdrInventor::E3d)
        return false;
    switch (nObjectIdentifier)
    {
        case SdrObjKind::NONE:
        case SdrObjKind::Group:
            return false;
        case SdrObjKind::Line:
        case SdrObjKind::Rectangle:
        case SdrObjKind::CircleOrEllipse:
        case SdrObjKind::CircleSection:
        case SdrObjKind::CircleArc:
        case SdrObjKind::CircleCut:
        case SdrObjKind::Polygon:
        case SdrObjKind::PolyLine:
        case SdrObjKind::PathLine:
        case SdrObjKind::PathFill:
        case SdrObjKind::FreehandLine:
        case SdrObjKind::FreehandFill:
        case SdrObjKind::Text:
        case SdrObjKind::TitleText:
        case SdrObjKind::OutlineText:
        case SdrObjKind::Graphic:
        case SdrObjKind::OLE2:
        case SdrObjKind::Table:
            return true;
        case SdrObjKind::Caption:
            return false;
        case SdrObjKind::Edge:
        case SdrObjKind::PathPoly:
        case SdrObjKind::PathPolyLine:
            return true;
        case SdrObjKind::Page:
        case SdrObjKind::Measure:
        case SdrObjKind::OLEPluginFrame:
        case SdrObjKind::UNO:
            return false;
        case SdrObjKind::CustomShape:
            return true;
        default:
            return false;
    }
}

static const WhichRangesContainer& GetFormatRangeImpl(bool bTextOnly, bool withParagraphAttr = true)
{
    static const WhichRangesContainer gFull(
        svl::Items<XATTR_LINE_FIRST, XATTR_LINE_LAST, XATTR_FILL_FIRST, XATTRSET_FILL,
                   SDRATTR_SHADOW_FIRST, SDRATTR_SHADOW_LAST, SDRATTR_MISC_FIRST,
                   SDRATTR_MISC_LAST, // table cell formats
                   SDRATTR_GRAF_FIRST, SDRATTR_GRAF_LAST, SDRATTR_TABLE_FIRST, SDRATTR_TABLE_LAST,
                   SDRATTR_GLOW_FIRST, SDRATTR_GLOW_LAST, SDRATTR_SOFTEDGE_FIRST,
                   SDRATTR_SOFTEDGE_LAST, SDRATTR_GLOW_TEXT_FIRST, SDRATTR_GLOW_TEXT_LAST,
                   EE_PARA_START, EE_PARA_END, EE_CHAR_START, EE_CHAR_END>);

    static const WhichRangesContainer gTextOnly(
        svl::Items<SDRATTR_MISC_FIRST, SDRATTR_MISC_LAST, EE_CHAR_START, EE_CHAR_END>);

    static const WhichRangesContainer gParaTextOnly(
        svl::Items<SDRATTR_MISC_FIRST, SDRATTR_MISC_LAST, EE_PARA_START, EE_PARA_END, EE_CHAR_START,
                   EE_CHAR_END>);

    return bTextOnly ? withParagraphAttr ? gParaTextOnly : gTextOnly : gFull;
}

sal_Int32 SdrObjEditView::TakeFormatPaintBrush(std::shared_ptr<SfxItemSet>& rFormatSet)
{
    sal_Int32 nDepth = -2;
    const SdrMarkList& rMarkList = GetMarkedObjectList();
    if (rMarkList.GetMarkCount() <= 0)
        return nDepth;

    OutlinerView* pOLV = GetTextEditOutlinerView();
    bool isParaSelection
        = pOLV ? !pOLV->GetEditView().HasSelection() || pOLV->GetEditView().IsSelectionFullPara()
               : false;
    rFormatSet = std::make_shared<SfxItemSet>(GetModel().GetItemPool(),
                                              GetFormatRangeImpl(pOLV != nullptr, isParaSelection));
    if (pOLV)
    {
        rFormatSet->Put(pOLV->GetAttribs());
        if (isParaSelection)
            nDepth = pOLV->GetDepth();
    }
    else
    {
        const bool bOnlyHardAttr = false;
        rFormatSet->Put(GetAttrFromMarked(bOnlyHardAttr));
    }

    // check for cloning from table cell, in which case we need to copy cell-specific formatting attributes
    const SdrObject* pObj = rMarkList.GetMark(0)->GetMarkedSdrObj();
    if (pObj && (pObj->GetObjInventor() == SdrInventor::Default)
        && (pObj->GetObjIdentifier() == SdrObjKind::Table))
    {
        auto pTable = static_cast<const sdr::table::SdrTableObj*>(pObj);
        if (mxSelectionController.is() && pTable->getActiveCell().is())
        {
            mxSelectionController->GetAttributes(*rFormatSet, false);
        }
    }
    return nDepth;
}

static SfxItemSet CreatePaintSet(const WhichRangesContainer& pRanges, SfxItemPool& rPool,
                                 const SfxItemSet& rSourceSet, const SfxItemSet& rTargetSet,
                                 bool bNoCharacterFormats, bool bNoParagraphFormats)
{
    SfxItemSet aPaintSet(rPool, pRanges);

    for (const auto& pRange : pRanges)
    {
        sal_uInt16 nWhich = pRange.first;
        const sal_uInt16 nLastWhich = pRange.second;

        if (bNoCharacterFormats && (nWhich == EE_CHAR_START))
            continue;

        if (bNoParagraphFormats && (nWhich == EE_PARA_START))
            continue;

        for (; nWhich <= nLastWhich; nWhich++)
        {
            const SfxPoolItem* pSourceItem = rSourceSet.GetItem(nWhich);
            const SfxPoolItem* pTargetItem = rTargetSet.GetItem(nWhich);

            if ((pSourceItem && !pTargetItem)
                || (pSourceItem && pTargetItem && *pSourceItem != *pTargetItem))
            {
                aPaintSet.Put(*pSourceItem);
            }
        }
    }
    return aPaintSet;
}

void SdrObjEditView::ApplyFormatPaintBrushToText(SfxItemSet const& rFormatSet, SdrTextObj& rTextObj,
                                                 SdrText* pText, sal_Int16 nDepth,
                                                 bool bNoCharacterFormats, bool bNoParagraphFormats)
{
    OutlinerParaObject* pParaObj = pText ? pText->GetOutlinerParaObject() : nullptr;
    if (!pParaObj)
        return;

    SdrOutliner& rOutliner = rTextObj.ImpGetDrawOutliner();
    rOutliner.SetText(*pParaObj);

    sal_Int32 nParaCount(rOutliner.GetParagraphCount());

    if (!nParaCount)
        return;

    for (sal_Int32 nPara = 0; nPara < nParaCount; nPara++)
    {
        if (!bNoCharacterFormats)
            rOutliner.RemoveCharAttribs(nPara);

        SfxItemSet aSet(rOutliner.GetParaAttribs(nPara));
        aSet.Put(CreatePaintSet(GetFormatRangeImpl(true), *aSet.GetPool(), rFormatSet, aSet,
                                bNoCharacterFormats, bNoParagraphFormats));
        rOutliner.SetParaAttribs(nPara, aSet);
        Paragraph* pParagraph = rOutliner.GetParagraph(nPara);
        if (nDepth > -2)
            rOutliner.SetDepth(pParagraph, nDepth);
    }

    std::optional<OutlinerParaObject> pTemp = rOutliner.CreateParaObject(0, nParaCount);
    rOutliner.Clear();

    rTextObj.NbcSetOutlinerParaObjectForText(std::move(pTemp), pText);
}

void SdrObjEditView::DisposeUndoManager()
{
    if (mpTextEditOutliner)
    {
        if (typeid(mpTextEditOutliner->GetUndoManager()) != typeid(EditUndoManager))
        {
            // Non-owning pointer, clear it.
            mpTextEditOutliner->SetUndoManager(nullptr);
        }
    }

    mpOldTextEditUndoManager = nullptr;
}

void SdrObjEditView::ApplyFormatPaintBrush(SfxItemSet& rFormatSet, sal_Int16 nDepth,
                                           bool bNoCharacterFormats, bool bNoParagraphFormats)
{
    if (mxSelectionController.is()
        && mxSelectionController->ApplyFormatPaintBrush(rFormatSet, nDepth, bNoCharacterFormats,
                                                        bNoParagraphFormats))
    {
        return;
    }

    OutlinerView* pOLV = GetTextEditOutlinerView();
    const SdrMarkList& rMarkList = GetMarkedObjectList();
    if (!pOLV)
    {
        SdrObject* pObj = rMarkList.GetMark(0)->GetMarkedSdrObj();
        const SfxItemSet& rShapeSet = pObj->GetMergedItemSet();

        // if not in text edit mode (aka the user selected text or clicked on a word)
        // apply formatting attributes to selected shape
        // All formatting items (see ranges above) that are unequal in selected shape and
        // the format paintbrush are hard set on the selected shape.

        const WhichRangesContainer& pRanges = rFormatSet.GetRanges();
        bool bTextOnly = true;

        for (const auto& pRange : pRanges)
        {
            if ((pRange.first != EE_PARA_START) && (pRange.first != EE_CHAR_START))
            {
                bTextOnly = false;
                break;
            }
        }

        if (!bTextOnly)
        {
            SfxItemSet aPaintSet(CreatePaintSet(GetFormatRangeImpl(false), *rShapeSet.GetPool(),
                                                rFormatSet, rShapeSet, bNoCharacterFormats,
                                                bNoParagraphFormats));
            SetAttrToMarked(aPaintSet, false /*bReplaceAll*/);
        }

        // now apply character and paragraph formatting to text, if the shape has any
        SdrTextObj* pTextObj = DynCastSdrTextObj(pObj);
        if (pTextObj)
        {
            sal_Int32 nText = pTextObj->getTextCount();

            while (--nText >= 0)
            {
                SdrText* pText = pTextObj->getText(nText);
                ApplyFormatPaintBrushToText(rFormatSet, *pTextObj, pText, nDepth,
                                            bNoCharacterFormats, bNoParagraphFormats);
            }
        }
    }
    else
    {
        ::Outliner& rOutliner = pOLV->GetOutliner();
        const EditEngine& rEditEngine = rOutliner.GetEditEngine();

        ESelection aSel(pOLV->GetSelection());
        bool fullParaSelection
            = aSel.end.nPara != aSel.start.nPara || pOLV->GetEditView().IsSelectionFullPara();
        if (!aSel.HasRange())
            pOLV->SetSelection(rEditEngine.GetWord(aSel, css::i18n::WordType::DICTIONARY_WORD));
        const bool bRemoveParaAttribs = !bNoParagraphFormats && !fullParaSelection;
        pOLV->RemoveAttribsKeepLanguages(bRemoveParaAttribs);
        SfxItemSet aSet(pOLV->GetAttribs());
        SfxItemSet aPaintSet(CreatePaintSet(GetFormatRangeImpl(true), *aSet.GetPool(), rFormatSet,
                                            aSet, bNoCharacterFormats, bNoParagraphFormats));
        pOLV->SetAttribs(aPaintSet);
        if (!bNoParagraphFormats && nDepth > -2)
        {
            for (sal_Int32 nPara = aSel.start.nPara; nPara <= aSel.end.nPara; ++nPara)
                pOLV->SetDepth(nPara, nDepth);
        }
    }

    // check for cloning to table cell, in which case we need to copy cell-specific formatting attributes
    SdrObject* pObj = rMarkList.GetMark(0)->GetMarkedSdrObj();
    if (pObj && (pObj->GetObjInventor() == SdrInventor::Default)
        && (pObj->GetObjIdentifier() == SdrObjKind::Table))
    {
        auto pTable = static_cast<sdr::table::SdrTableObj*>(pObj);
        if (pTable->getActiveCell().is() && mxSelectionController.is())
        {
            mxSelectionController->SetAttributes(rFormatSet, false);
        }
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
