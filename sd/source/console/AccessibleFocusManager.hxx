/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
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

#include <rtl/ref.hxx>
#include <memory>

class AccessibleObject;

//===== AccessibleFocusManager ================================================

/** A singleton class that makes sure that only one accessibility object in
    the PresenterConsole hierarchy has the focus.
*/
class AccessibleFocusManager
{
public:
    static AccessibleFocusManager & Instance();

    void AddFocusableObject (const ::rtl::Reference<AccessibleObject>& rpObject);
    void RemoveFocusableObject (const ::rtl::Reference<AccessibleObject>& rpObject);

    void FocusObject (const ::rtl::Reference<AccessibleObject>& rpObject);

    ~AccessibleFocusManager();

private:
    static std::unique_ptr<AccessibleFocusManager> mpInstance;
    ::std::vector<rtl::Reference<AccessibleObject> > maFocusableObjects;
    bool m_isInDtor = false;

    AccessibleFocusManager();
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
