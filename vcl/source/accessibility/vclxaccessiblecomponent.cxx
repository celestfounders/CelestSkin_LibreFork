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

#include <com/sun/star/accessibility/AccessibleRole.hpp>
#include <com/sun/star/accessibility/AccessibleStateType.hpp>
#include <com/sun/star/accessibility/AccessibleEventId.hpp>
#include <com/sun/star/accessibility/AccessibleRelationType.hpp>
#include <com/sun/star/lang/IndexOutOfBoundsException.hpp>
#include <comphelper/accessiblecontexthelper.hxx>
#include <cppuhelper/supportsservice.hxx>
#include <i18nlangtag/languagetag.hxx>
#include <vcl/accessibility/vclxaccessiblecomponent.hxx>
#include <vcl/toolkit/dialog.hxx>
#include <vcl/vclevent.hxx>
#include <vcl/window.hxx>
#include <vcl/toolkit/edit.hxx>
#include <vcl/settings.hxx>
#include <vcl/unohelp.hxx>
#include <tools/debug.hxx>
#include <unotools/accessiblerelationsethelper.hxx>
#include <vcl/svapp.hxx>
#include <vcl/menu.hxx>

using namespace ::com::sun::star;
using namespace ::comphelper;

VCLXAccessibleComponent::VCLXAccessibleComponent(vcl::Window* pWindow)
    : m_xWindow(pWindow)
{
    assert(m_xWindow && "VCLXAccessibleComponent - no window!");

    m_xWindow->AddEventListener(LINK(this, VCLXAccessibleComponent, WindowEventListener));
    m_xWindow->AddChildEventListener(LINK(this, VCLXAccessibleComponent, WindowChildEventListener));
}

void VCLXAccessibleComponent::DisconnectEvents()
{
    if (m_xWindow)
    {
        m_xWindow->RemoveEventListener(LINK(this, VCLXAccessibleComponent, WindowEventListener));
        m_xWindow->RemoveChildEventListener(
            LINK(this, VCLXAccessibleComponent, WindowChildEventListener));
        m_xWindow.reset();
    }
}

VCLXAccessibleComponent::~VCLXAccessibleComponent()
{
    ensureDisposed();
    DisconnectEvents();
}

OUString VCLXAccessibleComponent::getImplementationName()
{
    return u"com.sun.star.comp.toolkit.AccessibleWindow"_ustr;
}

sal_Bool VCLXAccessibleComponent::supportsService( const OUString& rServiceName )
{
    return cppu::supportsService(this, rServiceName);
}

uno::Sequence< OUString > VCLXAccessibleComponent::getSupportedServiceNames()
{
    uno::Sequence< OUString > aNames { u"com.sun.star.awt.AccessibleWindow"_ustr };
    return aNames;
}

IMPL_LINK( VCLXAccessibleComponent, WindowEventListener, VclWindowEvent&, rEvent, void )
{
    /* Ignore VclEventId::WindowEndPopupMode, because the UNO accessibility wrapper
     * might have been destroyed by the previous VCLEventListener (if no AT tool
     * is running), e.g. sub-toolbars in impress.
     */
    if (m_xWindow && (rEvent.GetId() != VclEventId::WindowEndPopupMode))
    {
        DBG_ASSERT( rEvent.GetWindow(), "Window???" );
        if( !rEvent.GetWindow()->IsAccessibilityEventsSuppressed() || ( rEvent.GetId() == VclEventId::ObjectDying ) )
        {
            ProcessWindowEvent( rEvent );
        }
    }
}

IMPL_LINK( VCLXAccessibleComponent, WindowChildEventListener, VclWindowEvent&, rEvent, void )
{
    if (m_xWindow)
    {
        DBG_ASSERT( rEvent.GetWindow(), "Window???" );
        if( !rEvent.GetWindow()->IsAccessibilityEventsSuppressed() )
        {
            // #103087# to prevent an early release of the component
            uno::Reference< accessibility::XAccessibleContext > xHoldAlive = this;

            ProcessWindowChildEvent( rEvent );
        }
    }
}

uno::Reference< accessibility::XAccessible > VCLXAccessibleComponent::GetChildAccessible( const VclWindowEvent& rVclWindowEvent )
{
    // checks if the data in the window event is our direct child
    // and returns its accessible

    // MT: Change this later, normally a show/hide event shouldn't have the vcl::Window* in pData.
    vcl::Window* pChildWindow = static_cast<vcl::Window *>(rVclWindowEvent.GetData());
    // tdf#141101/tdf#156561 Handle the event if this is either the a11y parent or the
    // vcl::Window parent, since child events are sent for the vcl::Window hierarchy
    // (s. Window::CallEventListeners) and e.g. DockingManager does manual partial reparenting
    // that would cause child events to not be forwarded to the a11y level when
    // not taking GetParent() into account here
    if (pChildWindow && (GetWindow() == pChildWindow->GetAccessibleParentWindow()
                         || GetWindow() == pChildWindow->GetParent()))
        return pChildWindow->GetAccessible( rVclWindowEvent.GetId() == VclEventId::WindowShow );
    else
        return uno::Reference< accessibility::XAccessible > ();
}

void VCLXAccessibleComponent::ProcessWindowChildEvent( const VclWindowEvent& rVclWindowEvent )
{
    uno::Any aOldValue, aNewValue;
    uno::Reference< accessibility::XAccessible > xAcc;

    switch ( rVclWindowEvent.GetId() )
    {
        case VclEventId::WindowShow:  // send create on show for direct accessible children
        {
            xAcc = GetChildAccessible( rVclWindowEvent );
            if( xAcc.is() )
            {
                aNewValue <<= xAcc;
                NotifyAccessibleEvent( accessibility::AccessibleEventId::CHILD, aOldValue, aNewValue );

                // CHILD event above results in a11y event listeners getting registered,
                // so send state change event for SHOWING event after that
                uno::Reference<XAccessibleContext> xChildContext = xAcc->getAccessibleContext();
                if (xChildContext.is())
                {
                    VCLXAccessibleComponent* pChildComponent = dynamic_cast<VCLXAccessibleComponent*>(xChildContext.get());
                    if (pChildComponent)
                    {
                        css::uno::Any aNewStateValue;
                        aNewStateValue <<= accessibility::AccessibleStateType::SHOWING;
                        pChildComponent->NotifyAccessibleEvent(accessibility::AccessibleEventId::STATE_CHANGED, css::uno::Any(), aNewStateValue);
                    }
                }
            }
        }
        break;
        case VclEventId::WindowHide:  // send destroy on hide for direct accessible children
        {
            xAcc = GetChildAccessible( rVclWindowEvent );
            if( xAcc.is() )
            {
                // send state change event for SHOWING before sending the CHILD event below,
                // since that one results in a11y event listeners getting removed
                uno::Reference<XAccessibleContext> xChildContext = xAcc->getAccessibleContext();
                if (xChildContext.is())
                {
                    VCLXAccessibleComponent* pChildComponent = dynamic_cast<VCLXAccessibleComponent*>(xChildContext.get());
                    if (pChildComponent)
                    {
                        css::uno::Any aOldStateValue;
                        aOldStateValue <<= accessibility::AccessibleStateType::SHOWING;
                        pChildComponent->NotifyAccessibleEvent(accessibility::AccessibleEventId::STATE_CHANGED, aOldStateValue, css::uno::Any());
                    }
                }

                aOldValue <<= xAcc;
                NotifyAccessibleEvent( accessibility::AccessibleEventId::CHILD, aOldValue, aNewValue );
            }
        }
        break;
        default: break;
    }
}

void VCLXAccessibleComponent::ProcessWindowEvent( const VclWindowEvent& rVclWindowEvent )
{
    uno::Any aOldValue, aNewValue;

    vcl::Window* pAccWindow = rVclWindowEvent.GetWindow();
    assert(pAccWindow && "VCLXAccessibleComponent::ProcessWindowEvent - Window?");

    switch ( rVclWindowEvent.GetId() )
    {
        case VclEventId::ObjectDying:
        {
            DisconnectEvents();
        }
        break;
        case VclEventId::WindowChildDestroyed:
        {
            vcl::Window* pWindow = static_cast<vcl::Window*>(rVclWindowEvent.GetData());
            DBG_ASSERT( pWindow, "VclEventId::WindowChildDestroyed - Window=?" );
            if ( pWindow->GetAccessible( false ).is() )
            {
                aOldValue <<= css::uno::Reference<accessibility::XAccessible>(
                    pWindow->GetAccessible(false));
                NotifyAccessibleEvent( accessibility::AccessibleEventId::CHILD, aOldValue, aNewValue );
            }
        }
        break;
        case VclEventId::WindowActivate:
        {
            sal_Int16 aAccessibleRole = getAccessibleRole();
            // avoid notification if a child frame is already active
            // only one frame may be active at a given time
            if ( !pAccWindow->HasActiveChildFrame() &&
                 ( aAccessibleRole == accessibility::AccessibleRole::FRAME ||
                   aAccessibleRole == accessibility::AccessibleRole::ALERT ||
                   aAccessibleRole == accessibility::AccessibleRole::DIALOG ) )  // #i18891#
            {
                aNewValue <<= accessibility::AccessibleStateType::ACTIVE;
                NotifyAccessibleEvent( accessibility::AccessibleEventId::STATE_CHANGED, aOldValue, aNewValue );
            }
        }
        break;
        case VclEventId::WindowDeactivate:
        {
            sal_Int16 aAccessibleRole = getAccessibleRole();
            if ( aAccessibleRole == accessibility::AccessibleRole::FRAME ||
                 aAccessibleRole == accessibility::AccessibleRole::ALERT ||
                 aAccessibleRole == accessibility::AccessibleRole::DIALOG )  // #i18891#
            {
                aOldValue <<= accessibility::AccessibleStateType::ACTIVE;
                NotifyAccessibleEvent( accessibility::AccessibleEventId::STATE_CHANGED, aOldValue, aNewValue );
            }
        }
        break;
        case VclEventId::WindowGetFocus:
        case VclEventId::ControlGetFocus:
        {
            if( (pAccWindow->IsCompoundControl() && rVclWindowEvent.GetId() == VclEventId::ControlGetFocus) ||
                (!pAccWindow->IsCompoundControl() && rVclWindowEvent.GetId() == VclEventId::WindowGetFocus) )
            {
                // if multiple listeners were registered it is possible that the
                // focus was changed during event processing (eg SfxTopWindow )
                // #106082# allow ChildPathFocus only for CompoundControls, for windows the focus must be in the window itself
                if( (pAccWindow->IsCompoundControl() && pAccWindow->HasChildPathFocus()) ||
                    (!pAccWindow->IsCompoundControl() && pAccWindow->HasFocus()) )
                {
                    aNewValue <<= accessibility::AccessibleStateType::FOCUSED;
                    NotifyAccessibleEvent( accessibility::AccessibleEventId::STATE_CHANGED, aOldValue, aNewValue );
                }
            }
        }
        break;
        case VclEventId::WindowLoseFocus:
        case VclEventId::ControlLoseFocus:
        {
            if( (pAccWindow->IsCompoundControl() && rVclWindowEvent.GetId() == VclEventId::ControlLoseFocus) ||
                (!pAccWindow->IsCompoundControl() && rVclWindowEvent.GetId() == VclEventId::WindowLoseFocus) )
            {
                aOldValue <<= accessibility::AccessibleStateType::FOCUSED;
                NotifyAccessibleEvent( accessibility::AccessibleEventId::STATE_CHANGED, aOldValue, aNewValue );
            }
        }
        break;
        case VclEventId::WindowFrameTitleChanged:
        {
            OUString aOldName( *static_cast<OUString*>(rVclWindowEvent.GetData()) );
            OUString aNewName( getAccessibleName() );
            aOldValue <<= aOldName;
            aNewValue <<= aNewName;
            NotifyAccessibleEvent( accessibility::AccessibleEventId::NAME_CHANGED, aOldValue, aNewValue );
        }
        break;
        case VclEventId::WindowEnabled:
        {
            aNewValue <<= accessibility::AccessibleStateType::ENABLED;
            NotifyAccessibleEvent( accessibility::AccessibleEventId::STATE_CHANGED, aOldValue, aNewValue );
            aNewValue <<= accessibility::AccessibleStateType::SENSITIVE;
            NotifyAccessibleEvent( accessibility::AccessibleEventId::STATE_CHANGED, aOldValue, aNewValue );
        }
        break;
        case VclEventId::WindowDisabled:
        {
            aOldValue <<= accessibility::AccessibleStateType::SENSITIVE;
            NotifyAccessibleEvent( accessibility::AccessibleEventId::STATE_CHANGED, aOldValue, aNewValue );

            aOldValue <<= accessibility::AccessibleStateType::ENABLED;
            NotifyAccessibleEvent( accessibility::AccessibleEventId::STATE_CHANGED, aOldValue, aNewValue );
        }
        break;
        case VclEventId::WindowMove:
        case VclEventId::WindowResize:
        {
            NotifyAccessibleEvent( accessibility::AccessibleEventId::BOUNDRECT_CHANGED, aOldValue, aNewValue );
        }
        break;
        case VclEventId::WindowMenubarAdded:
        {
            MenuBar* pMenuBar = static_cast<MenuBar*>(rVclWindowEvent.GetData());
            if ( pMenuBar )
            {
                uno::Reference< accessibility::XAccessible > xChild( pMenuBar->GetAccessible() );
                if ( xChild.is() )
                {
                    aNewValue <<= xChild;
                    NotifyAccessibleEvent( accessibility::AccessibleEventId::CHILD, aOldValue, aNewValue );
                }
            }
        }
        break;
        case VclEventId::WindowMenubarRemoved:
        {
            MenuBar* pMenuBar = static_cast<MenuBar*>(rVclWindowEvent.GetData());
            if ( pMenuBar )
            {
                uno::Reference< accessibility::XAccessible > xChild( pMenuBar->GetAccessible() );
                if ( xChild.is() )
                {
                    aOldValue <<= xChild;
                    NotifyAccessibleEvent( accessibility::AccessibleEventId::CHILD, aOldValue, aNewValue );
                }
            }
        }
        break;
        case VclEventId::WindowMinimize:
        {
            aNewValue <<= accessibility::AccessibleStateType::ICONIFIED;
            NotifyAccessibleEvent( accessibility::AccessibleEventId::STATE_CHANGED, aOldValue, aNewValue );
        }
        break;
        case VclEventId::WindowNormalize:
        {
            aOldValue <<= accessibility::AccessibleStateType::ICONIFIED;
            NotifyAccessibleEvent( accessibility::AccessibleEventId::STATE_CHANGED, aOldValue, aNewValue );
        }
        break;
        case VclEventId::WindowHide:
        case VclEventId::WindowShow:
        // WindowHide and WindowShow are handled in ProcessWindowChildEvent so the right order
        // regarding the CHILD event can be taken into account
        default:
        {
        }
        break;
    }
}

void VCLXAccessibleComponent::disposing()
{
    DisconnectEvents();

    OAccessible::disposing();
}

vcl::Window* VCLXAccessibleComponent::GetWindow() const { return m_xWindow; }

void VCLXAccessibleComponent::FillAccessibleRelationSet( utl::AccessibleRelationSetHelper& rRelationSet )
{
    VclPtr<vcl::Window> pWindow = GetWindow();
    if ( !pWindow )
        return;

    vcl::Window *pLabeledBy = pWindow->GetAccessibleRelationLabeledBy();
    if ( pLabeledBy && pLabeledBy != pWindow )
    {
        uno::Sequence<uno::Reference<css::accessibility::XAccessible>> aSequence { pLabeledBy->GetAccessible() };
        rRelationSet.AddRelation(accessibility::AccessibleRelation(accessibility::AccessibleRelationType_LABELED_BY, aSequence));
    }

    vcl::Window* pLabelFor = pWindow->GetAccessibleRelationLabelFor();
    if ( pLabelFor && pLabelFor != pWindow )
    {
        uno::Sequence<uno::Reference<css::accessibility::XAccessible>> aSequence { pLabelFor->GetAccessible() };
        rRelationSet.AddRelation(accessibility::AccessibleRelation(accessibility::AccessibleRelationType_LABEL_FOR, aSequence));
    }

    vcl::Window* pMemberOf = pWindow->GetAccessibleRelationMemberOf();
    if ( pMemberOf && pMemberOf != pWindow )
    {
        uno::Sequence<uno::Reference<css::accessibility::XAccessible>> aSequence { pMemberOf->GetAccessible() };
        rRelationSet.AddRelation(accessibility::AccessibleRelation(accessibility::AccessibleRelationType_MEMBER_OF, aSequence));
    }
}

void VCLXAccessibleComponent::FillAccessibleStateSet( sal_Int64& rStateSet )
{
    VclPtr<vcl::Window> pWindow = GetWindow();
    if ( pWindow )
    {
        if ( pWindow->IsVisible() )
        {
            rStateSet |= accessibility::AccessibleStateType::VISIBLE;
            rStateSet |= accessibility::AccessibleStateType::SHOWING;
        }
        else
        {
            rStateSet |= accessibility::AccessibleStateType::INVALID;
        }

        if ( pWindow->IsEnabled() )
        {
            rStateSet |= accessibility::AccessibleStateType::ENABLED;
            rStateSet |= accessibility::AccessibleStateType::SENSITIVE;
        }

        if ( pWindow->HasChildPathFocus() &&
             ( getAccessibleRole() == accessibility::AccessibleRole::FRAME ||
               getAccessibleRole() == accessibility::AccessibleRole::ALERT ||
               getAccessibleRole() == accessibility::AccessibleRole::DIALOG ) )  // #i18891#
            rStateSet |= accessibility::AccessibleStateType::ACTIVE;

        if ( pWindow->HasFocus() || ( pWindow->IsCompoundControl() && pWindow->HasChildPathFocus() ) )
            rStateSet |= accessibility::AccessibleStateType::FOCUSED;

        if ( pWindow->IsWait() )
            rStateSet |= accessibility::AccessibleStateType::BUSY;

        if ( pWindow->GetStyle() & WB_SIZEABLE )
            rStateSet |= accessibility::AccessibleStateType::RESIZABLE;
        // 6. frame doesn't have MOVABLE state
        // 10. for password text, where is the sensitive state?
        if( ( getAccessibleRole() == accessibility::AccessibleRole::FRAME ||getAccessibleRole() == accessibility::AccessibleRole::DIALOG )&& pWindow->GetStyle() & WB_MOVEABLE )
            rStateSet |= accessibility::AccessibleStateType::MOVEABLE;
        if( pWindow->IsDialog() )
        {
            Dialog *pDlg = static_cast< Dialog* >( pWindow.get() );
            if( pDlg->IsInExecute() )
                rStateSet |= accessibility::AccessibleStateType::MODAL;
        }
        //If a combobox or list's edit child isn't read-only,EDITABLE state
        //should be set.
        if( pWindow && pWindow->GetType() == WindowType::COMBOBOX )
        {
            if( !( pWindow->GetStyle() & WB_READONLY) ||
                !static_cast<Edit*>(pWindow.get())->IsReadOnly() )
                    rStateSet |= accessibility::AccessibleStateType::EDITABLE;
        }

        VclPtr<vcl::Window> pChild = pWindow->GetWindow( GetWindowType::FirstChild );

        while( pWindow && pChild )
        {
            VclPtr<vcl::Window> pWinTemp = pChild->GetWindow( GetWindowType::FirstChild );
            if( pWinTemp && pWinTemp->GetType() == WindowType::EDIT )
            {
                if( !( pWinTemp->GetStyle() & WB_READONLY) ||
                    !static_cast<Edit*>(pWinTemp.get())->IsReadOnly() )
                    rStateSet |= accessibility::AccessibleStateType::EDITABLE;
                break;
            }
            if( pChild->GetType() == WindowType::EDIT )
            {
                if( !( pChild->GetStyle() & WB_READONLY) ||
                    !static_cast<Edit*>(pChild.get())->IsReadOnly())
                    rStateSet |= accessibility::AccessibleStateType::EDITABLE;
                break;
            }
            pChild = pChild->GetWindow( GetWindowType::Next );
        }
    }
    else
    {
        rStateSet |= accessibility::AccessibleStateType::DEFUNC;
    }

/*

MUST BE SET FROM DERIVED CLASSES:

CHECKABLE
CHECKED
COLLAPSED
EXPANDED
EXPANDABLE
EDITABLE
FOCUSABLE
HORIZONTAL
VERTICAL
ICONIFIED
MULTILINE
MULTI_SELECTABLE
PRESSED
SELECTABLE
SELECTED
SINGLE_LINE
TRANSIENT

    */
}

// accessibility::XAccessibleContext
sal_Int64 VCLXAccessibleComponent::getAccessibleChildCount()
{
    OExternalLockGuard aGuard( this );

    sal_Int64 nChildren = 0;
    if ( GetWindow() )
        nChildren = GetWindow()->GetAccessibleChildWindowCount();

    return nChildren;
}

uno::Reference< accessibility::XAccessible > VCLXAccessibleComponent::getAccessibleChild( sal_Int64 i )
{
    OExternalLockGuard aGuard( this );

    if ( i >= getAccessibleChildCount() )
        throw lang::IndexOutOfBoundsException();

    uno::Reference< accessibility::XAccessible > xAcc;
    if ( GetWindow() )
    {
        vcl::Window* pChild = GetWindow()->GetAccessibleChildWindow( static_cast<sal_uInt16>(i) );
        if ( pChild )
            xAcc = pChild->GetAccessible();
    }

    return xAcc;
}

uno::Reference< accessibility::XAccessible > VCLXAccessibleComponent::getAccessibleParent(  )
{
    OExternalLockGuard aGuard( this );

    uno::Reference< accessibility::XAccessible > xAcc;
    if ( GetWindow() )
        xAcc = GetWindow()->GetAccessibleParent();
    return xAcc;
}

sal_Int16 VCLXAccessibleComponent::getAccessibleRole(  )
{
    OExternalLockGuard aGuard( this );

    sal_Int16 nRole = 0;

    if ( GetWindow() )
        nRole = GetWindow()->GetAccessibleRole();

    return nRole;
}

OUString VCLXAccessibleComponent::getAccessibleDescription(  )
{
    OExternalLockGuard aGuard( this );

    OUString aDescription;

    if ( GetWindow() )
        aDescription = GetWindow()->GetAccessibleDescription();

    return aDescription;
}

OUString VCLXAccessibleComponent::getAccessibleName(  )
{
    OExternalLockGuard aGuard( this );

    OUString aName;
    if ( GetWindow() )
    {
        aName = GetWindow()->GetAccessibleName();
#if OSL_DEBUG_LEVEL > 0
        // append window type to accessible name for debugging purposes
        // if LIBO_APPEND_WINDOW_TYPE_TO_ACCESSIBLE_NAME environment variable is set
        static const char* pEnvAppendType = getenv("LIBO_APPEND_WINDOW_TYPE_TO_ACCESSIBLE_NAME");
        if (pEnvAppendType && OUString::createFromAscii(pEnvAppendType) != u"0")
            aName += " (Type = " + OUString::number(static_cast<sal_Int32>(GetWindow()->GetType())) + ")";
#endif
    }
    return aName;
}

OUString VCLXAccessibleComponent::getAccessibleId(  )
{
    OExternalLockGuard aGuard( this );

    OUString aId;
    if ( GetWindow() )
    {
        const OUString &aWindowId = GetWindow()->get_id();
        aId = aWindowId;
    }
    return aId;
}

uno::Reference< accessibility::XAccessibleRelationSet > VCLXAccessibleComponent::getAccessibleRelationSet(  )
{
    OExternalLockGuard aGuard( this );

    rtl::Reference<utl::AccessibleRelationSetHelper> pRelationSetHelper = new utl::AccessibleRelationSetHelper;
    FillAccessibleRelationSet( *pRelationSetHelper );
    return pRelationSetHelper;
}

sal_Int64 VCLXAccessibleComponent::getAccessibleStateSet(  )
{
    OExternalLockGuard aGuard( this );

    sal_Int64 nStateSet = 0;
    FillAccessibleStateSet( nStateSet );
    return nStateSet;
}

lang::Locale VCLXAccessibleComponent::getLocale()
{
    OExternalLockGuard aGuard( this );

    return Application::GetSettings().GetLanguageTag().getLocale();
}

uno::Reference< accessibility::XAccessible > VCLXAccessibleComponent::getAccessibleAtPoint( const awt::Point& rPoint )
{
    OExternalLockGuard aGuard( this );

    uno::Reference< accessibility::XAccessible > xChild;
    for ( sal_Int64 i = 0, nCount = getAccessibleChildCount(); i < nCount; ++i )
    {
        uno::Reference< accessibility::XAccessible > xAcc = getAccessibleChild( i );
        if ( xAcc.is() )
        {
            uno::Reference< accessibility::XAccessibleComponent > xComp( xAcc->getAccessibleContext(), uno::UNO_QUERY );
            if ( xComp.is() )
            {
                tools::Rectangle aRect = vcl::unohelper::ConvertToVCLRect(xComp->getBounds());
                Point aPos = vcl::unohelper::ConvertToVCLPoint(rPoint);
                if ( aRect.Contains( aPos ) )
                {
                    xChild = std::move(xAcc);
                    break;
                }
            }
        }
    }

    return xChild;
}

// accessibility::XAccessibleComponent
awt::Rectangle VCLXAccessibleComponent::implGetBounds()
{
    VclPtr<vcl::Window> pWindow = GetWindow();
    if (!pWindow)
        return awt::Rectangle();

    AbsoluteScreenPixelRectangle aRect = pWindow->GetWindowExtentsAbsolute();
    awt::Rectangle aBounds = vcl::unohelper::ConvertToAWTRect(aRect);

    css::uno::Reference<css::accessibility::XAccessible> xParent = pWindow->GetAccessibleParent();
    if (!xParent.is())
        return aBounds;

    css::uno::Reference<css::accessibility::XAccessibleComponent> xParentComponent(
        xParent->getAccessibleContext(), css::uno::UNO_QUERY);
    if (!xParentComponent)
        return aBounds;

    awt::Point aParentScreenLoc = xParentComponent->getLocationOnScreen();
    aBounds.X -= aParentScreenLoc.X;
    aBounds.Y -= aParentScreenLoc.Y;

    return aBounds;
}

awt::Point VCLXAccessibleComponent::getLocationOnScreen(  )
{
    OExternalLockGuard aGuard( this );

    awt::Point aPos;
    if ( GetWindow() )
    {
        AbsoluteScreenPixelRectangle aRect = GetWindow()->GetWindowExtentsAbsolute();
        aPos.X = aRect.Left();
        aPos.Y = aRect.Top();
    }

    return aPos;
}

void VCLXAccessibleComponent::grabFocus(  )
{
    OExternalLockGuard aGuard( this );

    sal_Int64 nStates = getAccessibleStateSet();
    if (m_xWindow && (nStates & accessibility::AccessibleStateType::FOCUSABLE))
        m_xWindow->GrabFocus();
}

sal_Int32 SAL_CALL VCLXAccessibleComponent::getForeground(  )
{
    OExternalLockGuard aGuard( this );

    Color nColor;
    VclPtr<vcl::Window> pWindow = GetWindow();
    if ( pWindow )
    {
        if ( pWindow->IsControlForeground() )
            nColor = pWindow->GetControlForeground();
        else
        {
            vcl::Font aFont;
            if ( pWindow->IsControlFont() )
                aFont = pWindow->GetControlFont();
            else
                aFont = pWindow->GetFont();
            nColor = aFont.GetColor();
            // COL_AUTO is not very meaningful for AT
            if ( nColor == COL_AUTO)
                nColor = pWindow->GetTextColor();
        }
    }

    return sal_Int32(nColor);
}

sal_Int32 SAL_CALL VCLXAccessibleComponent::getBackground(  )
{
    OExternalLockGuard aGuard( this );

    Color nColor;
    VclPtr<vcl::Window> pWindow = GetWindow();
    if ( pWindow )
    {
        if ( pWindow->IsControlBackground() )
            nColor = pWindow->GetControlBackground();
        else
            nColor = pWindow->GetBackground().GetColor();
    }

    return sal_Int32(nColor);
}

// XAccessibleExtendedComponent

OUString SAL_CALL VCLXAccessibleComponent::getTitledBorderText(  )
{
    OExternalLockGuard aGuard( this );

    OUString sRet;
    if ( GetWindow() )
        sRet = GetWindow()->GetText();

    return sRet;
}

OUString SAL_CALL VCLXAccessibleComponent::getToolTipText(  )
{
    OExternalLockGuard aGuard( this );

    OUString sRet;
    if ( GetWindow() )
        sRet = GetWindow()->GetQuickHelpText();

    return sRet;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
