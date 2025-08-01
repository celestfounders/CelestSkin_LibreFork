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
#ifndef INCLUDED_SW_INC_UNOREDLINE_HXX
#define INCLUDED_SW_INC_UNOREDLINE_HXX

#include <svl/listener.hxx>
#include <com/sun/star/container/XEnumerationAccess.hpp>
#include "unotext.hxx"
#include "ndindex.hxx"

class SwRangeRedline;

/**
 * SwXRedlineText provides an XText which may be used to write
 * directly into a redline node. It got implemented to enable XML
 * import of redlines and should not be used directly via the API.
 */
class SwXRedlineText final :
    public SwXText,
    public cppu::OWeakObject,
    public css::container::XEnumerationAccess
{
    SwNodeIndex m_aNodeIndex;
    virtual const SwStartNode *GetStartNode() const override;

public:
    SwXRedlineText(SwDoc* pDoc, const SwNodeIndex& aNodeIndex);

    virtual     css::uno::Any SAL_CALL queryInterface( const css::uno::Type& aType ) override;
    virtual void SAL_CALL acquire(  ) noexcept override {OWeakObject::acquire();}
    virtual void SAL_CALL release(  ) noexcept override {OWeakObject::release();}

    virtual css::uno::Sequence< css::uno::Type > SAL_CALL getTypes(  ) override;
    virtual css::uno::Sequence< sal_Int8 > SAL_CALL getImplementationId(  ) override;

    //XText
    virtual rtl::Reference< SwXTextCursor > createXTextCursor() override;
    virtual rtl::Reference< SwXTextCursor > createXTextCursorByRange(
            const ::css::uno::Reference< ::css::text::XTextRange >& aTextPosition ) override;

    //XEnumerationAccess
    virtual css::uno::Reference< css::container::XEnumeration >  SAL_CALL createEnumeration() override;

    //XElementAccess (via XEnumerationAccess)
    virtual css::uno::Type SAL_CALL getElementType(  ) override;
    virtual sal_Bool SAL_CALL hasElements(  ) override;
};

typedef cppu::WeakImplHelper<css::container::XEnumerationAccess> SwXRedlineBaseClass;

class SwXRedline final
    : public SwXRedlineBaseClass
    , public SwXText
    , public SvtListener
{
    SwRangeRedline*  m_pRedline;
public:
    SwXRedline(SwRangeRedline& rRedline);
    virtual ~SwXRedline() override;


    virtual     css::uno::Any SAL_CALL queryInterface( const css::uno::Type& aType ) override;
    virtual void SAL_CALL acquire(  ) noexcept override {OWeakObject::acquire();}
    virtual void SAL_CALL release(  ) noexcept override {OWeakObject::release();}

    virtual css::uno::Sequence< css::uno::Type > SAL_CALL getTypes(  ) override;
    virtual css::uno::Sequence< sal_Int8 > SAL_CALL getImplementationId(  ) override;

    //XText
    virtual rtl::Reference< SwXTextCursor > createXTextCursor() override;
    virtual rtl::Reference< SwXTextCursor > createXTextCursorByRange(
            const ::css::uno::Reference< ::css::text::XTextRange >& aTextPosition ) override;

    //XPropertySet
    virtual css::uno::Reference< css::beans::XPropertySetInfo > SAL_CALL getPropertySetInfo(  ) override;
    virtual void SAL_CALL setPropertyValue( const OUString& aPropertyName, const css::uno::Any& aValue ) override;
    virtual css::uno::Any SAL_CALL getPropertyValue( const OUString& PropertyName ) override;
    virtual void SAL_CALL addPropertyChangeListener( const OUString& aPropertyName, const css::uno::Reference< css::beans::XPropertyChangeListener >& xListener ) override;
    virtual void SAL_CALL removePropertyChangeListener( const OUString& aPropertyName, const css::uno::Reference< css::beans::XPropertyChangeListener >& aListener ) override;
    virtual void SAL_CALL addVetoableChangeListener( const OUString& PropertyName, const css::uno::Reference< css::beans::XVetoableChangeListener >& aListener ) override;
    virtual void SAL_CALL removeVetoableChangeListener( const OUString& PropertyName, const css::uno::Reference< css::beans::XVetoableChangeListener >& aListener ) override;

    //XEnumerationAccess
    virtual css::uno::Reference< css::container::XEnumeration >  SAL_CALL createEnumeration() override;

    //XElementAccess
    virtual css::uno::Type SAL_CALL getElementType(  ) override;
    virtual sal_Bool SAL_CALL hasElements(  ) override;

    const SwRangeRedline*    GetRedline() const {return m_pRedline;}
    virtual void Notify( const SfxHint& ) override;
};

namespace sw
{
    struct SW_DLLPUBLIC FindRedlineHint final: SfxHint
    {
        const SwRangeRedline& m_rRedline;
        SwXRedline** m_ppXRedline;
        FindRedlineHint(const SwRangeRedline& rRedline, SwXRedline** ppXRedline)
            : SfxHint(SfxHintId::SwFindRedline), m_rRedline(rRedline), m_ppXRedline(ppXRedline) {}
    };
}

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
