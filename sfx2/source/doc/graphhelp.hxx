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

#ifndef INCLUDED_SFX2_SOURCE_DOC_GRAPHHELP_HXX
#define INCLUDED_SFX2_SOURCE_DOC_GRAPHHELP_HXX

#include <sal/config.h>

#include <string_view>

#include <com/sun/star/uno/Reference.hxx>
#include <com/sun/star/io/XStream.hpp>
#include <rtl/ustring.hxx>
#include <tools/gen.hxx>

class SvMemoryStream;
class GDIMetaFile;
class BitmapEx;
enum class ConvertDataFormat;

class GraphicHelper
{
public:

    static std::unique_ptr<SvMemoryStream> getFormatStrFromGDI_Impl( const GDIMetaFile* pGDIMeta, ConvertDataFormat nFormat );

    static void* getEnhMetaFileFromGDI_Impl( const GDIMetaFile* pGDIMeta );

    static void* getWinMetaFileFromGDI_Impl( const GDIMetaFile* pGDIMeta, const Size& aMetaSize );

    static bool supportsMetaFileHandle_Impl()
    {
#ifdef _WIN32
    return true;
#else
    return false;
#endif
    }

    static bool getThumbnailFormatFromBitmap_Impl(
            const Bitmap& rBitmap,
            const css::uno::Reference< css::io::XStream >& xStream );

    static OUString getThumbnailReplacementIDByFactoryName_Impl(
        std::u16string_view aFactoryShortName);

    static bool getThumbnailReplacement_Impl(
            std::u16string_view rResID,
            const css::uno::Reference< css::io::XStream >& xStream );

};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
