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

#pragma once

#include "accframebase.hxx"

#include <com/sun/star/accessibility/XAccessibleSelection.hpp>

class SwFlyFrame;

class SwAccessibleTextFrame : public SwAccessibleFrameBase,
        public css::accessibility::XAccessibleSelection
{
private:
    // #i73249#
    OUString msTitle;
    OUString msDesc;

protected:
    virtual ~SwAccessibleTextFrame() override;
    virtual void Notify(const SfxHint&) override;

public:
    SwAccessibleTextFrame(std::shared_ptr<SwAccessibleMap> const& pInitMap,
                          const SwFlyFrame& rFlyFrame);

    virtual css::uno::Any SAL_CALL queryInterface(
        css::uno::Type const & rType ) override;
    virtual void SAL_CALL acquire() noexcept override;
    virtual void SAL_CALL release() noexcept override;
    // XAccessibleSelection
    virtual void SAL_CALL selectAccessibleChild(
        sal_Int64 nChildIndex ) override;

    virtual sal_Bool SAL_CALL isAccessibleChildSelected(
        sal_Int64 nChildIndex ) override;

    virtual void SAL_CALL clearAccessibleSelection(  ) override;

    virtual void SAL_CALL selectAllAccessibleChildren(  ) override;

    virtual sal_Int64 SAL_CALL getSelectedAccessibleChildCount(  ) override;

    virtual css::uno::Reference< css::accessibility::XAccessible > SAL_CALL getSelectedAccessibleChild(
        sal_Int64 nSelectedChildIndex ) override;

    virtual void SAL_CALL deselectAccessibleChild(
        sal_Int64 nSelectedChildIndex ) override;

    // XAccessibleContext

    // #i73249# - Return the object's current name.
    virtual OUString SAL_CALL
        getAccessibleName() override;
    /// Return this object's description.
    virtual OUString SAL_CALL
        getAccessibleDescription() override;

    // XAccessibleContext::getAccessibleRelationSet

    // text frame may have accessible relations to their
    // predecessor/successor frames

private:
    // helper methods for getAccessibleRelationSet:
    SwFlyFrame* getFlyFrame() const;

    css::accessibility::AccessibleRelation makeRelation(
        css::accessibility::AccessibleRelationType eType, const SwFlyFrame* pFrame);

public:
    virtual css::uno::Reference< css::accessibility::XAccessibleRelationSet> SAL_CALL getAccessibleRelationSet() override;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
