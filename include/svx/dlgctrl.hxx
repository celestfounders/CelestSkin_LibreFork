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
#ifndef INCLUDED_SVX_DLGCTRL_HXX
#define INCLUDED_SVX_DLGCTRL_HXX

#include <sfx2/tabdlg.hxx>
#include <svx/svxdllapi.h>
#include <svx/rectenum.hxx>
#include <vcl/customweld.hxx>
#include <vcl/weld.hxx>
#include <vcl/virdev.hxx>
#include <svx/xtable.hxx>
#include <rtl/ref.hxx>
#include <o3tl/typed_flags_set.hxx>
#include <memory>
#include <array>

namespace com::sun::star::awt {
    struct Point;
}

/*************************************************************************
|* Derived from SfxTabPage for being able to get notified through the
|* virtual method from the control.
\************************************************************************/

class SAL_WARN_UNUSED SvxTabPage : public SfxTabPage
{

public:
    SvxTabPage(weld::Container* pPage, weld::DialogController* pController, const OUString& rUIXMLDescription, const OUString& rID, const SfxItemSet &rAttrSet)
        : SfxTabPage(pPage, pController, rUIXMLDescription, rID, &rAttrSet)
    {
    }
    virtual void PointChanged(weld::DrawingArea* pArea, RectPoint eRP) = 0;
};

/*************************************************************************
|* Control for display and selection of the corner and center points of
|* an object
\************************************************************************/

enum class CTL_STATE
{
    NONE     = 0,
    NOHORZ   = 1,       // no horizontal input information is used
    NOVERT   = 2,       // no vertical input information is used
};
namespace o3tl
{
    template<> struct typed_flags<CTL_STATE> : is_typed_flags<CTL_STATE, 0x03> {};
}

class SvxRectCtlAccessibleContext;
class SvxPixelCtlAccessible;

class SAL_WARN_UNUSED SVX_DLLPUBLIC SvxRectCtl : public weld::CustomWidgetController
{
private:
    SvxTabPage* m_pPage;
    rtl::Reference<SvxRectCtlAccessibleContext> pAccContext;
    sal_uInt16 nBorderWidth;
    Point aPtLT, aPtMT, aPtRT;
    Point aPtLM, aPtMM, aPtRM;
    Point aPtLB, aPtMB, aPtRB;
    Point aPtNew;
    RectPoint eRP, eDefRP;
    std::unique_ptr<Bitmap> pBitmap;
    CTL_STATE m_nState;
    bool mbCompleteDisable : 1;

    SVX_DLLPRIVATE static void      InitSettings(vcl::RenderContext& rRenderContext);
    SVX_DLLPRIVATE void             InitRectBitmap();
    SVX_DLLPRIVATE Bitmap&          GetRectBitmap();
    SVX_DLLPRIVATE void             Resize_Impl(const Size& rSize);

    SvxRectCtl(const SvxRectCtl&) = delete;
    SvxRectCtl& operator=(const SvxRectCtl&) = delete;

protected:
    RectPoint           GetRPFromPoint( Point, bool bRTL = false ) const;
    const Point&        GetPointFromRP( RectPoint ) const;
    Point               SetActualRPWithoutInvalidate( RectPoint eNewRP );  // returns the last point

    Point               GetApproxLogPtFromPixPt( const Point& rRoughPixelPoint ) const;
public:
    SvxRectCtl(SvxTabPage* pPage);
    void SetControlSettings(RectPoint eRpt, sal_uInt16 nBorder);
    virtual ~SvxRectCtl() override;

    virtual void Paint(vcl::RenderContext& rRenderContext, const tools::Rectangle&) override;
    virtual void Resize() override;
    virtual bool MouseButtonDown(const MouseEvent&) override;
    virtual bool KeyInput(const KeyEvent&) override;
    virtual void GetFocus() override;
    virtual void LoseFocus() override;
    virtual tools::Rectangle GetFocusRect() override;
    virtual void SetDrawingArea(weld::DrawingArea* pDrawingArea) override;
    virtual void StyleUpdated() override;

    void                Reset();
    RectPoint           GetActualRP() const { return eRP;}
    void                SetActualRP( RectPoint eNewRP );

    void                SetState( CTL_STATE nState );

    static const sal_uInt8 NO_CHILDREN = 9;   // returns number of usable radio buttons

    tools::Rectangle           CalculateFocusRectangle() const;
    tools::Rectangle           CalculateFocusRectangle( RectPoint eRectPoint ) const;

    css::uno::Reference<css::accessibility::XAccessible> getAccessibleParent() const { return GetDrawingArea()->get_accessible_parent(); }
    virtual rtl::Reference<comphelper::OAccessible> CreateAccessible() override;
    a11yrelationset get_accessible_relation_set() const { return GetDrawingArea()->get_accessible_relation_set(); }

    RectPoint          GetApproxRPFromPixPt( const css::awt::Point& rPixelPoint ) const;

    bool IsCompletelyDisabled() const { return mbCompleteDisable; }
    void DoCompletelyDisable(bool bNew);
};

/*************************************************************************
|* Control for editing bitmaps
\************************************************************************/

class SAL_WARN_UNUSED SVX_DLLPUBLIC SvxPixelCtl final : public weld::CustomWidgetController
{
private:
    static sal_uInt16 constexpr nLines = 8;
    static sal_uInt16 constexpr nSquares = nLines * nLines;

    SvxTabPage* m_pPage;

    Color       aPixelColor;
    Color       aBackgroundColor;
    Size        aRectSize;
    std::array<sal_uInt8,nSquares> maPixelData;
    bool        bPaintable;
    //Add member identifying position
    Point       aFocusPosition;
    rtl::Reference<SvxPixelCtlAccessible>  m_xAccess;

    SAL_DLLPRIVATE tools::Rectangle   implCalFocusRect( const Point& aPosition );
    SAL_DLLPRIVATE void    ChangePixel( sal_uInt16 nPixel );

    SvxPixelCtl(SvxPixelCtl const&) = delete;
    SvxPixelCtl(SvxPixelCtl&&) = delete;
    SvxPixelCtl& operator=(SvxPixelCtl const&) = delete;
    SvxPixelCtl& operator=(SvxPixelCtl&&) = delete;

public:
    SvxPixelCtl(SvxTabPage* pPage);

    virtual ~SvxPixelCtl() override;

    SAL_DLLPRIVATE virtual void SetDrawingArea(weld::DrawingArea* pDrawingArea) override;
    SAL_DLLPRIVATE virtual void Paint( vcl::RenderContext& rRenderContext, const tools::Rectangle& rRect ) override;
    SAL_DLLPRIVATE virtual bool MouseButtonDown( const MouseEvent& rMEvt ) override;
    SAL_DLLPRIVATE virtual void Resize() override;
    SAL_DLLPRIVATE virtual tools::Rectangle GetFocusRect() override;

    void    SetXBitmap( const Bitmap& rBitmap );

    void    SetPixelColor( const Color& rCol ) { aPixelColor = rCol; }
    void    SetBackgroundColor( const Color& rCol ) { aBackgroundColor = rCol; }

    static sal_uInt16 GetLineCount() { return nLines; }

    SAL_DLLPRIVATE sal_uInt8  GetBitmapPixel( const sal_uInt16 nPixelNumber ) const;
    std::array<sal_uInt8,64> const & GetBitmapPixelPtr() const { return maPixelData; }

    void    SetPaintable( bool bTmp ) { bPaintable = bTmp; }
    void    Reset();

    css::uno::Reference<css::accessibility::XAccessible> getAccessibleParent() const { return GetDrawingArea()->get_accessible_parent(); }
    SAL_DLLPRIVATE virtual rtl::Reference<comphelper::OAccessible> CreateAccessible() override;
    a11yrelationset get_accessible_relation_set() const { return GetDrawingArea()->get_accessible_relation_set(); }

    static tools::Long GetSquares() { return nSquares ; }
    tools::Long GetWidth() const { return aRectSize.getWidth() ; }
    tools::Long GetHeight() const { return aRectSize.getHeight() ; }

    //Device Pixel .
    SAL_DLLPRIVATE tools::Long ShowPosition( const Point &pt);

    SAL_DLLPRIVATE tools::Long PointToIndex(const Point &pt) const;
    SAL_DLLPRIVATE Point IndexToPoint(tools::Long nIndex) const ;
    SAL_DLLPRIVATE tools::Long GetFocusPosIndex() const ;
    //Keyboard function for key input and focus handling function
    SAL_DLLPRIVATE virtual bool        KeyInput( const KeyEvent& rKEvt ) override;
    SAL_DLLPRIVATE virtual void        GetFocus() override;
    SAL_DLLPRIVATE virtual void        LoseFocus() override;
};

/************************************************************************/

class SAL_WARN_UNUSED SVX_DLLPUBLIC SvxLineLB
{
private:
    std::unique_ptr<weld::ComboBox> m_xControl;

    /// defines if standard fields (none, solid) are added, default is true
    bool        mbAddStandardFields : 1;

public:
    SvxLineLB(std::unique_ptr<weld::ComboBox> pControl);

    void Fill(const XDashListRef &pList);
    bool getAddStandardFields() const { return mbAddStandardFields; }
    void setAddStandardFields(bool bNew);

    void Append(const XDashEntry& rEntry, const Bitmap& rBitmap );
    void Modify(const XDashEntry& rEntry, sal_Int32 nPos, const Bitmap& rBitmap );

    void clear() { m_xControl->clear(); }
    void remove(int nPos) { m_xControl->remove(nPos); }
    int get_active() const { return m_xControl->get_active(); }
    void set_active(int nPos) { m_xControl->set_active(nPos); }
    void set_active_text(const OUString& rStr) { m_xControl->set_active_text(rStr); }
    OUString get_active_text() const { return m_xControl->get_active_text(); }
    void connect_changed(const Link<weld::ComboBox&, void>& rLink) { m_xControl->connect_changed(rLink); }
    int get_count() const { return m_xControl->get_count(); }
    void append_text(const OUString& rStr) { m_xControl->append_text(rStr); }
    bool get_value_changed_from_saved() const { return m_xControl->get_value_changed_from_saved(); }
    void save_value() { m_xControl->save_value(); }
};

/************************************************************************/

class SAL_WARN_UNUSED SVX_DLLPUBLIC SvxLineEndLB
{
private:
    std::unique_ptr<weld::ComboBox> m_xControl;

public:
    SvxLineEndLB(std::unique_ptr<weld::ComboBox> pControl);

    void Fill( const XLineEndListRef &pList, bool bStart = true );

    void    Append( const XLineEndEntry& rEntry, const Bitmap& rBitmap );
    void    Modify( const XLineEndEntry& rEntry, sal_Int32 nPos, const Bitmap& rBitmap );

    void clear() { m_xControl->clear(); }
    void remove(int nPos) { m_xControl->remove(nPos); }
    int get_active() const { return m_xControl->get_active(); }
    void set_active(int nPos) { m_xControl->set_active(nPos); }
    void set_active_text(const OUString& rStr) { m_xControl->set_active_text(rStr); }
    OUString get_active_text() const { return m_xControl->get_active_text(); }
    void connect_changed(const Link<weld::ComboBox&, void>& rLink) { m_xControl->connect_changed(rLink); }
    int get_count() const { return m_xControl->get_count(); }
    void append_text(const OUString& rStr) { m_xControl->append_text(rStr); }
    bool get_value_changed_from_saved() const { return m_xControl->get_value_changed_from_saved(); }
    void save_value() { m_xControl->save_value(); }
    void set_sensitive(bool bSensitive) { m_xControl->set_sensitive(bSensitive); }
    bool get_sensitive() const { return m_xControl->get_sensitive(); }
};

class SdrObject;
class SdrPathObj;
class SdrModel;

class SAL_WARN_UNUSED SAL_DLLPUBLIC_RTTI SvxPreviewBase : public weld::CustomWidgetController
{
private:
    std::unique_ptr<SdrModel> mpModel;
    VclPtr<VirtualDevice> mpBufferDevice;

protected:
    void InitSettings();

    tools::Rectangle GetPreviewSize() const;

    // prepare buffered paint
    void LocalPrePaint(vcl::RenderContext const & rRenderContext);

    // end and output buffered paint
    void LocalPostPaint(vcl::RenderContext& rRenderContext);

public:
    SvxPreviewBase();
    virtual void SetDrawingArea(weld::DrawingArea*) override;
    virtual ~SvxPreviewBase() override;

    // change support
    virtual void StyleUpdated() override;

    void SetDrawMode(DrawModeFlags nDrawMode)
    {
        mpBufferDevice->SetDrawMode(nDrawMode);
    }

    Size GetOutputSize() const
    {
        return mpBufferDevice->PixelToLogic(GetOutputSizePixel());
    }

    // dada read access
    SdrModel& getModel() const
    {
        return *mpModel;
    }
    OutputDevice& getBufferDevice() const
    {
        return *mpBufferDevice;
    }
};


/*************************************************************************
|*
|* SvxLinePreview
|*
\************************************************************************/

class SAL_WARN_UNUSED SVX_DLLPUBLIC SvxXLinePreview final : public SvxPreviewBase
{
private:
    rtl::Reference<SdrPathObj>                      mpLineObjA;
    rtl::Reference<SdrPathObj>                      mpLineObjB;
    rtl::Reference<SdrPathObj>                      mpLineObjC;

    Graphic*                                        mpGraphic;
    bool                                            mbWithSymbol;
    Size                                            maSymbolSize;

public:
    SvxXLinePreview();
    virtual void SetDrawingArea(weld::DrawingArea* pDrawingArea) override;
    virtual ~SvxXLinePreview() override;

    void SetLineAttributes(const SfxItemSet& rItemSet);

    void ShowSymbol( bool b ) { mbWithSymbol = b; };
    void SetSymbol( Graphic* p, const Size& s );
    void ResizeSymbol( const Size& s );

    virtual void Paint( vcl::RenderContext& rRenderContext, const tools::Rectangle& rRect ) override;
    virtual void Resize() override;
};

class SAL_WARN_UNUSED SVX_DLLPUBLIC SvxXRectPreview final : public SvxPreviewBase
{
private:
    rtl::Reference<SdrObject> mpRectangleObject;

public:
    SvxXRectPreview();
    virtual void SetDrawingArea(weld::DrawingArea* pDrawingArea) override;
    virtual ~SvxXRectPreview() override;

    void SetAttributes(const SfxItemSet& rItemSet);

    virtual void Paint( vcl::RenderContext& rRenderContext, const tools::Rectangle& rRect ) override;
    virtual void Resize() override;
};

/*************************************************************************
|*
|* SvxXShadowPreview
|*
\************************************************************************/

class SAL_WARN_UNUSED SVX_DLLPUBLIC SvxXShadowPreview final : public SvxPreviewBase
{
private:
    Point maShadowOffset;

    rtl::Reference<SdrObject> mpRectangleObject;
    rtl::Reference<SdrObject> mpRectangleShadow;

public:
    SvxXShadowPreview();
    virtual void SetDrawingArea(weld::DrawingArea* pDrawingArea) override;
    virtual ~SvxXShadowPreview() override;

    void SetRectangleAttributes(const SfxItemSet& rItemSet);
    void SetShadowAttributes(const SfxItemSet& rItemSet);
    void SetShadowPosition(const Point& rPos);

    virtual void Paint( vcl::RenderContext& rRenderContext, const tools::Rectangle& rRect ) override;
};

class SvxRelativeField;

void limitWidthForSidebar(weld::SpinButton& rSpinButton);
SVX_DLLPUBLIC void limitWidthForSidebar(SvxRelativeField& rMetricSpinButton);
//tdf#130197 Give this toolbar a width as if it had 5 standard toolbutton entries
SVX_DLLPUBLIC void padWidthForSidebar(weld::Toolbar& rToolbar, const css::uno::Reference<css::frame::XFrame>& rFrame);

#endif // INCLUDED_SVX_DLGCTRL_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
