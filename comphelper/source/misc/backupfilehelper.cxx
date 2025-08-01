/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <sal/config.h>
#include <rtl/ustring.hxx>
#include <rtl/bootstrap.hxx>
#include <sal/log.hxx>
#include <osl/file.hxx>
#include <comphelper/backupfilehelper.hxx>
#include <comphelper/DirectoryHelper.hxx>
#include <rtl/crc.h>
#include <algorithm>
#include <deque>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>
#include <zlib.h>

#include <comphelper/processfactory.hxx>
#include <com/sun/star/lang/WrappedTargetRuntimeException.hpp>
#include <com/sun/star/ucb/CommandAbortedException.hpp>
#include <com/sun/star/ucb/CommandFailedException.hpp>
#include <com/sun/star/uno/Sequence.hxx>
#include <com/sun/star/uno/Reference.hxx>
#include <com/sun/star/deployment/DeploymentException.hpp>
#include <com/sun/star/deployment/ExtensionManager.hpp>
#include <com/sun/star/xml/dom/XDocumentBuilder.hpp>
#include <com/sun/star/xml/dom/DocumentBuilder.hpp>
#include <com/sun/star/xml/dom/XElement.hpp>
#include <com/sun/star/xml/dom/XNodeList.hpp>
#include <com/sun/star/xml/dom/XText.hpp>
#include <com/sun/star/xml/sax/XSAXSerializable.hpp>
#include <com/sun/star/xml/sax/Writer.hpp>
#include <com/sun/star/xml/sax/XWriter.hpp>
#include <com/sun/star/io/XStream.hpp>
#include <com/sun/star/io/TempFile.hpp>
#include <com/sun/star/io/XOutputStream.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <cppuhelper/exc_hlp.hxx>

using namespace comphelper;
using namespace css;
using namespace css::xml::dom;

const sal_uInt32 BACKUP_FILE_HELPER_BLOCK_SIZE = 16384;

namespace
{
    typedef std::shared_ptr< osl::File > FileSharedPtr;

    sal_uInt32 createCrc32(FileSharedPtr const & rCandidate, sal_uInt32 nOffset)
    {
        sal_uInt32 nCrc32(0);

        if (rCandidate && osl::File::E_None == rCandidate->open(osl_File_OpenFlag_Read))
        {
            sal_uInt8 aArray[BACKUP_FILE_HELPER_BLOCK_SIZE];
            sal_uInt64 nBytesTransfer(0);
            sal_uInt64 nSize(0);

            rCandidate->getSize(nSize);

            // set offset in source file - should be zero due to crc32 should
            // only be needed to be created for new entries, gets loaded with old
            // ones
            if (osl::File::E_None == rCandidate->setPos(osl_Pos_Absolut, sal_Int64(nOffset)))
            {
                while (nSize != 0)
                {
                    const sal_uInt64 nToTransfer(std::min(nSize, sal_uInt64(BACKUP_FILE_HELPER_BLOCK_SIZE)));

                    if (osl::File::E_None == rCandidate->read(static_cast<void*>(aArray), nToTransfer, nBytesTransfer) && nBytesTransfer == nToTransfer)
                    {
                        // add to crc and reduce size
                        nCrc32 = rtl_crc32(nCrc32, static_cast<void*>(aArray), static_cast<sal_uInt32>(nBytesTransfer));
                        nSize -= nToTransfer;
                    }
                    else
                    {
                        // error - reset to zero again
                        nSize = nCrc32 = 0;
                    }
                }
            }

            rCandidate->close();
        }

        return nCrc32;
    }

    bool read_sal_uInt32(FileSharedPtr const & rFile, sal_uInt32& rTarget)
    {
        sal_uInt8 aArray[4];
        sal_uInt64 nBaseRead(0);

        // read rTarget
        if (osl::File::E_None == rFile->read(static_cast<void*>(aArray), 4, nBaseRead) && 4 == nBaseRead)
        {
            rTarget = (sal_uInt32(aArray[0]) << 24) + (sal_uInt32(aArray[1]) << 16) + (sal_uInt32(aArray[2]) << 8) + sal_uInt32(aArray[3]);
            return true;
        }

        return false;
    }

    bool write_sal_uInt32(oslFileHandle& rHandle, sal_uInt32 nSource)
    {
        sal_uInt8 aArray[4];
        sal_uInt64 nBaseWritten(0);

        // write nSource
        aArray[0] = sal_uInt8((nSource & 0xff000000) >> 24);
        aArray[1] = sal_uInt8((nSource & 0x00ff0000) >> 16);
        aArray[2] = sal_uInt8((nSource & 0x0000ff00) >> 8);
        aArray[3] = sal_uInt8(nSource & 0x000000ff);

        return osl_File_E_None == osl_writeFile(rHandle, static_cast<const void*>(aArray), 4, &nBaseWritten) && 4 == nBaseWritten;
    }

    bool read_OString(FileSharedPtr const & rFile, OString& rTarget)
    {
        sal_uInt32 nLength(0);

        if (!read_sal_uInt32(rFile, nLength))
        {
            return false;
        }

        sal_uInt64 nPos;
        if (osl::File::E_None != rFile->getPos(nPos))
            return false;

        sal_uInt64 nSize;
        if (osl::File::E_None != rFile->getSize(nSize))
            return false;

        const auto nRemainingSize = nSize - nPos;
        if (nLength > nRemainingSize)
            return false;

        std::vector<char> aTarget(nLength);
        sal_uInt64 nBaseRead(0);

        // read rTarget
        if (osl::File::E_None == rFile->read(static_cast<void*>(aTarget.data()), nLength, nBaseRead) && nLength == nBaseRead)
        {
            rTarget = OString(aTarget.data(), static_cast<sal_Int32>(nBaseRead));
            return true;
        }

        return false;
    }

    bool write_OString(oslFileHandle& rHandle, const OString& rSource)
    {
        const sal_uInt32 nLength(rSource.getLength());

        if (!write_sal_uInt32(rHandle, nLength))
        {
            return false;
        }

        sal_uInt64 nBaseWritten(0);

        return osl_File_E_None == osl_writeFile(rHandle, static_cast<const void*>(rSource.getStr()), nLength, &nBaseWritten) && nLength == nBaseWritten;
    }

    OUString createFileURL(
        std::u16string_view rURL, std::u16string_view rName, std::u16string_view rExt)
    {
        OUString aRetval;

        if (!rURL.empty() && !rName.empty())
        {
            aRetval = OUString::Concat(rURL) + "/" + rName;

            if (!rExt.empty())
            {
                aRetval += OUString::Concat(".") + rExt;
            }
        }

        return aRetval;
    }

    OUString createPackURL(std::u16string_view rURL, std::u16string_view rName)
    {
        OUString aRetval;

        if (!rURL.empty() && !rName.empty())
        {
            aRetval = OUString::Concat(rURL) + "/" + rName + ".pack";
        }

        return aRetval;
    }
}

namespace
{
    enum PackageRepository { USER, SHARED, BUNDLED };

    class ExtensionInfoEntry
    {
    private:
        OString             maName;         // extension name
        PackageRepository   maRepository;   // user|shared|bundled
        bool                mbEnabled;      // state

    public:
        ExtensionInfoEntry()
        :   maRepository(USER),
            mbEnabled(false)
        {
        }

        ExtensionInfoEntry(OString aName, bool bEnabled)
        :   maName(std::move(aName)),
            maRepository(USER),
            mbEnabled(bEnabled)
        {
        }

        ExtensionInfoEntry(const uno::Reference< deployment::XPackage >& rxPackage)
        :   maName(OUStringToOString(rxPackage->getName(), RTL_TEXTENCODING_ASCII_US)),
            maRepository(USER),
            mbEnabled(false)
        {
            // check maRepository
            const OString aRepName(OUStringToOString(rxPackage->getRepositoryName(), RTL_TEXTENCODING_ASCII_US));

            if (aRepName == "shared")
            {
                maRepository = SHARED;
            }
            else if (aRepName == "bundled")
            {
                maRepository = BUNDLED;
            }

            // check mbEnabled
            const beans::Optional< beans::Ambiguous< sal_Bool > > option(
                rxPackage->isRegistered(uno::Reference< task::XAbortChannel >(),
                uno::Reference< ucb::XCommandEnvironment >()));

            if (option.IsPresent)
            {
                ::beans::Ambiguous< sal_Bool > const& reg = option.Value;

                if (!reg.IsAmbiguous)
                {
                    mbEnabled = reg.Value;
                }
            }
        }

        bool isSameExtension(const ExtensionInfoEntry& rComp) const
        {
            return (maRepository == rComp.maRepository && maName == rComp.maName);
        }

        bool operator<(const ExtensionInfoEntry& rComp) const
        {
            if (maRepository == rComp.maRepository)
            {
                if (maName == rComp.maName)
                {
                    return mbEnabled < rComp.mbEnabled;
                }
                else
                {
                    return 0 > maName.compareTo(rComp.maName);
                }
            }
            else
            {
                return maRepository < rComp.maRepository;
            }
        }

        bool read_entry(FileSharedPtr const & rFile)
        {
            // read maName
            if (!read_OString(rFile, maName))
            {
                return false;
            }

            // read maRepository
            sal_uInt32 nState(0);

            if (read_sal_uInt32(rFile, nState))
            {
                maRepository = static_cast< PackageRepository >(nState);
            }
            else
            {
                return false;
            }

            // read mbEnabled
            if (read_sal_uInt32(rFile, nState))
            {
                mbEnabled = static_cast< bool >(nState);
            }
            else
            {
                return false;
            }

            return true;
        }

        bool write_entry(oslFileHandle& rHandle) const
        {
            // write maName;
            if (!write_OString(rHandle, maName))
            {
                return false;
            }

            // write maRepository
            sal_uInt32 nState(maRepository);

            if (!write_sal_uInt32(rHandle, nState))
            {
                return false;
            }

            // write mbEnabled
            nState = static_cast< sal_uInt32 >(mbEnabled);

            return write_sal_uInt32(rHandle, nState);
        }

        const OString& getName() const
        {
            return maName;
        }

        bool isEnabled() const
        {
            return mbEnabled;
        }
    };

    typedef std::vector< ExtensionInfoEntry > ExtensionInfoEntryVector;

    constexpr OUString gaRegPath { u"/registry/com.sun.star.comp.deployment.bundle.PackageRegistryBackend/backenddb.xml"_ustr };

    class ExtensionInfo
    {
    private:
        ExtensionInfoEntryVector    maEntries;

    public:
        ExtensionInfo()
        {
        }

        const ExtensionInfoEntryVector& getExtensionInfoEntryVector() const
        {
            return maEntries;
        }

        void reset()
        {
            // clear all data
            maEntries.clear();
        }

        void createUsingXExtensionManager()
        {
            // clear all data
            reset();

            // create content from current extension configuration
            uno::Sequence< uno::Sequence< uno::Reference< deployment::XPackage > > > xAllPackages;
            const uno::Reference< uno::XComponentContext >& xContext = ::comphelper::getProcessComponentContext();
            uno::Reference< deployment::XExtensionManager > m_xExtensionManager = deployment::ExtensionManager::get(xContext);

            try
            {
                xAllPackages = m_xExtensionManager->getAllExtensions(uno::Reference< task::XAbortChannel >(),
                    uno::Reference< ucb::XCommandEnvironment >());
            }
            catch (const deployment::DeploymentException &)
            {
                return;
            }
            catch (const ucb::CommandFailedException &)
            {
                return;
            }
            catch (const ucb::CommandAbortedException &)
            {
                return;
            }
            catch (const lang::IllegalArgumentException & e)
            {
                css::uno::Any anyEx = cppu::getCaughtException();
                throw css::lang::WrappedTargetRuntimeException( e.Message,
                                e.Context, anyEx );
            }

            for (const uno::Sequence< uno::Reference< deployment::XPackage > > & xPackageList : xAllPackages)
            {
                for (const uno::Reference< deployment::XPackage > & xPackage : xPackageList)
                {
                    if (xPackage.is())
                    {
                        maEntries.emplace_back(xPackage);
                    }
                }
            }

            if (!maEntries.empty())
            {
                // sort the list
                std::sort(maEntries.begin(), maEntries.end());
            }
        }

    private:
        void visitNodesXMLRead(const uno::Reference< xml::dom::XElement >& rElement)
        {
            if (!rElement.is())
                return;

            const OUString aTagName(rElement->getTagName());

            if (aTagName == "extension")
            {
                OUString aAttrUrl(rElement->getAttribute(u"url"_ustr));
                const OUString aAttrRevoked(rElement->getAttribute(u"revoked"_ustr));

                if (!aAttrUrl.isEmpty())
                {
                    const sal_Int32 nIndex(aAttrUrl.lastIndexOf('/'));

                    if (nIndex > 0 && aAttrUrl.getLength() > nIndex + 1)
                    {
                        aAttrUrl = aAttrUrl.copy(nIndex + 1);
                    }

                    const bool bEnabled(aAttrRevoked.isEmpty() || !aAttrRevoked.toBoolean());
                    maEntries.emplace_back(
                            OUStringToOString(aAttrUrl, RTL_TEXTENCODING_ASCII_US),
                            bEnabled);
                }
            }
            else
            {
                uno::Reference< xml::dom::XNodeList > aList = rElement->getChildNodes();

                if (aList.is())
                {
                    const sal_Int32 nLength(aList->getLength());

                    for (sal_Int32 a(0); a < nLength; a++)
                    {
                        const uno::Reference< xml::dom::XElement > aChild(aList->item(a), uno::UNO_QUERY);

                        if (aChild.is())
                        {
                            visitNodesXMLRead(aChild);
                        }
                    }
                }
            }
        }

    public:
        void createUserExtensionRegistryEntriesFromXML(std::u16string_view rUserConfigWorkURL)
        {
            const OUString aPath(
                OUString::Concat(rUserConfigWorkURL) + "/uno_packages/cache" + gaRegPath);
            createExtensionRegistryEntriesFromXML(aPath);
        }

        void createSharedExtensionRegistryEntriesFromXML(std::u16string_view rUserConfigWorkURL)
        {
            const OUString aPath(
                OUString::Concat(rUserConfigWorkURL) + "/extensions/shared" + gaRegPath);
            createExtensionRegistryEntriesFromXML(aPath);
        }

        void createBundledExtensionRegistryEntriesFromXML(std::u16string_view rUserConfigWorkURL)
        {
            const OUString aPath(
                OUString::Concat(rUserConfigWorkURL) + "/extensions/bundled" + gaRegPath);
            createExtensionRegistryEntriesFromXML(aPath);
        }


        void createExtensionRegistryEntriesFromXML(const OUString& aPath)
        {
            if (DirectoryHelper::fileExists(aPath))
            {
                const uno::Reference< uno::XComponentContext >& xContext = ::comphelper::getProcessComponentContext();
                uno::Reference< xml::dom::XDocumentBuilder > xBuilder(xml::dom::DocumentBuilder::create(xContext));
                uno::Reference< xml::dom::XDocument > aDocument = xBuilder->parseURI(aPath);

                if (aDocument.is())
                {
                    visitNodesXMLRead(aDocument->getDocumentElement());
                }
            }

            if (!maEntries.empty())
            {
                // sort the list
                std::sort(maEntries.begin(), maEntries.end());
            }
        }

    private:
        static bool visitNodesXMLChange(
            const OUString& rTagToSearch,
            const uno::Reference< xml::dom::XElement >& rElement,
            const ExtensionInfoEntryVector& rToBeEnabled,
            const ExtensionInfoEntryVector& rToBeDisabled)
        {
            bool bChanged(false);

            if (rElement.is())
            {
                const OUString aTagName(rElement->getTagName());

                if (aTagName == rTagToSearch)
                {
                    const OString aAttrUrl(OUStringToOString(rElement->getAttribute(u"url"_ustr), RTL_TEXTENCODING_ASCII_US));
                    const OUString aAttrRevoked(rElement->getAttribute(u"revoked"_ustr));
                    const bool bEnabled(aAttrRevoked.isEmpty() || !aAttrRevoked.toBoolean());

                    if (!aAttrUrl.isEmpty())
                    {
                        for (const auto& enable : rToBeEnabled)
                        {
                            if (-1 != aAttrUrl.indexOf(enable.getName()))
                            {
                                if (!bEnabled)
                                {
                                    // needs to be enabled
                                    rElement->removeAttribute(u"revoked"_ustr);
                                    bChanged = true;
                                }
                            }
                        }

                        for (const auto& disable : rToBeDisabled)
                        {
                            if (-1 != aAttrUrl.indexOf(disable.getName()))
                            {
                                if (bEnabled)
                                {
                                    // needs to be disabled
                                    rElement->setAttribute(u"revoked"_ustr, u"true"_ustr);
                                    bChanged = true;
                                }
                            }
                        }
                    }
                }
                else
                {
                    uno::Reference< xml::dom::XNodeList > aList = rElement->getChildNodes();

                    if (aList.is())
                    {
                        const sal_Int32 nLength(aList->getLength());

                        for (sal_Int32 a(0); a < nLength; a++)
                        {
                            const uno::Reference< xml::dom::XElement > aChild(aList->item(a), uno::UNO_QUERY);

                            if (aChild.is())
                            {
                                bChanged |= visitNodesXMLChange(
                                    rTagToSearch,
                                    aChild,
                                    rToBeEnabled,
                                    rToBeDisabled);
                            }
                        }
                    }
                }
            }

            return bChanged;
        }

        static void visitNodesXMLChangeOneCase(
            const OUString& rUnoPackagReg,
            const OUString& rTagToSearch,
            const ExtensionInfoEntryVector& rToBeEnabled,
            const ExtensionInfoEntryVector& rToBeDisabled)
        {
            if (!DirectoryHelper::fileExists(rUnoPackagReg))
                return;

            const uno::Reference< uno::XComponentContext >& xContext = ::comphelper::getProcessComponentContext();
            uno::Reference< xml::dom::XDocumentBuilder > xBuilder = xml::dom::DocumentBuilder::create(xContext);
            uno::Reference< xml::dom::XDocument > aDocument = xBuilder->parseURI(rUnoPackagReg);

            if (!aDocument.is())
                return;

            if (!visitNodesXMLChange(
                rTagToSearch,
                aDocument->getDocumentElement(),
                rToBeEnabled,
                rToBeDisabled))
                return;

            // did change - write back
            uno::Reference< xml::sax::XSAXSerializable > xSerializer(aDocument, uno::UNO_QUERY);

            if (!xSerializer.is())
                return;

            // create a SAXWriter
            uno::Reference< xml::sax::XWriter > const xSaxWriter = xml::sax::Writer::create(xContext);
            uno::Reference< io::XTempFile > xTempFile = io::TempFile::create(xContext);
            uno::Reference< io::XOutputStream > xOutStrm = xTempFile->getOutputStream();

            // set output stream and do the serialization
            xSaxWriter->setOutputStream(xOutStrm);
            xSerializer->serialize(xSaxWriter, uno::Sequence< beans::StringPair >());

            // get URL from temp file
            OUString aTempURL = xTempFile->getUri();

            // copy back file
            if (aTempURL.isEmpty() || !DirectoryHelper::fileExists(aTempURL))
                return;

            if (DirectoryHelper::fileExists(rUnoPackagReg))
            {
                osl::File::remove(rUnoPackagReg);
            }

#if OSL_DEBUG_LEVEL > 1
            SAL_WARN_IF(osl::FileBase::E_None != osl::File::move(aTempURL, rUnoPackagReg), "comphelper.backupfilehelper", "could not copy back modified Extension configuration file");
#else
            osl::File::move(aTempURL, rUnoPackagReg);
#endif
        }

    public:
        static void changeEnableDisableStateInXML(
            std::u16string_view rUserConfigWorkURL,
            const ExtensionInfoEntryVector& rToBeEnabled,
            const ExtensionInfoEntryVector& rToBeDisabled)
        {
            static constexpr OUString aRegPathFront(u"/uno_packages/cache/registry/com.sun.star.comp.deployment."_ustr);
            static constexpr OUString aRegPathBack(u".PackageRegistryBackend/backenddb.xml"_ustr);
            // first appearance to check
            {
                const OUString aUnoPackagReg(OUString::Concat(rUserConfigWorkURL) + aRegPathFront + "bundle" + aRegPathBack);

                visitNodesXMLChangeOneCase(
                    aUnoPackagReg,
                    u"extension"_ustr,
                    rToBeEnabled,
                    rToBeDisabled);
            }

            // second appearance to check
            {
                const OUString aUnoPackagReg(OUString::Concat(rUserConfigWorkURL) + aRegPathFront + "configuration" + aRegPathBack);

                visitNodesXMLChangeOneCase(
                    aUnoPackagReg,
                    u"configuration"_ustr,
                    rToBeEnabled,
                    rToBeDisabled);
            }

            // third appearance to check
            {
                const OUString aUnoPackagReg(OUString::Concat(rUserConfigWorkURL) + aRegPathFront + "script" + aRegPathBack);

                visitNodesXMLChangeOneCase(
                    aUnoPackagReg,
                    u"script"_ustr,
                    rToBeEnabled,
                    rToBeDisabled);
            }
        }

        bool read_entries(FileSharedPtr const & rFile)
        {
            // read NumExtensionEntries
            sal_uInt32 nExtEntries(0);

            if (!read_sal_uInt32(rFile, nExtEntries))
            {
                return false;
            }

            // coverity#1373663 Untrusted loop bound, check file size
            // isn't utterly broken
            sal_uInt64 nFileSize(0);
            rFile->getSize(nFileSize);
            if (nFileSize < nExtEntries)
                return false;

            for (sal_uInt32 a(0); a < nExtEntries; a++)
            {
                ExtensionInfoEntry aNewEntry;

                if (aNewEntry.read_entry(rFile))
                {
                    maEntries.push_back(aNewEntry);
                }
                else
                {
                    return false;
                }
            }

            return true;
        }

        bool write_entries(oslFileHandle& rHandle) const
        {
            const sal_uInt32 nExtEntries(maEntries.size());

            if (!write_sal_uInt32(rHandle, nExtEntries))
            {
                return false;
            }

            for (const auto& a : maEntries)
            {
                if (!a.write_entry(rHandle))
                {
                    return false;
                }
            }

            return true;
        }

        bool createTempFile(OUString& rTempFileName)
        {
            oslFileHandle aHandle;
            bool bRetval(false);

            // create current configuration
            if (maEntries.empty())
            {
                createUsingXExtensionManager();
            }

            // open target temp file and write current configuration to it - it exists until deleted
            if (osl::File::E_None == osl::FileBase::createTempFile(nullptr, &aHandle, &rTempFileName))
            {
                bRetval = write_entries(aHandle);

                // close temp file - it exists until deleted
                osl_closeFile(aHandle);
            }

            return bRetval;
        }

        bool areThereEnabledExtensions() const
        {
            for (const auto& a : maEntries)
            {
                if (a.isEnabled())
                {
                    return true;
                }
            }

            return false;
        }
    };
}

namespace
{
    class PackedFileEntry
    {
    private:
        sal_uInt32          mnFullFileSize;     // size in bytes of unpacked original file
        sal_uInt32          mnPackFileSize;     // size in bytes in file backup package (smaller if compressed, same if not)
        sal_uInt32          mnOffset;           // offset in File (zero identifies new file)
        sal_uInt32          mnCrc32;            // checksum
        FileSharedPtr       maFile;             // file where to find the data (at offset)
        bool const          mbDoCompress;       // flag if this file is scheduled to be compressed when written

        bool copy_content_straight(oslFileHandle& rTargetHandle)
        {
            if (!maFile || osl::File::E_None != maFile->open(osl_File_OpenFlag_Read))
                return false;

            sal_uInt8 aArray[BACKUP_FILE_HELPER_BLOCK_SIZE];
            sal_uInt64 nBytesTransfer(0);
            sal_uInt64 nSize(getPackFileSize());

            // set offset in source file - when this is zero, a new file is to be added
            if (osl::File::E_None == maFile->setPos(osl_Pos_Absolut, sal_Int64(getOffset())))
            {
                while (nSize != 0)
                {
                    const sal_uInt64 nToTransfer(std::min(nSize, sal_uInt64(BACKUP_FILE_HELPER_BLOCK_SIZE)));

                    if (osl::File::E_None != maFile->read(static_cast<void*>(aArray), nToTransfer, nBytesTransfer) || nBytesTransfer != nToTransfer)
                    {
                        break;
                    }

                    if (osl_File_E_None != osl_writeFile(rTargetHandle, static_cast<const void*>(aArray), nToTransfer, &nBytesTransfer) || nBytesTransfer != nToTransfer)
                    {
                        break;
                    }

                    nSize -= nToTransfer;
                }
            }

            maFile->close();
            return (0 == nSize);
        }

        bool copy_content_compress(oslFileHandle& rTargetHandle)
        {
            if (!maFile || osl::File::E_None != maFile->open(osl_File_OpenFlag_Read))
                return false;

            sal_uInt8 aArray[BACKUP_FILE_HELPER_BLOCK_SIZE];
            sal_uInt8 aBuffer[BACKUP_FILE_HELPER_BLOCK_SIZE];
            sal_uInt64 nBytesTransfer(0);
            sal_uInt64 nSize(getPackFileSize());
            z_stream zstream;
            memset(&zstream, 0, sizeof(zstream));

            if (Z_OK == deflateInit(&zstream, Z_BEST_COMPRESSION))
            {
                // set offset in source file - when this is zero, a new file is to be added
                if (osl::File::E_None == maFile->setPos(osl_Pos_Absolut, sal_Int64(getOffset())))
                {
                    bool bOkay(true);

                    while (bOkay && nSize != 0)
                    {
                        const sal_uInt64 nToTransfer(std::min(nSize, sal_uInt64(BACKUP_FILE_HELPER_BLOCK_SIZE)));

                        if (osl::File::E_None != maFile->read(static_cast<void*>(aArray), nToTransfer, nBytesTransfer) || nBytesTransfer != nToTransfer)
                        {
                            break;
                        }

                        zstream.avail_in = nToTransfer;
                        zstream.next_in = reinterpret_cast<unsigned char*>(aArray);

                        do {
                            zstream.avail_out = BACKUP_FILE_HELPER_BLOCK_SIZE;
                            zstream.next_out = reinterpret_cast<unsigned char*>(aBuffer);
#if !defined Z_PREFIX
                            const sal_Int64 nRetval(deflate(&zstream, nSize == nToTransfer ? Z_FINISH : Z_NO_FLUSH));
#else
                            const sal_Int64 nRetval(z_deflate(&zstream, nSize == nToTransfer ? Z_FINISH : Z_NO_FLUSH));
#endif
                            if (Z_STREAM_ERROR == nRetval)
                            {
                                bOkay = false;
                            }
                            else
                            {
                                const sal_uInt64 nAvailable(BACKUP_FILE_HELPER_BLOCK_SIZE - zstream.avail_out);

                                if (osl_File_E_None != osl_writeFile(rTargetHandle, static_cast<const void*>(aBuffer), nAvailable, &nBytesTransfer) || nBytesTransfer != nAvailable)
                                {
                                    bOkay = false;
                                }
                            }
                        } while (bOkay && 0 == zstream.avail_out);

                        if (!bOkay)
                        {
                            break;
                        }

                        nSize -= nToTransfer;
                    }
                }

#if !defined Z_PREFIX
                deflateEnd(&zstream);
#else
                z_deflateEnd(&zstream);
#endif
            }

            maFile->close();

            // get compressed size and add to entry
            if (mnFullFileSize == mnPackFileSize && mnFullFileSize == zstream.total_in)
            {
                mnPackFileSize = zstream.total_out;
            }

            return (0 == nSize);
        }

        bool copy_content_uncompress(oslFileHandle& rTargetHandle)
        {
            if (!maFile || osl::File::E_None != maFile->open(osl_File_OpenFlag_Read))
                return false;

            sal_uInt8 aArray[BACKUP_FILE_HELPER_BLOCK_SIZE];
            sal_uInt8 aBuffer[BACKUP_FILE_HELPER_BLOCK_SIZE];
            sal_uInt64 nBytesTransfer(0);
            sal_uInt64 nSize(getPackFileSize());
            z_stream zstream;
            memset(&zstream, 0, sizeof(zstream));

            if (Z_OK == inflateInit(&zstream))
            {
                // set offset in source file - when this is zero, a new file is to be added
                if (osl::File::E_None == maFile->setPos(osl_Pos_Absolut, sal_Int64(getOffset())))
                {
                    bool bOkay(true);

                    while (bOkay && nSize != 0)
                    {
                        const sal_uInt64 nToTransfer(std::min(nSize, sal_uInt64(BACKUP_FILE_HELPER_BLOCK_SIZE)));

                        if (osl::File::E_None != maFile->read(static_cast<void*>(aArray), nToTransfer, nBytesTransfer) || nBytesTransfer != nToTransfer)
                        {
                            break;
                        }

                        zstream.avail_in = nToTransfer;
                        zstream.next_in = reinterpret_cast<unsigned char*>(aArray);

                        do {
                            zstream.avail_out = BACKUP_FILE_HELPER_BLOCK_SIZE;
                            zstream.next_out = reinterpret_cast<unsigned char*>(aBuffer);
#if !defined Z_PREFIX
                            const sal_Int64 nRetval(inflate(&zstream, Z_NO_FLUSH));
#else
                            const sal_Int64 nRetval(z_inflate(&zstream, Z_NO_FLUSH));
#endif
                            if (Z_STREAM_ERROR == nRetval)
                            {
                                bOkay = false;
                            }
                            else
                            {
                                const sal_uInt64 nAvailable(BACKUP_FILE_HELPER_BLOCK_SIZE - zstream.avail_out);

                                if (osl_File_E_None != osl_writeFile(rTargetHandle, static_cast<const void*>(aBuffer), nAvailable, &nBytesTransfer) || nBytesTransfer != nAvailable)
                                {
                                    bOkay = false;
                                }
                            }
                        } while (bOkay && 0 == zstream.avail_out);

                        if (!bOkay)
                        {
                            break;
                        }

                        nSize -= nToTransfer;
                    }

#if !defined Z_PREFIX
                    deflateEnd(&zstream);
#else
                    z_deflateEnd(&zstream);
#endif
                }
            }

            maFile->close();
            return (0 == nSize);
        }


    public:
        // create new, uncompressed entry
        PackedFileEntry(
            sal_uInt32 nFullFileSize,
            sal_uInt32 nCrc32,
            FileSharedPtr xFile,
            bool bDoCompress)
        :   mnFullFileSize(nFullFileSize),
            mnPackFileSize(nFullFileSize),
            mnOffset(0),
            mnCrc32(nCrc32),
            maFile(std::move(xFile)),
            mbDoCompress(bDoCompress)
        {
        }

        // create entry to be loaded as header (read_header)
        PackedFileEntry()
        :   mnFullFileSize(0),
            mnPackFileSize(0),
            mnOffset(0),
            mnCrc32(0),
            mbDoCompress(false)
        {
        }

        sal_uInt32 getFullFileSize() const
        {
            return  mnFullFileSize;
        }

        sal_uInt32 getPackFileSize() const
        {
            return  mnPackFileSize;
        }

        sal_uInt32 getOffset() const
        {
            return mnOffset;
        }

        void setOffset(sal_uInt32 nOffset)
        {
            mnOffset = nOffset;
        }

        static sal_uInt32 getEntrySize()
        {
            return 12;
        }

        sal_uInt32 getCrc32() const
        {
            return mnCrc32;
        }

        bool read_header(FileSharedPtr const & rFile)
        {
            if (!rFile)
            {
                return false;
            }

            maFile = rFile;

            // read and compute full file size
            if (!read_sal_uInt32(rFile, mnFullFileSize))
            {
                return false;
            }

            // read and compute entry crc32
            if (!read_sal_uInt32(rFile, mnCrc32))
            {
                return false;
            }

            // read and compute packed size
            if (!read_sal_uInt32(rFile, mnPackFileSize))
            {
                return false;
            }

            return true;
        }

        bool write_header(oslFileHandle& rHandle) const
        {
            // write full file size
            if (!write_sal_uInt32(rHandle, mnFullFileSize))
            {
                return false;
            }

            // write crc32
            if (!write_sal_uInt32(rHandle, mnCrc32))
            {
                return false;
            }

            // write packed file size
            if (!write_sal_uInt32(rHandle, mnPackFileSize))
            {
                return false;
            }

            return true;
        }

        bool copy_content(oslFileHandle& rTargetHandle, bool bUncompress)
        {
            if (bUncompress)
            {
                if (getFullFileSize() == getPackFileSize())
                {
                    // not compressed, just copy
                    return copy_content_straight(rTargetHandle);
                }
                else
                {
                    // compressed, need to uncompress on copy
                    return copy_content_uncompress(rTargetHandle);
                }
            }
            else if (0 == getOffset())
            {
                if (mbDoCompress)
                {
                    // compressed wanted, need to compress on copy
                    return copy_content_compress(rTargetHandle);
                }
                else
                {
                    // not compressed, straight copy
                    return copy_content_straight(rTargetHandle);
                }
            }
            else
            {
                return copy_content_straight(rTargetHandle);
            }
        }
    };
}

namespace
{
    class PackedFile
    {
    private:
        const OUString          maURL;
        std::deque< PackedFileEntry >
                                maPackedFileEntryVector;
        bool                    mbChanged;

    public:
        PackedFile(const OUString& rURL)
        :   maURL(rURL),
            mbChanged(false)
        {
            FileSharedPtr aSourceFile = std::make_shared<osl::File>(rURL);

            if (osl::File::E_None == aSourceFile->open(osl_File_OpenFlag_Read))
            {
                sal_uInt64 nBaseLen(0);
                aSourceFile->getSize(nBaseLen);

                // we need at least File_ID and num entries -> 8byte
                if (8 < nBaseLen)
                {
                    sal_uInt8 aArray[4];
                    sal_uInt64 nBaseRead(0);

                    // read and check File_ID
                    if (osl::File::E_None == aSourceFile->read(static_cast< void* >(aArray), 4, nBaseRead) && 4 == nBaseRead)
                    {
                        if ('P' == aArray[0] && 'A' == aArray[1] && 'C' == aArray[2] && 'K' == aArray[3])
                        {
                            // read and compute num entries in this file
                            if (osl::File::E_None == aSourceFile->read(static_cast<void*>(aArray), 4, nBaseRead) && 4 == nBaseRead)
                            {
                                sal_uInt32 nEntries((sal_uInt32(aArray[0]) << 24) + (sal_uInt32(aArray[1]) << 16) + (sal_uInt32(aArray[2]) << 8) + sal_uInt32(aArray[3]));

                                // if there are entries (and less than max), read them
                                if (nEntries >= 1 && nEntries <= 10)
                                {
                                    for (sal_uInt32 a(0); a < nEntries; a++)
                                    {
                                        // create new entry, read header (size, crc and PackedSize),
                                        // set offset and source file
                                        PackedFileEntry aEntry;

                                        if (aEntry.read_header(aSourceFile))
                                        {
                                            // add to local data
                                            maPackedFileEntryVector.push_back(aEntry);
                                        }
                                        else
                                        {
                                            // error
                                            nEntries = 0;
                                        }
                                    }

                                    if (0 == nEntries)
                                    {
                                        // on read error clear local data
                                        maPackedFileEntryVector.clear();
                                    }
                                    else
                                    {
                                        // calculate and set offsets to file binary content
                                        sal_uInt32 nHeaderSize(8);

                                        nHeaderSize += maPackedFileEntryVector.size() * PackedFileEntry::getEntrySize();

                                        sal_uInt32 nOffset(nHeaderSize);

                                        for (auto& b : maPackedFileEntryVector)
                                        {
                                            b.setOffset(nOffset);
                                            nOffset += b.getPackFileSize();
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                aSourceFile->close();
            }

            if (maPackedFileEntryVector.empty())
            {
                // on error or no data get rid of pack file
                osl::File::remove(maURL);
            }
        }

        void flush()
        {
            bool bRetval(true);

            if (maPackedFileEntryVector.empty())
            {
                // get rid of (now?) empty pack file
                osl::File::remove(maURL);
            }
            else if (mbChanged)
            {
                // need to create a new pack file, do this in a temp file to which data
                // will be copied from local file (so keep it here until this is done)
                oslFileHandle aHandle = nullptr;
                OUString aTempURL;

                // open target temp file - it exists until deleted
                if (osl::File::E_None == osl::FileBase::createTempFile(nullptr, &aHandle, &aTempURL))
                {
                    sal_uInt8 aArray[4];
                    sal_uInt64 nBaseWritten(0);

                    aArray[0] = 'P';
                    aArray[1] = 'A';
                    aArray[2] = 'C';
                    aArray[3] = 'K';

                    // write File_ID
                    if (osl_File_E_None == osl_writeFile(aHandle, static_cast<const void*>(aArray), 4, &nBaseWritten) && 4 == nBaseWritten)
                    {
                        const sal_uInt32 nSize(maPackedFileEntryVector.size());

                        // write number of entries
                        if (write_sal_uInt32(aHandle, nSize))
                        {
                            // write placeholder for headers. Due to the fact that
                            // PackFileSize for newly added files gets set during
                            // writing the content entry, write headers after content
                            // is written. To do so, write placeholders here
                            sal_uInt32 nWriteSize(0);

                            nWriteSize += maPackedFileEntryVector.size() * PackedFileEntry::getEntrySize();

                            aArray[0] = aArray[1] = aArray[2] = aArray[3] = 0;

                            for (sal_uInt32 a(0); bRetval && a < nWriteSize; a++)
                            {
                                if (osl_File_E_None != osl_writeFile(aHandle, static_cast<const void*>(aArray), 1, &nBaseWritten) || 1 != nBaseWritten)
                                {
                                    bRetval = false;
                                }
                            }

                            if (bRetval)
                            {
                                // write contents - this may adapt PackFileSize for new
                                // files
                                for (auto& candidate : maPackedFileEntryVector)
                                {
                                    if (!candidate.copy_content(aHandle, false))
                                    {
                                        bRetval = false;
                                        break;
                                    }
                                }
                            }

                            if (bRetval)
                            {
                                // seek back to header start (at position 8)
                                if (osl_File_E_None != osl_setFilePos(aHandle, osl_Pos_Absolut, sal_Int64(8)))
                                {
                                    bRetval = false;
                                }
                            }

                            if (bRetval)
                            {
                                // write headers
                                for (const auto& candidate : maPackedFileEntryVector)
                                {
                                    if (!candidate.write_header(aHandle))
                                    {
                                        // error
                                        bRetval = false;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }

                // close temp file (in all cases) - it exists until deleted
                osl_closeFile(aHandle);

                if (bRetval)
                {
                    // copy over existing file by first deleting original
                    // and moving the temp file to old original
                    osl::File::remove(maURL);
                    osl::File::move(aTempURL, maURL);
                }

                // delete temp file (in all cases - it may be moved already)
                osl::File::remove(aTempURL);
            }
        }

        bool tryPush(FileSharedPtr const & rFileCandidate, bool bCompress)
        {
            sal_uInt64 nFileSize(0);

            if (rFileCandidate && osl::File::E_None == rFileCandidate->open(osl_File_OpenFlag_Read))
            {
                rFileCandidate->getSize(nFileSize);
                rFileCandidate->close();
            }

            if (0 == nFileSize)
            {
                // empty file offered
                return false;
            }

            bool bNeedToAdd(false);
            sal_uInt32 nCrc32(0);

            if (maPackedFileEntryVector.empty())
            {
                // no backup yet, add as 1st backup
                bNeedToAdd = true;
            }
            else
            {
                // already backups there, check if different from last entry
                const PackedFileEntry& aLastEntry = maPackedFileEntryVector.back();

                // check if file is different
                if (aLastEntry.getFullFileSize() != static_cast<sal_uInt32>(nFileSize))
                {
                    // different size, different file
                    bNeedToAdd = true;
                }
                else
                {
                    // same size, check crc32
                    nCrc32 = createCrc32(rFileCandidate, 0);

                    if (nCrc32 != aLastEntry.getCrc32())
                    {
                        // different crc, different file
                        bNeedToAdd = true;
                    }
                }
            }

            if (bNeedToAdd)
            {
                // create crc32 if not yet done
                if (0 == nCrc32)
                {
                    nCrc32 = createCrc32(rFileCandidate, 0);
                }

                // create a file entry for a new file. Offset is set automatically
                // to 0 to mark the entry as new file entry
                maPackedFileEntryVector.emplace_back(
                        static_cast< sal_uInt32 >(nFileSize),
                        nCrc32,
                        rFileCandidate,
                        bCompress);

                mbChanged = true;
            }

            return bNeedToAdd;
        }

        bool tryPop(oslFileHandle& rHandle)
        {
            if (maPackedFileEntryVector.empty())
                return false;

            // already backups there, check if different from last entry
            PackedFileEntry& aLastEntry = maPackedFileEntryVector.back();

            // here the uncompress flag has to be determined, true
            // means to add the file compressed, false means to add it
            // uncompressed
            bool bRetval = aLastEntry.copy_content(rHandle, true);

            if (bRetval)
            {
                maPackedFileEntryVector.pop_back();
                mbChanged = true;
            }

            return bRetval;
        }

        void tryReduceToNumBackups(sal_uInt16 nNumBackups)
        {
            while (maPackedFileEntryVector.size() > nNumBackups)
            {
                maPackedFileEntryVector.pop_front();
                mbChanged = true;
            }
        }

        bool empty() const
        {
            return maPackedFileEntryVector.empty();
        }
    };
}

namespace comphelper
{
    bool BackupFileHelper::mbExitWasCalled = false;
    bool BackupFileHelper::mbSafeModeDirExists = false;
    OUString BackupFileHelper::maInitialBaseURL;
    OUString BackupFileHelper::maUserConfigBaseURL;
    OUString BackupFileHelper::maUserConfigWorkURL;
    OUString BackupFileHelper::maRegModName;
    OUString BackupFileHelper::maExt;

    const OUString& BackupFileHelper::getInitialBaseURL()
    {
        if (maInitialBaseURL.isEmpty())
        {
            // try to access user layer configuration file URL, the one that
            // points to registrymodifications.xcu
            OUString conf(u"${CONFIGURATION_LAYERS}"_ustr);
            rtl::Bootstrap::expandMacros(conf);
            static constexpr OUString aTokenUser(u"user:"_ustr);
            sal_Int32 nStart(conf.indexOf(aTokenUser));

            if (-1 != nStart)
            {
                nStart += aTokenUser.getLength();
                sal_Int32 nEnd(conf.indexOf(' ', nStart));

                if (-1 == nEnd)
                {
                    nEnd = conf.getLength();
                }

                maInitialBaseURL = conf.copy(nStart, nEnd - nStart);
                (void)maInitialBaseURL.startsWith("!", &maInitialBaseURL);
            }

            if (!maInitialBaseURL.isEmpty())
            {
                // split URL at extension and at last path separator
                maUserConfigBaseURL = DirectoryHelper::splitAtLastToken(
                    DirectoryHelper::splitAtLastToken(maInitialBaseURL, '.', maExt), '/',
                    maRegModName);
            }

            if (!maUserConfigBaseURL.isEmpty())
            {
                // check if SafeModeDir exists
                mbSafeModeDirExists = DirectoryHelper::dirExists(maUserConfigBaseURL + "/" + getSafeModeName());
            }

            maUserConfigWorkURL = maUserConfigBaseURL;

            if (mbSafeModeDirExists)
            {
                // adapt work URL to do all repair op's in the correct directory
                maUserConfigWorkURL += "/" + getSafeModeName();
            }
        }

        return maInitialBaseURL;
    }

    const OUString& BackupFileHelper::getSafeModeName()
    {
        static constexpr OUString aSafeMode(u"SafeMode"_ustr);

        return aSafeMode;
    }

    BackupFileHelper::BackupFileHelper()
    :   mnNumBackups(2),
        mnMode(1),
        mbActive(false),
        mbExtensions(true),
        mbCompress(true)
    {
        OUString sTokenOut;

        // read configuration item 'SecureUserConfig' -> bool on/off
        if (rtl::Bootstrap::get(u"SecureUserConfig"_ustr, sTokenOut))
        {
            mbActive = sTokenOut.toBoolean();
        }

        if (mbActive)
        {
            // ensure existence
            getInitialBaseURL();

            // if not found, we are out of business (maExt may be empty)
            mbActive = !maInitialBaseURL.isEmpty() && !maUserConfigBaseURL.isEmpty() && !maRegModName.isEmpty();
        }

        if (mbActive && rtl::Bootstrap::get(u"SecureUserConfigNumCopies"_ustr, sTokenOut))
        {
            const sal_uInt16 nConfigNumCopies(static_cast<sal_uInt16>(sTokenOut.toUInt32()));

            // limit to range [1..mnMaxAllowedBackups]
            mnNumBackups = std::clamp(mnNumBackups, nConfigNumCopies, mnMaxAllowedBackups);
        }

        if (mbActive && rtl::Bootstrap::get(u"SecureUserConfigMode"_ustr, sTokenOut))
        {
            const sal_uInt16 nMode(static_cast<sal_uInt16>(sTokenOut.toUInt32()));

            // limit to range [0..2]
            mnMode = std::min(nMode, sal_uInt16(2));
        }

        if (mbActive && rtl::Bootstrap::get(u"SecureUserConfigExtensions"_ustr, sTokenOut))
        {
            mbExtensions = sTokenOut.toBoolean();
        }

        if (mbActive && rtl::Bootstrap::get(u"SecureUserConfigCompress"_ustr, sTokenOut))
        {
            mbCompress = sTokenOut.toBoolean();
        }
    }

    void BackupFileHelper::setExitWasCalled()
    {
        mbExitWasCalled = true;
    }

    void BackupFileHelper::reactOnSafeMode(bool bSafeMode)
    {
        // ensure existence of needed paths
        getInitialBaseURL();

        if (maUserConfigBaseURL.isEmpty())
            return;

        if (bSafeMode)
        {
            if (!mbSafeModeDirExists)
            {
                std::set< OUString > aExcludeList;

                // do not move SafeMode directory itself
                aExcludeList.insert(getSafeModeName());

                // init SafeMode by creating the 'SafeMode' directory and moving
                // all stuff there. All repairs will happen there. Both Dirs have to exist.
                // extend maUserConfigWorkURL as needed
                maUserConfigWorkURL = maUserConfigBaseURL + "/" + getSafeModeName();

                osl::Directory::createPath(maUserConfigWorkURL);
                DirectoryHelper::moveDirContent(maUserConfigBaseURL, maUserConfigWorkURL, aExcludeList);

                // switch local flag, maUserConfigWorkURL is already reset
                mbSafeModeDirExists = true;
            }
        }
        else
        {
            if (mbSafeModeDirExists)
            {
                // SafeMode has ended, return to normal mode by moving all content
                // from 'SafeMode' directory back to UserDirectory and deleting it.
                // Both Dirs have to exist
                std::set< OUString > aExcludeList;

                DirectoryHelper::moveDirContent(maUserConfigWorkURL, maUserConfigBaseURL, aExcludeList);
                osl::Directory::remove(maUserConfigWorkURL);

                // switch local flag and reset maUserConfigWorkURL
                mbSafeModeDirExists = false;
                maUserConfigWorkURL = maUserConfigBaseURL;
            }
        }
    }

    void BackupFileHelper::tryPush()
    {
        // no push when SafeModeDir exists, it may be Office's exit after SafeMode
        // where SafeMode flag is already deleted, but SafeModeDir cleanup is not
        // done yet (is done at next startup)
        if (!mbActive || mbSafeModeDirExists)
            return;

        const OUString aPackURL(getPackURL());

        // ensure dir and file vectors
        fillDirFileInfo();

        // process all files in question recursively
        if (!maDirs.empty() || !maFiles.empty())
        {
            tryPush_Files(
                maDirs,
                maFiles,
                maUserConfigWorkURL,
                aPackURL);
        }
    }

    void BackupFileHelper::tryPushExtensionInfo()
    {
        // no push when SafeModeDir exists, it may be Office's exit after SafeMode
        // where SafeMode flag is already deleted, but SafeModeDir cleanup is not
        // done yet (is done at next startup)
        if (mbActive && mbExtensions && !mbSafeModeDirExists)
        {
            const OUString aPackURL(getPackURL());

            tryPush_extensionInfo(aPackURL);
        }
    }

    bool BackupFileHelper::isPopPossible()
    {
        bool bPopPossible(false);

        if (mbActive)
        {
            const OUString aPackURL(getPackURL());

            // ensure dir and file vectors
            fillDirFileInfo();

            // process all files in question recursively
            if (!maDirs.empty() || !maFiles.empty())
            {
                bPopPossible = isPopPossible_files(
                    maDirs,
                    maFiles,
                    maUserConfigWorkURL,
                    aPackURL);
            }
        }

        return bPopPossible;
    }

    void BackupFileHelper::tryPop()
    {
        if (!mbActive)
            return;

        bool bDidPop(false);
        const OUString aPackURL(getPackURL());

        // ensure dir and file vectors
        fillDirFileInfo();

        // process all files in question recursively
        if (!maDirs.empty() || !maFiles.empty())
        {
            bDidPop = tryPop_files(
                maDirs,
                maFiles,
                maUserConfigWorkURL,
                aPackURL);
        }

        if (bDidPop)
        {
            // try removal of evtl. empty directory
            osl::Directory::remove(aPackURL);
        }
    }

    bool BackupFileHelper::isPopPossibleExtensionInfo() const
    {
        bool bPopPossible(false);

        if (mbActive && mbExtensions)
        {
            const OUString aPackURL(getPackURL());

            bPopPossible = isPopPossible_extensionInfo(aPackURL);
        }

        return bPopPossible;
    }

    void BackupFileHelper::tryPopExtensionInfo()
    {
        if (!(mbActive && mbExtensions))
            return;

        bool bDidPop(false);
        const OUString aPackURL(getPackURL());

        bDidPop = tryPop_extensionInfo(aPackURL);

        if (bDidPop)
        {
            // try removal of evtl. empty directory
            osl::Directory::remove(aPackURL);
        }
    }

    bool BackupFileHelper::isTryDisableAllExtensionsPossible()
    {
        // check if there are still enabled extension which can be disabled,
        // but as we are now in SafeMode, use XML infos for this since the
        // extensions are not loaded from XExtensionManager
        class ExtensionInfo aExtensionInfo;

        aExtensionInfo.createUserExtensionRegistryEntriesFromXML(maUserConfigWorkURL);

        return aExtensionInfo.areThereEnabledExtensions();
    }

    void BackupFileHelper::tryDisableAllExtensions()
    {
        // disable all still enabled extensions,
        // but as we are now in SafeMode, use XML infos for this since the
        // extensions are not loaded from XExtensionManager
        ExtensionInfo aCurrentExtensionInfo;
        const ExtensionInfoEntryVector aToBeEnabled{};
        ExtensionInfoEntryVector aToBeDisabled;

        aCurrentExtensionInfo.createUserExtensionRegistryEntriesFromXML(maUserConfigWorkURL);

        const ExtensionInfoEntryVector& rCurrentVector = aCurrentExtensionInfo.getExtensionInfoEntryVector();

        for (const auto& rCurrentInfo : rCurrentVector)
        {
            if (rCurrentInfo.isEnabled())
            {
                aToBeDisabled.push_back(rCurrentInfo);
            }
        }

        ExtensionInfo::changeEnableDisableStateInXML(maUserConfigWorkURL, aToBeEnabled, aToBeDisabled);
    }

    bool BackupFileHelper::isTryDeinstallUserExtensionsPossible()
    {
        // check if there are User Extensions installed.
        class ExtensionInfo aExtensionInfo;

        aExtensionInfo.createUserExtensionRegistryEntriesFromXML(maUserConfigWorkURL);

        return !aExtensionInfo.getExtensionInfoEntryVector().empty();
    }

    void BackupFileHelper::tryDeinstallUserExtensions()
    {
        // delete User Extension installs
        DirectoryHelper::deleteDirRecursively(maUserConfigWorkURL + "/uno_packages");
    }

    bool BackupFileHelper::isTryResetSharedExtensionsPossible()
    {
        // check if there are shared Extensions installed
        class ExtensionInfo aExtensionInfo;

        aExtensionInfo.createSharedExtensionRegistryEntriesFromXML(maUserConfigWorkURL);

        return !aExtensionInfo.getExtensionInfoEntryVector().empty();
    }

    void BackupFileHelper::tryResetSharedExtensions()
    {
        // reset shared extension info
        DirectoryHelper::deleteDirRecursively(maUserConfigWorkURL + "/extensions/shared");
    }

    bool BackupFileHelper::isTryResetBundledExtensionsPossible()
    {
        // check if there are shared Extensions installed
        class ExtensionInfo aExtensionInfo;

        aExtensionInfo.createBundledExtensionRegistryEntriesFromXML(maUserConfigWorkURL);

        return !aExtensionInfo.getExtensionInfoEntryVector().empty();
    }

    void BackupFileHelper::tryResetBundledExtensions()
    {
        // reset shared extension info
        DirectoryHelper::deleteDirRecursively(maUserConfigWorkURL + "/extensions/bundled");
    }

    const std::vector< OUString >& BackupFileHelper::getCustomizationDirNames()
    {
        static std::vector< OUString > aDirNames =
        {
            u"config"_ustr,     // UI config stuff
            u"registry"_ustr,   // most of the registry stuff
            u"psprint"_ustr,    // not really needed, can be abandoned
            u"store"_ustr,      // not really needed, can be abandoned
            u"temp"_ustr,       // not really needed, can be abandoned
            u"pack"_ustr       // own backup dir
        };

        return aDirNames;
    }

    const std::vector< OUString >& BackupFileHelper::getCustomizationFileNames()
    {
        static std::vector< OUString > aFileNames =
        {
            u"registrymodifications.xcu"_ustr // personal registry stuff
        };

        return aFileNames;
    }

    namespace {
        uno::Reference<XElement> lcl_getConfigElement(const uno::Reference<XDocument>& xDocument, const OUString& rPath,
                                  const OUString& rKey, const OUString& rValue)
        {
            uno::Reference< XElement > itemElement = xDocument->createElement(u"item"_ustr);
            itemElement->setAttribute(u"oor:path"_ustr, rPath);

            uno::Reference< XElement > propElement = xDocument->createElement(u"prop"_ustr);
            propElement->setAttribute(u"oor:name"_ustr, rKey);
            propElement->setAttribute(u"oor:op"_ustr, u"replace"_ustr); // Replace any other options

            uno::Reference< XElement > valueElement = xDocument->createElement(u"value"_ustr);
            uno::Reference< XText > textElement = xDocument->createTextNode(rValue);

            valueElement->appendChild(textElement);
            propElement->appendChild(valueElement);
            itemElement->appendChild(propElement);

            return itemElement;
        }
    }

    void BackupFileHelper::tryDisableHWAcceleration()
    {
        const OUString aRegistryModifications(maUserConfigWorkURL + "/registrymodifications.xcu");
        if (!DirectoryHelper::fileExists(aRegistryModifications))
            return;

        const uno::Reference< uno::XComponentContext >& xContext = ::comphelper::getProcessComponentContext();
        uno::Reference< XDocumentBuilder > xBuilder = DocumentBuilder::create(xContext);
        uno::Reference< XDocument > xDocument = xBuilder->parseURI(aRegistryModifications);
        uno::Reference< XElement > xRootElement = xDocument->getDocumentElement();

        xRootElement->appendChild(lcl_getConfigElement(xDocument, u"/org.openoffice.Office.Common/VCL"_ustr,
                                                       u"DisableOpenGL"_ustr, u"true"_ustr));
        xRootElement->appendChild(lcl_getConfigElement(xDocument, u"/org.openoffice.Office.Common/Misc"_ustr,
                                                       u"UseOpenCL"_ustr, u"false"_ustr));
        // Do not disable Skia entirely, just force its CPU-based raster mode.
        xRootElement->appendChild(lcl_getConfigElement(xDocument, u"/org.openoffice.Office.Common/VCL"_ustr,
                                                       u"ForceSkia"_ustr, u"false"_ustr));
        xRootElement->appendChild(lcl_getConfigElement(xDocument, u"/org.openoffice.Office.Common/VCL"_ustr,
                                                       u"ForceSkiaRaster"_ustr, u"true"_ustr));

        OUString aTempURL;
        {
            // use the scope to make sure that the temp file gets properly closed before move

            // write back
            uno::Reference< xml::sax::XSAXSerializable > xSerializer(xDocument, uno::UNO_QUERY);

            if (!xSerializer.is())
                return;

            // create a SAXWriter
            uno::Reference< xml::sax::XWriter > const xSaxWriter = xml::sax::Writer::create(xContext);
            uno::Reference< io::XTempFile > xTempFile = io::TempFile::create(xContext);
            xTempFile->setRemoveFile(false); // avoid removal of tempfile when leaving the scope
            uno::Reference< io::XOutputStream > xOutStrm = xTempFile->getOutputStream();

            // set output stream and do the serialization
            xSaxWriter->setOutputStream(xOutStrm);
            xSerializer->serialize(xSaxWriter, uno::Sequence< beans::StringPair >());

            // get URL from temp file
            aTempURL = xTempFile->getUri();
        }

        // copy back file
        if (aTempURL.isEmpty() || !DirectoryHelper::fileExists(aTempURL))
            return;

        if (DirectoryHelper::fileExists(aRegistryModifications))
        {
            osl::File::remove(aRegistryModifications);
        }

        int result = osl::File::move(aTempURL, aRegistryModifications);
        SAL_WARN_IF(result != osl::FileBase::E_None, "comphelper.backupfilehelper", "could not copy back modified Extension configuration file");
    }

    bool BackupFileHelper::isTryResetCustomizationsPossible()
    {
        // return true if not all of the customization selection dirs or files are deleted
        const std::vector< OUString >& rDirs = getCustomizationDirNames();

        for (const auto& a : rDirs)
        {
            if (DirectoryHelper::dirExists(maUserConfigWorkURL + "/" + a))
            {
                return true;
            }
        }

        const std::vector< OUString >& rFiles = getCustomizationFileNames();

        for (const auto& b : rFiles)
        {
            if (DirectoryHelper::fileExists(maUserConfigWorkURL + "/" + b))
            {
                return true;
            }
        }

        return false;
    }

    void BackupFileHelper::tryResetCustomizations()
    {
        // delete all of the customization selection dirs
        const std::vector< OUString >& rDirs = getCustomizationDirNames();

        for (const auto& a : rDirs)
        {
            DirectoryHelper::deleteDirRecursively(maUserConfigWorkURL + "/" + a);
        }

        const std::vector< OUString >& rFiles = getCustomizationFileNames();

        for (const auto& b : rFiles)
        {
            osl::File::remove(maUserConfigWorkURL + "/" + b);
        }
    }

    void BackupFileHelper::tryResetUserProfile()
    {
        // completely delete the current UserProfile
        DirectoryHelper::deleteDirRecursively(maUserConfigWorkURL);
    }

    const OUString& BackupFileHelper::getUserProfileURL()
    {
        return maUserConfigBaseURL;
    }

    const OUString& BackupFileHelper::getUserProfileWorkURL()
    {
        return maUserConfigWorkURL;
    }

    /////////////////// helpers ///////////////////////

    OUString BackupFileHelper::getPackURL()
    {
        return OUString(maUserConfigWorkURL + "/pack");
    }

    /////////////////// file push helpers ///////////////////////

    bool BackupFileHelper::tryPush_Files(
        const std::set< OUString >& rDirs,
        const std::set< std::pair< OUString, OUString > >& rFiles,
        std::u16string_view rSourceURL, // source dir without trailing '/'
        const OUString& rTargetURL // target dir without trailing '/'
        )
    {
        bool bDidPush(false);
        osl::Directory::createPath(rTargetURL);

        // process files
        for (const auto& file : rFiles)
        {
            bDidPush |= tryPush_file(
                rSourceURL,
                rTargetURL,
                file.first,
                file.second);
        }

        // process dirs
        for (const auto& dir : rDirs)
        {
            OUString aNewSourceURL(OUString::Concat(rSourceURL) + "/" + dir);
            OUString aNewTargetURL(rTargetURL + "/" + dir);
            std::set< OUString > aNewDirs;
            std::set< std::pair< OUString, OUString > > aNewFiles;

            DirectoryHelper::scanDirsAndFiles(
                aNewSourceURL,
                aNewDirs,
                aNewFiles);

            if (!aNewDirs.empty() || !aNewFiles.empty())
            {
                bDidPush |= tryPush_Files(
                    aNewDirs,
                    aNewFiles,
                    aNewSourceURL,
                    aNewTargetURL);
            }
        }

        if (!bDidPush)
        {
            // try removal of evtl. empty directory
            osl::Directory::remove(rTargetURL);
        }

        return bDidPush;
    }

    bool BackupFileHelper::tryPush_file(
        std::u16string_view rSourceURL, // source dir without trailing '/'
        std::u16string_view rTargetURL, // target dir without trailing '/'
        std::u16string_view rName,  // filename
        std::u16string_view rExt    // extension (or empty)
        )
    {
        const OUString aFileURL(createFileURL(rSourceURL, rName, rExt));

        if (DirectoryHelper::fileExists(aFileURL))
        {
            const OUString aPackURL(createPackURL(rTargetURL, rName));
            PackedFile aPackedFile(aPackURL);
            FileSharedPtr aBaseFile = std::make_shared<osl::File>(aFileURL);

            if (aPackedFile.tryPush(aBaseFile, mbCompress))
            {
                // reduce to allowed number and flush
                aPackedFile.tryReduceToNumBackups(mnNumBackups);
                aPackedFile.flush();

                return true;
            }
        }

        return false;
    }

    /////////////////// file pop possibilities helper ///////////////////////

    bool BackupFileHelper::isPopPossible_files(
        const std::set< OUString >& rDirs,
        const std::set< std::pair< OUString, OUString > >& rFiles,
        std::u16string_view rSourceURL, // source dir without trailing '/'
        std::u16string_view rTargetURL // target dir without trailing '/'
        )
    {
        bool bPopPossible(false);

        // process files
        for (const auto& file : rFiles)
        {
            bPopPossible |= isPopPossible_file(
                rSourceURL,
                rTargetURL,
                file.first,
                file.second);
        }

        // process dirs
        for (const auto& dir : rDirs)
        {
            OUString aNewSourceURL(OUString::Concat(rSourceURL) + "/" + dir);
            OUString aNewTargetURL(OUString::Concat(rTargetURL) + "/" + dir);
            std::set< OUString > aNewDirs;
            std::set< std::pair< OUString, OUString > > aNewFiles;

            DirectoryHelper::scanDirsAndFiles(
                aNewSourceURL,
                aNewDirs,
                aNewFiles);

            if (!aNewDirs.empty() || !aNewFiles.empty())
            {
                bPopPossible |= isPopPossible_files(
                    aNewDirs,
                    aNewFiles,
                    aNewSourceURL,
                    aNewTargetURL);
            }
        }

        return bPopPossible;
    }

    bool BackupFileHelper::isPopPossible_file(
        std::u16string_view rSourceURL, // source dir without trailing '/'
        std::u16string_view rTargetURL, // target dir without trailing '/'
        std::u16string_view rName,  // filename
        std::u16string_view rExt    // extension (or empty)
        )
    {
        const OUString aFileURL(createFileURL(rSourceURL, rName, rExt));

        if (DirectoryHelper::fileExists(aFileURL))
        {
            const OUString aPackURL(createPackURL(rTargetURL, rName));
            PackedFile aPackedFile(aPackURL);

            return !aPackedFile.empty();
        }

        return false;
    }

    /////////////////// file pop helpers ///////////////////////

    bool BackupFileHelper::tryPop_files(
        const std::set< OUString >& rDirs,
        const std::set< std::pair< OUString, OUString > >& rFiles,
        std::u16string_view rSourceURL, // source dir without trailing '/'
        const OUString& rTargetURL  // target dir without trailing '/'
        )
    {
        bool bDidPop(false);

        // process files
        for (const auto& file : rFiles)
        {
            bDidPop |= tryPop_file(
                rSourceURL,
                rTargetURL,
                file.first,
                file.second);
        }

        // process dirs
        for (const auto& dir : rDirs)
        {
            OUString aNewSourceURL(OUString::Concat(rSourceURL) + "/" + dir);
            OUString aNewTargetURL(rTargetURL + "/" + dir);
            std::set< OUString > aNewDirs;
            std::set< std::pair< OUString, OUString > > aNewFiles;

            DirectoryHelper::scanDirsAndFiles(
                aNewSourceURL,
                aNewDirs,
                aNewFiles);

            if (!aNewDirs.empty() || !aNewFiles.empty())
            {
                bDidPop |= tryPop_files(
                    aNewDirs,
                    aNewFiles,
                    aNewSourceURL,
                    aNewTargetURL);
            }
        }

        if (bDidPop)
        {
            // try removal of evtl. empty directory
            osl::Directory::remove(rTargetURL);
        }

        return bDidPop;
    }

    bool BackupFileHelper::tryPop_file(
        std::u16string_view rSourceURL, // source dir without trailing '/'
        std::u16string_view rTargetURL, // target dir without trailing '/'
        std::u16string_view rName,  // filename
        std::u16string_view rExt    // extension (or empty)
        )
    {
        const OUString aFileURL(createFileURL(rSourceURL, rName, rExt));

        if (!DirectoryHelper::fileExists(aFileURL))
            return false;

        // try Pop for base file
        const OUString aPackURL(createPackURL(rTargetURL, rName));
        PackedFile aPackedFile(aPackURL);

        if (aPackedFile.empty())
            return false;

        oslFileHandle aHandle;
        OUString aTempURL;

        // open target temp file - it exists until deleted
        if (osl::File::E_None != osl::FileBase::createTempFile(nullptr, &aHandle, &aTempURL))
            return false;

        bool bRetval(aPackedFile.tryPop(aHandle));

        // close temp file (in all cases) - it exists until deleted
        osl_closeFile(aHandle);

        if (bRetval)
        {
            // copy over existing file by first deleting original
            // and moving the temp file to old original
            osl::File::remove(aFileURL);
            osl::File::move(aTempURL, aFileURL);

            // reduce to allowed number and flush
            aPackedFile.tryReduceToNumBackups(mnNumBackups);
            aPackedFile.flush();
        }

        // delete temp file (in all cases - it may be moved already)
        osl::File::remove(aTempURL);

        return bRetval;
    }

    /////////////////// ExtensionInfo helpers ///////////////////////

    bool BackupFileHelper::tryPush_extensionInfo(
        std::u16string_view rTargetURL // target dir without trailing '/'
        )
    {
        ExtensionInfo aExtensionInfo;
        OUString aTempURL;
        bool bRetval(false);

        // create current configuration and write to temp file - it exists until deleted
        if (aExtensionInfo.createTempFile(aTempURL))
        {
            const OUString aPackURL(createPackURL(rTargetURL, u"ExtensionInfo"));
            PackedFile aPackedFile(aPackURL);
            FileSharedPtr aBaseFile = std::make_shared<osl::File>(aTempURL);

            if (aPackedFile.tryPush(aBaseFile, mbCompress))
            {
                // reduce to allowed number and flush
                aPackedFile.tryReduceToNumBackups(mnNumBackups);
                aPackedFile.flush();
                bRetval = true;
            }
        }

        // delete temp file (in all cases)
        osl::File::remove(aTempURL);
        return bRetval;
    }

    bool BackupFileHelper::isPopPossible_extensionInfo(
        std::u16string_view rTargetURL // target dir without trailing '/'
        )
    {
        // extensionInfo always exists internally, no test needed
        const OUString aPackURL(createPackURL(rTargetURL, u"ExtensionInfo"));
        PackedFile aPackedFile(aPackURL);

        return !aPackedFile.empty();
    }

    bool BackupFileHelper::tryPop_extensionInfo(
        std::u16string_view rTargetURL // target dir without trailing '/'
        )
    {
        // extensionInfo always exists internally, no test needed
        const OUString aPackURL(createPackURL(rTargetURL, u"ExtensionInfo"));
        PackedFile aPackedFile(aPackURL);

        if (aPackedFile.empty())
            return false;

        oslFileHandle aHandle;
        OUString aTempURL;

        // open target temp file - it exists until deleted
        if (osl::File::E_None != osl::FileBase::createTempFile(nullptr, &aHandle, &aTempURL))
            return false;

        bool bRetval(aPackedFile.tryPop(aHandle));

        // close temp file (in all cases) - it exists until deleted
        osl_closeFile(aHandle);

        if (bRetval)
        {
            // last config is in temp file, load it to ExtensionInfo
            ExtensionInfo aLoadedExtensionInfo;
            FileSharedPtr aBaseFile = std::make_shared<osl::File>(aTempURL);

            if (osl::File::E_None == aBaseFile->open(osl_File_OpenFlag_Read))
            {
                if (aLoadedExtensionInfo.read_entries(aBaseFile))
                {
                    // get current extension info, but from XML config files
                    ExtensionInfo aCurrentExtensionInfo;

                    aCurrentExtensionInfo.createUserExtensionRegistryEntriesFromXML(maUserConfigWorkURL);

                    // now we have loaded last_working (aLoadedExtensionInfo) and
                    // current (aCurrentExtensionInfo) ExtensionInfo and may react on
                    // differences by de/activating these as needed
                    const ExtensionInfoEntryVector& aUserEntries = aCurrentExtensionInfo.getExtensionInfoEntryVector();
                    const ExtensionInfoEntryVector& rLoadedVector = aLoadedExtensionInfo.getExtensionInfoEntryVector();
                    ExtensionInfoEntryVector aToBeDisabled;
                    ExtensionInfoEntryVector aToBeEnabled;

                    for (const auto& rCurrentInfo : aUserEntries)
                    {
                        const ExtensionInfoEntry* pLoadedInfo = nullptr;

                        for (const auto& rLoadedInfo : rLoadedVector)
                        {
                            if (rCurrentInfo.isSameExtension(rLoadedInfo))
                            {
                                pLoadedInfo = &rLoadedInfo;
                                break;
                            }
                        }

                        if (nullptr != pLoadedInfo)
                        {
                            // loaded info contains information about the Extension rCurrentInfo
                            const bool bCurrentEnabled(rCurrentInfo.isEnabled());
                            const bool bLoadedEnabled(pLoadedInfo->isEnabled());

                            if (bCurrentEnabled && !bLoadedEnabled)
                            {
                                aToBeDisabled.push_back(rCurrentInfo);
                            }
                            else if (!bCurrentEnabled && bLoadedEnabled)
                            {
                                aToBeEnabled.push_back(rCurrentInfo);
                            }
                        }
                        else
                        {
                            // There is no loaded info about the Extension rCurrentInfo.
                            // It needs to be disabled
                            if (rCurrentInfo.isEnabled())
                            {
                                aToBeDisabled.push_back(rCurrentInfo);
                            }
                        }
                    }

                    if (!aToBeDisabled.empty() || !aToBeEnabled.empty())
                    {
                        ExtensionInfo::changeEnableDisableStateInXML(maUserConfigWorkURL, aToBeEnabled, aToBeDisabled);
                    }

                    bRetval = true;
                }
            }

            // reduce to allowed number and flush
            aPackedFile.tryReduceToNumBackups(mnNumBackups);
            aPackedFile.flush();
        }

        // delete temp file (in all cases - it may be moved already)
        osl::File::remove(aTempURL);

        return bRetval;
    }

    /////////////////// FileDirInfo helpers ///////////////////////

    void BackupFileHelper::fillDirFileInfo()
    {
        if (!maDirs.empty() || !maFiles.empty())
        {
            // already done
            return;
        }

        // Information about the configuration and the role/purpose of directories in
        // the UserConfiguration is taken from: https://wiki.documentfoundation.org/UserProfile

        // fill dir and file info list to work with dependent on work mode
        switch (mnMode)
        {
        case 0:
        {
            // simple mode: add just registrymodifications
            // (the orig file in maInitialBaseURL)
            maFiles.insert(std::pair< OUString, OUString >(maRegModName, maExt));
            break;
        }
        case 1:
        {
            // defined mode: Add a selection of dirs containing User-Defined and thus
            // valuable configuration information.
            // This is clearly discussable in every single point and may be adapted/corrected
            // over time. Main focus is to secure User-Defined/adapted values

            // add registrymodifications (the orig file in maInitialBaseURL)
            maFiles.insert(std::pair< OUString, OUString >(maRegModName, maExt));

            // User-defined substitution table (Tools/AutoCorrect)
            maDirs.insert(u"autocorr"_ustr);

            // User-Defined AutoText (Edit/AutoText)
            maDirs.insert(u"autotext"_ustr);

            // User-defined Macros
            maDirs.insert(u"basic"_ustr);

            // User-adapted toolbars for modules
            maDirs.insert(u"config"_ustr);

            // Initial and User-defined Databases
            maDirs.insert(u"database"_ustr);

            // most part of registry files
            maDirs.insert(u"registry"_ustr);

            // User-Defined Scripts
            maDirs.insert(u"Scripts"_ustr);

            // Template files
            maDirs.insert(u"template"_ustr);

            // Custom Dictionaries
            maDirs.insert(u"wordbook"_ustr);

            // Questionable - where and how is Extension stuff held and how
            // does this interact with enabled/disabled states which are extra handled?
            // Keep out of business until deeper evaluated
            //
            // maDirs.insert("extensions");
            // maDirs.insert("uno-packages");
            break;
        }
        case 2:
        {
            // whole directory. To do so, scan directory and exclude some dirs
            // from which we know they do not need to be secured explicitly. This
            // should already include registrymodifications, too.
            DirectoryHelper::scanDirsAndFiles(
                maUserConfigWorkURL,
                maDirs,
                maFiles);

            // should not exist, but for the case an error occurred and it got
            // copied somehow, avoid further recursive copying/saving
            maDirs.erase(u"SafeMode"_ustr);

            // not really needed, can be abandoned
            maDirs.erase(u"psprint"_ustr);

            // not really needed, can be abandoned
            maDirs.erase(u"store"_ustr);

            // not really needed, can be abandoned
            maDirs.erase(u"temp"_ustr);

            // exclude own backup dir to avoid recursion
            maDirs.erase(u"pack"_ustr);

            break;
        }
        }
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
