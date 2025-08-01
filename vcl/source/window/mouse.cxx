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


#include <config_feature_desktop.h>
#include <config_vclplug.h>

#include <tools/time.hxx>

#include <LibreOfficeKit/LibreOfficeKitEnums.h>

#include <vcl/ITiledRenderable.hxx>
#include <vcl/dndlistenercontainer.hxx>
#include <vcl/svapp.hxx>
#include <vcl/window.hxx>
#include <vcl/cursor.hxx>
#include <vcl/sysdata.hxx>
#include <vcl/event.hxx>

#include <sal/types.h>

#include <window.h>
#include <svdata.hxx>
#include <salobj.hxx>
#include <salgdi.hxx>
#include <salframe.hxx>
#include <salinst.hxx>

#include <dndeventdispatcher.hxx>

#include <com/sun/star/datatransfer/dnd/XDragSource.hpp>
#include <com/sun/star/datatransfer/dnd/XDropTarget.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>

#include <comphelper/processfactory.hxx>

using namespace ::com::sun::star::uno;

namespace vcl {

WindowHitTest Window::ImplHitTest( const Point& rFramePos )
{
    Point aFramePos( rFramePos );
    if( GetOutDev()->ImplIsAntiparallel() )
    {
        const OutputDevice *pOutDev = GetOutDev();
        pOutDev->ReMirror( aFramePos );
    }
    if ( !GetOutputRectPixel().Contains( aFramePos ) )
        return WindowHitTest::NONE;
    if ( mpWindowImpl->mbWinRegion )
    {
        Point aTempPos = aFramePos;
        aTempPos.AdjustX( -GetOutDev()->mnOutOffX );
        aTempPos.AdjustY( -GetOutDev()->mnOutOffY );
        if ( !mpWindowImpl->maWinRegion.Contains( aTempPos ) )
            return WindowHitTest::NONE;
    }

    WindowHitTest nHitTest = WindowHitTest::Inside;
    if ( mpWindowImpl->mbMouseTransparent )
        nHitTest |= WindowHitTest::Transparent;
    return nHitTest;
}

bool Window::ImplTestMousePointerSet()
{
    // as soon as mouse is captured, switch mouse-pointer
    if ( IsMouseCaptured() )
        return true;

    // if the mouse is over the window, switch it
    tools::Rectangle aClientRect( Point( 0, 0 ), GetOutputSizePixel() );
    return aClientRect.Contains( GetPointerPosPixel() );
}

PointerStyle Window::ImplGetMousePointer() const
{
    PointerStyle    ePointerStyle;
    bool            bWait = false;

    if ( IsEnabled() && IsInputEnabled() && ! IsInModalMode() )
        ePointerStyle = GetPointer();
    else
        ePointerStyle = PointerStyle::Arrow;

    const vcl::Window* pWindow = this;
    do
    {
        // when the pointer is not visible stop the search, as
        // this status should not be overwritten
        if ( pWindow->mpWindowImpl->mbNoPtrVisible )
            return PointerStyle::Null;

        if ( !bWait )
        {
            if ( pWindow->mpWindowImpl->mnWaitCount )
            {
                ePointerStyle = PointerStyle::Wait;
                bWait = true;
            }
            else
            {
                if ( pWindow->mpWindowImpl->mbChildPtrOverwrite )
                    ePointerStyle = pWindow->GetPointer();
            }
        }

        if ( pWindow->ImplIsOverlapWindow() )
            break;

        pWindow = pWindow->ImplGetParent();
    }
    while ( pWindow );

    return ePointerStyle;
}

void Window::ImplCallMouseMove( sal_uInt16 nMouseCode, bool bModChanged )
{
    if ( !(mpWindowImpl->mpFrameData->mbMouseIn && mpWindowImpl->mpFrameWindow->mpWindowImpl->mbReallyVisible) )
        return;

    sal_uInt64 nTime   = tools::Time::GetSystemTicks();
    tools::Long    nX      = mpWindowImpl->mpFrameData->mnLastMouseX;
    tools::Long    nY      = mpWindowImpl->mpFrameData->mnLastMouseY;
    sal_uInt16  nCode   = nMouseCode;
    MouseEventModifiers nMode = mpWindowImpl->mpFrameData->mnMouseMode;
    bool    bLeave;
    // check for MouseLeave
    bLeave = ((nX < 0) || (nY < 0) ||
              (nX >= mpWindowImpl->mpFrameWindow->GetOutDev()->mnOutWidth) ||
              (nY >= mpWindowImpl->mpFrameWindow->GetOutDev()->mnOutHeight)) &&
             !ImplGetSVData()->mpWinData->mpCaptureWin;
    nMode |= MouseEventModifiers::SYNTHETIC;
    if ( bModChanged )
        nMode |= MouseEventModifiers::MODIFIERCHANGED;
    ImplHandleMouseEvent( mpWindowImpl->mpFrameWindow, NotifyEventType::MOUSEMOVE, bLeave, nX, nY, nTime, nCode, nMode );
}

void Window::ImplGenerateMouseMove()
{
    if ( mpWindowImpl && mpWindowImpl->mpFrameData &&
         !mpWindowImpl->mpFrameData->mnMouseMoveId )
        mpWindowImpl->mpFrameData->mnMouseMoveId = Application::PostUserEvent( LINK( mpWindowImpl->mpFrameWindow, Window, ImplGenerateMouseMoveHdl ), nullptr, true );
}

IMPL_LINK_NOARG(Window, ImplGenerateMouseMoveHdl, void*, void)
{
    mpWindowImpl->mpFrameData->mnMouseMoveId = nullptr;
    vcl::Window* pCaptureWin = ImplGetSVData()->mpWinData->mpCaptureWin;
    if( ! pCaptureWin ||
        (pCaptureWin->mpWindowImpl && pCaptureWin->mpWindowImpl->mpFrame == mpWindowImpl->mpFrame)
    )
    {
        ImplCallMouseMove( mpWindowImpl->mpFrameData->mnMouseCode );
    }
}

void Window::ImplInvertFocus( const tools::Rectangle& rRect )
{
    InvertTracking( rRect, ShowTrackFlags::Small | ShowTrackFlags::TrackWindow );
}

static bool IsWindowFocused(const WindowImpl& rWinImpl)
{
    if (rWinImpl.mpSysObj)
        return true;

    if (rWinImpl.mpFrameData->mbHasFocus)
        return true;

    if (rWinImpl.mbFakeFocusSet)
        return true;

    return false;
}

void Window::ImplGrabFocus( GetFocusFlags nFlags )
{
    // #143570# no focus for destructing windows
    if( !mpWindowImpl || mpWindowImpl->mbInDispose )
        return;

    // some event listeners do really bad stuff
    // => prepare for the worst
    VclPtr<vcl::Window> xWindow( this );

    // Currently the client window should always get the focus
    // Should the border window at some point be focusable
    // we need to change all GrabFocus() instances in VCL,
    // e.g. in ToTop()

    if ( mpWindowImpl->mpClientWindow )
    {
        // For a lack of design we need a little hack here to
        // ensure that dialogs on close pass the focus back to
        // the correct window
        if ( mpWindowImpl->mpLastFocusWindow && (mpWindowImpl->mpLastFocusWindow.get() != this) &&
             !(mpWindowImpl->mnDlgCtrlFlags & DialogControlFlags::WantFocus) &&
             mpWindowImpl->mpLastFocusWindow->IsEnabled() &&
             mpWindowImpl->mpLastFocusWindow->IsInputEnabled() &&
             ! mpWindowImpl->mpLastFocusWindow->IsInModalMode()
             )
            mpWindowImpl->mpLastFocusWindow->GrabFocus();
        else
            mpWindowImpl->mpClientWindow->GrabFocus();
        return;
    }
    else if ( mpWindowImpl->mbFrame )
    {
        // For a lack of design we need a little hack here to
        // ensure that dialogs on close pass the focus back to
        // the correct window
        if ( mpWindowImpl->mpLastFocusWindow && (mpWindowImpl->mpLastFocusWindow.get() != this) &&
             !(mpWindowImpl->mnDlgCtrlFlags & DialogControlFlags::WantFocus) &&
             mpWindowImpl->mpLastFocusWindow->IsEnabled() &&
             mpWindowImpl->mpLastFocusWindow->IsInputEnabled() &&
             ! mpWindowImpl->mpLastFocusWindow->IsInModalMode()
             )
        {
            mpWindowImpl->mpLastFocusWindow->GrabFocus();
            return;
        }
    }

    // If the Window is disabled, then we don't change the focus
    if ( !IsEnabled() || !IsInputEnabled() || IsInModalMode() )
        return;

    // we only need to set the focus if it is not already set
    // note: if some other frame is waiting for an asynchronous focus event
    // we also have to post an asynchronous focus event for this frame
    // which is done using ToTop
    ImplSVData* pSVData = ImplGetSVData();

    bool bAsyncFocusWaiting = false;
    vcl::Window *pFrame = pSVData->maFrameData.mpFirstFrame;
    while( pFrame && pFrame->mpWindowImpl && pFrame->mpWindowImpl->mpFrameData )
    {
        if( pFrame != mpWindowImpl->mpFrameWindow.get() && pFrame->mpWindowImpl->mpFrameData->mnFocusId )
        {
            bAsyncFocusWaiting = true;
            break;
        }
        pFrame = pFrame->mpWindowImpl->mpFrameData->mpNextFrame;
    }

    bool bHasFocus = IsWindowFocused(*mpWindowImpl);

    bool bMustNotGrabFocus = false;
    // #100242#, check parent hierarchy if some floater prohibits grab focus

    vcl::Window *pParent = this;
    while( pParent )
    {
        if ((pParent->GetStyle() & WB_SYSTEMFLOATWIN) && !(pParent->GetStyle() & WB_MOVEABLE))
        {
            bMustNotGrabFocus = true;
            break;
        }
        if (!pParent->mpWindowImpl)
            break;
        pParent = pParent->mpWindowImpl->mpParent;
    }

    if ( !(( pSVData->mpWinData->mpFocusWin.get() != this &&
             !mpWindowImpl->mbInDispose ) ||
           ( bAsyncFocusWaiting && !bHasFocus && !bMustNotGrabFocus )) )
        return;

    // EndExtTextInput if it is not the same window
    if (pSVData->mpWinData->mpExtTextInputWin
        && (pSVData->mpWinData->mpExtTextInputWin.get() != this))
        pSVData->mpWinData->mpExtTextInputWin->EndExtTextInput();

    // mark this windows as the last FocusWindow
    vcl::Window* pOverlapWindow = ImplGetFirstOverlapWindow();
    if (pOverlapWindow->mpWindowImpl)
        pOverlapWindow->mpWindowImpl->mpLastFocusWindow = this;
    mpWindowImpl->mpFrameData->mpFocusWin = this;

    if( !bHasFocus )
    {
        // menu windows never get the system focus
        // the application will keep the focus
        if( bMustNotGrabFocus )
            return;
        else
        {
            // here we already switch focus as ToTop()
            // should not give focus to another window
            mpWindowImpl->mpFrame->ToTop( SalFrameToTop::GrabFocus | SalFrameToTop::GrabFocusOnly );
            return;
        }
    }

    VclPtr<vcl::Window> pOldFocusWindow = pSVData->mpWinData->mpFocusWin;

    pSVData->mpWinData->mpFocusWin = this;

    if ( pOldFocusWindow && pOldFocusWindow->mpWindowImpl )
    {
        // Cursor hidden
        if ( pOldFocusWindow->mpWindowImpl->mpCursor )
            pOldFocusWindow->mpWindowImpl->mpCursor->ImplHide();
    }

    // !!!!! due to old SV-Office Activate/Deactivate handling
    // !!!!! first as before
    if ( pOldFocusWindow )
    {
        // remember Focus
        vcl::Window* pOldOverlapWindow = pOldFocusWindow->ImplGetFirstOverlapWindow();
        vcl::Window* pNewOverlapWindow = ImplGetFirstOverlapWindow();
        if ( pOldOverlapWindow != pNewOverlapWindow )
            ImplCallFocusChangeActivate( pNewOverlapWindow, pOldOverlapWindow );
    }
    else
    {
        vcl::Window* pNewOverlapWindow = ImplGetFirstOverlapWindow();
        if ( pNewOverlapWindow && pNewOverlapWindow->mpWindowImpl )
        {
            vcl::Window* pNewRealWindow = pNewOverlapWindow->ImplGetWindow();
            pNewOverlapWindow->mpWindowImpl->mbActive = true;
            pNewOverlapWindow->Activate();
            if ( pNewRealWindow != pNewOverlapWindow  && pNewRealWindow && pNewRealWindow->mpWindowImpl )
            {
                pNewRealWindow->mpWindowImpl->mbActive = true;
                pNewRealWindow->Activate();
            }
        }
    }

    // call Get- and LoseFocus
    if ( pOldFocusWindow && ! pOldFocusWindow->isDisposed() )
    {
        NotifyEvent aNEvt( NotifyEventType::LOSEFOCUS, pOldFocusWindow );
        if ( !ImplCallPreNotify( aNEvt ) )
            pOldFocusWindow->CompatLoseFocus();
        pOldFocusWindow->ImplCallDeactivateListeners( this );
    }

    if (pSVData->mpWinData->mpFocusWin.get() == this)
    {
        if ( mpWindowImpl->mpSysObj )
        {
            mpWindowImpl->mpFrameData->mpFocusWin = this;
            if ( !mpWindowImpl->mpFrameData->mbInSysObjFocusHdl )
                mpWindowImpl->mpSysObj->GrabFocus();
        }

        if (pSVData->mpWinData->mpFocusWin.get() == this)
        {
            if ( mpWindowImpl->mpCursor )
                mpWindowImpl->mpCursor->ImplShow();
            mpWindowImpl->mbInFocusHdl = true;
            mpWindowImpl->mnGetFocusFlags = nFlags;
            // if we're changing focus due to closing a popup floating window
            // notify the new focus window so it can restore the inner focus
            // eg, toolboxes can select their recent active item
            if( pOldFocusWindow &&
                ! pOldFocusWindow->isDisposed() &&
                ( pOldFocusWindow->GetDialogControlFlags() & DialogControlFlags::FloatWinPopupModeEndCancel ) )
                mpWindowImpl->mnGetFocusFlags |= GetFocusFlags::FloatWinPopupModeEndCancel;
            NotifyEvent aNEvt( NotifyEventType::GETFOCUS, this );
            if ( !ImplCallPreNotify( aNEvt ) && !xWindow->isDisposed() )
                CompatGetFocus();
            if( !xWindow->isDisposed() )
            {
                if (pOldFocusWindow && pOldFocusWindow->isDisposed())
                    pOldFocusWindow = nullptr;
                ImplCallActivateListeners(pOldFocusWindow);
            }
            if( !xWindow->isDisposed() )
            {
                mpWindowImpl->mnGetFocusFlags = GetFocusFlags::NONE;
                mpWindowImpl->mbInFocusHdl = false;
            }
        }
    }

    ImplNewInputContext();

}

void Window::ImplGrabFocusToDocument( GetFocusFlags nFlags )
{
    vcl::Window *pWin = this;
    while( pWin )
    {
        if( !pWin->GetParent() )
        {
            pWin->mpWindowImpl->mpFrame->GrabFocus();
            pWin->ImplGetFrameWindow()->GetWindow( GetWindowType::Client )->ImplGrabFocus(nFlags);
            return;
        }
        pWin = pWin->GetParent();
    }
}

void Window::MouseMove( const MouseEvent& rMEvt )
{
    NotifyEvent aNEvt( NotifyEventType::MOUSEMOVE, this, &rMEvt );
    EventNotify(aNEvt);
}

void Window::MouseButtonDown( const MouseEvent& rMEvt )
{
    NotifyEvent aNEvt( NotifyEventType::MOUSEBUTTONDOWN, this, &rMEvt );
    if (!EventNotify(aNEvt) && mpWindowImpl)
        mpWindowImpl->mbMouseButtonDown = true;
}

void Window::MouseButtonUp( const MouseEvent& rMEvt )
{
    NotifyEvent aNEvt( NotifyEventType::MOUSEBUTTONUP, this, &rMEvt );
    if (!EventNotify(aNEvt) && mpWindowImpl)
        mpWindowImpl->mbMouseButtonUp = true;
}

void Window::SetMouseTransparent( bool bTransparent )
{

    if ( mpWindowImpl->mpBorderWindow )
        mpWindowImpl->mpBorderWindow->SetMouseTransparent( bTransparent );

    if( mpWindowImpl->mpSysObj )
        mpWindowImpl->mpSysObj->SetMouseTransparent( bTransparent );

    mpWindowImpl->mbMouseTransparent = bTransparent;
}

void Window::LocalStartDrag()
{
    ImplGetFrameData()->mbDragging = true;
}

void Window::CaptureMouse()
{
    ImplSVData* pSVData = ImplGetSVData();

    // possibly stop tracking
    if (pSVData->mpWinData->mpTrackWin.get() != this)
    {
        if (pSVData->mpWinData->mpTrackWin)
            pSVData->mpWinData->mpTrackWin->EndTracking(TrackingEventFlags::Cancel);
    }

    if (pSVData->mpWinData->mpCaptureWin.get() != this)
    {
        pSVData->mpWinData->mpCaptureWin = this;
        mpWindowImpl->mpFrame->CaptureMouse( true );
    }
}

void Window::ReleaseMouse()
{
    if (IsMouseCaptured())
    {
        ImplSVData* pSVData = ImplGetSVData();
        pSVData->mpWinData->mpCaptureWin = nullptr;
        if (mpWindowImpl && mpWindowImpl->mpFrame)
            mpWindowImpl->mpFrame->CaptureMouse( false );
        ImplGenerateMouseMove();
    }
}

bool Window::IsMouseCaptured() const
{
    return (this == ImplGetSVData()->mpWinData->mpCaptureWin);
}

void Window::SetPointer( PointerStyle nPointer )
{
    if ( mpWindowImpl->maPointer == nPointer )
        return;

    mpWindowImpl->maPointer   = nPointer;

    // possibly immediately move pointer
    if ( !mpWindowImpl->mpFrameData->mbInMouseMove && ImplTestMousePointerSet() )
        mpWindowImpl->mpFrame->SetPointer( ImplGetMousePointer() );
}

void Window::EnableChildPointerOverwrite( bool bOverwrite )
{

    if ( mpWindowImpl->mbChildPtrOverwrite == bOverwrite )
        return;

    mpWindowImpl->mbChildPtrOverwrite  = bOverwrite;

    // possibly immediately move pointer
    if ( !mpWindowImpl->mpFrameData->mbInMouseMove && ImplTestMousePointerSet() )
        mpWindowImpl->mpFrame->SetPointer( ImplGetMousePointer() );
}

void Window::SetPointerPosPixel( const Point& rPos )
{
    Point aPos = OutputToScreenPixel( rPos );
    const OutputDevice *pOutDev = GetOutDev();
    if( pOutDev->HasMirroredGraphics() )
    {
        if( !IsRTLEnabled() )
        {
            pOutDev->ReMirror( aPos );
        }
        // mirroring is required here, SetPointerPos bypasses SalGraphics
        aPos.setX( GetOutDev()->mpGraphics->mirror2( aPos.X(), *GetOutDev() ) );
    }
    else if( GetOutDev()->ImplIsAntiparallel() )
    {
        pOutDev->ReMirror( aPos );
    }
    mpWindowImpl->mpFrame->SetPointerPos( aPos.X(), aPos.Y() );
}

void Window::SetLastMousePos(const Point& rPos)
{
    // Do this conversion, so when GetPointerPosPixel() calls
    // ScreenToOutputPixel(), we get back the original position.
    Point aPos = OutputToScreenPixel(rPos);
    mpWindowImpl->mpFrameData->mnLastMouseX = aPos.X();
    mpWindowImpl->mpFrameData->mnLastMouseY = aPos.Y();
}

Point Window::GetPointerPosPixel()
{

    Point aPos( mpWindowImpl->mpFrameData->mnLastMouseX, mpWindowImpl->mpFrameData->mnLastMouseY );
    if( GetOutDev()->ImplIsAntiparallel() )
    {
        const OutputDevice *pOutDev = GetOutDev();
        pOutDev->ReMirror( aPos );
    }
    return ScreenToOutputPixel( aPos );
}

Point Window::GetLastPointerPosPixel()
{

    Point aPos( mpWindowImpl->mpFrameData->mnBeforeLastMouseX, mpWindowImpl->mpFrameData->mnBeforeLastMouseY );
    if( GetOutDev()->ImplIsAntiparallel() )
    {
        const OutputDevice *pOutDev = GetOutDev();
        pOutDev->ReMirror( aPos );
    }
    return ScreenToOutputPixel( aPos );
}

void Window::ShowPointer( bool bVisible )
{

    if ( mpWindowImpl->mbNoPtrVisible != !bVisible )
    {
        mpWindowImpl->mbNoPtrVisible = !bVisible;

        // possibly immediately move pointer
        if ( !mpWindowImpl->mpFrameData->mbInMouseMove && ImplTestMousePointerSet() )
            mpWindowImpl->mpFrame->SetPointer( ImplGetMousePointer() );
    }
}

Window::PointerState Window::GetPointerState()
{
    PointerState aState;
    aState.mnState = 0;

    if (mpWindowImpl->mpFrame)
    {
        SalFrame::SalPointerState aSalPointerState = mpWindowImpl->mpFrame->GetPointerState();
        if( GetOutDev()->ImplIsAntiparallel() )
        {
            const OutputDevice *pOutDev = GetOutDev();
            pOutDev->ReMirror( aSalPointerState.maPos );
        }
        aState.maPos = ScreenToOutputPixel( aSalPointerState.maPos );
        aState.mnState = aSalPointerState.mnState;
    }
    return aState;
}

bool Window::IsMouseOver() const
{
    return ImplGetWinData()->mbMouseOver;
}

void Window::EnterWait()
{

    mpWindowImpl->mnWaitCount++;

    if ( mpWindowImpl->mnWaitCount == 1 )
    {
        // possibly immediately move pointer
        if ( !mpWindowImpl->mpFrameData->mbInMouseMove && ImplTestMousePointerSet() )
            mpWindowImpl->mpFrame->SetPointer( ImplGetMousePointer() );
    }
}

void Window::LeaveWait()
{
    if( !mpWindowImpl )
        return;

    if ( mpWindowImpl->mnWaitCount )
    {
        mpWindowImpl->mnWaitCount--;

        if ( !mpWindowImpl->mnWaitCount )
        {
            // possibly immediately move pointer
            if ( !mpWindowImpl->mpFrameData->mbInMouseMove && ImplTestMousePointerSet() )
                mpWindowImpl->mpFrame->SetPointer( ImplGetMousePointer() );
        }
    }
}

bool Window::ImplStopDnd()
{
    bool bRet = false;
    if( mpWindowImpl->mpFrameData && mpWindowImpl->mpFrameData->mxDropTargetListener.is() )
    {
        bRet = true;
        mpWindowImpl->mpFrameData->mxDropTarget.clear();
        mpWindowImpl->mpFrameData->mxDragSource.clear();
        mpWindowImpl->mpFrameData->mxDropTargetListener.clear();
    }

    return bRet;
}

void Window::ImplStartDnd()
{
    GetDropTarget();
}

rtl::Reference<DNDListenerContainer> Window::GetDropTarget()
{
    if( !mpWindowImpl )
        return {};

    if( ! mpWindowImpl->mxDNDListenerContainer.is() )
    {
        sal_Int8 nDefaultActions = 0;

        if( mpWindowImpl->mpFrameData )
        {
            if( ! mpWindowImpl->mpFrameData->mxDropTarget.is() )
            {
                // initialization is done in GetDragSource
                GetDragSource();
            }

            if( mpWindowImpl->mpFrameData->mxDropTarget.is() )
            {
                nDefaultActions = mpWindowImpl->mpFrameData->mxDropTarget->getDefaultActions();

                if( ! mpWindowImpl->mpFrameData->mxDropTargetListener.is() )
                {
                    mpWindowImpl->mpFrameData->mxDropTargetListener = new DNDEventDispatcher( mpWindowImpl->mpFrameWindow );

                    try
                    {
                        mpWindowImpl->mpFrameData->mxDropTarget->addDropTargetListener( mpWindowImpl->mpFrameData->mxDropTargetListener );

                        // register also as drag gesture listener if directly supported by drag source
                        Reference< css::datatransfer::dnd::XDragGestureRecognizer > xDragGestureRecognizer(
                            mpWindowImpl->mpFrameData->mxDragSource, UNO_QUERY);

                        if( xDragGestureRecognizer.is() )
                        {
                            xDragGestureRecognizer->addDragGestureListener(mpWindowImpl->mpFrameData->mxDropTargetListener);
                        }
                        else
                            mpWindowImpl->mpFrameData->mbInternalDragGestureRecognizer = true;

                    }
                    catch (const RuntimeException&)
                    {
                        // release all instances
                        mpWindowImpl->mpFrameData->mxDropTarget.clear();
                        mpWindowImpl->mpFrameData->mxDragSource.clear();
                    }
                }
            }

        }

        mpWindowImpl->mxDNDListenerContainer = new DNDListenerContainer( nDefaultActions );
    }

    // this object is located in the same process, so there will be no runtime exception
    return mpWindowImpl->mxDNDListenerContainer;
}

Reference< css::datatransfer::dnd::XDragSource > Window::GetDragSource()
{
#if HAVE_FEATURE_DESKTOP
    const SystemEnvData* pEnvData = GetSystemData();
    if (!mpWindowImpl->mpFrameData || !pEnvData)
        return Reference<css::datatransfer::dnd::XDragSource>();
    if (mpWindowImpl->mpFrameData->mxDragSource.is())
        return mpWindowImpl->mpFrameData->mxDragSource;

    try
    {
        SalInstance* pInst = ImplGetSVData()->mpDefInst;
        mpWindowImpl->mpFrameData->mxDragSource = pInst->CreateDragSource(*pEnvData);
        mpWindowImpl->mpFrameData->mxDropTarget = pInst->CreateDropTarget(*pEnvData);
    }
    catch (const Exception&)
    {
        mpWindowImpl->mpFrameData->mxDropTarget.clear();
        mpWindowImpl->mpFrameData->mxDragSource.clear();
    }
    return mpWindowImpl->mpFrameData->mxDragSource;
#else
    return Reference< css::datatransfer::dnd::XDragSource > ();
#endif
}

} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
