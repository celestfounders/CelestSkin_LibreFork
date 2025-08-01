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

#include <config_folders.h>
#include <config_features.h>

#include <dp_misc.h>
#include <dp_interact.h>
#include <dp_shared.hxx>
#include <o3tl/string_view.hxx>
#include <rtl/uri.hxx>
#include <rtl/digest.h>
#include <rtl/random.h>
#include <rtl/bootstrap.hxx>
#include <rtl/ustrbuf.hxx>
#include <sal/log.hxx>
#include <unotools/bootstrap.hxx>
#include <osl/diagnose.h>
#include <osl/file.hxx>
#include <osl/pipe.hxx>
#include <osl/security.hxx>
#include <osl/thread.hxx>
#include <com/sun/star/ucb/CommandAbortedException.hpp>
#include <com/sun/star/task/XInteractionHandler.hpp>
#include <com/sun/star/bridge/BridgeFactory.hpp>
#include <com/sun/star/bridge/UnoUrlResolver.hpp>
#include <com/sun/star/bridge/XUnoUrlResolver.hpp>
#include <com/sun/star/deployment/ExtensionManager.hpp>
#include <com/sun/star/lang/DisposedException.hpp>
#include <com/sun/star/task/OfficeRestartManager.hpp>
#include <atomic>
#include <memory>
#include <string_view>
#include <thread>
#include <comphelper/lok.hxx>
#include <comphelper/processfactory.hxx>
#include <salhelper/linkhelper.hxx>

using namespace ::com::sun::star;
using namespace ::com::sun::star::uno;

namespace dp_misc {
namespace {

std::shared_ptr<rtl::Bootstrap> & UnoRc()
{
    static std::shared_ptr<rtl::Bootstrap> theRc = []()
        {
            OUString unorc( u"$BRAND_BASE_DIR/" LIBO_ETC_FOLDER "/" SAL_CONFIGFILE("louno") ""_ustr );
            ::rtl::Bootstrap::expandMacros( unorc );
            auto ret = std::make_shared<::rtl::Bootstrap>( unorc );
            OSL_ASSERT( ret->getHandle() != nullptr );
            return ret;
        }();
    return theRc;
};

#if !defined EMSCRIPTEN

bool existsOfficePipe()
{
    static const OUString OfficePipeId = generateOfficePipeId();

    if (OfficePipeId.isEmpty())
        return false;
    ::osl::Security sec;
    ::osl::Pipe pipe( OfficePipeId, osl_Pipe_OPEN, sec );
    return pipe.is();
}

#endif

//get modification time
bool getModifyTimeTargetFile(const OUString &rFileURL, TimeValue &rTime)
{
    salhelper::LinkResolver aResolver(osl_FileStatus_Mask_ModifyTime);

    if (aResolver.fetchFileStatus(rFileURL) != osl::FileBase::E_None)
        return false;

    rTime = aResolver.m_aStatus.getModifyTime();

    return true;
}

//Returns true if the Folder was more recently modified then
//the lastsynchronized file. That is the repository needs to
//be synchronized.
bool compareExtensionFolderWithLastSynchronizedFile(
    OUString const & folderURL, OUString const & fileURL)
{
    bool bNeedsSync = false;
    ::osl::DirectoryItem itemExtFolder;
    ::osl::File::RC err1 =
          ::osl::DirectoryItem::get(folderURL, itemExtFolder);
    //If it does not exist, then there is nothing to be done
    if (err1 == ::osl::File::E_NOENT)
    {
        return false;
    }
    else if (err1 != ::osl::File::E_None)
    {
        OSL_FAIL("Cannot access extension folder");
        return true; //sync just in case
    }

    //If last synchronized does not exist, then OOo is started for the first time
    ::osl::DirectoryItem itemFile;
    ::osl::File::RC err2 = ::osl::DirectoryItem::get(fileURL, itemFile);
    if (err2 == ::osl::File::E_NOENT)
    {
        return true;

    }
    else if (err2 != ::osl::File::E_None)
    {
        OSL_FAIL("Cannot access file lastsynchronized");
        return true; //sync just in case
    }

    //compare the modification time of the extension folder and the last
    //modified file
    TimeValue timeFolder;
    if (getModifyTimeTargetFile(folderURL, timeFolder))
    {
        TimeValue timeFile;
        if (getModifyTimeTargetFile(fileURL, timeFile))
        {
            if (timeFile.Seconds < timeFolder.Seconds)
                bNeedsSync = true;
        }
        else
        {
            OSL_ASSERT(false);
            bNeedsSync = true;
        }
    }
    else
    {
        OSL_ASSERT(false);
        bNeedsSync = true;
    }

    return bNeedsSync;
}

bool needToSyncRepository(std::u16string_view name)
{
    OUString folder;
    OUString file;
    if ( name == u"bundled" )
    {
        folder = "$BUNDLED_EXTENSIONS";
        file = "$BUNDLED_EXTENSIONS_USER/lastsynchronized";
    }
    else if ( name == u"shared" )
    {
        folder = "$UNO_SHARED_PACKAGES_CACHE/uno_packages";
        file = "$SHARED_EXTENSIONS_USER/lastsynchronized";
    }
    else
    {
        OSL_ASSERT(false);
        return true;
    }
    ::rtl::Bootstrap::expandMacros(folder);
    ::rtl::Bootstrap::expandMacros(file);
    return compareExtensionFolderWithLastSynchronizedFile(
        folder, file);
}

// True when IPC thread is running in this process. Set in PipeIpcThread::execute. Used to avoid
// attempts to open the pipe from office_is_running, which could deadlock.
std::atomic<bool> s_bOfficeIpcThreadRunning = false;

OUString encodeForRcFile( std::u16string_view str )
{
    // escape $\{} (=> rtl bootstrap files)
    OUStringBuffer buf(64);
    size_t pos = 0;
    const size_t len = str.size();
    for ( ; pos < len; ++pos ) {
        sal_Unicode c = str[ pos ];
        switch (c) {
        case '$':
        case '\\':
        case '{':
        case '}':
            buf.append( '\\' );
            break;
        }
        buf.append( c );
    }
    return buf.makeStringAndClear();
}
} // anon namespace


OUString makeURL( std::u16string_view baseURL, OUString const & relPath_ )
{
    if (baseURL.ends_with('/'))
        baseURL.remove_suffix(1);

    OUString relPath(relPath_);
    if( relPath.startsWith("/") )
        relPath = relPath.copy( 1 );

    if (relPath.isEmpty())
        return OUString(baseURL);

    if (baseURL.starts_with(u"vnd.sun.star.expand:"))
    {
        // encode for macro expansion: relPath is supposed to have no
        // macros, so encode $, {} \ (bootstrap mimic)
        relPath = encodeForRcFile(relPath);

        // encode once more for vnd.sun.star.expand schema:
        // vnd.sun.star.expand:$UNO_...
        // will expand to file-url
        relPath = ::rtl::Uri::encode(relPath, rtl_UriCharClassUric, rtl_UriEncodeIgnoreEscapes,
                                     RTL_TEXTENCODING_UTF8);
    }
    return OUString::Concat(baseURL) + "/" + relPath;
}

OUString makeURLAppendSysPathSegment( std::u16string_view baseURL, OUString const & segment )
{
    OSL_ASSERT(segment.indexOf(u'/') == -1);

    ::rtl::Uri::encode(
        segment, rtl_UriCharClassPchar, rtl_UriEncodeIgnoreEscapes,
        RTL_TEXTENCODING_UTF8);
    return makeURL(baseURL, segment);
}


OUString expandUnoRcTerm( OUString const & term_ )
{
    OUString term(term_);
    UnoRc()->expandMacrosFrom( term );
    return term;
}

OUString makeRcTerm( OUString const & url )
{
    OSL_ASSERT( url.match( "vnd.sun.star.expand:" ));
    if (OUString rcterm; url.startsWithIgnoreAsciiCase("vnd.sun.star.expand:", &rcterm)) {
        // decode uric class chars:
        rcterm = ::rtl::Uri::decode(
            rcterm, rtl_UriDecodeWithCharset, RTL_TEXTENCODING_UTF8 );
        return rcterm;
    }
    else
        return url;
}


OUString expandUnoRcUrl( OUString const & url )
{
    if (OUString rcurl; url.startsWithIgnoreAsciiCase("vnd.sun.star.expand:", &rcurl)) {
        // decode uric class chars:
        rcurl = ::rtl::Uri::decode(
            rcurl, rtl_UriDecodeWithCharset, RTL_TEXTENCODING_UTF8 );
        // expand macro string:
        UnoRc()->expandMacrosFrom( rcurl );
        return rcurl;
    }
    else {
        return url;
    }
}

OUString generateOfficePipeId()
{
    // The name of the named pipe is created with the hashcode of the user installation directory
    // (without /user). We have to retrieve this information from a unotools implementation.

    OUString userPath;
    ::utl::Bootstrap::PathStatus aLocateResult =
    ::utl::Bootstrap::locateUserInstallation( userPath );
    if (aLocateResult != ::utl::Bootstrap::PATH_EXISTS &&
        aLocateResult != ::utl::Bootstrap::PATH_VALID)
    {
        throw Exception(u"Extension Manager: Could not obtain path for UserInstallation."_ustr, nullptr);
    }

    rtlDigest digest = rtl_digest_create( rtl_Digest_AlgorithmMD5 );
    if (!digest) {
        throw RuntimeException(u"cannot get digest rtl_Digest_AlgorithmMD5!"_ustr, nullptr );
    }

    sal_uInt8 const * data =
        reinterpret_cast<sal_uInt8 const *>(userPath.getStr());
    std::size_t size = userPath.getLength() * sizeof (sal_Unicode);
    sal_uInt32 md5_key_len = rtl_digest_queryLength( digest );
    std::unique_ptr<sal_uInt8[]> md5_buf( new sal_uInt8 [ md5_key_len ] );

    rtl_digest_init( digest, data, static_cast<sal_uInt32>(size) );
    rtl_digest_update( digest, data, static_cast<sal_uInt32>(size) );
    rtl_digest_get( digest, md5_buf.get(), md5_key_len );
    rtl_digest_destroy( digest );

    // create hex-value string from the MD5 value to keep
    // the string size minimal
    OUStringBuffer buf( "SingleOfficeIPC_" );
    for ( sal_uInt32 i = 0; i < md5_key_len; ++i ) {
        buf.append( static_cast<sal_Int32>(md5_buf[ i ]), 0x10 );
    }
    return buf.makeStringAndClear();
}

bool office_is_running()
{
#if defined EMSCRIPTEN
    return true;
#else
    // i#82778: We need to check if we run within the office process. Then we must not use the pipe,
    // because this could cause a deadlock (if called from IPC thread).
    return s_bOfficeIpcThreadRunning || existsOfficePipe();
#endif
}

void setOfficeIpcThreadRunning(bool bRunning) { s_bOfficeIpcThreadRunning = bRunning; }

oslProcess raiseProcess(
    OUString const & appURL, Sequence<OUString> const & args )
{
    ::osl::Security sec;
    oslProcess hProcess = nullptr;
    oslProcessError rc = osl_executeProcess(
        appURL.pData,
        reinterpret_cast<rtl_uString **>(
            const_cast<OUString *>(args.getConstArray()) ),
        args.getLength(),
        osl_Process_DETACHED,
        sec.getHandle(),
        nullptr, // => current working dir
        nullptr, 0, // => no env vars
        &hProcess );

    switch (rc) {
    case osl_Process_E_None:
        break;
    case osl_Process_E_NotFound:
        throw RuntimeException( u"image not found!"_ustr, nullptr );
    case osl_Process_E_TimedOut:
        throw RuntimeException( u"timeout occurred!"_ustr, nullptr );
    case osl_Process_E_NoPermission:
        throw RuntimeException( u"permission denied!"_ustr, nullptr );
    case osl_Process_E_Unknown:
        throw RuntimeException( u"unknown error!"_ustr, nullptr );
    case osl_Process_E_InvalidError:
    default:
        throw RuntimeException( u"unmapped error!"_ustr, nullptr );
    }

    return hProcess;
}


OUString generateRandomPipeId()
{
    // compute some good pipe id:
    sal_uInt8 bytes[ 32 ];
    if (rtl_random_getBytes(
            nullptr, bytes, std::size(bytes) ) != rtl_Random_E_None) {
        throw RuntimeException( u"random pool error!?"_ustr, nullptr );
    }
    OUStringBuffer buf;
    for (unsigned char byte : bytes) {
        buf.append( static_cast<sal_Int32>(byte), 0x10 );
    }
    return buf.makeStringAndClear();
}


Reference<XInterface> resolveUnoURL(
    OUString const & connectString,
    Reference<XComponentContext> const & xLocalContext,
    AbortChannel const * abortChannel )
{
    Reference<bridge::XUnoUrlResolver> xUnoUrlResolver(
        bridge::UnoUrlResolver::create( xLocalContext ) );

    for (int i = 0; i <= 40; ++i) // 20 seconds
    {
        if (abortChannel != nullptr && abortChannel->isAborted()) {
            throw ucb::CommandAbortedException( u"abort!"_ustr );
        }
        try {
            return xUnoUrlResolver->resolve( connectString );
        }
        catch (const connection::NoConnectException &) {
            if (i < 40)
            {
                std::this_thread::sleep_for( std::chrono::milliseconds(500) );
            }
            else throw;
        }
    }
    return nullptr; // warning C4715
}

static void writeConsoleWithStream(std::u16string_view sText, FILE * stream)
{
    OString s = OUStringToOString(sText, osl_getThreadTextEncoding());
    fprintf(stream, "%s", s.getStr());
    fflush(stream);
}

void writeConsole(std::u16string_view sText)
{
    writeConsoleWithStream(sText, stdout);
}

void writeConsoleError(std::u16string_view sText)
{
    writeConsoleWithStream(sText, stderr);
}

OUString readConsole()
{
    char buf[1024];
    memset(buf, 0, 1024);
    // read one char less so that the last char in buf is always zero
    if (fgets(buf, 1024, stdin) != nullptr)
    {
        OUString value = OStringToOUString(std::string_view(buf), osl_getThreadTextEncoding());
        return value.trim();
    }
    throw css::uno::RuntimeException(u"reading from stdin failed"_ustr);
}

void TRACE(OUString const & sText)
{
    SAL_INFO("desktop.deployment", sText);
}

void syncRepositories(
    bool force, Reference<ucb::XCommandEnvironment> const & xCmdEnv)
{
    OUString sDisable;
    ::rtl::Bootstrap::get( u"DISABLE_EXTENSION_SYNCHRONIZATION"_ustr, sDisable, OUString() );
    if (!sDisable.isEmpty())
        return;

    Reference<deployment::XExtensionManager> xExtensionManager;
    //synchronize shared before bundled otherwise there are
    //more revoke and registration calls.
    bool bModified = false;
    if (force || needToSyncRepository(u"shared") || needToSyncRepository(u"bundled"))
    {
        xExtensionManager =
            deployment::ExtensionManager::get(
                comphelper::getProcessComponentContext());

        if (xExtensionManager.is())
        {
            bModified = xExtensionManager->synchronize(
                Reference<task::XAbortChannel>(), xCmdEnv);
        }
    }
#if HAVE_FEATURE_MACOSX_SANDBOX
    (void) bModified;
#else
    if (bModified && !comphelper::LibreOfficeKit::isActive())
    {
        Reference<task::XRestartManager> restarter(task::OfficeRestartManager::get(comphelper::getProcessComponentContext()));
        if (restarter.is())
        {
            restarter->requestRestart(xCmdEnv.is() ? xCmdEnv->getInteractionHandler() :
                                      Reference<task::XInteractionHandler>());
        }
    }
#endif
}

void disposeBridges(Reference<css::uno::XComponentContext> const & ctx)
{
    if (!ctx.is())
        return;

    Reference<css::bridge::XBridgeFactory2> bridgeFac( css::bridge::BridgeFactory::create(ctx) );

    const Sequence< Reference<css::bridge::XBridge> >seqBridges = bridgeFac->getExistingBridges();
    for (const Reference<css::bridge::XBridge>& bridge : seqBridges)
    {
        Reference<css::lang::XComponent> comp(bridge, UNO_QUERY);
        if (comp.is())
        {
            try {
                comp->dispose();
            }
            catch ( const css::lang::DisposedException& )
            {
            }
        }
    }
}

}

OUString DpResId(TranslateId aId)
{
    static std::locale SINGLETON = Translate::Create("dkt");
    return Translate::get(aId, SINGLETON);
}


/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
