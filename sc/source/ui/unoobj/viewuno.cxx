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

#include <com/sun/star/awt/MouseButton.hpp>
#include <com/sun/star/drawing/ShapeCollection.hpp>
#include <com/sun/star/lang/IndexOutOfBoundsException.hpp>
#include <com/sun/star/script/vba/XVBAEventProcessor.hpp>
#include <com/sun/star/util/VetoException.hpp>
#include <com/sun/star/view/DocumentZoomType.hpp>

#include <editeng/outliner.hxx>
#include <svx/svditer.hxx>
#include <svx/svdmark.hxx>
#include <svx/svdpage.hxx>
#include <svx/svdpagv.hxx>
#include <svx/svdview.hxx>
#include <svx/unoshape.hxx>
#include <svx/fmshell.hxx>
#include <sfx2/bindings.hxx>
#include <sfx2/dispatch.hxx>
#include <sfx2/request.hxx>
#include <sfx2/viewfrm.hxx>
#include <comphelper/profilezone.hxx>
#include <comphelper/processfactory.hxx>
#include <comphelper/sequence.hxx>
#include <cppuhelper/supportsservice.hxx>
#include <cppuhelper/queryinterface.hxx>
#include <toolkit/helper/vclunohelper.hxx>
#include <vcl/svapp.hxx>
#include <vcl/unohelp.hxx>
#include <tools/multisel.hxx>

#include <drawsh.hxx>
#include <drtxtob.hxx>
#include <transobj.hxx>
#include <editsh.hxx>
#include <viewuno.hxx>
#include <cellsuno.hxx>
#include <miscuno.hxx>
#include <tabvwsh.hxx>
#include <prevwsh.hxx>
#include <docsh.hxx>
#include <drwlayer.hxx>
#include <attrib.hxx>
#include <drawview.hxx>
#include <fupoor.hxx>
#include <sc.hrc>
#include <unonames.hxx>
#include <scmod.hxx>
#include <appoptio.hxx>
#include <gridwin.hxx>
#include <sheetevents.hxx>
#include <markdata.hxx>
#include <scextopt.hxx>
#include <preview.hxx>
#include <inputhdl.hxx>
#include <inputwin.hxx>
#include <svx/sdrhittesthelper.hxx>
#include <formatsh.hxx>
#include <sfx2/app.hxx>
#include <scitems.hxx>

using namespace com::sun::star;

//! Clipping Marks

//  no Which-ID here, Map only for PropertySetInfo

static std::span<const SfxItemPropertyMapEntry> lcl_GetViewOptPropertyMap()
{
    static const SfxItemPropertyMapEntry aViewOptPropertyMap_Impl[] =
    {
        { OLD_UNO_COLROWHDR,   0,  cppu::UnoType<bool>::get(),          0, 0},
        { SC_UNO_GRIDCOLOR,    0,  cppu::UnoType<sal_Int32>::get(),    0, 0},
        { SC_UNO_COLROWHDR,    0,  cppu::UnoType<bool>::get(),          0, 0},
        { SC_UNO_HORSCROLL,    0,  cppu::UnoType<bool>::get(),          0, 0},
        { SC_UNO_SHEETTABS,    0,  cppu::UnoType<bool>::get(),          0, 0},
        { SC_UNO_VERTSCROLL,   0,  cppu::UnoType<bool>::get(),          0, 0},
        { SC_UNO_HIDESPELL,    0,  cppu::UnoType<bool>::get(),          0, 0},  /* deprecated #i91949 */
        { OLD_UNO_HORSCROLL,   0,  cppu::UnoType<bool>::get(),          0, 0},
        { SC_UNO_OUTLSYMB,     0,  cppu::UnoType<bool>::get(),          0, 0},
        { SC_UNO_VALUEHIGH,    0,  cppu::UnoType<bool>::get(),          0, 0},
        { OLD_UNO_OUTLSYMB,    0,  cppu::UnoType<bool>::get(),          0, 0},
        { OLD_UNO_SHEETTABS,   0,  cppu::UnoType<bool>::get(),          0, 0},
        { SC_UNO_SHOWANCHOR,   0,  cppu::UnoType<bool>::get(),          0, 0},
        { SC_UNO_SHOWCHARTS,   0,  cppu::UnoType<sal_Int16>::get(),    0, 0},
        { SC_UNO_SHOWDRAW,     0,  cppu::UnoType<sal_Int16>::get(),    0, 0},
        { SC_UNO_SHOWFORM,     0,  cppu::UnoType<bool>::get(),          0, 0},
        { SC_UNO_SHOWGRID,     0,  cppu::UnoType<bool>::get(),          0, 0},
        { SC_UNO_SHOWHELP,     0,  cppu::UnoType<bool>::get(),          0, 0},
        { SC_UNO_SHOWNOTES,    0,  cppu::UnoType<bool>::get(),          0, 0},
        { SC_UNO_SHOWNOTEAUTHOR,    0,  cppu::UnoType<bool>::get(),          0, 0},
        { SC_UNO_SHOWFORMULASMARKS,    0,  cppu::UnoType<bool>::get(),          0, 0},
        { SC_UNO_SHOWOBJ,      0,  cppu::UnoType<sal_Int16>::get(),    0, 0},
        { SC_UNO_SHOWPAGEBR,   0,  cppu::UnoType<bool>::get(),          0, 0},
        { SC_UNO_SHOWZERO,     0,  cppu::UnoType<bool>::get(),          0, 0},
        { OLD_UNO_VALUEHIGH,   0,  cppu::UnoType<bool>::get(),          0, 0},
        { OLD_UNO_VERTSCROLL,  0,  cppu::UnoType<bool>::get(),          0, 0},
        { SC_UNO_VISAREA,      0,  cppu::UnoType<awt::Rectangle>::get(), 0, 0},
        { SC_UNO_ZOOMTYPE,     0,  cppu::UnoType<sal_Int16>::get(),    0, 0},
        { SC_UNO_ZOOMVALUE,    0,  cppu::UnoType<sal_Int16>::get(),    0, 0},
        { SC_UNO_VISAREASCREEN,0,  cppu::UnoType<awt::Rectangle>::get(), 0, 0},
        { SC_UNO_FORMULABARHEIGHT,0,cppu::UnoType<sal_Int16>::get(),     0, 0},
    };
    return aViewOptPropertyMap_Impl;
}

constexpr OUString SCTABVIEWOBJ_SERVICE = u"com.sun.star.sheet.SpreadsheetView"_ustr;
constexpr OUString SCVIEWSETTINGS_SERVICE = u"com.sun.star.sheet.SpreadsheetViewSettings"_ustr;

SC_SIMPLE_SERVICE_INFO( ScViewPaneBase, u"ScViewPaneObj"_ustr, u"com.sun.star.sheet.SpreadsheetViewPane"_ustr )

ScViewPaneBase::ScViewPaneBase(ScTabViewShell* pViewSh, sal_uInt16 nP) :
    pViewShell( pViewSh ),
    nPane( nP )
{
    if (pViewShell)
        StartListening(*pViewShell);
}

ScViewPaneBase::~ScViewPaneBase()
{
    SolarMutexGuard g;

    if (pViewShell)
        EndListening(*pViewShell);
}

void ScViewPaneBase::Notify( SfxBroadcaster&, const SfxHint& rHint )
{
    if ( rHint.GetId() == SfxHintId::Dying )
        pViewShell = nullptr;
}

uno::Any SAL_CALL ScViewPaneBase::queryInterface( const uno::Type& rType )
{
    uno::Any aReturn = ::cppu::queryInterface(rType,
                    static_cast<sheet::XViewPane*>(this),
                    static_cast<sheet::XCellRangeReferrer*>(this),
                    static_cast<view::XFormLayerAccess*>(this),
                    static_cast<view::XControlAccess*>(this),
                    static_cast<lang::XServiceInfo*>(this),
                    static_cast<lang::XTypeProvider*>(this));
    if ( aReturn.hasValue() )
        return aReturn;

    return uno::Any();          // OWeakObject is in derived objects
}

uno::Sequence<uno::Type> SAL_CALL ScViewPaneBase::getTypes()
{
    static const uno::Sequence<uno::Type> aTypes
    {
        cppu::UnoType<sheet::XViewPane>::get(),
        cppu::UnoType<sheet::XCellRangeReferrer>::get(),
        cppu::UnoType<view::XFormLayerAccess>::get(),
        cppu::UnoType<lang::XServiceInfo>::get(),
        cppu::UnoType<lang::XTypeProvider>::get(),
    };
    return aTypes;
}

uno::Sequence<sal_Int8> SAL_CALL ScViewPaneBase::getImplementationId()
{
    return css::uno::Sequence<sal_Int8>();
}

// XViewPane

sal_Int32 SAL_CALL ScViewPaneBase::getFirstVisibleColumn()
{
    SolarMutexGuard aGuard;
    if (pViewShell)
    {
        ScViewData& rViewData = pViewShell->GetViewData();
        ScSplitPos eWhich = ( nPane == SC_VIEWPANE_ACTIVE ) ?
                                rViewData.GetActivePart() :
                                static_cast<ScSplitPos>(nPane);
        ScHSplitPos eWhichH = WhichH( eWhich );

        return rViewData.GetPosX( eWhichH );
    }
    OSL_FAIL("no View ?!?"); //! Exception?
    return 0;
}

void SAL_CALL ScViewPaneBase::setFirstVisibleColumn(sal_Int32 nFirstVisibleColumn)
{
    SolarMutexGuard aGuard;
    if (pViewShell)
    {
        ScViewData& rViewData = pViewShell->GetViewData();
        ScSplitPos eWhich = ( nPane == SC_VIEWPANE_ACTIVE ) ?
                                rViewData.GetActivePart() :
                                static_cast<ScSplitPos>(nPane);
        ScHSplitPos eWhichH = WhichH( eWhich );

        tools::Long nDeltaX = static_cast<tools::Long>(nFirstVisibleColumn) - rViewData.GetPosX( eWhichH );
        pViewShell->ScrollX( nDeltaX, eWhichH );
    }
}

sal_Int32 SAL_CALL ScViewPaneBase::getFirstVisibleRow()
{
    SolarMutexGuard aGuard;
    if (pViewShell)
    {
        ScViewData& rViewData = pViewShell->GetViewData();
        ScSplitPos eWhich = ( nPane == SC_VIEWPANE_ACTIVE ) ?
                                rViewData.GetActivePart() :
                                static_cast<ScSplitPos>(nPane);
        ScVSplitPos eWhichV = WhichV( eWhich );

        return rViewData.GetPosY( eWhichV );
    }
    OSL_FAIL("no View ?!?"); //! Exception?
    return 0;
}

void SAL_CALL ScViewPaneBase::setFirstVisibleRow( sal_Int32 nFirstVisibleRow )
{
    SolarMutexGuard aGuard;
    if (pViewShell)
    {
        ScViewData& rViewData = pViewShell->GetViewData();
        ScSplitPos eWhich = ( nPane == SC_VIEWPANE_ACTIVE ) ?
                                rViewData.GetActivePart() :
                                static_cast<ScSplitPos>(nPane);
        ScVSplitPos eWhichV = WhichV( eWhich );

        tools::Long nDeltaY = static_cast<tools::Long>(nFirstVisibleRow) - rViewData.GetPosY( eWhichV );
        pViewShell->ScrollY( nDeltaY, eWhichV );
    }
}

table::CellRangeAddress SAL_CALL ScViewPaneBase::getVisibleRange()
{
    SolarMutexGuard aGuard;
    table::CellRangeAddress aAdr;
    if (pViewShell)
    {
        ScViewData& rViewData = pViewShell->GetViewData();
        ScSplitPos eWhich = ( nPane == SC_VIEWPANE_ACTIVE ) ?
                                rViewData.GetActivePart() :
                                static_cast<ScSplitPos>(nPane);
        ScHSplitPos eWhichH = WhichH( eWhich );
        ScVSplitPos eWhichV = WhichV( eWhich );

        //  VisibleCellsX returns only completely visible cells
        //  VisibleRange in Excel also partially visible ones
        //! do the same ???

        SCCOL nVisX = rViewData.VisibleCellsX( eWhichH );
        SCROW nVisY = rViewData.VisibleCellsY( eWhichV );
        if (!nVisX) nVisX = 1;  // there has to be something in the range
        if (!nVisY) nVisY = 1;
        aAdr.Sheet       = rViewData.GetTabNo();
        aAdr.StartColumn = rViewData.GetPosX( eWhichH );
        aAdr.StartRow    = rViewData.GetPosY( eWhichV );
        aAdr.EndColumn   = aAdr.StartColumn + nVisX - 1;
        aAdr.EndRow      = aAdr.StartRow    + nVisY - 1;
    }
    return aAdr;
}

// XCellRangeSource

uno::Reference<table::XCellRange> SAL_CALL ScViewPaneBase::getReferredCells()
{
    SolarMutexGuard aGuard;
    if (pViewShell)
    {
        ScDocShell& rDocSh = pViewShell->GetViewData().GetDocShell();

        table::CellRangeAddress aAdr(getVisibleRange());        //! helper function with ScRange?
        ScRange aRange( static_cast<SCCOL>(aAdr.StartColumn), static_cast<SCROW>(aAdr.StartRow), aAdr.Sheet,
                        static_cast<SCCOL>(aAdr.EndColumn), static_cast<SCROW>(aAdr.EndRow), aAdr.Sheet );
        if ( aRange.aStart == aRange.aEnd )
            return new ScCellObj( &rDocSh, aRange.aStart );
        else
            return new ScCellRangeObj( &rDocSh, aRange );
    }

    return nullptr;
}

namespace
{
    bool lcl_prepareFormShellCall( ScTabViewShell* _pViewShell, sal_uInt16 _nPane, FmFormShell*& _rpFormShell, vcl::Window*& _rpWindow, SdrView*& _rpSdrView )
    {
        if ( !_pViewShell )
            return false;

        ScViewData& rViewData = _pViewShell->GetViewData();
        ScSplitPos eWhich = ( _nPane == SC_VIEWPANE_ACTIVE ) ?
                                rViewData.GetActivePart() :
                                static_cast<ScSplitPos>(_nPane);
        _rpWindow = _pViewShell->GetWindowByPos( eWhich );
        _rpSdrView = _pViewShell->GetScDrawView();
        _rpFormShell = _pViewShell->GetFormShell();
        return ( _rpFormShell != nullptr ) && ( _rpSdrView != nullptr )&& ( _rpWindow != nullptr );
    }
}

// XFormLayerAccess
uno::Reference< form::runtime::XFormController > SAL_CALL ScViewPaneBase::getFormController( const uno::Reference< form::XForm >& Form )
{
    SolarMutexGuard aGuard;

    uno::Reference< form::runtime::XFormController > xController;

    vcl::Window* pWindow( nullptr );
    SdrView* pSdrView( nullptr );
    FmFormShell* pFormShell( nullptr );
    if ( lcl_prepareFormShellCall( pViewShell, nPane, pFormShell, pWindow, pSdrView ) )
        xController = FmFormShell::GetFormController( Form, *pSdrView, *pWindow->GetOutDev() );

    return xController;
}

sal_Bool SAL_CALL ScViewPaneBase::isFormDesignMode(  )
{
    SolarMutexGuard aGuard;

    bool bIsFormDesignMode( true );

    FmFormShell* pFormShell( pViewShell ? pViewShell->GetFormShell() : nullptr );
    if ( pFormShell )
        bIsFormDesignMode = pFormShell->IsDesignMode();

    return bIsFormDesignMode;
}

void SAL_CALL ScViewPaneBase::setFormDesignMode( sal_Bool DesignMode )
{
    SolarMutexGuard aGuard;

    vcl::Window* pWindow( nullptr );
    SdrView* pSdrView( nullptr );
    FmFormShell* pFormShell( nullptr );
    if ( lcl_prepareFormShellCall( pViewShell, nPane, pFormShell, pWindow, pSdrView ) )
        pFormShell->SetDesignMode( DesignMode );
}

// XControlAccess

uno::Reference<awt::XControl> SAL_CALL ScViewPaneBase::getControl(
                            const uno::Reference<awt::XControlModel>& xModel )
{
    SolarMutexGuard aGuard;

    uno::Reference<awt::XControl> xRet;

    vcl::Window* pWindow( nullptr );
    SdrView* pSdrView( nullptr );
    FmFormShell* pFormShell( nullptr );
    if ( lcl_prepareFormShellCall( pViewShell, nPane, pFormShell, pWindow, pSdrView ) )
        pFormShell->GetFormControl( xModel, *pSdrView, *pWindow->GetOutDev(), xRet );

    if ( !xRet.is() )
        throw container::NoSuchElementException();      // no control found

    return xRet;
}

awt::Rectangle ScViewPaneBase::GetVisArea() const
{
    awt::Rectangle aVisArea;
    if (pViewShell)
    {
        ScSplitPos eWhich = ( nPane == SC_VIEWPANE_ACTIVE ) ?
                                pViewShell->GetViewData().GetActivePart() :
                                static_cast<ScSplitPos>(nPane);
        ScGridWindow* pWindow = static_cast<ScGridWindow*>(pViewShell->GetWindowByPos(eWhich));
        ScDocument& rDoc = pViewShell->GetViewData().GetDocument();
        if (pWindow)
        {
            ScHSplitPos eWhichH = ((eWhich == SC_SPLIT_TOPLEFT) || (eWhich == SC_SPLIT_BOTTOMLEFT)) ?
                                    SC_SPLIT_LEFT : SC_SPLIT_RIGHT;
            ScVSplitPos eWhichV = ((eWhich == SC_SPLIT_TOPLEFT) || (eWhich == SC_SPLIT_TOPRIGHT)) ?
                                    SC_SPLIT_TOP : SC_SPLIT_BOTTOM;
            ScAddress aCell(pViewShell->GetViewData().GetPosX(eWhichH),
                pViewShell->GetViewData().GetPosY(eWhichV),
                pViewShell->GetViewData().GetTabNo());
            tools::Rectangle aCellRect( rDoc.GetMMRect( aCell.Col(), aCell.Row(), aCell.Col(), aCell.Row(), aCell.Tab() ) );
            Size aVisSize( pWindow->PixelToLogic( pWindow->GetSizePixel(), pWindow->GetDrawMapMode( true ) ) );
            Point aVisPos( aCellRect.TopLeft() );
            if ( rDoc.IsLayoutRTL( aCell.Tab() ) )
            {
                aVisPos = aCellRect.TopRight();
                aVisPos.AdjustX( -(aVisSize.Width()) );
            }
            tools::Rectangle aVisRect( aVisPos, aVisSize );
            aVisArea = vcl::unohelper::ConvertToAWTRect(aVisRect);
        }
    }
    return aVisArea;
}

ScViewPaneObj::ScViewPaneObj(ScTabViewShell* pViewSh, sal_uInt16 nP) :
    ScViewPaneBase( pViewSh, nP )
{
}

ScViewPaneObj::~ScViewPaneObj()
{
}

uno::Any SAL_CALL ScViewPaneObj::queryInterface( const uno::Type& rType )
{
    //  ScViewPaneBase has everything except OWeakObject

    uno::Any aRet(ScViewPaneBase::queryInterface( rType ));
    if (!aRet.hasValue())
        aRet = OWeakObject::queryInterface( rType );
    return aRet;
}

void SAL_CALL ScViewPaneObj::acquire() noexcept
{
    OWeakObject::acquire();
}

void SAL_CALL ScViewPaneObj::release() noexcept
{
    OWeakObject::release();
}

//  We need default ctor for SMART_REFLECTION_IMPLEMENTATION

ScTabViewObj::ScTabViewObj( ScTabViewShell* pViewSh ) :
    ScViewPaneBase( pViewSh, SC_VIEWPANE_ACTIVE ),
    SfxBaseController( pViewSh ),
    aPropSet( lcl_GetViewOptPropertyMap() ),
    aMouseClickHandlers( 0 ),
    aActivationListeners( 0 ),
    nPreviousTab( 0 ),
    bDrawSelModeSet(false),
    bFilteredRangeSelection(false),
    mbLeftMousePressed(false)
{
    if (pViewSh)
        nPreviousTab = pViewSh->GetViewData().GetTabNo();
}

ScTabViewObj::~ScTabViewObj()
{
    //! Listening or something along that line
    if (!aMouseClickHandlers.empty())
    {
        acquire();
        EndMouseListening();
    }
    if (!aActivationListeners.empty())
    {
        acquire();
        EndActivationListening();
    }
}

uno::Any SAL_CALL ScTabViewObj::queryInterface( const uno::Type& rType )
{
    uno::Any aReturn = ::cppu::queryInterface(rType,
                    static_cast<sheet::XSpreadsheetView*>(this),
                    static_cast<sheet::XEnhancedMouseClickBroadcaster*>(this),
                    static_cast<sheet::XActivationBroadcaster*>(this),
                    static_cast<container::XEnumerationAccess*>(this),
                    static_cast<container::XIndexAccess*>(this),
                    static_cast<container::XElementAccess*>(static_cast<container::XIndexAccess*>(this)),
                    static_cast<view::XSelectionSupplier*>(this),
                    static_cast<beans::XPropertySet*>(this),
                    static_cast<sheet::XViewSplitable*>(this),
                    static_cast<sheet::XViewFreezable*>(this),
                    static_cast<sheet::XRangeSelection*>(this),
                    static_cast<sheet::XSheetRange*>(this),
                    static_cast<sheet::XSelectedSheetsSupplier*>(this),
                    static_cast<datatransfer::XTransferableSupplier*>(this));
    if ( aReturn.hasValue() )
        return aReturn;

    uno::Any aRet(ScViewPaneBase::queryInterface( rType ));
    if (!aRet.hasValue())
        aRet = SfxBaseController::queryInterface( rType );
    return aRet;
}

void SAL_CALL ScTabViewObj::acquire() noexcept
{
    SfxBaseController::acquire();
}

void SAL_CALL ScTabViewObj::release() noexcept
{
    SfxBaseController::release();
}

static void lcl_CallActivate( ScDocShell& rDocSh, SCTAB nTab, ScSheetEventId nEvent )
{
    ScDocument& rDoc = rDocSh.GetDocument();
    // when deleting a sheet, nPreviousTab can be invalid
    // (could be handled with reference updates)
    if (!rDoc.HasTable(nTab))
        return;

    const ScSheetEvents* pEvents = rDoc.GetSheetEvents(nTab);
    if (pEvents)
    {
        const OUString* pScript = pEvents->GetScript(nEvent);
        if (pScript)
        {
            uno::Any aRet;
            uno::Sequence<uno::Any> aParams;
            uno::Sequence<sal_Int16> aOutArgsIndex;
            uno::Sequence<uno::Any> aOutArgs;
            /*ErrCode eRet =*/ rDocSh.CallXScript( *pScript, aParams, aRet, aOutArgsIndex, aOutArgs );
        }
    }

    // execute VBA event handlers
    try
    {
        uno::Reference< script::vba::XVBAEventProcessor > xVbaEvents( rDoc.GetVbaEventProcessor(), uno::UNO_SET_THROW );
        // the parameter is the clicked object, as in the mousePressed call above
        uno::Sequence< uno::Any > aArgs{ uno::Any(nTab) };
        xVbaEvents->processVbaEvent( ScSheetEvents::GetVbaSheetEventId( nEvent ), aArgs );
    }
    catch( uno::Exception& )
    {
    }
}

void ScTabViewObj::SheetChanged( bool bSameTabButMoved )
{
    if ( !GetViewShell() )
        return;

    ScViewData& rViewData = GetViewShell()->GetViewData();
    ScDocShell& rDocSh = rViewData.GetDocShell();
    if (!aActivationListeners.empty())
    {
        sheet::ActivationEvent aEvent;
        uno::Reference< sheet::XSpreadsheetView > xView(this);
        aEvent.Source.set(xView, uno::UNO_QUERY);
        aEvent.ActiveSheet = new ScTableSheetObj(&rDocSh, rViewData.GetTabNo());
        // Listener's handler may remove it from the listeners list
        for (size_t i = aActivationListeners.size(); i > 0; --i)
        {
            try
            {
                aActivationListeners[i - 1]->activeSpreadsheetChanged( aEvent );
            }
            catch( uno::Exception& )
            {
                aActivationListeners.erase(aActivationListeners.begin() + (i - 1));
            }
        }
    }

    /*  Handle sheet events, but do not trigger event handlers, if the old
        active sheet gets re-activated after inserting/deleting/moving a sheet. */
    SCTAB nNewTab = rViewData.GetTabNo();
    if ( !bSameTabButMoved && (nNewTab != nPreviousTab) )
    {
        lcl_CallActivate( rDocSh, nPreviousTab, ScSheetEventId::UNFOCUS );
        lcl_CallActivate( rDocSh, nNewTab, ScSheetEventId::FOCUS );
    }
    nPreviousTab = nNewTab;
}

uno::Sequence<uno::Type> SAL_CALL ScTabViewObj::getTypes()
{
    return comphelper::concatSequences(
        ScViewPaneBase::getTypes(),
        SfxBaseController::getTypes(),
        uno::Sequence<uno::Type>
        {
            cppu::UnoType<sheet::XSpreadsheetView>::get(),
            cppu::UnoType<container::XEnumerationAccess>::get(),
            cppu::UnoType<container::XIndexAccess>::get(),
            cppu::UnoType<view::XSelectionSupplier>::get(),
            cppu::UnoType<beans::XPropertySet>::get(),
            cppu::UnoType<sheet::XViewSplitable>::get(),
            cppu::UnoType<sheet::XViewFreezable>::get(),
            cppu::UnoType<sheet::XRangeSelection>::get(),
            cppu::UnoType<sheet::XSheetRange>::get(),
            cppu::UnoType<lang::XUnoTunnel>::get(),
            cppu::UnoType<sheet::XEnhancedMouseClickBroadcaster>::get(),
            cppu::UnoType<sheet::XActivationBroadcaster>::get(),
            cppu::UnoType<datatransfer::XTransferableSupplier>::get()
        } );
}

uno::Sequence<sal_Int8> SAL_CALL ScTabViewObj::getImplementationId()
{
    return css::uno::Sequence<sal_Int8>();
}

// XDocumentView

static bool lcl_TabInRanges( SCTAB nTab, const ScRangeList& rRanges )
{
    for (size_t i = 0, nCount = rRanges.size(); i < nCount; ++i)
    {
        const ScRange & rRange = rRanges[ i ];
        if ( nTab >= rRange.aStart.Tab() && nTab <= rRange.aEnd.Tab() )
            return true;
    }
    return false;
}

static void lcl_ShowObject( ScTabViewShell& rViewSh, const ScDrawView& rDrawView, const SdrObject* pSelObj )
{
    bool bFound = false;
    SCTAB nObjectTab = 0;

    SdrModel& rModel = rDrawView.GetModel();
    sal_uInt16 nPageCount = rModel.GetPageCount();
    for (sal_uInt16 i=0; i<nPageCount && !bFound; i++)
    {
        SdrPage* pPage = rModel.GetPage(i);
        if (pPage)
        {
            SdrObjListIter aIter( pPage, SdrIterMode::DeepWithGroups );
            SdrObject* pObject = aIter.Next();
            while (pObject && !bFound)
            {
                if ( pObject == pSelObj )
                {
                    bFound = true;
                    nObjectTab = static_cast<SCTAB>(i);
                }
                pObject = aIter.Next();
            }
        }
    }

    if (bFound)
    {
        rViewSh.SetTabNo( nObjectTab );
        rViewSh.ScrollToObject( pSelObj );
    }
}

sal_Bool SAL_CALL ScTabViewObj::select( const uno::Any& aSelection )
{
    SolarMutexGuard aGuard;
    ScTabViewShell* pViewSh = GetViewShell();

    if ( !pViewSh )
        return false;

    //! Type of aSelection can be some specific interface instead of XInterface

    bool bRet = false;
    uno::Reference<uno::XInterface> xInterface(aSelection, uno::UNO_QUERY);
    if ( !xInterface.is() )  //clear all selections
    {
        ScDrawView* pDrawView = pViewSh->GetScDrawView();
        if (pDrawView)
        {
            pDrawView->ScEndTextEdit();
            pDrawView->UnmarkAll();
        }
        else //#102232#; if there is  no DrawView remove range selection
            pViewSh->Unmark();
        bRet = true;
    }

    if (bDrawSelModeSet) // remove DrawSelMode if set by API; if necessary it will be set again later
    {
        pViewSh->SetDrawSelMode(false);
        pViewSh->UpdateLayerLocks();
        bDrawSelModeSet = false;
    }

    if (bRet)
        return bRet;

    ScCellRangesBase* pRangesImp = dynamic_cast<ScCellRangesBase*>( xInterface.get() );
    uno::Reference<drawing::XShapes> xShapeColl( xInterface, uno::UNO_QUERY );
    uno::Reference<drawing::XShape> xShapeSel( xInterface, uno::UNO_QUERY );
    SvxShape* pShapeImp = comphelper::getFromUnoTunnel<SvxShape>( xShapeSel );

    if (pRangesImp)                                     // Cell ranges
    {
        ScViewData& rViewData = pViewSh->GetViewData();
        if ( &rViewData.GetDocShell() == pRangesImp->GetDocShell() )
        {
            //  perhaps remove drawing selection first
            //  (MarkListHasChanged removes sheet selection)

            ScDrawView* pDrawView = pViewSh->GetScDrawView();
            if (pDrawView)
            {
                pDrawView->ScEndTextEdit();
                pDrawView->UnmarkAll();
            }
            FuPoor* pFunc = pViewSh->GetDrawFuncPtr();
            if ( pFunc && pFunc->GetSlotID() != SID_OBJECT_SELECT )
            {
                //  execute the slot of drawing function again -> switch off
                SfxDispatcher* pDisp = pViewSh->GetDispatcher();
                if (pDisp)
                    pDisp->Execute( pFunc->GetSlotID(), SfxCallMode::SYNCHRON );
            }
            pViewSh->SetDrawShell(false);
            pViewSh->SetDrawSelMode(false); // after Dispatcher-Execute

            //  select ranges

            const ScRangeList& rRanges = pRangesImp->GetRangeList();
            size_t nRangeCount = rRanges.size();
            // for empty range list, remove selection (cursor remains where it was)
            if ( nRangeCount == 0 )
                pViewSh->Unmark();
            else if ( nRangeCount == 1 )
                pViewSh->MarkRange( rRanges[ 0 ] );
            else
            {
                // multiselection

                const ScRange & rFirst = rRanges[ 0 ];
                if ( !lcl_TabInRanges( rViewData.GetTabNo(), rRanges ) )
                    pViewSh->SetTabNo( rFirst.aStart.Tab() );
                pViewSh->DoneBlockMode();
                pViewSh->InitOwnBlockMode( rFirst );    /* TODO: or even the overall range? */
                rViewData.GetMarkData().MarkFromRangeList( rRanges, true );
                pViewSh->MarkDataChanged();
                rViewData.GetDocShell().PostPaintGridAll();   // Marks (old&new)
                pViewSh->AlignToCursor( rFirst.aStart.Col(), rFirst.aStart.Row(),
                                            SC_FOLLOW_JUMP );
                pViewSh->SetCursor( rFirst.aStart.Col(), rFirst.aStart.Row() );

                //! method of the view to select RangeList
            }
            bRet = true;
        }
    }
    else if ( pShapeImp || xShapeColl.is() )            // Drawing-Layer
    {
        ScDrawView* pDrawView = pViewSh->GetScDrawView();
        if (pDrawView)
        {
            pDrawView->ScEndTextEdit();
            pDrawView->UnmarkAll();

            if (pShapeImp)      // single shape
            {
                SdrObject *pObj = pShapeImp->GetSdrObject();
                if (pObj)
                {
                    lcl_ShowObject( *pViewSh, *pDrawView, pObj );
                    SdrPageView* pPV = pDrawView->GetSdrPageView();
                    if ( pPV && pObj->getSdrPageFromSdrObject() == pPV->GetPage() )
                    {
                        pDrawView->MarkObj( pObj, pPV );
                        bRet = true;
                    }
                }
            }
            else                // Shape-Collection (xShapeColl is not 0)
            {
                //  We'll switch to the sheet where the first object is
                //  and select all objects on that sheet
                //!?throw exception when objects are on different sheets?

                tools::Long nCount = xShapeColl->getCount();
                if (nCount)
                {
                    SdrPageView* pPV = nullptr;
                    bool bAllMarked(true);
                    for ( tools::Long i = 0; i < nCount; i++ )
                    {
                        uno::Reference<drawing::XShape> xShapeInt(xShapeColl->getByIndex(i), uno::UNO_QUERY);
                        if (xShapeInt.is())
                        {
                            SdrObject* pObj = SdrObject::getSdrObjectFromXShape( xShapeInt );
                            if (pObj)
                            {
                                if (!bDrawSelModeSet && (pObj->GetLayer() == SC_LAYER_BACK))
                                {
                                    pViewSh->SetDrawSelMode(true);
                                    pViewSh->UpdateLayerLocks();
                                    bDrawSelModeSet = true;
                                }
                                if (!pPV)               // first object
                                {
                                    lcl_ShowObject( *pViewSh, *pDrawView, pObj );
                                    pPV = pDrawView->GetSdrPageView();
                                }
                                if ( pPV && pObj->getSdrPageFromSdrObject() == pPV->GetPage() )
                                {
                                    if (pDrawView->IsObjMarkable( pObj, pPV ))
                                        pDrawView->MarkObj( pObj, pPV );
                                    else
                                        bAllMarked = false;
                                }
                            }
                        }
                    }
                    if (bAllMarked)
                        bRet = true;
                }
                else
                    bRet = true; // empty XShapes (all shapes are deselected)
            }

            if (bRet)
                pViewSh->SetDrawShell(true);
        }
    }

    if (!bRet)
        throw lang::IllegalArgumentException();

    return bRet;
}

uno::Reference<drawing::XShapes> ScTabViewShell::getSelectedXShapes()
{
    uno::Reference<drawing::XShapes> xShapes;
    SdrView* pSdrView = GetScDrawView();
    if (pSdrView)
    {
        const SdrMarkList& rMarkList = pSdrView->GetMarkedObjectList();
        const size_t nMarkCount = rMarkList.GetMarkCount();
        if (nMarkCount)
        {
            //  generate ShapeCollection (like in SdXImpressView::getSelection in Draw)
            //  XInterfaceRef will be returned and it has to be UsrObject-XInterface
            xShapes = drawing::ShapeCollection::create(comphelper::getProcessComponentContext());

            for (size_t i = 0; i < nMarkCount; ++i)
            {
                SdrObject* pDrawObj = rMarkList.GetMark(i)->GetMarkedSdrObj();
                if (pDrawObj)
                {
                    uno::Reference<drawing::XShape> xShape( pDrawObj->getUnoShape(), uno::UNO_QUERY );
                    if (xShape.is())
                        xShapes->add(xShape);
                }
            }
        }
    }
    return xShapes;
}

uno::Any SAL_CALL ScTabViewObj::getSelection()
{
    SolarMutexGuard aGuard;
    ScTabViewShell* pViewSh = GetViewShell();
    rtl::Reference<ScCellRangesBase> pObj;
    if (pViewSh)
    {
        //  is something selected in drawing layer?
        uno::Reference<uno::XInterface> xRet(pViewSh->getSelectedXShapes());
        if (xRet.is())
            return uno::Any(xRet);

        //  otherwise sheet (cell) selection

        ScViewData& rViewData = pViewSh->GetViewData();
        ScDocShell& rDocSh = rViewData.GetDocShell();

        const ScMarkData& rMark = rViewData.GetMarkData();
        SCTAB nTabs = rMark.GetSelectCount();

        ScRange aRange;
        ScMarkType eMarkType = rViewData.GetSimpleArea(aRange);
        if ( nTabs == 1 && (eMarkType == SC_MARK_SIMPLE) )
        {
            // tdf#154803 - check if range is entirely merged
            ScDocument& rDoc = rDocSh.GetDocument();
            const ScMergeAttr* pMergeAttr = rDoc.GetAttr(aRange.aStart, ATTR_MERGE);
            SCCOL nColSpan = 1;
            SCROW nRowSpan = 1;
            if (pMergeAttr && pMergeAttr->IsMerged())
            {
                nColSpan = pMergeAttr->GetColMerge();
                nRowSpan = pMergeAttr->GetRowMerge();
            }
            // tdf#147122 - return cell object when a simple selection is entirely merged
            if (aRange.aStart == aRange.aEnd
                || (aRange.aEnd.Col() - aRange.aStart.Col() == nColSpan - 1
                    && aRange.aEnd.Row() - aRange.aStart.Row() == nRowSpan - 1))
                pObj = new ScCellObj( &rDocSh, aRange.aStart );
            else
                pObj = new ScCellRangeObj( &rDocSh, aRange );
        }
        else if ( nTabs == 1 && (eMarkType == SC_MARK_SIMPLE_FILTERED) )
        {
            ScMarkData aFilteredMark( rMark );
            ScViewUtil::UnmarkFiltered( aFilteredMark, rDocSh.GetDocument());
            ScRangeList aRangeList;
            aFilteredMark.FillRangeListWithMarks( &aRangeList, false);
            // Theoretically a selection may start and end on a filtered row.
            switch ( aRangeList.size() )
            {
                case 0:
                    // No unfiltered row, we have to return some object, so
                    // here is one with no ranges.
                    pObj = new ScCellRangesObj( &rDocSh, aRangeList );
                    break;
                case 1:
                    {
                        const ScRange& rRange = aRangeList[ 0 ];
                        if (rRange.aStart == rRange.aEnd)
                            pObj = new ScCellObj( &rDocSh, rRange.aStart );
                        else
                            pObj = new ScCellRangeObj( &rDocSh, rRange );
                    }
                    break;
                default:
                    pObj = new ScCellRangesObj( &rDocSh, aRangeList );
            }
        }
        else            //  multiselection
        {
            ScRangeListRef xRanges;
            rViewData.GetMultiArea( xRanges );

            //  if there are more sheets, copy ranges
            //! should this happen in ScMarkData::FillRangeListWithMarks already?
            if ( nTabs > 1 )
                rMark.ExtendRangeListTables( xRanges.get() );

            pObj = new ScCellRangesObj( &rDocSh, *xRanges );
        }

        if ( !rMark.IsMarked() && !rMark.IsMultiMarked() )
        {
            //  remember if the selection was from the cursor position without anything selected
            //  (used when rendering the selection)

            pObj->SetCursorOnly( true );
        }
    }

    return uno::Any(uno::Reference(cppu::getXWeak(pObj.get())));
}

uno::Any SAL_CALL ScTabViewObj::getSelectionFromString( const OUString& aStrRange )
{
    ScDocShell& rDocSh = GetViewShell()->GetViewData().GetDocShell();
    const sal_Int16 nTabCount = rDocSh.GetDocument().GetTableCount();

    StringRangeEnumerator aRangeEnum(aStrRange , 0, nTabCount-1);

    // iterate through sheet range

    StringRangeEnumerator::Iterator aIter = aRangeEnum.begin();
    StringRangeEnumerator::Iterator aEnd  = aRangeEnum.end();

    ScRangeListRef aRangeList = new ScRangeList;

    while ( aIter != aEnd )
    {
        ScRange currentTab(SCCOL(0), SCROW(0), SCTAB(*aIter));
        aRangeList->push_back(currentTab);
        ++aIter;
    }

    rtl::Reference<ScCellRangesBase> pObj = new ScCellRangesObj(&rDocSh, *aRangeList);

    // SetCursorOnly tells the range the specific cells selected are irrelevant - maybe could rename?
    pObj->SetCursorOnly(true);

    return uno::Any(uno::Reference<uno::XInterface>(static_cast<cppu::OWeakObject*>(pObj.get())));
}

// XEnumerationAccess

uno::Reference<container::XEnumeration> SAL_CALL ScTabViewObj::createEnumeration()
{
    SolarMutexGuard aGuard;
    return new ScIndexEnumeration(this, u"com.sun.star.sheet.SpreadsheetViewPanesEnumeration"_ustr);
}

// XIndexAccess

sal_Int32 SAL_CALL ScTabViewObj::getCount()
{
    SolarMutexGuard aGuard;
    ScTabViewShell* pViewSh = GetViewShell();
    sal_uInt16 nPanes = 0;
    if (pViewSh)
    {
        nPanes = 1;
        ScViewData& rViewData = pViewSh->GetViewData();
        if ( rViewData.GetHSplitMode() != SC_SPLIT_NONE )
            nPanes *= 2;
        if ( rViewData.GetVSplitMode() != SC_SPLIT_NONE )
            nPanes *= 2;
    }
    return nPanes;
}

uno::Any SAL_CALL ScTabViewObj::getByIndex( sal_Int32 nIndex )
{
    SolarMutexGuard aGuard;
    rtl::Reference<ScViewPaneObj> xPane(GetObjectByIndex_Impl(static_cast<sal_uInt16>(nIndex)));
    if (!xPane.is())
        throw lang::IndexOutOfBoundsException();

    return uno::Any(uno::Reference<sheet::XViewPane>(xPane));
}

uno::Type SAL_CALL ScTabViewObj::getElementType()
{
    return cppu::UnoType<sheet::XViewPane>::get();
}

sal_Bool SAL_CALL ScTabViewObj::hasElements()
{
    SolarMutexGuard aGuard;
    return ( getCount() != 0 );
}

// XSpreadsheetView

rtl::Reference<ScViewPaneObj> ScTabViewObj::GetObjectByIndex_Impl(sal_uInt16 nIndex) const
{
    static const ScSplitPos ePosHV[4] =
        { SC_SPLIT_TOPLEFT, SC_SPLIT_BOTTOMLEFT, SC_SPLIT_TOPRIGHT, SC_SPLIT_BOTTOMRIGHT };

    ScTabViewShell* pViewSh = GetViewShell();
    if (pViewSh)
    {
        ScSplitPos eWhich = SC_SPLIT_BOTTOMLEFT;    // default position
        bool bError = false;
        ScViewData& rViewData = pViewSh->GetViewData();
        bool bHor = ( rViewData.GetHSplitMode() != SC_SPLIT_NONE );
        bool bVer = ( rViewData.GetVSplitMode() != SC_SPLIT_NONE );
        if ( bHor && bVer )
        {
            //  bottom left, bottom right, top left, top right - like in Excel
            if ( nIndex < 4 )
                eWhich = ePosHV[nIndex];
            else
                bError = true;
        }
        else if ( bHor )
        {
            if ( nIndex > 1 )
                bError = true;
            else if ( nIndex == 1 )
                eWhich = SC_SPLIT_BOTTOMRIGHT;
            // otherwise SC_SPLIT_BOTTOMLEFT
        }
        else if ( bVer )
        {
            if ( nIndex > 1 )
                bError = true;
            else if ( nIndex == 0 )
                eWhich = SC_SPLIT_TOPLEFT;
            // otherwise SC_SPLIT_BOTTOMLEFT
        }
        else if ( nIndex > 0 )
            bError = true;          // not split: only 0 is valid

        if (!bError)
            return new ScViewPaneObj( pViewSh, sal::static_int_cast<sal_uInt16>(eWhich) );
    }

    return nullptr;
}

uno::Reference<sheet::XSpreadsheet> SAL_CALL ScTabViewObj::getActiveSheet()
{
    SolarMutexGuard aGuard;
    ScTabViewShell* pViewSh = GetViewShell();
    if (pViewSh)
    {
        ScViewData& rViewData = pViewSh->GetViewData();
        SCTAB nTab = rViewData.GetTabNo();
        return new ScTableSheetObj( &rViewData.GetDocShell(), nTab );
    }
    return nullptr;
}

// support expand (but not replace) the active sheet
void SAL_CALL ScTabViewObj::setActiveSheet( const uno::Reference<sheet::XSpreadsheet>& xActiveSheet )
{
    SolarMutexGuard aGuard;
    comphelper::ProfileZone aZone("setActiveSheet");

    ScTabViewShell* pViewSh = GetViewShell();
    if ( !(pViewSh && xActiveSheet.is()) )
        return;

    //  XSpreadsheet and ScCellRangesBase -> has to be the same sheet

    ScCellRangesBase* pRangesImp = dynamic_cast<ScCellRangesBase*>( xActiveSheet.get() );
    if ( pRangesImp && &pViewSh->GetViewData().GetDocShell() == pRangesImp->GetDocShell() )
    {
        const ScRangeList& rRanges = pRangesImp->GetRangeList();
        if ( rRanges.size() == 1 )
        {
            SCTAB nNewTab = rRanges[ 0 ].aStart.Tab();
            if ( pViewSh->GetViewData().GetDocument().HasTable(nNewTab) )
                pViewSh->SetTabNo( nNewTab );
        }
    }
}

uno::Reference< uno::XInterface > ScTabViewObj::GetClickedObject(const Point& rPoint) const
{
    uno::Reference< uno::XInterface > xTarget;
    if (GetViewShell())
    {
        SCCOL nX;
        SCROW nY;
        ScViewData& rData = GetViewShell()->GetViewData();
        ScSplitPos eSplitMode = rData.GetActivePart();
        SCTAB nTab(rData.GetTabNo());
        rData.GetPosFromPixel( rPoint.X(), rPoint.Y(), eSplitMode, nX, nY);

        ScAddress aCellPos (nX, nY, nTab);
        rtl::Reference<ScCellObj> pCellObj = new ScCellObj(&rData.GetDocShell(), aCellPos);

        xTarget.set(uno::Reference<table::XCell>(pCellObj), uno::UNO_QUERY);

        ScDocument& rDoc = rData.GetDocument();
        if (ScDrawLayer* pDrawLayer = rDoc.GetDrawLayer())
        {
            SdrPage* pDrawPage = nullptr;
            if (pDrawLayer->HasObjects() && (pDrawLayer->GetPageCount() > nTab))
                pDrawPage = pDrawLayer->GetPage(static_cast<sal_uInt16>(nTab));

            SdrView* pDrawView = GetViewShell()->GetScDrawView();

            if (pDrawPage && pDrawView && pDrawView->GetSdrPageView())
            {
                vcl::Window* pActiveWin = rData.GetActiveWin();
                Point aPos = pActiveWin->PixelToLogic(rPoint);

                double fHitLog = pActiveWin->PixelToLogic(Size(pDrawView->GetHitTolerancePixel(),0)).Width();

                for (const rtl::Reference<SdrObject>& pObj : *pDrawPage)
                {
                    if (SdrObjectPrimitiveHit(*pObj, aPos, {fHitLog, fHitLog}, *pDrawView->GetSdrPageView(), nullptr, false))
                    {
                        xTarget.set(pObj->getUnoShape(), uno::UNO_QUERY);
                        break;
                    }
                }
            }
        }
    }
    return xTarget;
}

bool ScTabViewObj::IsMouseListening() const
{
    if ( !aMouseClickHandlers.empty() )
        return true;

    // also include sheet events, because MousePressed must be called for them
    ScViewData& rViewData = GetViewShell()->GetViewData();
    ScDocument& rDoc = rViewData.GetDocument();
    SCTAB nTab = rViewData.GetTabNo();
    return
        rDoc.HasSheetEventScript( nTab, ScSheetEventId::RIGHTCLICK, true ) ||
        rDoc.HasSheetEventScript( nTab, ScSheetEventId::DOUBLECLICK, true ) ||
        rDoc.HasSheetEventScript( nTab, ScSheetEventId::SELECT, true );

}

bool ScTabViewObj::MousePressed( const awt::MouseEvent& e )
{
    bool bReturn(false);
    if ( e.Buttons == css::awt::MouseButton::LEFT )
        mbLeftMousePressed = true;

    uno::Reference< uno::XInterface > xTarget = GetClickedObject(Point(e.X, e.Y));
    if (!aMouseClickHandlers.empty() && xTarget.is())
    {
        awt::EnhancedMouseEvent aMouseEvent;

        aMouseEvent.Buttons = e.Buttons;
        aMouseEvent.X = e.X;
        aMouseEvent.Y = e.Y;
        aMouseEvent.ClickCount = e.ClickCount;
        aMouseEvent.PopupTrigger = e.PopupTrigger;
        aMouseEvent.Target = xTarget;
        aMouseEvent.Modifiers = e.Modifiers;

        // Listener's handler may remove it from the listeners list
        for (size_t i = aMouseClickHandlers.size(); i > 0; --i)
        {
            try
            {
                if (!aMouseClickHandlers[i - 1]->mousePressed(aMouseEvent))
                    bReturn = true;
            }
            catch ( uno::Exception& )
            {
                aMouseClickHandlers.erase(aMouseClickHandlers.begin() + (i - 1));
            }
        }
    }

    // handle sheet events
    bool bDoubleClick = ( e.Buttons == awt::MouseButton::LEFT && e.ClickCount == 2 );
    bool bRightClick = ( e.Buttons == awt::MouseButton::RIGHT && e.ClickCount == 1 );
    if ( ( bDoubleClick || bRightClick ) && !bReturn && xTarget.is())
    {
        ScSheetEventId nEvent = bDoubleClick ? ScSheetEventId::DOUBLECLICK : ScSheetEventId::RIGHTCLICK;

        ScTabViewShell* pViewSh = GetViewShell();
        ScViewData& rViewData = pViewSh->GetViewData();
        ScDocShell& rDocSh = rViewData.GetDocShell();
        ScDocument& rDoc = rDocSh.GetDocument();
        SCTAB nTab = rViewData.GetTabNo();
        const ScSheetEvents* pEvents = rDoc.GetSheetEvents(nTab);
        if (pEvents)
        {
            const OUString* pScript = pEvents->GetScript(nEvent);
            if (pScript)
            {
                // the macro parameter is the clicked object, as in the mousePressed call above
                uno::Sequence<uno::Any> aParams{ uno::Any(xTarget) };

                uno::Any aRet;
                uno::Sequence<sal_Int16> aOutArgsIndex;
                uno::Sequence<uno::Any> aOutArgs;

                /*ErrCode eRet =*/ rDocSh.CallXScript( *pScript, aParams, aRet, aOutArgsIndex, aOutArgs );

                // look for a boolean return value of true
                bool bRetValue = false;
                if ((aRet >>= bRetValue) && bRetValue)
                    bReturn = true;
            }
        }

        // execute VBA event handler
        if (!bReturn && xTarget.is()) try
        {
            uno::Reference< script::vba::XVBAEventProcessor > xVbaEvents( rDoc.GetVbaEventProcessor(), uno::UNO_SET_THROW );
            // the parameter is the clicked object, as in the mousePressed call above
            uno::Sequence< uno::Any > aArgs{ uno::Any(xTarget) };
            xVbaEvents->processVbaEvent( ScSheetEvents::GetVbaSheetEventId( nEvent ), aArgs );
        }
        catch( util::VetoException& )
        {
            bReturn = true;
        }
        catch( uno::Exception& )
        {
        }
    }

    return bReturn;
}

bool ScTabViewObj::MouseReleased( const awt::MouseEvent& e )
{
    if ( e.Buttons == css::awt::MouseButton::LEFT )
    {
        try
        {
            ScTabViewShell* pViewSh = GetViewShell();
            ScViewData& rViewData = pViewSh->GetViewData();
            ScDocShell& rDocSh = rViewData.GetDocShell();
            ScDocument& rDoc = rDocSh.GetDocument();
            uno::Reference< script::vba::XVBAEventProcessor > xVbaEvents( rDoc.GetVbaEventProcessor(), uno::UNO_SET_THROW );
            uno::Sequence< uno::Any > aArgs{ getSelection() };
            xVbaEvents->processVbaEvent( ScSheetEvents::GetVbaSheetEventId( ScSheetEventId::SELECT ), aArgs );
        }
        catch( uno::Exception& )
        {
        }
        mbLeftMousePressed = false;
    }

    bool bReturn(false);

    if (!aMouseClickHandlers.empty())
    {
        uno::Reference< uno::XInterface > xTarget = GetClickedObject(Point(e.X, e.Y));

        if (xTarget.is())
        {
            awt::EnhancedMouseEvent aMouseEvent;

            aMouseEvent.Buttons = e.Buttons;
            aMouseEvent.X = e.X;
            aMouseEvent.Y = e.Y;
            aMouseEvent.ClickCount = e.ClickCount;
            aMouseEvent.PopupTrigger = e.PopupTrigger;
            aMouseEvent.Target = std::move(xTarget);
            aMouseEvent.Modifiers = e.Modifiers;

            // Listener's handler may remove it from the listeners list
            for (size_t i = aMouseClickHandlers.size(); i > 0; --i)
            {
                try
                {
                    if (!aMouseClickHandlers[i - 1]->mouseReleased( aMouseEvent ))
                        bReturn = true;
                }
                catch ( uno::Exception& )
                {
                    aMouseClickHandlers.erase(aMouseClickHandlers.begin() + (i - 1));
                }
            }
        }
    }
    return bReturn;
}

// XEnhancedMouseClickBroadcaster

void ScTabViewObj::EndMouseListening()
{
    lang::EventObject aEvent;
    aEvent.Source = getXWeak();
    for (const auto& rListener : aMouseClickHandlers)
    {
        try
        {
            rListener->disposing(aEvent);
        }
        catch ( uno::Exception& )
        {
        }
    }
    aMouseClickHandlers.clear();
}

void ScTabViewObj::EndActivationListening()
{
    lang::EventObject aEvent;
    aEvent.Source = getXWeak();
    for (const auto& rListener : aActivationListeners)
    {
        try
        {
            rListener->disposing(aEvent);
        }
        catch ( uno::Exception& )
        {
        }
    }
    aActivationListeners.clear();
}

void SAL_CALL ScTabViewObj::addEnhancedMouseClickHandler( const uno::Reference< awt::XEnhancedMouseClickHandler >& aListener )
{
    SolarMutexGuard aGuard;

    if (aListener.is())
    {
        aMouseClickHandlers.push_back( aListener );
    }
}

void SAL_CALL ScTabViewObj::removeEnhancedMouseClickHandler( const uno::Reference< awt::XEnhancedMouseClickHandler >& aListener )
{
    SolarMutexGuard aGuard;
    sal_uInt16 nCount = aMouseClickHandlers.size();
    std::erase(aMouseClickHandlers, aListener);
    if (aMouseClickHandlers.empty() && (nCount > 0)) // only if last listener removed
        EndMouseListening();
}

// XActivationBroadcaster

void SAL_CALL ScTabViewObj::addActivationEventListener( const uno::Reference< sheet::XActivationEventListener >& aListener )
{
    SolarMutexGuard aGuard;

    if (aListener.is())
    {
        aActivationListeners.push_back( aListener );
    }
}

void SAL_CALL ScTabViewObj::removeActivationEventListener( const uno::Reference< sheet::XActivationEventListener >& aListener )
{
    SolarMutexGuard aGuard;
    sal_uInt16 nCount = aActivationListeners.size();
    std::erase(aActivationListeners, aListener);
    if (aActivationListeners.empty() && (nCount > 0)) // only if last listener removed
        EndActivationListening();
}

sal_Int16 ScTabViewObj::GetZoom() const
{
    ScTabViewShell* pViewSh = GetViewShell();
    if (pViewSh)
    {
        const Fraction& rZoomY = pViewSh->GetViewData().GetZoomY();    // Y will be shown
        return static_cast<sal_Int16>(tools::Long( rZoomY * 100 ));
    }
    return 0;
}

void ScTabViewObj::SetZoom(sal_Int16 nZoom)
{
    ScTabViewShell* pViewSh = GetViewShell();
    if (!pViewSh)
        return;

    if ( nZoom != GetZoom() && nZoom != 0 )
    {
        if (!pViewSh->GetViewData().IsPagebreakMode())
        {
            ScModule* pScMod = ScModule::get();
            ScAppOptions aNewOpt(pScMod->GetAppOptions());
            aNewOpt.SetZoom( nZoom );
            aNewOpt.SetZoomType( pViewSh->GetViewData().GetView()->GetZoomType() );
            pScMod->SetAppOptions( aNewOpt );
        }
    }
    Fraction aFract( nZoom, 100 );
    pViewSh->SetZoom( aFract, aFract, true );
    pViewSh->PaintGrid();
    pViewSh->PaintTop();
    pViewSh->PaintLeft();
    pViewSh->GetViewFrame().GetBindings().Invalidate( SID_ATTR_ZOOM );
    pViewSh->GetViewFrame().GetBindings().Invalidate( SID_ATTR_ZOOMSLIDER );
    pViewSh->GetViewFrame().GetBindings().Invalidate(SID_ZOOM_IN);
    pViewSh->GetViewFrame().GetBindings().Invalidate(SID_ZOOM_OUT);
}

sal_Int16 ScTabViewObj::GetZoomType() const
{
    sal_Int16 aZoomType = view::DocumentZoomType::OPTIMAL;
    ScTabViewShell* pViewSh = GetViewShell();
    if (pViewSh)
    {
        SvxZoomType eZoomType = pViewSh->GetViewData().GetView()->GetZoomType();
        switch (eZoomType)
        {
        case SvxZoomType::PERCENT:
            aZoomType = view::DocumentZoomType::BY_VALUE;
            break;
        case SvxZoomType::OPTIMAL:
            aZoomType = view::DocumentZoomType::OPTIMAL;
            break;
        case SvxZoomType::WHOLEPAGE:
            aZoomType = view::DocumentZoomType::ENTIRE_PAGE;
            break;
        case SvxZoomType::PAGEWIDTH:
            aZoomType = view::DocumentZoomType::PAGE_WIDTH;
            break;
        case SvxZoomType::PAGEWIDTH_NOBORDER:
            aZoomType = view::DocumentZoomType::PAGE_WIDTH_EXACT;
            break;
        }
    }
    return aZoomType;
}

void ScTabViewObj::SetZoomType(sal_Int16 aZoomType)
{
    ScTabViewShell* pViewSh = GetViewShell();
    if (!pViewSh)
        return;

    ScDBFunc* pView = pViewSh->GetViewData().GetView();
    if (!pView)
        return;

    SvxZoomType eZoomType;
    switch (aZoomType)
    {
    case view::DocumentZoomType::BY_VALUE:
        eZoomType = SvxZoomType::PERCENT;
        break;
    case view::DocumentZoomType::OPTIMAL:
        eZoomType = SvxZoomType::OPTIMAL;
        break;
    case view::DocumentZoomType::ENTIRE_PAGE:
        eZoomType = SvxZoomType::WHOLEPAGE;
        break;
    case view::DocumentZoomType::PAGE_WIDTH:
        eZoomType = SvxZoomType::PAGEWIDTH;
        break;
    case view::DocumentZoomType::PAGE_WIDTH_EXACT:
        eZoomType = SvxZoomType::PAGEWIDTH_NOBORDER;
        break;
    default:
        eZoomType = SvxZoomType::OPTIMAL;
    }
    sal_Int16 nZoom(GetZoom());
    sal_Int16 nOldZoom(nZoom);
    if ( eZoomType == SvxZoomType::PERCENT )
    {
        if ( nZoom < MINZOOM )  nZoom = MINZOOM;
        if ( nZoom > MAXZOOM )  nZoom = MAXZOOM;
    }
    else
        nZoom = pView->CalcZoom( eZoomType, nOldZoom );

    switch ( eZoomType )
    {
        case SvxZoomType::WHOLEPAGE:
        case SvxZoomType::PAGEWIDTH:
            pView->SetZoomType( eZoomType, true );
            break;

        default:
            pView->SetZoomType( SvxZoomType::PERCENT, true );
    }
    SetZoom( nZoom );
}

sal_Bool SAL_CALL ScTabViewObj::getIsWindowSplit()
{
    SolarMutexGuard aGuard;
    //  what menu slot SID_WINDOW_SPLIT does

    ScTabViewShell* pViewSh = GetViewShell();
    if (pViewSh)
    {
        ScViewData& rViewData = pViewSh->GetViewData();
        return ( rViewData.GetHSplitMode() == SC_SPLIT_NORMAL ||
                 rViewData.GetVSplitMode() == SC_SPLIT_NORMAL );
    }

    return false;
}

sal_Bool SAL_CALL ScTabViewObj::hasFrozenPanes()
{
    SolarMutexGuard aGuard;
    //  what menu slot SID_WINDOW_FIX does

    ScTabViewShell* pViewSh = GetViewShell();
    if (pViewSh)
    {
        ScViewData& rViewData = pViewSh->GetViewData();
        return ( rViewData.GetHSplitMode() == SC_SPLIT_FIX ||
                 rViewData.GetVSplitMode() == SC_SPLIT_FIX );
    }

    return false;
}

sal_Int32 SAL_CALL ScTabViewObj::getSplitHorizontal()
{
    SolarMutexGuard aGuard;
    ScTabViewShell* pViewSh = GetViewShell();
    if (pViewSh)
    {
        ScViewData& rViewData = pViewSh->GetViewData();
        if ( rViewData.GetHSplitMode() != SC_SPLIT_NONE )
            return rViewData.GetHSplitPos();
    }
    return 0;
}

sal_Int32 SAL_CALL ScTabViewObj::getSplitVertical()
{
    SolarMutexGuard aGuard;
    ScTabViewShell* pViewSh = GetViewShell();
    if (pViewSh)
    {
        ScViewData& rViewData = pViewSh->GetViewData();
        if ( rViewData.GetVSplitMode() != SC_SPLIT_NONE )
            return rViewData.GetVSplitPos();
    }
    return 0;
}

sal_Int32 SAL_CALL ScTabViewObj::getSplitColumn()
{
    SolarMutexGuard aGuard;
    ScTabViewShell* pViewSh = GetViewShell();
    if (pViewSh)
    {
        ScViewData& rViewData = pViewSh->GetViewData();
        if ( rViewData.GetHSplitMode() != SC_SPLIT_NONE )
        {
            tools::Long nSplit = rViewData.GetHSplitPos();

            ScSplitPos ePos = SC_SPLIT_BOTTOMLEFT;
            if ( rViewData.GetVSplitMode() != SC_SPLIT_NONE )
                ePos = SC_SPLIT_TOPLEFT;

            SCCOL nCol;
            SCROW nRow;
            rViewData.GetPosFromPixel( nSplit, 0, ePos, nCol, nRow, false );
            if ( nCol > 0 )
                return nCol;
        }
    }
    return 0;
}

sal_Int32 SAL_CALL ScTabViewObj::getSplitRow()
{
    SolarMutexGuard aGuard;
    ScTabViewShell* pViewSh = GetViewShell();
    if (pViewSh)
    {
        ScViewData& rViewData = pViewSh->GetViewData();
        if ( rViewData.GetVSplitMode() != SC_SPLIT_NONE )
        {
            tools::Long nSplit = rViewData.GetVSplitPos();

            // split vertically
            SCCOL nCol;
            SCROW nRow;
            rViewData.GetPosFromPixel( 0, nSplit, SC_SPLIT_TOPLEFT, nCol, nRow, false );
            if ( nRow > 0 )
                return nRow;
        }
    }
    return 0;
}

void SAL_CALL ScTabViewObj::splitAtPosition( sal_Int32 nPixelX, sal_Int32 nPixelY )
{
    SolarMutexGuard aGuard;
    ScTabViewShell* pViewSh = GetViewShell();
    if (pViewSh)
    {
        pViewSh->SplitAtPixel( Point( nPixelX, nPixelY ) );
        pViewSh->FreezeSplitters( false );
        pViewSh->InvalidateSplit();
    }
}

void SAL_CALL ScTabViewObj::freezeAtPosition( sal_Int32 nColumns, sal_Int32 nRows )
{
    SolarMutexGuard aGuard;
    ScTabViewShell* pViewSh = GetViewShell();
    if (!pViewSh)
        return;

    //  first, remove them all -> no stress with scrolling in the meantime

    pViewSh->RemoveSplit();

    Point aWinStart;
    vcl::Window* pWin = pViewSh->GetWindowByPos( SC_SPLIT_BOTTOMLEFT );
    if (pWin)
        aWinStart = pWin->GetPosPixel();

    ScViewData& rViewData = pViewSh->GetViewData();
    Point aSplit(rViewData.GetScrPos( static_cast<SCCOL>(nColumns), static_cast<SCROW>(nRows), SC_SPLIT_BOTTOMLEFT, true ));
    aSplit += aWinStart;

    pViewSh->SplitAtPixel( aSplit );
    pViewSh->FreezeSplitters( true );
    pViewSh->InvalidateSplit();
}

void SAL_CALL ScTabViewObj::addSelectionChangeListener(
    const uno::Reference<view::XSelectionChangeListener>& xListener )
{
    SolarMutexGuard aGuard;
    aSelectionChgListeners.push_back( xListener );
}

void SAL_CALL ScTabViewObj::removeSelectionChangeListener(
                const uno::Reference< view::XSelectionChangeListener >& xListener )
{
    SolarMutexGuard aGuard;
    auto it = std::find(aSelectionChgListeners.begin(), aSelectionChgListeners.end(), xListener); //! why the hassle with queryInterface?
    if (it != aSelectionChgListeners.end())
        aSelectionChgListeners.erase(it);
}

void ScTabViewObj::SelectionChanged()
{
    // Selection changed so end any style preview
    // Note: executing this slot through the dispatcher
    // will cause the style dialog to be raised so we go
    // direct here
    ScFormatShell aShell( GetViewShell()->GetViewData() );
    SfxAllItemSet reqList( SfxGetpApp()->GetPool() );
    SfxRequest aReq( SID_STYLE_END_PREVIEW, SfxCallMode::SLOT, reqList );
    aShell.ExecuteStyle( aReq );
    lang::EventObject aEvent;
    aEvent.Source.set(getXWeak());
    for (const auto& rListener : aSelectionChgListeners)
        rListener->selectionChanged( aEvent );

    // handle sheet events
    ScTabViewShell* pViewSh = GetViewShell();
    ScViewData& rViewData = pViewSh->GetViewData();
    ScDocShell& rDocSh = rViewData.GetDocShell();
    ScDocument& rDoc = rDocSh.GetDocument();
    SCTAB nTab = rViewData.GetTabNo();
    const ScSheetEvents* pEvents = rDoc.GetSheetEvents(nTab);
    if (pEvents)
    {
        const OUString* pScript = pEvents->GetScript(ScSheetEventId::SELECT);
        if (pScript)
        {
            // the macro parameter is the selection as returned by getSelection
            uno::Sequence<uno::Any> aParams{ getSelection() };
            uno::Any aRet;
            uno::Sequence<sal_Int16> aOutArgsIndex;
            uno::Sequence<uno::Any> aOutArgs;
            /*ErrCode eRet =*/ rDocSh.CallXScript( *pScript, aParams, aRet, aOutArgsIndex, aOutArgs );
        }
    }

    SfxApplication::Get()->Broadcast( SfxHint( SfxHintId::ScSelectionChanged ) );

    if ( mbLeftMousePressed ) // selection still in progress
        return;

    try
    {
        uno::Reference< script::vba::XVBAEventProcessor > xVbaEvents( rDoc.GetVbaEventProcessor(), uno::UNO_SET_THROW );
        uno::Sequence< uno::Any > aArgs{ getSelection() };
        xVbaEvents->processVbaEvent( ScSheetEvents::GetVbaSheetEventId( ScSheetEventId::SELECT ), aArgs );
    }
    catch( uno::Exception& )
    {
    }
}

//  XPropertySet (view options)
//! provide those also in application?

uno::Reference<beans::XPropertySetInfo> SAL_CALL ScTabViewObj::getPropertySetInfo()
{
    SolarMutexGuard aGuard;
    static uno::Reference<beans::XPropertySetInfo> aRef(
        new SfxItemPropertySetInfo( aPropSet.getPropertyMap() ));
    return aRef;
}

void SAL_CALL ScTabViewObj::setPropertyValue(
                        const OUString& aPropertyName, const uno::Any& aValue )
{
    SolarMutexGuard aGuard;

    if ( aPropertyName == SC_UNO_FILTERED_RANGE_SELECTION )
    {
        bFilteredRangeSelection = ScUnoHelpFunctions::GetBoolFromAny(aValue);
        return;
    }

    ScTabViewShell* pViewSh = GetViewShell();
    if (!pViewSh)
        return;

    ScViewData& rViewData = pViewSh->GetViewData();
    const ScViewOptions& rOldOpt = pViewSh->GetViewData().GetOptions();
    ScViewOptions aNewOpt(rOldOpt);

    if ( aPropertyName == SC_UNO_COLROWHDR || aPropertyName == OLD_UNO_COLROWHDR )
        aNewOpt.SetOption(sc::ViewOption::HEADER, ScUnoHelpFunctions::GetBoolFromAny( aValue ) );
    else if ( aPropertyName == SC_UNO_HORSCROLL || aPropertyName == OLD_UNO_HORSCROLL )
        aNewOpt.SetOption(sc::ViewOption::HSCROLL, ScUnoHelpFunctions::GetBoolFromAny( aValue ) );
    else if ( aPropertyName == SC_UNO_OUTLSYMB || aPropertyName == OLD_UNO_OUTLSYMB )
        aNewOpt.SetOption(sc::ViewOption::OUTLINER, ScUnoHelpFunctions::GetBoolFromAny( aValue ) );
    else if ( aPropertyName == SC_UNO_SHEETTABS || aPropertyName == OLD_UNO_SHEETTABS )
        aNewOpt.SetOption(sc::ViewOption::TABCONTROLS, ScUnoHelpFunctions::GetBoolFromAny( aValue ) );
    else if ( aPropertyName == SC_UNO_SHOWANCHOR )
        aNewOpt.SetOption(sc::ViewOption::ANCHOR, ScUnoHelpFunctions::GetBoolFromAny( aValue ) );
    else if ( aPropertyName == SC_UNO_SHOWFORM )
        aNewOpt.SetOption(sc::ViewOption::FORMULAS, ScUnoHelpFunctions::GetBoolFromAny( aValue ) );
    else if ( aPropertyName == SC_UNO_SHOWGRID )
        aNewOpt.SetOption(sc::ViewOption::GRID, ScUnoHelpFunctions::GetBoolFromAny( aValue ) );
    else if ( aPropertyName == SC_UNO_SHOWHELP )
        aNewOpt.SetOption(sc::ViewOption::HELPLINES, ScUnoHelpFunctions::GetBoolFromAny( aValue ) );
    else if ( aPropertyName == SC_UNO_SHOWNOTES )
        aNewOpt.SetOption(sc::ViewOption::NOTES, ScUnoHelpFunctions::GetBoolFromAny( aValue ) );
    else if ( aPropertyName == SC_UNO_SHOWNOTEAUTHOR )
        aNewOpt.SetOption(sc::ViewOption::NOTEAUTHOR, ScUnoHelpFunctions::GetBoolFromAny( aValue ) );
    else if ( aPropertyName == SC_UNO_SHOWFORMULASMARKS )
        aNewOpt.SetOption(sc::ViewOption::FORMULAS_MARKS, ScUnoHelpFunctions::GetBoolFromAny( aValue ) );
    else if ( aPropertyName == SC_UNO_SHOWPAGEBR )
        aNewOpt.SetOption(sc::ViewOption::PAGEBREAKS, ScUnoHelpFunctions::GetBoolFromAny( aValue ) );
    else if ( aPropertyName == SC_UNO_SHOWZERO )
        aNewOpt.SetOption(sc::ViewOption::NULLVALS, ScUnoHelpFunctions::GetBoolFromAny( aValue ) );
    else if ( aPropertyName == SC_UNO_VALUEHIGH || aPropertyName == OLD_UNO_VALUEHIGH )
        aNewOpt.SetOption(sc::ViewOption::SYNTAX, ScUnoHelpFunctions::GetBoolFromAny( aValue ) );
    else if ( aPropertyName == SC_UNO_VERTSCROLL || aPropertyName == OLD_UNO_VERTSCROLL )
        aNewOpt.SetOption(sc::ViewOption::VSCROLL, ScUnoHelpFunctions::GetBoolFromAny( aValue ) );
    else if ( aPropertyName == SC_UNO_SHOWOBJ )
    {
        sal_Int16 nIntVal = 0;
        if ( aValue >>= nIntVal )
        {
            //#i80528# adapt to new range eventually
            if(sal_Int16(VOBJ_MODE_HIDE) < nIntVal) nIntVal = sal_Int16(VOBJ_MODE_SHOW);

            aNewOpt.SetObjMode(sc::ViewObjectType::OLE, static_cast<ScVObjMode>(nIntVal));
        }
    }
    else if ( aPropertyName == SC_UNO_SHOWCHARTS )
    {
        sal_Int16 nIntVal = 0;
        if ( aValue >>= nIntVal )
        {
            //#i80528# adapt to new range eventually
            if(sal_Int16(VOBJ_MODE_HIDE) < nIntVal) nIntVal = sal_Int16(VOBJ_MODE_SHOW);

            aNewOpt.SetObjMode(sc::ViewObjectType::CHART, static_cast<ScVObjMode>(nIntVal));
        }
    }
    else if ( aPropertyName == SC_UNO_SHOWDRAW )
    {
        sal_Int16 nIntVal = 0;
        if ( aValue >>= nIntVal )
        {
            //#i80528# adapt to new range eventually
            if(sal_Int16(VOBJ_MODE_HIDE) < nIntVal) nIntVal = sal_Int16(VOBJ_MODE_SHOW);

            aNewOpt.SetObjMode(sc::ViewObjectType::DRAW, static_cast<ScVObjMode>(nIntVal));
        }
    }
    else if ( aPropertyName == SC_UNO_GRIDCOLOR )
    {
        Color nIntVal;
        if ( aValue >>= nIntVal )
            aNewOpt.SetGridColor( nIntVal, OUString() );
    }
    else if ( aPropertyName == SC_UNO_ZOOMTYPE )
    {
        sal_Int16 nIntVal = 0;
        if ( aValue >>= nIntVal )
            SetZoomType(nIntVal);
    }
    else if ( aPropertyName == SC_UNO_ZOOMVALUE )
    {
        sal_Int16 nIntVal = 0;
        if ( aValue >>= nIntVal )
            SetZoom(nIntVal);
    }
    else if ( aPropertyName == SC_UNO_FORMULABARHEIGHT )
    {
        sal_Int16 nIntVal = ScUnoHelpFunctions::GetInt16FromAny(aValue);
        if (nIntVal > 0)
        {
            rViewData.SetFormulaBarLines(nIntVal);
            // Notify formula bar about changed lines
            ScInputHandler* pInputHdl = ScModule::get()->GetInputHdl();
            if (pInputHdl)
            {
                ScInputWindow* pInputWin = pInputHdl->GetInputWindow();
                if (pInputWin)
                    pInputWin->NumLinesChanged();
            }
        }
    }

    //  Options are set on the view and document (for new views),
    //  so that they remain during saving.
    //! In the app (module) we need an extra options to tune that
    //! (for new documents)

    if ( aNewOpt == rOldOpt )
        return;

    rViewData.SetOptions( aNewOpt );
    rViewData.GetDocument().SetViewOptions( aNewOpt );
    rViewData.GetDocShell().SetDocumentModified();    //! really?

    pViewSh->UpdateFixPos();
    pViewSh->PaintGrid();
    pViewSh->PaintTop();
    pViewSh->PaintLeft();
    pViewSh->PaintExtras();
    pViewSh->InvalidateBorder();

    SfxBindings& rBindings = pViewSh->GetViewFrame().GetBindings();
    rBindings.Invalidate( FID_TOGGLEHEADERS ); // -> check in menu
    rBindings.Invalidate( FID_TOGGLESYNTAX );
}

uno::Any SAL_CALL ScTabViewObj::getPropertyValue( const OUString& aPropertyName )
{
    SolarMutexGuard aGuard;
    uno::Any aRet;

    if ( aPropertyName == SC_UNO_FILTERED_RANGE_SELECTION )
    {
        aRet <<= bFilteredRangeSelection;
        return aRet;
    }

    ScTabViewShell* pViewSh = GetViewShell();
    if (pViewSh)
    {
        ScViewData& rViewData = pViewSh->GetViewData();
        const ScViewOptions& rOpt = rViewData.GetOptions();

        if ( aPropertyName == SC_UNO_COLROWHDR || aPropertyName == OLD_UNO_COLROWHDR )
            aRet <<= rOpt.GetOption(sc::ViewOption::HEADER);
        else if ( aPropertyName == SC_UNO_HORSCROLL || aPropertyName == OLD_UNO_HORSCROLL )
            aRet <<= rOpt.GetOption(sc::ViewOption::HSCROLL);
        else if ( aPropertyName == SC_UNO_OUTLSYMB || aPropertyName == OLD_UNO_OUTLSYMB )
            aRet <<= rOpt.GetOption(sc::ViewOption::OUTLINER);
        else if ( aPropertyName == SC_UNO_SHEETTABS || aPropertyName == OLD_UNO_SHEETTABS )
            aRet <<= rOpt.GetOption(sc::ViewOption::TABCONTROLS);
        else if ( aPropertyName == SC_UNO_SHOWANCHOR ) aRet <<= rOpt.GetOption(sc::ViewOption::ANCHOR);
        else if ( aPropertyName == SC_UNO_SHOWFORM )   aRet <<= rOpt.GetOption(sc::ViewOption::FORMULAS);
        else if ( aPropertyName == SC_UNO_SHOWGRID )   aRet <<= rOpt.GetOption(sc::ViewOption::GRID);
        else if ( aPropertyName == SC_UNO_SHOWHELP )   aRet <<= rOpt.GetOption(sc::ViewOption::HELPLINES);
        else if ( aPropertyName == SC_UNO_SHOWNOTES )  aRet <<= rOpt.GetOption(sc::ViewOption::NOTES);
        else if ( aPropertyName == SC_UNO_SHOWNOTEAUTHOR )  aRet <<= rOpt.GetOption(sc::ViewOption::NOTEAUTHOR);
        else if ( aPropertyName == SC_UNO_SHOWFORMULASMARKS )  aRet <<= rOpt.GetOption(sc::ViewOption::FORMULAS_MARKS);
        else if ( aPropertyName == SC_UNO_SHOWPAGEBR ) aRet <<= rOpt.GetOption(sc::ViewOption::PAGEBREAKS);
        else if ( aPropertyName == SC_UNO_SHOWZERO )   aRet <<= rOpt.GetOption(sc::ViewOption::NULLVALS);
        else if ( aPropertyName == SC_UNO_VALUEHIGH || aPropertyName == OLD_UNO_VALUEHIGH )
            aRet <<= rOpt.GetOption(sc::ViewOption::SYNTAX);
        else if ( aPropertyName == SC_UNO_VERTSCROLL || aPropertyName == OLD_UNO_VERTSCROLL )
            aRet <<= rOpt.GetOption(sc::ViewOption::VSCROLL);
        else if ( aPropertyName == SC_UNO_SHOWOBJ )    aRet <<= static_cast<sal_Int16>( rOpt.GetObjMode(sc::ViewObjectType::OLE) );
        else if ( aPropertyName == SC_UNO_SHOWCHARTS ) aRet <<= static_cast<sal_Int16>( rOpt.GetObjMode(sc::ViewObjectType::CHART) );
        else if ( aPropertyName == SC_UNO_SHOWDRAW )   aRet <<= static_cast<sal_Int16>( rOpt.GetObjMode(sc::ViewObjectType::DRAW) );
        else if ( aPropertyName == SC_UNO_GRIDCOLOR )  aRet <<= rOpt.GetGridColor();
        else if ( aPropertyName == SC_UNO_VISAREA ) aRet <<= GetVisArea();
        else if ( aPropertyName == SC_UNO_ZOOMTYPE ) aRet <<= GetZoomType();
        else if ( aPropertyName == SC_UNO_ZOOMVALUE ) aRet <<= GetZoom();
        else if ( aPropertyName == SC_UNO_FORMULABARHEIGHT ) aRet <<= rViewData.GetFormulaBarLines();
        else if ( aPropertyName == SC_UNO_VISAREASCREEN )
        {
            vcl::Window* pActiveWin = rViewData.GetActiveWin();
            if ( pActiveWin )
            {
                AbsoluteScreenPixelRectangle aRect = pActiveWin->GetWindowExtentsAbsolute();
                aRet <<= vcl::unohelper::ConvertToAWTRect(aRect);
            }
        }
    }

    return aRet;
}

void SAL_CALL ScTabViewObj::addPropertyChangeListener( const OUString& /* aPropertyName */,
    const uno::Reference<beans::XPropertyChangeListener >& xListener )
{
    SolarMutexGuard aGuard;
    aPropertyChgListeners.push_back( xListener );
}

void SAL_CALL ScTabViewObj::removePropertyChangeListener( const OUString& /* aPropertyName */,
                                    const uno::Reference<beans::XPropertyChangeListener >& xListener )
{
    SolarMutexGuard aGuard;
    auto it = std::find(aPropertyChgListeners.begin(), aPropertyChgListeners.end(), xListener); //! Why the nonsense with queryInterface?
    if (it != aPropertyChgListeners.end())
        aPropertyChgListeners.erase(it);
}

void SAL_CALL ScTabViewObj::addVetoableChangeListener( const OUString& /* PropertyName */,
                                    const uno::Reference<beans::XVetoableChangeListener >& /* aListener */ )
{
}

void SAL_CALL ScTabViewObj::removeVetoableChangeListener( const OUString& /* PropertyName */,
                                    const uno::Reference<beans::XVetoableChangeListener >& /* aListener */ )
{
}

void ScTabViewObj::VisAreaChanged()
{
    beans::PropertyChangeEvent aEvent;
    aEvent.Source.set(getXWeak());
    for (const auto& rListener : aPropertyChgListeners)
        rListener->propertyChange( aEvent );
}

// XRangeSelection

void SAL_CALL ScTabViewObj::startRangeSelection(
                                const uno::Sequence<beans::PropertyValue>& aArguments )
{
    SolarMutexGuard aGuard;
    ScTabViewShell* pViewSh = GetViewShell();
    if (!pViewSh)
        return;

    OUString aInitVal, aTitle;
    bool bCloseOnButtonUp = false;
    bool bSingleCell = false;
    bool bMultiSelection = false;

    OUString aStrVal;
    for (const beans::PropertyValue& rProp : aArguments)
    {
        OUString aPropName(rProp.Name);

        if (aPropName == SC_UNONAME_CLOSEONUP )
            bCloseOnButtonUp = ScUnoHelpFunctions::GetBoolFromAny( rProp.Value );
        else if (aPropName == SC_UNONAME_TITLE )
        {
            if ( rProp.Value >>= aStrVal )
                aTitle = aStrVal;
        }
        else if (aPropName == SC_UNONAME_INITVAL )
        {
            if ( rProp.Value >>= aStrVal )
                aInitVal = aStrVal;
        }
        else if (aPropName == SC_UNONAME_SINGLECELL )
            bSingleCell = ScUnoHelpFunctions::GetBoolFromAny( rProp.Value );
        else if (aPropName == SC_UNONAME_MULTISEL )
            bMultiSelection = ScUnoHelpFunctions::GetBoolFromAny( rProp.Value );
    }

    pViewSh->StartSimpleRefDialog( aTitle, aInitVal, bCloseOnButtonUp, bSingleCell, bMultiSelection );
}

void SAL_CALL ScTabViewObj::abortRangeSelection()
{
    SolarMutexGuard aGuard;
    ScTabViewShell* pViewSh = GetViewShell();
    if (pViewSh)
        pViewSh->StopSimpleRefDialog();
}

void SAL_CALL ScTabViewObj::addRangeSelectionListener(
    const uno::Reference<sheet::XRangeSelectionListener>& xListener )
{
    SolarMutexGuard aGuard;
    aRangeSelListeners.push_back( xListener );
}

void SAL_CALL ScTabViewObj::removeRangeSelectionListener(
                                const uno::Reference<sheet::XRangeSelectionListener>& xListener )
{
    SolarMutexGuard aGuard;
    auto it = std::find(aRangeSelListeners.begin(), aRangeSelListeners.end(), xListener);
    if (it != aRangeSelListeners.end())
        aRangeSelListeners.erase(it);
}

void SAL_CALL ScTabViewObj::addRangeSelectionChangeListener(
    const uno::Reference<sheet::XRangeSelectionChangeListener>& xListener )
{
    SolarMutexGuard aGuard;
    aRangeChgListeners.push_back( xListener );
}

void SAL_CALL ScTabViewObj::removeRangeSelectionChangeListener(
                                const uno::Reference<sheet::XRangeSelectionChangeListener>& xListener )
{
    SolarMutexGuard aGuard;
    auto it = std::find(aRangeChgListeners.begin(), aRangeChgListeners.end(), xListener);
    if (it != aRangeChgListeners.end())
        aRangeChgListeners.erase(it);
}

void ScTabViewObj::RangeSelDone( const OUString& rText )
{
    sheet::RangeSelectionEvent aEvent;
    aEvent.Source.set(getXWeak());
    aEvent.RangeDescriptor = rText;

    // copy on the stack because listener could remove itself
    const RangeSelListeners listeners(aRangeSelListeners);

    for (const auto& rListener : listeners)
        rListener->done( aEvent );
}

void ScTabViewObj::RangeSelAborted( const OUString& rText )
{
    sheet::RangeSelectionEvent aEvent;
    aEvent.Source.set(getXWeak());
    aEvent.RangeDescriptor = rText;

    // copy on the stack because listener could remove itself
    const RangeSelListeners listeners(aRangeSelListeners);

    for (const auto& rListener : listeners)
        rListener->aborted( aEvent );
}

void ScTabViewObj::RangeSelChanged( const OUString& rText )
{
    sheet::RangeSelectionEvent aEvent;
    aEvent.Source.set(getXWeak());
    aEvent.RangeDescriptor = rText;

    // copy on the stack because listener could remove itself
    const std::vector<css::uno::Reference<css::sheet::XRangeSelectionChangeListener>> listener(aRangeChgListeners);
    for (const auto& rListener : listener)
        rListener->descriptorChanged( aEvent );
}

// XServiceInfo
OUString SAL_CALL ScTabViewObj::getImplementationName()
{
    return u"ScTabViewObj"_ustr;
}

sal_Bool SAL_CALL ScTabViewObj::supportsService( const OUString& rServiceName )
{
    return cppu::supportsService(this, rServiceName);
}

uno::Sequence<OUString> SAL_CALL ScTabViewObj::getSupportedServiceNames()
{
    return {SCTABVIEWOBJ_SERVICE, SCVIEWSETTINGS_SERVICE};
}

// XUnoTunnel

css::uno::Reference< css::datatransfer::XTransferable > SAL_CALL ScTabViewObj::getTransferable()
{
    SolarMutexGuard aGuard;
    ScEditShell* pShell = dynamic_cast<ScEditShell*>( GetViewShell()->GetViewFrame().GetDispatcher()->GetShell(0)  );
    if (pShell)
        return pShell->GetEditView()->GetTransferable();

    ScDrawTextObjectBar* pTextShell = dynamic_cast<ScDrawTextObjectBar*>( GetViewShell()->GetViewFrame().GetDispatcher()->GetShell(0)  );
    if (pTextShell)
    {
        ScViewData& rViewData = GetViewShell()->GetViewData();
        ScDrawView* pView = rViewData.GetScDrawView();
        OutlinerView* pOutView = pView->GetTextEditOutlinerView();
        if (pOutView)
            return pOutView->GetEditView().GetTransferable();
    }

    ScDrawShell* pDrawShell = dynamic_cast<ScDrawShell*>( GetViewShell()->GetViewFrame().GetDispatcher()->GetShell(0)  );
    if (pDrawShell)
        return pDrawShell->GetDrawView()->CopyToTransferable();

    return GetViewShell()->CopyToTransferable();
}

void SAL_CALL ScTabViewObj::insertTransferable( const css::uno::Reference< css::datatransfer::XTransferable >& xTrans )
{
    SolarMutexGuard aGuard;
    ScEditShell* pShell = dynamic_cast<ScEditShell*>( GetViewShell()->GetViewFrame().GetDispatcher()->GetShell(0)  );
    if (pShell)
        pShell->GetEditView()->InsertText( xTrans, OUString(), false );
    else
    {
        ScDrawTextObjectBar* pTextShell = dynamic_cast<ScDrawTextObjectBar*>( GetViewShell()->GetViewFrame().GetDispatcher()->GetShell(0)  );
        if (pTextShell)
        {
            ScViewData& rViewData = GetViewShell()->GetViewData();
            ScDrawView* pView = rViewData.GetScDrawView();
            OutlinerView* pOutView = pView->GetTextEditOutlinerView();
            if ( pOutView )
            {
                pOutView->GetEditView().InsertText( xTrans, OUString(), false );
                return;
            }
        }

        GetViewShell()->PasteFromTransferable( xTrans );
    }
}

namespace {

uno::Sequence<sal_Int32> toSequence(const ScMarkData::MarkedTabsType& rSelected)
{
    uno::Sequence<sal_Int32> aRet(rSelected.size());
    auto aRetRange = asNonConstRange(aRet);
    size_t i = 0;
    for (const auto& rTab : rSelected)
    {
        aRetRange[i] = static_cast<sal_Int32>(rTab);
        ++i;
    }

    return aRet;
}

}

uno::Sequence<sal_Int32> ScTabViewObj::getSelectedSheets()
{
    ScTabViewShell* pViewSh = GetViewShell();
    if (!pViewSh)
        return uno::Sequence<sal_Int32>();

    ScViewData& rViewData = pViewSh->GetViewData();

    // #i95280# when printing from the shell, the view is never activated,
    // so Excel view settings must also be evaluated here.
    ScExtDocOptions* pExtOpt = rViewData.GetDocument().GetExtDocOptions();
    if (pExtOpt && pExtOpt->IsChanged())
    {
        pViewSh->GetViewData().ReadExtOptions(*pExtOpt);        // Excel view settings
        pViewSh->SetTabNo(pViewSh->GetViewData().GetTabNo(), true);
        pExtOpt->SetChanged(false);
    }

    return toSequence(rViewData.GetMarkData().GetSelectedTabs());
}

ScPreviewObj::ScPreviewObj(ScPreviewShell* pViewSh) :
    SfxBaseController(pViewSh),
    mpViewShell(pViewSh)
{
    if (mpViewShell)
        StartListening(*mpViewShell);
}

ScPreviewObj::~ScPreviewObj()
{
    if (mpViewShell)
        EndListening(*mpViewShell);
}

uno::Any ScPreviewObj::queryInterface(const uno::Type& rType)
{
    uno::Any aReturn = ::cppu::queryInterface(rType,
                    static_cast<sheet::XSelectedSheetsSupplier*>(this));
    if ( aReturn.hasValue() )
        return aReturn;
    return SfxBaseController::queryInterface(rType);
}

void ScPreviewObj::acquire() noexcept
{
    SfxBaseController::acquire();
}

void ScPreviewObj::release() noexcept
{
    SfxBaseController::release();
}

void ScPreviewObj::Notify(SfxBroadcaster&, const SfxHint& rHint)
{
    if (rHint.GetId() == SfxHintId::Dying)
        mpViewShell = nullptr;
}

uno::Sequence<sal_Int32> ScPreviewObj::getSelectedSheets()
{
    ScPreview* p = mpViewShell ? mpViewShell->GetPreview() : nullptr;
    if (!p)
        return uno::Sequence<sal_Int32>();

    return toSequence(p->GetSelectedTabs());
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
