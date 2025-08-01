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

#include <sal/config.h>

#include <cassert>

#include <unotxdoc.hxx>
#include <sfx2/app.hxx>
#include <com/sun/star/sdb/CommandType.hpp>
#include <com/sun/star/sdb/XDocumentDataSource.hpp>
#include <com/sun/star/lang/DisposedException.hpp>
#include <com/sun/star/lang/XEventListener.hpp>
#include <com/sun/star/uri/UriReferenceFactory.hpp>
#include <com/sun/star/uri/VndSunStarPkgUrlReferenceFactory.hpp>
#include <com/sun/star/util/NumberFormatter.hpp>
#include <com/sun/star/sdb/DatabaseContext.hpp>
#include <com/sun/star/sdb/TextConnectionSettings.hpp>
#include <com/sun/star/sdb/XCompletedConnection.hpp>
#include <com/sun/star/sdb/XCompletedExecution.hpp>
#include <com/sun/star/container/XChild.hpp>
#include <com/sun/star/text/MailMergeEvent.hpp>
#include <com/sun/star/frame/XStorable.hpp>
#include <com/sun/star/task/InteractionHandler.hpp>
#include <com/sun/star/ui/dialogs/TemplateDescription.hpp>
#include <com/sun/star/ui/dialogs/XFilePicker3.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <vcl/errinf.hxx>
#include <vcl/print.hxx>
#include <vcl/scheduler.hxx>
#include <sfx2/fcontnr.hxx>
#include <sfx2/filedlghelper.hxx>
#include <sfx2/viewfrm.hxx>
#include <dbconfig.hxx>
#include <unotools/tempfile.hxx>
#include <unotools/pathoptions.hxx>
#include <svl/numformat.hxx>
#include <svl/zforlist.hxx>
#include <svl/stritem.hxx>
#include <sfx2/docfile.hxx>
#include <sfx2/docfilt.hxx>
#include <sfx2/progress.hxx>
#include <sfx2/dispatch.hxx>
#include <cmdid.h>
#include <swmodule.hxx>
#include <view.hxx>
#include <docsh.hxx>
#include <edtwin.hxx>
#include <wrtsh.hxx>
#include <fldbas.hxx>
#include <dbui.hxx>
#include <dbmgr.hxx>
#include <doc.hxx>
#include <IDocumentLinksAdministration.hxx>
#include <IDocumentFieldsAccess.hxx>
#include <IDocumentUndoRedo.hxx>
#include <swwait.hxx>
#include <swunohelper.hxx>
#include <strings.hrc>
#include <mmconfigitem.hxx>
#include <com/sun/star/sdbc/XRowSet.hpp>
#include <com/sun/star/sdbcx/XTablesSupplier.hpp>
#include <com/sun/star/sdbcx/XColumnsSupplier.hpp>
#include <com/sun/star/sdb/XQueriesSupplier.hpp>
#include <com/sun/star/sdb/XColumn.hpp>
#include <com/sun/star/sdbc/DataType.hpp>
#include <com/sun/star/sdbc/ResultSetType.hpp>
#include <com/sun/star/sdbc/SQLException.hpp>
#include <com/sun/star/mail/MailAttachment.hpp>
#include <comphelper/processfactory.hxx>
#include <comphelper/property.hxx>
#include <comphelper/propertyvalue.hxx>
#include <comphelper/scopeguard.hxx>
#include <comphelper/storagehelper.hxx>
#include <comphelper/string.hxx>
#include <comphelper/types.hxx>
#include <mailmergehelper.hxx>
#include <maildispatcher.hxx>
#include <svtools/htmlcfg.hxx>
#include <i18nlangtag/languagetag.hxx>
#include <com/sun/star/util/XNumberFormatTypes.hpp>
#include <svl/numuno.hxx>
#include <connectivity/dbtools.hxx>
#include <connectivity/dbconversion.hxx>
#include <unotools/charclass.hxx>
#include <comphelper/diagnose_ex.hxx>

#include <unomailmerge.hxx>
#include <sfx2/event.hxx>
#include <svx/dataaccessdescriptor.hxx>
#include <rtl/textenc.h>
#include <rtl/tencinfo.h>
#include <cppuhelper/implbase.hxx>
#include <ndindex.hxx>
#include <swevent.hxx>
#include <sal/log.hxx>
#include <swabstdlg.hxx>
#include <vector>
#include <section.hxx>
#include <rootfrm.hxx>
#include <calc.hxx>
#include <dbfld.hxx>
#include <IDocumentState.hxx>
#include <imaildsplistener.hxx>
#include <iodetect.hxx>
#include <IDocumentDeviceAccess.hxx>

#include <memory>
#include <mutex>
#include <comphelper/propertysequence.hxx>

using namespace ::com::sun::star;
using namespace sw;

namespace {

void lcl_emitEvent(SfxEventHintId nEventId, sal_Int32 nStrId, SfxObjectShell* pDocShell)
{
    SfxGetpApp()->NotifyEvent(SfxEventHint(nEventId,
                                           SwDocShell::GetEventName(nStrId),
                                           pDocShell));
}

// Construct vnd.sun.star.pkg:// URL
OUString ConstructVndSunStarPkgUrl(const OUString& rMainURL, std::u16string_view rStreamRelPath)
{
    const auto& xContext(comphelper::getProcessComponentContext());
    auto xUri = css::uri::UriReferenceFactory::create(xContext)->parse(rMainURL);
    assert(xUri.is());
    xUri = css::uri::VndSunStarPkgUrlReferenceFactory::create(xContext)
        ->createVndSunStarPkgUrlReference(xUri);
    assert(xUri.is());
    return xUri->getUriReference() + "/"
        + INetURLObject::encode(
            rStreamRelPath, INetURLObject::PART_FPATH,
            INetURLObject::EncodeMechanism::All);
}

}

std::vector<std::pair<SwDocShell*, OUString>> SwDBManager::s_aUncommittedRegistrations;

namespace {

enum class SwDBNextRecord { NEXT, FIRST };

}

static bool lcl_ToNextRecord( SwDSParam* pParam, const SwDBNextRecord action = SwDBNextRecord::NEXT );

namespace {

enum class WorkingDocType { SOURCE, TARGET, COPY };

}

static SfxObjectShell* lcl_CreateWorkingDocument(
    const WorkingDocType aType, const SwWrtShell &rSourceWrtShell,
    const vcl::Window *pSourceWindow,
    SwDBManager** const ppDBManager,
    SwView** const pView, SwWrtShell** const pWrtShell, rtl::Reference<SwDoc>* const pDoc );

static bool lcl_getCountFromResultSet( sal_Int32& rCount, const SwDSParam* pParam )
{
    rCount = pParam->aSelection.getLength();
    if ( rCount > 0 )
        return true;

    uno::Reference<beans::XPropertySet> xPrSet(pParam->xResultSet, uno::UNO_QUERY);
    if ( xPrSet.is() )
    {
        try
        {
            bool bFinal = false;
            uno::Any aFinal = xPrSet->getPropertyValue(u"IsRowCountFinal"_ustr);
            aFinal >>= bFinal;
            if(!bFinal)
            {
                pParam->xResultSet->last();
                pParam->xResultSet->first();
            }
            uno::Any aCount = xPrSet->getPropertyValue(u"RowCount"_ustr);
            if( aCount >>= rCount )
                return true;
        }
        catch(const uno::Exception&)
        {
        }
    }
    return false;
}

class SwDBManager::ConnectionDisposedListener_Impl
    : public cppu::WeakImplHelper< lang::XEventListener >
{
private:
    SwDBManager * m_pDBManager;

    virtual void SAL_CALL disposing( const lang::EventObject& Source ) override;

public:
    explicit ConnectionDisposedListener_Impl(SwDBManager& rMgr);

    void Dispose() { m_pDBManager = nullptr; }

};

namespace {

/// Listens to removed data sources, and if it's one that's embedded into this document, triggers embedding removal.
class SwDataSourceRemovedListener : public cppu::WeakImplHelper<sdb::XDatabaseRegistrationsListener>
{
    uno::Reference<sdb::XDatabaseContext> m_xDatabaseContext;
    SwDBManager* m_pDBManager;

public:
    explicit SwDataSourceRemovedListener(SwDBManager& rDBManager);
    virtual ~SwDataSourceRemovedListener() override;
    virtual void SAL_CALL registeredDatabaseLocation(const sdb::DatabaseRegistrationEvent& rEvent) override;
    virtual void SAL_CALL revokedDatabaseLocation(const sdb::DatabaseRegistrationEvent& rEvent) override;
    virtual void SAL_CALL changedDatabaseLocation(const sdb::DatabaseRegistrationEvent& rEvent) override;
    virtual void SAL_CALL disposing(const lang::EventObject& rObject) override;
    void Dispose();
};

}

SwDataSourceRemovedListener::SwDataSourceRemovedListener(SwDBManager& rDBManager)
    : m_pDBManager(&rDBManager)
{
    const uno::Reference<uno::XComponentContext>& xComponentContext(comphelper::getProcessComponentContext());
    m_xDatabaseContext = sdb::DatabaseContext::create(xComponentContext);
    m_xDatabaseContext->addDatabaseRegistrationsListener(this);
}

SwDataSourceRemovedListener::~SwDataSourceRemovedListener()
{
    if (m_xDatabaseContext.is())
        m_xDatabaseContext->removeDatabaseRegistrationsListener(this);
}

void SAL_CALL SwDataSourceRemovedListener::registeredDatabaseLocation(const sdb::DatabaseRegistrationEvent& /*rEvent*/)
{
}

void SAL_CALL SwDataSourceRemovedListener::revokedDatabaseLocation(const sdb::DatabaseRegistrationEvent& rEvent)
{
    if (!m_pDBManager || m_pDBManager->getEmbeddedName().isEmpty())
        return;

    SwDoc* pDoc = m_pDBManager->getDoc();
    if (!pDoc)
        return;

    SwDocShell* pDocShell = pDoc->GetDocShell();
    if (!pDocShell)
        return;

    const OUString sTmpName = ConstructVndSunStarPkgUrl(
        pDocShell->GetMedium()->GetURLObject().GetMainURL(INetURLObject::DecodeMechanism::NONE),
        m_pDBManager->getEmbeddedName());

    if (sTmpName != rEvent.OldLocation)
        return;

    // The revoked database location is inside this document, then remove the
    // embedding, as otherwise it would be back on the next reload of the
    // document.
    pDocShell->GetStorage()->removeElement(m_pDBManager->getEmbeddedName());
    m_pDBManager->setEmbeddedName(OUString(), *pDocShell);
}

void SAL_CALL SwDataSourceRemovedListener::changedDatabaseLocation(const sdb::DatabaseRegistrationEvent& rEvent)
{
    if (rEvent.OldLocation != rEvent.NewLocation)
        revokedDatabaseLocation(rEvent);
}

void SwDataSourceRemovedListener::disposing(const lang::EventObject& /*rObject*/)
{
    m_xDatabaseContext.clear();
}

void SwDataSourceRemovedListener::Dispose()
{
    m_pDBManager = nullptr;
}

struct SwDBManager::SwDBManager_Impl
{
    std::unique_ptr<SwDSParam> pMergeData;
    VclPtr<AbstractMailMergeDlg>  pMergeDialog;
    rtl::Reference<SwDBManager::ConnectionDisposedListener_Impl> m_xDisposeListener;
    rtl::Reference<SwDataSourceRemovedListener> m_xDataSourceRemovedListener;
    std::mutex                    m_aAllEmailSendMutex;
    uno::Reference< mail::XMailMessage> m_xLastMessage;

    explicit SwDBManager_Impl(SwDBManager& rDBManager)
        : m_xDisposeListener(new ConnectionDisposedListener_Impl(rDBManager))
        {}

    ~SwDBManager_Impl()
    {
        m_xDisposeListener->Dispose();
        if (m_xDataSourceRemovedListener.is())
            m_xDataSourceRemovedListener->Dispose();
    }
};

static void lcl_InitNumberFormatter(SwDSParam& rParam, uno::Reference<sdbc::XDataSource> const & xSource)
{
    const uno::Reference<uno::XComponentContext>& xContext = ::comphelper::getProcessComponentContext();
    rParam.xFormatter = util::NumberFormatter::create(xContext);
    uno::Reference<beans::XPropertySet> xSourceProps(
        (xSource.is()
         ? xSource
         : SwDBManager::getDataSourceAsParent(
             rParam.xConnection, rParam.sDataSource)),
        uno::UNO_QUERY);
    if(!xSourceProps.is())
        return;

    uno::Any aFormats = xSourceProps->getPropertyValue(u"NumberFormatsSupplier"_ustr);
    if(!aFormats.hasValue())
        return;

    uno::Reference<util::XNumberFormatsSupplier> xSuppl;
    aFormats >>= xSuppl;
    if(xSuppl.is())
    {
        uno::Reference< beans::XPropertySet > xSettings = xSuppl->getNumberFormatSettings();
        uno::Any aNull = xSettings->getPropertyValue(u"NullDate"_ustr);
        aNull >>= rParam.aNullDate;
        if(rParam.xFormatter.is())
            rParam.xFormatter->attachNumberFormatsSupplier(xSuppl);
    }
}

static bool lcl_MoveAbsolute(SwDSParam* pParam, tools::Long nAbsPos)
{
    bool bRet = false;
    try
    {
        if(pParam->aSelection.hasElements())
        {
            if(pParam->aSelection.getLength() <= nAbsPos)
            {
                pParam->bEndOfDB = true;
                bRet = false;
            }
            else
            {
                pParam->nSelectionIndex = nAbsPos;
                sal_Int32 nPos = 0;
                pParam->aSelection.getConstArray()[ pParam->nSelectionIndex ] >>= nPos;
                pParam->bEndOfDB = !pParam->xResultSet->absolute( nPos );
                bRet = !pParam->bEndOfDB;
            }
        }
        else if(pParam->bScrollable)
        {
            bRet = pParam->xResultSet->absolute( nAbsPos );
        }
        else
        {
            OSL_FAIL("no absolute positioning available");
        }
    }
    catch(const uno::Exception&)
    {
    }
    return bRet;
}

static void lcl_GetColumnCnt(SwDSParam *pParam,
                             const uno::Reference< beans::XPropertySet > &rColumnProps,
                             LanguageType nLanguage, OUString &rResult, double* pNumber)
{
    SwDBFormatData aFormatData;
    if(!pParam->xFormatter.is())
    {
        uno::Reference<sdbc::XDataSource> xSource = SwDBManager::getDataSourceAsParent(
                                    pParam->xConnection,pParam->sDataSource);
        lcl_InitNumberFormatter(*pParam, xSource );
    }
    aFormatData.aNullDate = pParam->aNullDate;
    aFormatData.xFormatter = pParam->xFormatter;

    aFormatData.aLocale = LanguageTag( nLanguage ).getLocale();

    rResult = SwDBManager::GetDBField( rColumnProps, aFormatData, pNumber);
}

static bool lcl_GetColumnCnt(SwDSParam* pParam, const OUString& rColumnName,
                             LanguageType nLanguage, OUString& rResult, double* pNumber)
{
    uno::Reference< sdbcx::XColumnsSupplier > xColsSupp( pParam->xResultSet, uno::UNO_QUERY );
    uno::Reference<container::XNameAccess> xCols;
    try
    {
        xCols = xColsSupp->getColumns();
    }
    catch(const lang::DisposedException&)
    {
    }
    if(!xCols.is() || !xCols->hasByName(rColumnName))
        return false;
    uno::Any aCol = xCols->getByName(rColumnName);
    uno::Reference< beans::XPropertySet > xColumnProps;
    aCol >>= xColumnProps;
    lcl_GetColumnCnt( pParam, xColumnProps, nLanguage, rResult, pNumber );
    return true;
};

// import data
bool SwDBManager::Merge( const SwMergeDescriptor& rMergeDesc )
{
    assert( !m_bInMerge && !m_pImpl->pMergeData && "merge already activated!" );

    SfxObjectShellLock  xWorkObjSh;
    SwWrtShell         *pWorkShell            = nullptr;
    rtl::Reference<SwDoc> pWorkDoc;
    SwDBManager        *pWorkDocOrigDBManager = nullptr;

    switch( rMergeDesc.nMergeType )
    {
        case DBMGR_MERGE_PRINTER:
        case DBMGR_MERGE_EMAIL:
        case DBMGR_MERGE_FILE:
        case DBMGR_MERGE_SHELL:
        {
            SwDocShell *pSourceDocSh = rMergeDesc.rSh.GetView().GetDocShell();
            if( pSourceDocSh->IsModified() )
            {
                pWorkDocOrigDBManager = this;
                xWorkObjSh = lcl_CreateWorkingDocument(
                    WorkingDocType::SOURCE, rMergeDesc.rSh, nullptr,
                    &pWorkDocOrigDBManager, nullptr, &pWorkShell, &pWorkDoc );
            }
            [[fallthrough]];
        }

        default:
            if( !xWorkObjSh.Is() )
                pWorkShell = &rMergeDesc.rSh;
            break;
    }

    SwDBData aData;
    aData.nCommandType = sdb::CommandType::TABLE;
    uno::Reference<sdbc::XResultSet>  xResSet;
    uno::Sequence<uno::Any> aSelection;
    uno::Reference< sdbc::XConnection> xConnection;

    aData.sDataSource = rMergeDesc.rDescriptor.getDataSource();
    rMergeDesc.rDescriptor[svx::DataAccessDescriptorProperty::Command]      >>= aData.sCommand;
    rMergeDesc.rDescriptor[svx::DataAccessDescriptorProperty::CommandType]  >>= aData.nCommandType;

    if ( rMergeDesc.rDescriptor.has(svx::DataAccessDescriptorProperty::Cursor) )
        rMergeDesc.rDescriptor[svx::DataAccessDescriptorProperty::Cursor] >>= xResSet;
    if ( rMergeDesc.rDescriptor.has(svx::DataAccessDescriptorProperty::Selection) )
        rMergeDesc.rDescriptor[svx::DataAccessDescriptorProperty::Selection] >>= aSelection;
    if ( rMergeDesc.rDescriptor.has(svx::DataAccessDescriptorProperty::Connection) )
        rMergeDesc.rDescriptor[svx::DataAccessDescriptorProperty::Connection] >>= xConnection;

    if((aData.sDataSource.isEmpty() || aData.sCommand.isEmpty()) && !xResSet.is())
    {
        return false;
    }

    m_pImpl->pMergeData.reset(new SwDSParam(aData, xResSet, aSelection));
    SwDSParam*  pTemp = FindDSData(aData, false);
    if(pTemp)
        *pTemp = *m_pImpl->pMergeData;
    else
    {
        // calls from the calculator may have added a connection with an invalid commandtype
        //"real" data base connections added here have to re-use the already available
        //DSData and set the correct CommandType
        aData.nCommandType = -1;
        pTemp = FindDSData(aData, false);
        if(pTemp)
            *pTemp = *m_pImpl->pMergeData;
        else
        {
            m_DataSourceParams.push_back(std::make_unique<SwDSParam>(*m_pImpl->pMergeData));
            try
            {
                uno::Reference<lang::XComponent> xComponent(m_DataSourceParams.back()->xConnection, uno::UNO_QUERY);
                if(xComponent.is())
                    xComponent->addEventListener(m_pImpl->m_xDisposeListener);
            }
            catch(const uno::Exception&)
            {
            }
        }
    }
    if(!m_pImpl->pMergeData->xConnection.is())
        m_pImpl->pMergeData->xConnection = xConnection;
    // add an XEventListener

    lcl_ToNextRecord(m_pImpl->pMergeData.get(), SwDBNextRecord::FIRST);

    uno::Reference<sdbc::XDataSource> xSource = SwDBManager::getDataSourceAsParent(xConnection,aData.sDataSource);

    lcl_InitNumberFormatter(*m_pImpl->pMergeData, xSource);

    assert(pWorkShell);
    pWorkShell->ChgDBData(aData);
    m_bInMerge = true;

    if (IsInitDBFields())
    {
        // with database fields without DB-Name, use DB-Name from Doc
        std::vector<OUString> aDBNames;
        aDBNames.emplace_back();
        SwDBData aInsertData = pWorkShell->GetDBData();
        OUString sDBName = aInsertData.sDataSource
            + OUStringChar(DB_DELIM) + aInsertData.sCommand
            + OUStringChar(DB_DELIM)
            + OUString::number(aInsertData.nCommandType);
        pWorkShell->ChangeDBFields( aDBNames, sDBName);
        SetInitDBFields(false);
    }

    bool bRet = true;
    switch(rMergeDesc.nMergeType)
    {
        case DBMGR_MERGE:
            pWorkShell->StartAllAction();
            pWorkShell->SwViewShell::UpdateFields( true );
            pWorkShell->SetModified();
            pWorkShell->EndAllAction();
            break;

        case DBMGR_MERGE_PRINTER:
        case DBMGR_MERGE_EMAIL:
        case DBMGR_MERGE_FILE:
        case DBMGR_MERGE_SHELL:
            // save files and send them as e-Mail if required
            bRet = MergeMailFiles(pWorkShell, rMergeDesc);
            break;

        default:
            // insert selected entries
            // (was: InsertRecord)
            ImportFromConnection(pWorkShell);
            break;
    }

    m_pImpl->pMergeData.reset();

    if( xWorkObjSh.Is() )
    {
        pWorkDoc->SetDBManager( pWorkDocOrigDBManager );
        xWorkObjSh->DoClose();
    }

    m_bInMerge = false;

    return bRet;
}

void SwDBManager::ImportFromConnection(  SwWrtShell* pSh )
{
    if(!m_pImpl->pMergeData || m_pImpl->pMergeData->bEndOfDB)
        return;

    pSh->StartAllAction();
    pSh->StartUndo();
    bool bGroupUndo(pSh->DoesGroupUndo());
    pSh->DoGroupUndo(false);

    if( pSh->HasSelection() )
        pSh->DelRight();

    std::optional<SwWait> oWait;

    {
        sal_uInt32 i = 0;
        do {

            ImportDBEntry(pSh);
            if( 10 == ++i )
                oWait.emplace( *pSh->GetView().GetDocShell(), true);

        } while(ToNextMergeRecord());
    }

    pSh->DoGroupUndo(bGroupUndo);
    pSh->EndUndo();
    pSh->EndAllAction();
}

void SwDBManager::ImportDBEntry(SwWrtShell* pSh)
{
    if(!m_pImpl->pMergeData || m_pImpl->pMergeData->bEndOfDB)
        return;

    uno::Reference< sdbcx::XColumnsSupplier > xColsSupp( m_pImpl->pMergeData->xResultSet, uno::UNO_QUERY );
    uno::Reference<container::XNameAccess> xCols = xColsSupp->getColumns();
    OUStringBuffer sStr;
    uno::Sequence<OUString> aColNames = xCols->getElementNames();
    const OUString* pColNames = aColNames.getConstArray();
    tools::Long nLength = aColNames.getLength();
    for(tools::Long i = 0; i < nLength; i++)
    {
        uno::Any aCol = xCols->getByName(pColNames[i]);
        uno::Reference< beans::XPropertySet > xColumnProp;
        aCol >>= xColumnProp;
        SwDBFormatData aDBFormat;
        sStr.append(GetDBField( xColumnProp, aDBFormat));
        if (i < nLength - 1)
            sStr.append("\t");
    }
    pSh->SwEditShell::Insert2(sStr.makeStringAndClear());
    pSh->SwFEShell::SplitNode();    // line feed
}

bool SwDBManager::GetTableNames(weld::ComboBox& rBox, const OUString& rDBName)
{
    bool bRet = false;
    OUString sOldTableName(rBox.get_active_text());
    rBox.clear();
    SwDSParam* pParam = FindDSConnection(rDBName, false);
    uno::Reference< sdbc::XConnection> xConnection;
    if (pParam && pParam->xConnection.is())
        xConnection = pParam->xConnection;
    else
    {
        if ( !rDBName.isEmpty() )
            xConnection = RegisterConnection( rDBName );
    }
    if (xConnection.is())
    {
        uno::Reference<sdbcx::XTablesSupplier> xTSupplier(xConnection, uno::UNO_QUERY);
        if(xTSupplier.is())
        {
            uno::Reference<container::XNameAccess> xTables = xTSupplier->getTables();
            const uno::Sequence<OUString> aTables = xTables->getElementNames();
            for (const OUString& rTable : aTables)
                rBox.append(u"0"_ustr, rTable);
        }
        uno::Reference<sdb::XQueriesSupplier> xQSupplier(xConnection, uno::UNO_QUERY);
        if(xQSupplier.is())
        {
            uno::Reference<container::XNameAccess> xQueries = xQSupplier->getQueries();
            const uno::Sequence<OUString> aQueries = xQueries->getElementNames();
            for (const OUString& rQuery : aQueries)
                rBox.append(u"1"_ustr, rQuery);
        }
        if (!sOldTableName.isEmpty())
            rBox.set_active_text(sOldTableName);
        bRet = true;
    }
    return bRet;
}

// fill Listbox with column names of a database
void SwDBManager::GetColumnNames(weld::ComboBox& rBox,
                             const OUString& rDBName, const OUString& rTableName)
{
    SwDBData aData;
    aData.sDataSource = rDBName;
    aData.sCommand = rTableName;
    aData.nCommandType = -1;
    SwDSParam* pParam = FindDSData(aData, false);
    uno::Reference< sdbc::XConnection> xConnection;
    if(pParam && pParam->xConnection.is())
        xConnection = pParam->xConnection;
    else
    {
        xConnection = RegisterConnection( rDBName );
    }
    GetColumnNames(rBox, xConnection, rTableName);
}

void SwDBManager::GetColumnNames(weld::ComboBox& rBox,
        uno::Reference< sdbc::XConnection> const & xConnection,
        const OUString& rTableName)
{
    rBox.clear();
    uno::Reference< sdbcx::XColumnsSupplier> xColsSupp = SwDBManager::GetColumnSupplier(xConnection, rTableName);
    if(xColsSupp.is())
    {
        uno::Reference<container::XNameAccess> xCols = xColsSupp->getColumns();
        const uno::Sequence<OUString> aColNames = xCols->getElementNames();
        for (const OUString& rColName : aColNames)
        {
            rBox.append_text(rColName);
        }
        ::comphelper::disposeComponent( xColsSupp );
    }
}

SwDBManager::SwDBManager(SwDoc* pDoc)
    : m_aMergeStatus( MergeStatus::Ok )
    , m_bInitDBFields(false)
    , m_bInMerge(false)
    , m_bMergeSilent(false)
    , m_pImpl(new SwDBManager_Impl(*this))
    , m_pMergeEvtSrc(nullptr)
    , m_pDoc(pDoc)
{
}

void SwDBManager::ImplDestroy()
{
    RevokeLastRegistrations();

    // copy required, m_DataSourceParams can be modified while disposing components
    std::vector<uno::Reference<sdbc::XConnection>> aCopiedConnections;
    for (const auto & pParam : m_DataSourceParams)
    {
        if(pParam->xConnection.is())
        {
            aCopiedConnections.push_back(pParam->xConnection);
        }
    }
    for (const auto & xConnection : aCopiedConnections)
    {
        try
        {
            uno::Reference<lang::XComponent> xComp(xConnection, uno::UNO_QUERY);
            if(xComp.is())
                xComp->dispose();
        }
        catch(const uno::RuntimeException&)
        {
            //may be disposed already since multiple entries may have used the same connection
        }
    }
}

SwDBManager::~SwDBManager()
{
    suppress_fun_call_w_exception(ImplDestroy());
}

static void lcl_RemoveSectionLinks( SwWrtShell& rWorkShell )
{
    //reset all links of the sections of synchronized labels
    size_t nSections = rWorkShell.GetSectionFormatCount();
    for (size_t nSection = 0; nSection < nSections; ++nSection)
    {
        SwSectionData aSectionData( *rWorkShell.GetSectionFormat( nSection ).GetSection() );
        if( aSectionData.GetType() == SectionType::FileLink )
        {
            aSectionData.SetType( SectionType::Content );
            aSectionData.SetLinkFileName( OUString() );
            rWorkShell.UpdateSection( nSection, aSectionData );
        }
    }
    rWorkShell.SetLabelDoc( false );
}

static void lcl_SaveDebugDoc( SfxObjectShell *xTargetDocShell,
                              const char *name, int no = 0 )
{
    static OUString sTempDirURL;
    if( sTempDirURL.isEmpty() )
    {
        SvtPathOptions aPathOpt;
        utl::TempFileNamed aTempDir( &aPathOpt.GetTempPath(), true );
        if( aTempDir.IsValid() )
        {
            INetURLObject aTempDirURL( aTempDir.GetURL() );
            sTempDirURL = aTempDirURL.GetMainURL( INetURLObject::DecodeMechanism::NONE );
            SAL_INFO( "sw.mailmerge", "Dump directory: " << sTempDirURL );
        }
    }
    if( sTempDirURL.isEmpty() )
        return;

    OUString basename = OUString::createFromAscii( name );
    if (no > 0)
        basename += OUString::number(no) + "-";
    // aTempFile is not deleted, but that seems to be intentional
    utl::TempFileNamed aTempFile( basename, true, u".odt", &sTempDirURL );
    INetURLObject aTempFileURL( aTempFile.GetURL() );
    SfxMedium aDstMed(
        aTempFileURL.GetMainURL( INetURLObject::DecodeMechanism::NONE ),
        StreamMode::STD_READWRITE );
    bool bAnyError = !xTargetDocShell->DoSaveAs( aDstMed );
    // xObjectShell->DoSaveCompleted crashes the mail merge unit tests, so skip it
    bAnyError |= (ERRCODE_NONE != xTargetDocShell->GetErrorIgnoreWarning());
    if( bAnyError )
        SAL_WARN( "sw.mailmerge", "Error saving: " << aTempFile.GetURL() );
    else
        SAL_INFO( "sw.mailmerge", "Saved doc as: " << aTempFile.GetURL() );
}

static bool lcl_SaveDoc(
    const INetURLObject* pFileURL,
    const std::shared_ptr<const SfxFilter>& pStoreToFilter,
    const OUString* pStoreToFilterOptions,
    const uno::Sequence< beans::PropertyValue >* pSaveToFilterData,
    const bool bIsPDFexport,
    SfxObjectShell* xObjectShell,
    SwWrtShell& rWorkShell,
    OUString * const decodedURL = nullptr )
{
    OUString url = pFileURL->GetMainURL( INetURLObject::DecodeMechanism::NONE );
    if( decodedURL )
        (*decodedURL) = url;

    SfxMedium* pDstMed = new SfxMedium( url, StreamMode::STD_READWRITE );
    pDstMed->SetFilter( pStoreToFilter );
    if( pStoreToFilterOptions )
        pDstMed->GetItemSet().Put( SfxStringItem(SID_FILE_FILTEROPTIONS,
                                   *pStoreToFilterOptions));
    if( pSaveToFilterData->hasElements() )
        pDstMed->GetItemSet().Put( SfxUnoAnyItem(SID_FILTER_DATA,
                                   uno::Any(*pSaveToFilterData)));

    // convert fields to text if we are exporting to PDF.
    // this prevents a second merge while updating the fields
    // in SwXTextDocument::getRendererCount()
    if( bIsPDFexport )
        rWorkShell.ConvertFieldsToText();

    bool bAnyError = !xObjectShell->DoSaveAs(*pDstMed);
    // Actually this should be a bool... so in case of email and individual
    // files, where this is set, we skip the recently used handling
    bAnyError |= !xObjectShell->DoSaveCompleted( pDstMed, !decodedURL );
    bAnyError |= (ERRCODE_NONE != xObjectShell->GetErrorIgnoreWarning());
    if( bAnyError )
    {
        // error message ??
        ErrorHandler::HandleError( xObjectShell->GetErrorIgnoreWarning() );
    }
    return !bAnyError;
}

static void lcl_PreparePrinterOptions(
    const uno::Sequence< beans::PropertyValue >& rInPrintOptions,
    uno::Sequence< beans::PropertyValue >& rOutPrintOptions)
{
    // printing should be done synchronously otherwise the document
    // might already become invalid during the process

    rOutPrintOptions = { comphelper::makePropertyValue(u"Wait"_ustr, true) };

    // copy print options
    sal_Int32 nIndex = 1;
    for( const beans::PropertyValue& rOption : rInPrintOptions)
    {
        if( rOption.Name == "CopyCount" || rOption.Name == "FileName"
            || rOption.Name == "Collate" || rOption.Name == "Pages"
            || rOption.Name == "Wait" || rOption.Name == "PrinterName" )
        {
            // add an option
            rOutPrintOptions.realloc( nIndex + 1 );
            auto pOutPrintOptions = rOutPrintOptions.getArray();
            pOutPrintOptions[ nIndex ].Name = rOption.Name;
            pOutPrintOptions[ nIndex++ ].Value = rOption.Value ;
        }
    }
}

static void lcl_PrepareSaveFilterDataOptions(
    const uno::Sequence< beans::PropertyValue >& rInSaveFilterDataptions,
    uno::Sequence< beans::PropertyValue >& rOutSaveFilterDataOptions,
    const OUString& sPassword)
{
    rOutSaveFilterDataOptions
        = { comphelper::makePropertyValue(u"EncryptFile"_ustr, true),
            comphelper::makePropertyValue(u"DocumentOpenPassword"_ustr, sPassword) };

    // copy other options
    sal_Int32 nIndex = 2;
    for( const beans::PropertyValue& rOption : rInSaveFilterDataptions)
    {
         rOutSaveFilterDataOptions.realloc( nIndex + 1 );
         auto pOutSaveFilterDataOptions = rOutSaveFilterDataOptions.getArray();
         pOutSaveFilterDataOptions[ nIndex ].Name = rOption.Name;
         pOutSaveFilterDataOptions[ nIndex++ ].Value = rOption.Value ;
    }

}


static SfxObjectShell* lcl_CreateWorkingDocument(
    // input
    const WorkingDocType aType, const SwWrtShell &rSourceWrtShell,
    // optional input
    const vcl::Window *pSourceWindow,
    // optional in and output to swap the DB manager
    SwDBManager** const ppDBManager,
    // optional output
    SwView** const pView, SwWrtShell** const pWrtShell, rtl::Reference<SwDoc>* const pDoc )
{
    const SwDoc *pSourceDoc = rSourceWrtShell.GetDoc();
    SfxObjectShellRef xWorkObjectShell = pSourceDoc->CreateCopy( true, (aType == WorkingDocType::TARGET) );
    SfxViewFrame* pWorkFrame = SfxViewFrame::LoadHiddenDocument( *xWorkObjectShell, SFX_INTERFACE_NONE );

    if( pSourceWindow )
    {
        // the created window has to be located at the same position as the source window
        vcl::Window& rTargetWindow = pWorkFrame->GetFrame().GetWindow();
        rTargetWindow.SetPosPixel( pSourceWindow->GetPosPixel() );
    }

    SwView* pWorkView = static_cast< SwView* >( pWorkFrame->GetViewShell() );

    if (SwWrtShell* pWorkWrtShell = pWorkView->GetWrtShellPtr())
    {
        pWorkWrtShell->GetViewOptions()->SetIdle( false );
        pWorkView->AttrChangedNotify(nullptr);// in order for SelectShell to be called
        SwDoc* pWorkDoc = pWorkWrtShell->GetDoc();
        pWorkDoc->GetIDocumentUndoRedo().DoUndo( false );
        pWorkDoc->ReplaceDocumentProperties( *pSourceDoc );

        // import print settings
        const SwPrintData &rPrintData = pSourceDoc->getIDocumentDeviceAccess().getPrintData();
        pWorkDoc->getIDocumentDeviceAccess().setPrintData(rPrintData);
        const JobSetup *pJobSetup = pSourceDoc->getIDocumentDeviceAccess().getJobsetup();
        if (pJobSetup)
            pWorkDoc->getIDocumentDeviceAccess().setJobsetup(*pJobSetup);

        if( aType == WorkingDocType::TARGET )
        {
            assert( !ppDBManager );
            pWorkDoc->SetInMailMerge( true );
            pWorkWrtShell->SetLabelDoc( false );
        }
        else
        {
            // We have to swap the DBmanager of the new doc, so we also need input
            assert(ppDBManager && *ppDBManager);
            SwDBManager *pWorkDBManager = pWorkDoc->GetDBManager();
            pWorkDoc->SetDBManager( *ppDBManager );
            *ppDBManager = pWorkDBManager;

            if( aType == WorkingDocType::SOURCE )
            {
                // the GetDBData call constructs the data, if it's missing - kind of const...
                pWorkWrtShell->ChgDBData( const_cast<SwDoc*>(pSourceDoc)->GetDBData() );
                // some DocumentSettings are currently not copied by SwDoc::CreateCopy
                pWorkWrtShell->SetLabelDoc( rSourceWrtShell.IsLabelDoc() );
                pWorkDoc->getIDocumentState().ResetModified();
            }
            else
                pWorkDoc->getIDocumentLinksAdministration().EmbedAllLinks();
        }

        if( pView )     *pView     = pWorkView;
        if( pWrtShell ) *pWrtShell = pWorkWrtShell;
        if( pDoc )      *pDoc      = pWorkDoc;
    }

    return xWorkObjectShell.get();
}

static rtl::Reference<SwMailMessage> lcl_CreateMailFromDoc(
    const SwMergeDescriptor &rMergeDescriptor,
    const OUString &sFileURL, const OUString &sMailRecipient,
    const OUString &sMailBodyMimeType, rtl_TextEncoding sMailEncoding,
    const OUString &sAttachmentMimeType )
{
    rtl::Reference<SwMailMessage> pMessage = new SwMailMessage;
    if( rMergeDescriptor.pMailMergeConfigItem->IsMailReplyTo() )
        pMessage->setReplyToAddress(rMergeDescriptor.pMailMergeConfigItem->GetMailReplyTo());
    pMessage->addRecipient( sMailRecipient );
    pMessage->SetSenderAddress( rMergeDescriptor.pMailMergeConfigItem->GetMailAddress() );

    OUStringBuffer sBody;
    if( rMergeDescriptor.bSendAsAttachment )
    {
        sBody = rMergeDescriptor.sMailBody;
        mail::MailAttachment aAttach;
        aAttach.Data = new SwMailTransferable( sFileURL,
            rMergeDescriptor.sAttachmentName, sAttachmentMimeType );
        aAttach.ReadableName = rMergeDescriptor.sAttachmentName;
        pMessage->addAttachment( aAttach );
    }
    else
    {
        //read in the temporary file and use it as mail body
        SfxMedium aMedium( sFileURL, StreamMode::READ );
        SvStream* pInStream = aMedium.GetInStream();
        assert( pInStream && "no output file created?" );
        if( !pInStream )
            return pMessage;

        pInStream->SetStreamCharSet( sMailEncoding );
        OStringBuffer sLine;
        while ( pInStream->ReadLine( sLine ) )
        {
            sBody.append(OStringToOUString( sLine, sMailEncoding ) + "\n");
        }
    }
    pMessage->setSubject( rMergeDescriptor.sSubject );
    uno::Reference< datatransfer::XTransferable> xBody =
                new SwMailTransferable( sBody.makeStringAndClear(), sMailBodyMimeType );
    pMessage->setBody( xBody );

    for( const OUString& sCcRecipient : rMergeDescriptor.aCopiesTo )
        pMessage->addCcRecipient( sCcRecipient );
    for( const OUString& sBccRecipient : rMergeDescriptor.aBlindCopiesTo )
        pMessage->addBccRecipient( sBccRecipient );

    return pMessage;
}

class SwDBManager::MailDispatcherListener_Impl : public IMailDispatcherListener
{
    SwDBManager &m_rDBManager;

public:
    explicit MailDispatcherListener_Impl( SwDBManager &rDBManager )
        : m_rDBManager( rDBManager ) {}

    virtual void idle() override {}

    virtual void mailDelivered( uno::Reference< mail::XMailMessage> xMessage ) override
    {
        std::unique_lock aGuard( m_rDBManager.m_pImpl->m_aAllEmailSendMutex );
        if ( m_rDBManager.m_pImpl->m_xLastMessage == xMessage )
            m_rDBManager.m_pImpl->m_xLastMessage.clear();
    }

    virtual void mailDeliveryError( ::rtl::Reference<MailDispatcher> xMailDispatcher,
                uno::Reference< mail::XMailMessage>, const OUString& ) override
    {
        std::unique_lock aGuard( m_rDBManager.m_pImpl->m_aAllEmailSendMutex );
        m_rDBManager.m_aMergeStatus = MergeStatus::Error;
        m_rDBManager.m_pImpl->m_xLastMessage.clear();
        xMailDispatcher->stop();
    }
};

/**
 * Please have a look at the README in the same directory, before you make
 * larger changes in this function!
 */
bool SwDBManager::MergeMailFiles(SwWrtShell* pSourceShell,
                                 const SwMergeDescriptor& rMergeDescriptor)
{
    // deconstruct mail merge type for better readability.
    // uppercase naming is intentional!
    const bool bMT_EMAIL   = rMergeDescriptor.nMergeType == DBMGR_MERGE_EMAIL;
    const bool bMT_SHELL   = rMergeDescriptor.nMergeType == DBMGR_MERGE_SHELL;
    const bool bMT_PRINTER = rMergeDescriptor.nMergeType == DBMGR_MERGE_PRINTER;
    const bool bMT_FILE    = rMergeDescriptor.nMergeType == DBMGR_MERGE_FILE;

    //check if the doc is synchronized and contains at least one linked section
    const bool bSynchronizedDoc = pSourceShell->IsLabelDoc() && pSourceShell->GetSectionFormatCount() > 1;
    const bool bNeedsTempFiles = ( bMT_EMAIL || bMT_FILE );
    const bool bIsMergeSilent = IsMergeSilent();

    bool bCheckSingleFile_ = rMergeDescriptor.bCreateSingleFile;
    OUString sPrefix_ = rMergeDescriptor.sPrefix;
    if( bMT_EMAIL )
    {
        assert( !rMergeDescriptor.bPrefixIsFilename );
        assert(!bCheckSingleFile_);
        bCheckSingleFile_ = false;
    }
    else if( bMT_SHELL || bMT_PRINTER )
    {
        assert(bCheckSingleFile_);
        bCheckSingleFile_ = true;
        assert(sPrefix_.isEmpty());
        sPrefix_.clear();
    }
    const bool bCreateSingleFile = bCheckSingleFile_;
    const OUString sDescriptorPrefix = sPrefix_;

    // Setup for dumping debugging documents
    static const sal_Int32 nMaxDumpDocs = []() {
        if (const char* sEnv = getenv("SW_DEBUG_MAILMERGE_DOCS"))
            return OUString(sEnv, strlen(sEnv), osl_getThreadTextEncoding()).toInt32();
        else
            return sal_Int32(0);
    }();

    ::rtl::Reference< MailDispatcher >          xMailDispatcher;
    ::rtl::Reference< IMailDispatcherListener > xMailListener;
    OUString                            sMailBodyMimeType;
    rtl_TextEncoding                    sMailEncoding = ::osl_getThreadTextEncoding();

    uno::Reference< beans::XPropertySet > xColumnProp;
    uno::Reference< beans::XPropertySet > xPasswordColumnProp;

    // Check for (mandatory) email or (optional) filename column
    SwDBFormatData aColumnDBFormat;
    bool bColumnName = !rMergeDescriptor.sDBcolumn.isEmpty();
    bool bPasswordColumnName = !rMergeDescriptor.sDBPasswordColumn.isEmpty();

    if( ! bColumnName )
    {
        if( bMT_EMAIL )
            return false;
    }
    else
    {
        uno::Reference< sdbcx::XColumnsSupplier > xColsSupp( m_pImpl->pMergeData->xResultSet, uno::UNO_QUERY );
        uno::Reference<container::XNameAccess> xCols = xColsSupp->getColumns();
        if( !xCols->hasByName( rMergeDescriptor.sDBcolumn ) )
            return false;
        uno::Any aCol = xCols->getByName( rMergeDescriptor.sDBcolumn );
        aCol >>= xColumnProp;

        if(bPasswordColumnName)
        {
            aCol = xCols->getByName( rMergeDescriptor.sDBPasswordColumn );
            aCol >>= xPasswordColumnProp;
        }

        aColumnDBFormat.xFormatter = m_pImpl->pMergeData->xFormatter;
        aColumnDBFormat.aNullDate  = m_pImpl->pMergeData->aNullDate;

        if( bMT_EMAIL )
        {
            // Reset internal mail accounting data
            {
                std::unique_lock aGuard(m_pImpl->m_aAllEmailSendMutex);
                m_pImpl->m_xLastMessage.clear();
            }

            xMailDispatcher.set( new MailDispatcher(rMergeDescriptor.xSmtpServer) );
            xMailListener = new MailDispatcherListener_Impl( *this );
            xMailDispatcher->addListener( xMailListener );
            if(!rMergeDescriptor.bSendAsAttachment && rMergeDescriptor.bSendAsHTML)
            {
                sMailBodyMimeType = u"text/html; charset=utf-8"_ustr;
                sMailEncoding = RTL_TEXTENCODING_UTF8;
            }
            else
                sMailBodyMimeType = u"text/plain; charset=UTF-8; format=flowed"_ustr;
        }
    }

    SwDocShell  *pSourceDocSh = pSourceShell->GetView().GetDocShell();

    // setup the output format
    std::shared_ptr<const SfxFilter> pStoreToFilter = SwIoSystem::GetFileFilter(
        pSourceDocSh->GetMedium()->GetURLObject().GetMainURL(INetURLObject::DecodeMechanism::NONE));
    SfxFilterContainer* pFilterContainer = SwDocShell::Factory().GetFilterContainer();
    const OUString* pStoreToFilterOptions = nullptr;

    // if a save_to filter is set then use it - otherwise use the default
    if( bMT_EMAIL && !rMergeDescriptor.bSendAsAttachment )
    {
        OUString sExtension = rMergeDescriptor.bSendAsHTML ? u"html"_ustr : u"txt"_ustr;
        pStoreToFilter = pFilterContainer->GetFilter4Extension(sExtension, SfxFilterFlags::EXPORT);
    }
    else if( !rMergeDescriptor.sSaveToFilter.isEmpty())
    {
        std::shared_ptr<const SfxFilter> pFilter =
                pFilterContainer->GetFilter4FilterName( rMergeDescriptor.sSaveToFilter );
        if(pFilter)
        {
            pStoreToFilter = std::move(pFilter);
            if(!rMergeDescriptor.sSaveToFilterOptions.isEmpty())
                pStoreToFilterOptions = &rMergeDescriptor.sSaveToFilterOptions;
        }
    }
    const bool bIsPDFexport = pStoreToFilter && pStoreToFilter->GetFilterName() == "writer_pdf_Export";
    const bool bIsMultiFile = bMT_FILE && !bCreateSingleFile;

    m_aMergeStatus = MergeStatus::Ok;

    // in case of creating a single resulting file this has to be created here
    SwView*           pTargetView     = rMergeDescriptor.pMailMergeConfigItem ?
                                        rMergeDescriptor.pMailMergeConfigItem->GetTargetView() : nullptr;
    SwWrtShell*       pTargetShell    = nullptr;
    SfxObjectShellRef xTargetDocShell;
    rtl::Reference<SwDoc> pTargetDoc;

    std::unique_ptr< utl::TempFileNamed > aTempFile;
    sal_uInt16 nStartingPageNo = 0;

    std::shared_ptr<weld::GenericDialogController> xProgressDlg;

    comphelper::ScopeGuard restoreInMailMerge(
        [pSourceShell, val = pSourceShell->GetDoc()->IsInMailMerge()]
        { pSourceShell->GetDoc()->SetInMailMerge(val); });
    pSourceShell->GetDoc()->SetInMailMerge(true);
    try
    {
        vcl::Window *pSourceWindow = nullptr;
        if( !bIsMergeSilent )
        {
            // construct the process dialog
            pSourceWindow = &pSourceShell->GetView().GetEditWin();
            if (!bMT_PRINTER)
                xProgressDlg = std::make_shared<CreateMonitor>(pSourceWindow->GetFrameWeld());
            else
            {
                xProgressDlg = std::make_shared<PrintMonitor>(pSourceWindow->GetFrameWeld());
                static_cast<PrintMonitor*>(xProgressDlg.get())->set_title(
                    pSourceDocSh->GetTitle(22));
            }
            weld::DialogController::runAsync(xProgressDlg, [this, &xProgressDlg](sal_Int32 nResult){
                if (nResult == RET_CANCEL)
                    MergeCancel();
                xProgressDlg.reset();
            });

            Application::Reschedule( true );
        }

        if( bCreateSingleFile && !pTargetView )
        {
            // create a target docshell to put the merged document into
            xTargetDocShell = lcl_CreateWorkingDocument( WorkingDocType::TARGET,
                *pSourceShell, bMT_SHELL ? pSourceWindow : nullptr,
                nullptr, &pTargetView, &pTargetShell, &pTargetDoc );

            if (nMaxDumpDocs)
                lcl_SaveDebugDoc( xTargetDocShell.get(), "MergeDoc" );
        }
        else if( pTargetView )
        {
            pTargetShell = pTargetView->GetWrtShellPtr();
            if (pTargetShell)
            {
                pTargetDoc = pTargetShell->GetDoc();
                xTargetDocShell = pTargetView->GetDocShell();
            }
        }

        if( bCreateSingleFile )
        {
            // determine the page style and number used at the start of the source document
            pSourceShell->SttEndDoc(true);
            nStartingPageNo = pSourceShell->GetVirtPageNum();
        }

        // Progress, to prohibit KeyInputs
        SfxProgress aProgress(pSourceDocSh, OUString(), 1);

        // lock all dispatchers
        SfxViewFrame* pViewFrame = SfxViewFrame::GetFirst(pSourceDocSh);
        while (pViewFrame)
        {
            pViewFrame->GetDispatcher()->Lock(true);
            pViewFrame = SfxViewFrame::GetNext(*pViewFrame, pSourceDocSh);
        }

        sal_Int32 nDocNo = 1;

        // For single file mode, the number of pages in the target document so far, which is used
        // by AppendDoc() to adjust position of page-bound objects. Getting this information directly
        // from the target doc would require repeated layouts of the doc, which is expensive, but
        // it can be manually computed from the source documents (for which we do layouts, so the page
        // count is known, and there is a blank page between each of them in the target document).
        int targetDocPageCount = 0;

        if( !bIsMergeSilent && !bMT_PRINTER )
        {
            sal_Int32 nRecordCount = 1;
            lcl_getCountFromResultSet( nRecordCount, m_pImpl->pMergeData.get() );

            // Synchronized docs don't auto-advance the record set, but there is a
            // "security" check, which will always advance the record set, if there
            // is no "next record" field in a synchronized doc => nRecordPerDoc > 0
            sal_Int32 nRecordPerDoc = pSourceShell->GetDoc()
                    ->getIDocumentFieldsAccess().GetRecordsPerDocument();
            if ( bSynchronizedDoc && (nRecordPerDoc > 1) )
                --nRecordPerDoc;
            assert( nRecordPerDoc > 0 );

            sal_Int32 nMaxDocs = nRecordCount / nRecordPerDoc;
            if ( 0 != nRecordCount % nRecordPerDoc )
                nMaxDocs += 1;
            static_cast<CreateMonitor*>(xProgressDlg.get())->SetTotalCount(nMaxDocs);
        }

        sal_Int32 nStartRow, nEndRow;
        bool bFreezedLayouts = false;
        // to collect temporary email files
        std::vector< OUString> aFilesToRemove;

        // The SfxObjectShell will be closed explicitly later but
        // it is more safe to use SfxObjectShellLock here
        SfxObjectShellLock xWorkDocSh;
        SwView*            pWorkView             = nullptr;
        rtl::Reference<SwDoc> pWorkDoc;
        SwDBManager*       pWorkDocOrigDBManager = nullptr;
        SwWrtShell*        pWorkShell            = nullptr;
        bool               bWorkDocInitialized   = false;

        do
        {
            nStartRow = m_pImpl->pMergeData ? m_pImpl->pMergeData->xResultSet->getRow() : 0;

            OUString sColumnData;

            // Read the indicated data column, which should contain a valid mail
            // address or an optional file name
            if( bMT_EMAIL || bColumnName )
            {
                sColumnData = GetDBField( xColumnProp, aColumnDBFormat );
            }

            // create a new temporary file name - only done once in case of bCreateSingleFile
            if( bNeedsTempFiles && ( !bWorkDocInitialized || !bCreateSingleFile ))
            {
                OUString sPrefix = sDescriptorPrefix;
                OUString sLeading;

                //#i97667# if the name is from a database field then it will be used _as is_
                if( bColumnName && !bMT_EMAIL )
                {
                    if (!sColumnData.isEmpty())
                        sLeading = sColumnData;
                    else
                        sLeading = u"_"_ustr;
                }
                else
                {
                    INetURLObject aEntry( sPrefix );
                    sLeading = aEntry.GetBase();
                    aEntry.removeSegment();
                    sPrefix = aEntry.GetMainURL( INetURLObject::DecodeMechanism::NONE );
                }

                OUString sExt(comphelper::string::stripStart(pStoreToFilter->GetDefaultExtension(), '*'));
                aTempFile.reset( new utl::TempFileNamed(sLeading, sColumnData.isEmpty(), sExt, &sPrefix, true) );
                if( !aTempFile->IsValid() )
                {
                    ErrorHandler::HandleError( ERRCODE_IO_NOTSUPPORTED );
                    m_aMergeStatus = MergeStatus::Error;
                }
            }

            uno::Sequence< beans::PropertyValue > aSaveToFilterDataOptions( rMergeDescriptor.aSaveToFilterData );

            if( bMT_EMAIL || bPasswordColumnName )
            {
                OUString sPasswordColumnData = GetDBField( xPasswordColumnProp, aColumnDBFormat );
                lcl_PrepareSaveFilterDataOptions( rMergeDescriptor.aSaveToFilterData, aSaveToFilterDataOptions, sPasswordColumnData );
            }

            if( IsMergeOk() )
            {
                std::unique_ptr< INetURLObject > aTempFileURL;
                if( bNeedsTempFiles )
                    aTempFileURL.reset( new INetURLObject(aTempFile->GetURL()));
                if( !bIsMergeSilent ) {
                    if( !bMT_PRINTER )
                        static_cast<CreateMonitor*>(xProgressDlg.get())->SetCurrentPosition(nDocNo);
                    else {
                        PrintMonitor *pPrintMonDlg = static_cast<PrintMonitor*>(xProgressDlg.get());
                        pPrintMonDlg->m_xPrinter->set_label(bNeedsTempFiles
                            ? aTempFileURL->GetBase() : pSourceDocSh->GetTitle( 2));
                        OUString sStat = SwResId(STR_STATSTR_LETTER) + " " + OUString::number( nDocNo );
                        pPrintMonDlg->m_xPrintInfo->set_label(sStat);
                    }
                    //TODO xProgressDlg->queue_draw();
                }

                Scheduler::ProcessEventsToIdle();

                // Create a copy of the source document and work with that one instead of the source.
                // If we're not in the single file mode (which requires modifying the document for the merging),
                // it is enough to do this just once. Currently PDF also has to be treated special.
                if( !bWorkDocInitialized || bCreateSingleFile || bIsPDFexport || bIsMultiFile )
                {
                    assert( !xWorkDocSh.Is());
                    pWorkDocOrigDBManager = this;
                    xWorkDocSh = lcl_CreateWorkingDocument( WorkingDocType::COPY,
                        *pSourceShell, nullptr, &pWorkDocOrigDBManager,
                        &pWorkView, &pWorkShell, &pWorkDoc );
                    if ( (nMaxDumpDocs < 0) || (nDocNo <= nMaxDumpDocs) )
                        lcl_SaveDebugDoc( xWorkDocSh, "WorkDoc", nDocNo );

                    // #i69458# lock fields to prevent access to the result set while calculating layout
                    // tdf#92324: and do not unlock: keep document locked during printing to avoid
                    // ExpFields update during printing, generation of preview, etc.
                    pWorkShell->LockExpFields();
                    pWorkShell->CalcLayout();
                    // tdf#121168: Now force correct page descriptions applied to page frames. Without
                    // this, e.g., page frames starting with sections could have page descriptions set
                    // wrong. This would lead to wrong page styles applied in SwDoc::AppendDoc below.
                    pWorkShell->GetViewOptions()->SetIdle(true);
                    for (auto aLayout : pWorkShell->GetDoc()->GetAllLayouts())
                    {
                        aLayout->FreezeLayout(false);
                        aLayout->AllCheckPageDescs();
                    }
                }

                lcl_emitEvent(SfxEventHintId::SwEventFieldMerge, STR_SW_EVENT_FIELD_MERGE, xWorkDocSh);

                // tdf#92324: Allow ExpFields update only by explicit instruction to avoid
                // database cursor movement on any other fields update, for example during
                // print preview and other operations
                if ( pWorkShell->IsExpFieldsLocked() )
                    pWorkShell->UnlockExpFields();
                pWorkShell->SwViewShell::UpdateFields();
                pWorkShell->LockExpFields();

                lcl_emitEvent(SfxEventHintId::SwEventFieldMergeFinished, STR_SW_EVENT_FIELD_MERGE_FINISHED, xWorkDocSh);

                // also emit MailMergeEvent on XInterface if possible
                const SwXMailMerge *pEvtSrc = GetMailMergeEvtSrc();
                if(pEvtSrc)
                {
                    rtl::Reference< SwXMailMerge > xRef(
                        const_cast<SwXMailMerge*>(pEvtSrc) );
                    text::MailMergeEvent aEvt( static_cast<text::XMailMergeBroadcaster*>(xRef.get()), xWorkDocSh->GetModel() );
                    SolarMutexReleaser rel;
                    xRef->LaunchMailMergeEvent( aEvt );
                }

                // working copy is merged - prepare final steps depending on merge options

                if( bCreateSingleFile )
                {
                    assert( pTargetShell && "no target shell available!" );

                    // prepare working copy and target to append

                    pWorkDoc->RemoveInvisibleContent();
                    // remove of invisible content has influence on page count and so on fields for page count,
                    // therefore layout has to be updated before fields are converted to text
                    pWorkShell->CalcLayout();
                    pWorkShell->ConvertFieldsToText();
                    pWorkShell->SetNumberingRestart();
                    if( bSynchronizedDoc )
                    {
                        lcl_RemoveSectionLinks( *pWorkShell );
                    }

                    if ( (nMaxDumpDocs < 0) || (nDocNo <= nMaxDumpDocs) )
                        lcl_SaveDebugDoc( xWorkDocSh, "WorkDoc", nDocNo );

                    // append the working document to the target document
                    if( targetDocPageCount % 2 == 1 )
                        ++targetDocPageCount; // Docs always start on odd pages (so offset must be even).
                    SwNodeIndex appendedDocStart = pTargetDoc->AppendDoc( *pWorkDoc,
                        nStartingPageNo, !bWorkDocInitialized, targetDocPageCount, nDocNo);
                    targetDocPageCount += pWorkShell->GetPageCnt();

                    if ( (nMaxDumpDocs < 0) || (nDocNo <= nMaxDumpDocs) )
                        lcl_SaveDebugDoc( xTargetDocShell.get(), "MergeDoc" );

                    if (bMT_SHELL)
                    {
                        SwDocMergeInfo aMergeInfo;
                        // Name of the mark is actually irrelevant, UNO bookmarks have internals names.
                        aMergeInfo.startPageInTarget = pTargetDoc->getIDocumentMarkAccess()->makeMark(
                            SwPaM(appendedDocStart), SwMarkName(), IDocumentMarkAccess::MarkType::UNO_BOOKMARK,
                            ::sw::mark::InsertMode::New);
                        aMergeInfo.nDBRow = nStartRow;
                        rMergeDescriptor.pMailMergeConfigItem->AddMergedDocument( aMergeInfo );
                    }
                }
                else
                {
                    assert( bNeedsTempFiles );
                    assert( pWorkShell->IsExpFieldsLocked() );

                    if (bIsMultiFile && pWorkDoc->HasInvisibleContent())
                    {
                        pWorkDoc->RemoveInvisibleContent();
                        pWorkShell->CalcLayout();
                        pWorkShell->ConvertFieldsToText();
                    }

                    // fields are locked, so it's fine to
                    // restore the old / empty DB manager for save
                    pWorkDoc->SetDBManager( pWorkDocOrigDBManager );

                    // save merged document
                    OUString sFileURL;
                    if( !lcl_SaveDoc( aTempFileURL.get(), pStoreToFilter, pStoreToFilterOptions,
                                    &aSaveToFilterDataOptions, bIsPDFexport,
                                    xWorkDocSh, *pWorkShell, &sFileURL ) )
                    {
                        m_aMergeStatus = MergeStatus::Error;
                    }

                    // back to the MM DB manager
                    pWorkDoc->SetDBManager( this );

                    if( bMT_EMAIL && !IsMergeError() )
                    {
                        // schedule file for later removal
                        aFilesToRemove.push_back( sFileURL );

                        if( !SwMailMergeHelper::CheckMailAddress( sColumnData ) )
                        {
                            OSL_FAIL("invalid e-Mail address in database column");
                        }
                        else
                        {
                            rtl::Reference< SwMailMessage > xMessage = lcl_CreateMailFromDoc(
                                rMergeDescriptor, sFileURL, sColumnData, sMailBodyMimeType,
                                sMailEncoding, pStoreToFilter->GetMimeType() );
                            if( xMessage.is() )
                            {
                                std::unique_lock aGuard( m_pImpl->m_aAllEmailSendMutex );
                                m_pImpl->m_xLastMessage.set( xMessage );
                                xMailDispatcher->enqueueMailMessage( xMessage );
                                if( !xMailDispatcher->isStarted() )
                                    xMailDispatcher->start();
                            }
                        }
                    }
                }
                if( bCreateSingleFile || bIsPDFexport || bIsMultiFile)
                {
                    pWorkDoc->SetDBManager( pWorkDocOrigDBManager );
                    pWorkDoc.clear();
                    xWorkDocSh->DoClose();
                    xWorkDocSh = nullptr;
                }
            }

            bWorkDocInitialized = true;
            nDocNo++;
            nEndRow = m_pImpl->pMergeData ? m_pImpl->pMergeData->xResultSet->getRow() : 0;

            // Freeze the layouts of the target document after the first inserted
            // sub-document, to get the correct PageDesc.
            if(!bFreezedLayouts && bCreateSingleFile)
            {
                for ( auto aLayout : pTargetShell->GetDoc()->GetAllLayouts() )
                    aLayout->FreezeLayout(true);
                bFreezedLayouts = true;
            }
        } while( IsMergeOk() &&
            ((bSynchronizedDoc && (nStartRow != nEndRow)) ? IsValidMergeRecord() : ToNextMergeRecord()));

        if ( xWorkDocSh.Is() && pWorkView->GetWrtShell().IsExpFieldsLocked() )
        {
            // Unlock document fields after merge complete
            pWorkView->GetWrtShell().UnlockExpFields();
        }

        if( !bCreateSingleFile )
        {
            if( bMT_PRINTER )
                Printer::FinishPrintJob( pWorkView->GetPrinterController());
            if( !bIsPDFexport )
            {
                if (pWorkDoc)
                    pWorkDoc->SetDBManager(pWorkDocOrigDBManager);
                if (xWorkDocSh.Is())
                    xWorkDocSh->DoClose();
            }
        }
        else if( IsMergeOk() ) // && bCreateSingleFile
        {
            Application::Reschedule( true );

            // sw::DocumentLayoutManager::CopyLayoutFormat() did not generate
            // unique fly names, do it here once.
            pTargetDoc->SetInMailMerge(false);
            pTargetDoc->SetAllUniqueFlyNames();

            // Unfreeze target document layouts and correct all PageDescs.
            SAL_INFO( "sw.pageframe", "(MergeMailFiles pTargetShell->CalcLayout in" );
            pTargetShell->CalcLayout();
            SAL_INFO( "sw.pageframe", "MergeMailFiles pTargetShell->CalcLayout out)" );
            pTargetShell->GetViewOptions()->SetIdle( true );
            pTargetDoc->GetIDocumentUndoRedo().DoUndo( true );
            for ( auto aLayout : pTargetShell->GetDoc()->GetAllLayouts() )
            {
                aLayout->FreezeLayout(false);
                aLayout->AllCheckPageDescs();
            }

            Application::Reschedule( true );

            if( IsMergeOk() && bMT_FILE )
            {
                // save merged document
                assert( aTempFile );
                INetURLObject aTempFileURL;
                if (sDescriptorPrefix.isEmpty() || !rMergeDescriptor.bPrefixIsFilename)
                    aTempFileURL.SetURL( aTempFile->GetURL() );
                else
                {
                    aTempFileURL.SetURL(sDescriptorPrefix);
                    // remove the unneeded temporary file
                    aTempFile->EnableKillingFile();
                }
                if( !lcl_SaveDoc( &aTempFileURL, pStoreToFilter,
                        pStoreToFilterOptions, &rMergeDescriptor.aSaveToFilterData,
                        bIsPDFexport, xTargetDocShell.get(), *pTargetShell ) )
                {
                    m_aMergeStatus = MergeStatus::Error;
                }
            }
            else if( IsMergeOk() && bMT_PRINTER )
            {
                // print the target document
                uno::Sequence< beans::PropertyValue > aOptions( rMergeDescriptor.aPrintOptions );
                lcl_PreparePrinterOptions( rMergeDescriptor.aPrintOptions, aOptions );
                pTargetView->ExecPrint( aOptions, bIsMergeSilent, false/*bPrintAsync*/ );
            }
        }

        // we also show canceled documents, as long as there was no error
        if( !IsMergeError() && bMT_SHELL )
            // leave docshell available for caller (e.g. MM wizard)
            rMergeDescriptor.pMailMergeConfigItem->SetTargetView( pTargetView );
        else if( xTargetDocShell.is() )
            xTargetDocShell->DoClose();

        Application::Reschedule( true );

        if (xProgressDlg)
        {
            xProgressDlg->response(RET_OK);
        }

        // unlock all dispatchers
        pViewFrame = SfxViewFrame::GetFirst(pSourceDocSh);
        while (pViewFrame)
        {
            pViewFrame->GetDispatcher()->Lock(false);
            pViewFrame = SfxViewFrame::GetNext(*pViewFrame, pSourceDocSh);
        }

        SwModule::get()->SetView(&pSourceShell->GetView());

        if( xMailDispatcher.is() )
        {
            if( IsMergeOk() )
            {
                // TODO: Instead of polling via an AutoTimer, post an Idle event,
                // if the main loop has been made thread-safe.
                AutoTimer aEmailDispatcherPollTimer("sw::SwDBManager aEmailDispatcherPollTimer");
                aEmailDispatcherPollTimer.SetTimeout( 500 );
                aEmailDispatcherPollTimer.Start();
                while( IsMergeOk() && m_pImpl->m_xLastMessage.is() && !Application::IsQuit())
                    Application::Yield();
                aEmailDispatcherPollTimer.Stop();
            }
            xMailDispatcher->stop();
            xMailDispatcher->shutdown();
        }

        // remove the temporary files
        // has to be done after xMailDispatcher is finished, as mails may be
        // delivered as message attachments!
        for( const OUString &sFileURL : aFilesToRemove )
            SWUnoHelper::UCB_DeleteFile( sFileURL );
    }
    catch (const uno::Exception&)
    {
        if (xProgressDlg)
        {
            xProgressDlg->response(RET_CANCEL);
        }
    }

    return !IsMergeError();
}

void SwDBManager::MergeCancel()
{
    if (m_aMergeStatus < MergeStatus::Cancel)
        m_aMergeStatus = MergeStatus::Cancel;
}

// determine the column's Numberformat and transfer to the forwarded Formatter,
// if applicable.
sal_uInt32 SwDBManager::GetColumnFormat( const OUString& rDBName,
                                const OUString& rTableName,
                                const OUString& rColNm,
                                SvNumberFormatter* pNFormatr,
                                LanguageType nLanguage )
{
    sal_uInt32 nRet = 0;
    if(pNFormatr)
    {
        uno::Reference< sdbc::XDataSource> xSource;
        uno::Reference< sdbc::XConnection> xConnection;
        bool bUseMergeData = false;
        uno::Reference< sdbcx::XColumnsSupplier> xColsSupp;
        bool bDisposeConnection = false;
        if(m_pImpl->pMergeData &&
            ((m_pImpl->pMergeData->sDataSource == rDBName && m_pImpl->pMergeData->sCommand == rTableName) ||
            (rDBName.isEmpty() && rTableName.isEmpty())))
        {
            xConnection = m_pImpl->pMergeData->xConnection;
            xSource = SwDBManager::getDataSourceAsParent(xConnection,rDBName);
            bUseMergeData = true;
            xColsSupp.set(m_pImpl->pMergeData->xResultSet, css::uno::UNO_QUERY);
        }
        if(!xConnection.is())
        {
            SwDBData aData;
            aData.sDataSource = rDBName;
            aData.sCommand = rTableName;
            aData.nCommandType = -1;
            SwDSParam* pParam = FindDSData(aData, false);
            if(pParam && pParam->xConnection.is())
            {
                xConnection = pParam->xConnection;
                xColsSupp.set(pParam->xResultSet, css::uno::UNO_QUERY);
            }
            else
            {
                xConnection = RegisterConnection( rDBName );
                bDisposeConnection = true;
            }
            if(bUseMergeData)
                m_pImpl->pMergeData->xConnection = xConnection;
        }
        bool bDispose = !xColsSupp.is();
        if(bDispose)
        {
            xColsSupp = SwDBManager::GetColumnSupplier(xConnection, rTableName);
        }
        if(xColsSupp.is())
        {
            uno::Reference<container::XNameAccess> xCols;
            try
            {
                xCols = xColsSupp->getColumns();
            }
            catch (const uno::Exception&)
            {
                TOOLS_WARN_EXCEPTION("sw.mailmerge", "Exception in getColumns()");
            }
            if(!xCols.is() || !xCols->hasByName(rColNm))
                return nRet;
            uno::Any aCol = xCols->getByName(rColNm);
            uno::Reference< beans::XPropertySet > xColumn;
            aCol >>= xColumn;
            nRet = GetColumnFormat(xSource, xConnection, xColumn, pNFormatr, nLanguage);
            if(bDispose)
            {
                ::comphelper::disposeComponent( xColsSupp );
            }
            if(bDisposeConnection)
            {
                ::comphelper::disposeComponent( xConnection );
            }
        }
        else
            nRet = pNFormatr->GetFormatIndex( NF_NUMBER_STANDARD, LANGUAGE_SYSTEM );
    }
    return nRet;
}

sal_uInt32 SwDBManager::GetColumnFormat( uno::Reference< sdbc::XDataSource> const & xSource_in,
                        uno::Reference< sdbc::XConnection> const & xConnection,
                        uno::Reference< beans::XPropertySet> const & xColumn,
                        SvNumberFormatter* pNFormatr,
                        LanguageType nLanguage )
{
    auto xSource = xSource_in;

    // set the NumberFormat in the doc if applicable
    sal_uInt32 nRet = 0;

    if(!xSource.is())
    {
        uno::Reference<container::XChild> xChild(xConnection, uno::UNO_QUERY);
        if ( xChild.is() )
            xSource.set(xChild->getParent(), uno::UNO_QUERY);
    }
    if(xSource.is() && xConnection.is() && xColumn.is() && pNFormatr)
    {
        rtl::Reference<SvNumberFormatsSupplierObj> pNumFormat = new SvNumberFormatsSupplierObj( pNFormatr );
        uno::Reference< util::XNumberFormats > xDocNumberFormats = pNumFormat->getNumberFormats();
        uno::Reference< util::XNumberFormatTypes > xDocNumberFormatTypes(xDocNumberFormats, uno::UNO_QUERY);

        css::lang::Locale aLocale( LanguageTag( nLanguage ).getLocale());

        //get the number formatter of the data source
        uno::Reference<beans::XPropertySet> xSourceProps(xSource, uno::UNO_QUERY);
        uno::Reference< util::XNumberFormats > xNumberFormats;
        if(xSourceProps.is())
        {
            uno::Any aFormats = xSourceProps->getPropertyValue(u"NumberFormatsSupplier"_ustr);
            if(aFormats.hasValue())
            {
                uno::Reference<util::XNumberFormatsSupplier> xSuppl;
                aFormats >>= xSuppl;
                if(xSuppl.is())
                {
                    xNumberFormats = xSuppl->getNumberFormats();
                }
            }
        }
        bool bUseDefault = true;
        try
        {
            uno::Any aFormatKey = xColumn->getPropertyValue(u"FormatKey"_ustr);
            if(aFormatKey.hasValue())
            {
                sal_Int32 nFormat = 0;
                aFormatKey >>= nFormat;
                if(xNumberFormats.is())
                {
                    try
                    {
                        uno::Reference<beans::XPropertySet> xNumProps = xNumberFormats->getByKey( nFormat );
                        uno::Any aFormatString = xNumProps->getPropertyValue(u"FormatString"_ustr);
                        uno::Any aLocaleVal = xNumProps->getPropertyValue(u"Locale"_ustr);
                        OUString sFormat;
                        aFormatString >>= sFormat;
                        lang::Locale aLoc;
                        aLocaleVal >>= aLoc;
                        nFormat = xDocNumberFormats->queryKey( sFormat, aLoc, false );
                        if(NUMBERFORMAT_ENTRY_NOT_FOUND == sal::static_int_cast< sal_uInt32, sal_Int32>(nFormat))
                            nFormat = xDocNumberFormats->addNew( sFormat, aLoc );

                        nRet = nFormat;
                        bUseDefault = false;
                    }
                    catch (const uno::Exception&)
                    {
                        TOOLS_WARN_EXCEPTION("sw.mailmerge", "illegal number format key");
                    }
                }
            }
        }
        catch(const uno::Exception&)
        {
            SAL_WARN("sw.mailmerge", "no FormatKey property found");
        }
        if(bUseDefault)
            nRet = dbtools::getDefaultNumberFormat(xColumn, xDocNumberFormatTypes,  aLocale);
    }
    return nRet;
}

sal_Int32 SwDBManager::GetColumnType( const OUString& rDBName,
                          const OUString& rTableName,
                          const OUString& rColNm )
{
    sal_Int32 nRet = sdbc::DataType::SQLNULL;
    SwDBData aData;
    aData.sDataSource = rDBName;
    aData.sCommand = rTableName;
    aData.nCommandType = -1;
    SwDSParam* pParam = FindDSData(aData, false);
    uno::Reference< sdbc::XConnection> xConnection;
    uno::Reference< sdbcx::XColumnsSupplier > xColsSupp;
    bool bDispose = false;
    if(pParam && pParam->xConnection.is())
    {
        xConnection = pParam->xConnection;
        xColsSupp.set( pParam->xResultSet, uno::UNO_QUERY );
    }
    else
    {
        xConnection = RegisterConnection( rDBName );
    }
    if( !xColsSupp.is() )
    {
        xColsSupp = SwDBManager::GetColumnSupplier(xConnection, rTableName);
        bDispose = true;
    }
    if(xColsSupp.is())
    {
        uno::Reference<container::XNameAccess> xCols = xColsSupp->getColumns();
        if(xCols->hasByName(rColNm))
        {
            uno::Any aCol = xCols->getByName(rColNm);
            uno::Reference<beans::XPropertySet> xCol;
            aCol >>= xCol;
            uno::Any aType = xCol->getPropertyValue(u"Type"_ustr);
            aType >>= nRet;
        }
        if(bDispose)
            ::comphelper::disposeComponent( xColsSupp );
    }
    return nRet;
}

uno::Reference< sdbc::XConnection> SwDBManager::GetConnection(const OUString& rDataSource,
                                                              uno::Reference<sdbc::XDataSource>& rxSource, const SwView *pView)
{
    uno::Reference< sdbc::XConnection> xConnection;
    const uno::Reference< uno::XComponentContext >& xContext( ::comphelper::getProcessComponentContext() );
    try
    {
        uno::Reference<sdb::XCompletedConnection> xComplConnection(dbtools::getDataSource(rDataSource, xContext), uno::UNO_QUERY);
        if ( !xComplConnection )
            return xConnection;
        rxSource.set(xComplConnection, uno::UNO_QUERY);
        weld::Window* pWindow = pView ? pView->GetFrameWeld() : nullptr;
        uno::Reference< task::XInteractionHandler > xHandler( task::InteractionHandler::createWithParent(xContext, pWindow ? pWindow->GetXWindow() : nullptr) );
        if (!xHandler)
            return xConnection;
        xConnection = xComplConnection->connectWithCompletion( xHandler );
    }
    catch(const uno::Exception&)
    {
    }

    return xConnection;
}

uno::Reference< sdbcx::XColumnsSupplier> SwDBManager::GetColumnSupplier(uno::Reference<sdbc::XConnection> const & xConnection,
                                    const OUString& rTableOrQuery,
                                    SwDBSelect   eTableOrQuery)
{
    uno::Reference< sdbcx::XColumnsSupplier> xRet;
    try
    {
        if(eTableOrQuery == SwDBSelect::UNKNOWN)
        {
            //search for a table with the given command name
            uno::Reference<sdbcx::XTablesSupplier> xTSupplier(xConnection, uno::UNO_QUERY);
            if(xTSupplier.is())
            {
                uno::Reference<container::XNameAccess> xTables = xTSupplier->getTables();
                eTableOrQuery = xTables->hasByName(rTableOrQuery) ?
                            SwDBSelect::TABLE : SwDBSelect::QUERY;
            }
        }
        sal_Int32 nCommandType = SwDBSelect::TABLE == eTableOrQuery ?
                sdb::CommandType::TABLE : sdb::CommandType::QUERY;
        uno::Reference< lang::XMultiServiceFactory > xMgr( ::comphelper::getProcessServiceFactory() );
        uno::Reference<sdbc::XRowSet> xRowSet(xMgr->createInstance(u"com.sun.star.sdb.RowSet"_ustr), uno::UNO_QUERY);

        OUString sDataSource;
        uno::Reference<sdbc::XDataSource> xSource = SwDBManager::getDataSourceAsParent(xConnection, sDataSource);
        uno::Reference<beans::XPropertySet> xSourceProperties(xSource, uno::UNO_QUERY);
        if(xSourceProperties.is())
        {
            xSourceProperties->getPropertyValue(u"Name"_ustr) >>= sDataSource;
        }

        uno::Reference<beans::XPropertySet> xRowProperties(xRowSet, uno::UNO_QUERY);
        xRowProperties->setPropertyValue(u"DataSourceName"_ustr, uno::Any(sDataSource));
        xRowProperties->setPropertyValue(u"Command"_ustr, uno::Any(rTableOrQuery));
        xRowProperties->setPropertyValue(u"CommandType"_ustr, uno::Any(nCommandType));
        xRowProperties->setPropertyValue(u"FetchSize"_ustr, uno::Any(sal_Int32(10)));
        xRowProperties->setPropertyValue(u"ActiveConnection"_ustr, uno::Any(xConnection));
        xRowSet->execute();
        xRet.set( xRowSet, uno::UNO_QUERY );
    }
    catch (const uno::Exception&)
    {
        TOOLS_WARN_EXCEPTION("sw.mailmerge", "Exception in SwDBManager::GetColumnSupplier");
    }

    return xRet;
}

OUString SwDBManager::GetDBField(uno::Reference<beans::XPropertySet> const & xColumnProps,
                        const SwDBFormatData& rDBFormatData,
                        double* pNumber)
{
    uno::Reference< sdb::XColumn > xColumn(xColumnProps, uno::UNO_QUERY);
    OUString sRet;
    assert( xColumn.is() && "SwDBManager::::ImportDBField: illegal arguments" );
    if(!xColumn.is())
        return sRet;

    uno::Any aType = xColumnProps->getPropertyValue(u"Type"_ustr);
    sal_Int32 eDataType = sdbc::DataType::SQLNULL;
    aType >>= eDataType;
    switch(eDataType)
    {
        case sdbc::DataType::CHAR:
        case sdbc::DataType::VARCHAR:
        case sdbc::DataType::LONGVARCHAR:
            try
            {
                sRet = xColumn->getString();
                sRet = sRet.replace( '\xb', '\n' ); // MSWord uses \xb as a newline
            }
            catch(const sdbc::SQLException&)
            {
            }
        break;
        case sdbc::DataType::BIT:
        case sdbc::DataType::BOOLEAN:
        case sdbc::DataType::TINYINT:
        case sdbc::DataType::SMALLINT:
        case sdbc::DataType::INTEGER:
        case sdbc::DataType::BIGINT:
        case sdbc::DataType::FLOAT:
        case sdbc::DataType::REAL:
        case sdbc::DataType::DOUBLE:
        case sdbc::DataType::NUMERIC:
        case sdbc::DataType::DECIMAL:
        case sdbc::DataType::DATE:
        case sdbc::DataType::TIME:
        case sdbc::DataType::TIMESTAMP:
        {

            try
            {
                sRet = dbtools::DBTypeConversion::getFormattedValue(
                    xColumnProps,
                    rDBFormatData.xFormatter,
                    rDBFormatData.aLocale,
                    rDBFormatData.aNullDate);
                if (pNumber)
                {
                    double fVal = xColumn->getDouble();
                    if(!xColumn->wasNull())
                    {
                        *pNumber = fVal;
                    }
                }
            }
            catch (const uno::Exception&)
            {
                TOOLS_WARN_EXCEPTION("sw.mailmerge", "");
            }

        }
        break;
    }

    return sRet;
}

// checks if a desired data source table or query is open
bool    SwDBManager::IsDataSourceOpen(const OUString& rDataSource,
                                  const OUString& rTableOrQuery, bool bMergeShell)
{
    if(m_pImpl->pMergeData)
    {
        return ((rDataSource == m_pImpl->pMergeData->sDataSource
                 && rTableOrQuery == m_pImpl->pMergeData->sCommand)
                || (rDataSource.isEmpty() && rTableOrQuery.isEmpty()))
               && m_pImpl->pMergeData->xResultSet.is();
    }
    else if(!bMergeShell)
    {
        SwDBData aData;
        aData.sDataSource = rDataSource;
        aData.sCommand = rTableOrQuery;
        aData.nCommandType = -1;
        SwDSParam* pFound = FindDSData(aData, false);
        return (pFound && pFound->xResultSet.is());
    }
    return false;
}

// read column data at a specified position
bool SwDBManager::GetColumnCnt(const OUString& rSourceName, const OUString& rTableName,
                           const OUString& rColumnName, sal_uInt32 nAbsRecordId,
                           LanguageType nLanguage,
                           OUString& rResult, double* pNumber)
{
    bool bRet = false;
    SwDSParam* pFound = nullptr;
    //check if it's the merge data source
    if(m_pImpl->pMergeData &&
        rSourceName == m_pImpl->pMergeData->sDataSource &&
        rTableName == m_pImpl->pMergeData->sCommand)
    {
        pFound = m_pImpl->pMergeData.get();
    }
    else
    {
        SwDBData aData;
        aData.sDataSource = rSourceName;
        aData.sCommand = rTableName;
        aData.nCommandType = -1;
        pFound = FindDSData(aData, false);
    }
    if (!pFound)
        return false;
    //check validity of supplied record Id
    if(pFound->aSelection.hasElements())
    {
        //the destination has to be an element of the selection
        bool bFound = std::any_of(std::cbegin(pFound->aSelection), std::cend(pFound->aSelection),
            [nAbsRecordId](const uno::Any& rSelection) {
                sal_Int32 nSelection = 0;
                rSelection >>= nSelection;
                return nSelection == static_cast<sal_Int32>(nAbsRecordId);
            });
        if(!bFound)
            return false;
    }
    if( pFound->HasValidRecord() )
    {
        sal_Int32 nOldRow = 0;
        try
        {
            nOldRow = pFound->xResultSet->getRow();
        }
        catch(const uno::Exception&)
        {
            return false;
        }
        //position to the desired index
        bool bMove = true;
        if ( nOldRow != static_cast<sal_Int32>(nAbsRecordId) )
            bMove = lcl_MoveAbsolute(pFound, nAbsRecordId);
        if(bMove)
            bRet = lcl_GetColumnCnt(pFound, rColumnName, nLanguage, rResult, pNumber);
        if ( nOldRow != static_cast<sal_Int32>(nAbsRecordId) )
            lcl_MoveAbsolute(pFound, nOldRow);
    }
    return bRet;
}

// reads the column data at the current position
bool    SwDBManager::GetMergeColumnCnt(const OUString& rColumnName, LanguageType nLanguage,
                                   OUString &rResult, double *pNumber)
{
    if( !IsValidMergeRecord() )
    {
        rResult.clear();
        return false;
    }

    bool bRet = lcl_GetColumnCnt(m_pImpl->pMergeData.get(), rColumnName, nLanguage, rResult, pNumber);
    return bRet;
}

bool SwDBManager::ToNextMergeRecord()
{
    assert( m_pImpl->pMergeData && m_pImpl->pMergeData->xResultSet.is() && "no data source in merge" );
    return lcl_ToNextRecord( m_pImpl->pMergeData.get() );
}

bool SwDBManager::FillCalcWithMergeData( SvNumberFormatter *pDocFormatter,
                                         LanguageType nLanguage, SwCalc &rCalc )
{
    if( !IsValidMergeRecord() )
        return false;

    uno::Reference< sdbcx::XColumnsSupplier > xColsSupp( m_pImpl->pMergeData->xResultSet, uno::UNO_QUERY );
    if( !xColsSupp.is() )
        return false;

    {
        uno::Reference<container::XNameAccess> xCols = xColsSupp->getColumns();
        const uno::Sequence<OUString> aColNames = xCols->getElementNames();
        OUString aString;

        // add the "record number" variable, as SwCalc::VarLook would.
        rCalc.VarChange( GetAppCharClass().lowercase(
            SwFieldType::GetTypeStr(SwFieldTypesEnum::DatabaseSetNumber) ), GetSelectedRecordId() );

        for( const OUString& rColName : aColNames )
        {
            // get the column type
            sal_Int32 nColumnType = sdbc::DataType::SQLNULL;
            uno::Any aCol = xCols->getByName( rColName );
            uno::Reference<beans::XPropertySet> xColumnProps;
            aCol >>= xColumnProps;
            uno::Any aType = xColumnProps->getPropertyValue( u"Type"_ustr );
            aType >>= nColumnType;
            double aNumber = DBL_MAX;

            lcl_GetColumnCnt( m_pImpl->pMergeData.get(), xColumnProps, nLanguage, aString, &aNumber );

            sal_uInt32 nFormat = GetColumnFormat( m_pImpl->pMergeData->sDataSource,
                                            m_pImpl->pMergeData->sCommand,
                                            rColName, pDocFormatter, nLanguage );
            // aNumber is overwritten by SwDBField::FormatValue, so store initial status
            bool colIsNumber = aNumber != DBL_MAX;
            bool bValidValue = SwDBField::FormatValue( pDocFormatter, aString, nFormat,
                                                       aNumber, nColumnType );
            if( colIsNumber )
            {
                if( bValidValue )
                {
                    SwSbxValue aValue;
                    aValue.PutDouble( aNumber );
                    aValue.SetDBvalue( true );
                    SAL_INFO( "sw.ui", "'" << rColName << "': " << aNumber << " / " << aString );
                    rCalc.VarChange( rColName, aValue );
                }
            }
            else
            {
                SwSbxValue aValue;
                aValue.PutString( aString );
                aValue.SetDBvalue( true );
                SAL_INFO( "sw.ui", "'" << rColName << "': " << aString );
                rCalc.VarChange( rColName, aValue );
            }
        }
    }

    return true;
}

void SwDBManager::ToNextRecord(
    const OUString& rDataSource, const OUString& rCommand)
{
    SwDSParam* pFound = nullptr;
    if(m_pImpl->pMergeData &&
        rDataSource == m_pImpl->pMergeData->sDataSource &&
        rCommand == m_pImpl->pMergeData->sCommand)
    {
        pFound = m_pImpl->pMergeData.get();
    }
    else
    {
        SwDBData aData;
        aData.sDataSource = rDataSource;
        aData.sCommand = rCommand;
        aData.nCommandType = -1;
        pFound = FindDSData(aData, false);
    }
    lcl_ToNextRecord( pFound );
}

static bool lcl_ToNextRecord( SwDSParam* pParam, const SwDBNextRecord action )
{
    bool bRet = true;

    assert( SwDBNextRecord::NEXT == action ||
         (SwDBNextRecord::FIRST == action && pParam) );
    if( nullptr == pParam )
        return false;

    if( action == SwDBNextRecord::FIRST )
    {
        pParam->nSelectionIndex = 0;
        pParam->bEndOfDB        = false;
    }

    if( !pParam->HasValidRecord() )
        return false;

    try
    {
        if( pParam->aSelection.hasElements() )
        {
            if( pParam->nSelectionIndex >= pParam->aSelection.getLength() )
                pParam->bEndOfDB = true;
            else
            {
                sal_Int32 nPos = 0;
                pParam->aSelection.getConstArray()[ pParam->nSelectionIndex ] >>= nPos;
                pParam->bEndOfDB = !pParam->xResultSet->absolute( nPos );
            }
        }
        else if( action == SwDBNextRecord::FIRST )
        {
            pParam->bEndOfDB = !pParam->xResultSet->first();
        }
        else
        {
            sal_Int32 nBefore = pParam->xResultSet->getRow();
            pParam->bEndOfDB = !pParam->xResultSet->next();
            if( !pParam->bEndOfDB && nBefore == pParam->xResultSet->getRow() )
            {
                // next returned true but it didn't move
                ::dbtools::throwFunctionSequenceException( pParam->xResultSet );
            }
        }

        ++pParam->nSelectionIndex;
        bRet = !pParam->bEndOfDB;
    }
    catch( const uno::Exception & )
    {
        // we allow merging with empty databases, so don't warn on init
        TOOLS_WARN_EXCEPTION_IF(action == SwDBNextRecord::NEXT,
                    "sw.mailmerge", "exception in ToNextRecord()");
        pParam->bEndOfDB = true;
        bRet = false;
    }
    return bRet;
}

// synchronized labels contain a next record field at their end
// to assure that the next page can be created in mail merge
// the cursor position must be validated
bool SwDBManager::IsValidMergeRecord() const
{
    return( m_pImpl->pMergeData && m_pImpl->pMergeData->HasValidRecord() );
}

sal_uInt32  SwDBManager::GetSelectedRecordId()
{
    sal_uInt32  nRet = 0;
    assert( m_pImpl->pMergeData &&
            m_pImpl->pMergeData->xResultSet.is() && "no data source in merge" );
    if(!m_pImpl->pMergeData || !m_pImpl->pMergeData->xResultSet.is())
        return 0;
    try
    {
        nRet = m_pImpl->pMergeData->xResultSet->getRow();
    }
    catch(const uno::Exception&)
    {
    }
    return nRet;
}

bool SwDBManager::ToRecordId(sal_Int32 nSet)
{
    assert( m_pImpl->pMergeData &&
            m_pImpl->pMergeData->xResultSet.is() && "no data source in merge" );
    if(!m_pImpl->pMergeData || !m_pImpl->pMergeData->xResultSet.is()|| nSet < 0)
        return false;
    bool bRet = false;
    sal_Int32 nAbsPos = nSet;
    assert(nAbsPos >= 0);
    bRet = lcl_MoveAbsolute(m_pImpl->pMergeData.get(), nAbsPos);
    m_pImpl->pMergeData->bEndOfDB = !bRet;
    return bRet;
}

bool SwDBManager::OpenDataSource(const OUString& rDataSource, const OUString& rTableOrQuery)
{
    SwDBData aData;
    aData.sDataSource = rDataSource;
    aData.sCommand = rTableOrQuery;
    aData.nCommandType = -1;

    SwDSParam* pFound = FindDSData(aData, true);
    if(pFound->xResultSet.is())
        return true;
    SwDSParam* pParam = FindDSConnection(rDataSource, false);
    if(pParam && pParam->xConnection.is())
        pFound->xConnection = pParam->xConnection;
    if(pFound->xConnection.is())
    {
        try
        {
            uno::Reference< sdbc::XDatabaseMetaData >  xMetaData = pFound->xConnection->getMetaData();
            try
            {
                pFound->bScrollable = xMetaData
                        ->supportsResultSetType(sal_Int32(sdbc::ResultSetType::SCROLL_INSENSITIVE));
            }
            catch(const uno::Exception&)
            {
                // DB driver may not be ODBC 3.0 compliant
                pFound->bScrollable = true;
            }
            pFound->xStatement = pFound->xConnection->createStatement();
            OUString aQuoteChar = xMetaData->getIdentifierQuoteString();
            OUString sStatement = "SELECT * FROM " + aQuoteChar + rTableOrQuery + aQuoteChar;
            pFound->xResultSet = pFound->xStatement->executeQuery( sStatement );

            //after executeQuery the cursor must be positioned
            pFound->bEndOfDB = !pFound->xResultSet->next();
            ++pFound->nSelectionIndex;
        }
        catch (const uno::Exception&)
        {
            pFound->xResultSet = nullptr;
            pFound->xStatement = nullptr;
            pFound->xConnection = nullptr;
        }
    }
    return pFound->xResultSet.is();
}

uno::Reference< sdbc::XConnection> const & SwDBManager::RegisterConnection(OUString const& rDataSource)
{
    SwDSParam* pFound = SwDBManager::FindDSConnection(rDataSource, true);
    uno::Reference< sdbc::XDataSource> xSource;
    if(!pFound->xConnection.is())
    {
        SwView* pView = (m_pDoc && m_pDoc->GetDocShell()) ? m_pDoc->GetDocShell()->GetView() : nullptr;
        pFound->xConnection = SwDBManager::GetConnection(rDataSource, xSource, pView);
        try
        {
            uno::Reference<lang::XComponent> xComponent(pFound->xConnection, uno::UNO_QUERY);
            if(xComponent.is())
                xComponent->addEventListener(m_pImpl->m_xDisposeListener);
        }
        catch(const uno::Exception&)
        {
        }
    }
    return pFound->xConnection;
}

sal_uInt32      SwDBManager::GetSelectedRecordId(
    const OUString& rDataSource, const OUString& rTableOrQuery, sal_Int32 nCommandType)
{
    sal_uInt32 nRet = 0xffffffff;
    //check for merge data source first
    if(m_pImpl->pMergeData &&
        ((rDataSource == m_pImpl->pMergeData->sDataSource &&
        rTableOrQuery == m_pImpl->pMergeData->sCommand) ||
        (rDataSource.isEmpty() && rTableOrQuery.isEmpty())) &&
        (nCommandType == -1 || nCommandType == m_pImpl->pMergeData->nCommandType) &&
        m_pImpl->pMergeData->xResultSet.is())
    {
        nRet = GetSelectedRecordId();
    }
    else
    {
        SwDBData aData;
        aData.sDataSource = rDataSource;
        aData.sCommand = rTableOrQuery;
        aData.nCommandType = nCommandType;
        SwDSParam* pFound = FindDSData(aData, false);
        if(pFound && pFound->xResultSet.is())
        {
            try
            {   //if a selection array is set the current row at the result set may not be set yet
                if(pFound->aSelection.hasElements())
                {
                    sal_Int32 nSelIndex = pFound->nSelectionIndex;
                    if(nSelIndex >= pFound->aSelection.getLength())
                        nSelIndex = pFound->aSelection.getLength() -1;
                    pFound->aSelection.getConstArray()[nSelIndex] >>= nRet;

                }
                else
                    nRet = pFound->xResultSet->getRow();
            }
            catch(const uno::Exception&)
            {
            }
        }
    }
    return nRet;
}

// close all data sources - after fields were updated
void    SwDBManager::CloseAll(bool bIncludingMerge)
{
    //the only thing done here is to reset the selection index
    //all connections stay open
    for (auto & pParam : m_DataSourceParams)
    {
        if (bIncludingMerge || pParam.get() != m_pImpl->pMergeData.get())
        {
            pParam->nSelectionIndex = 0;
            pParam->bEndOfDB = false;
            try
            {
                if(!m_bInMerge && pParam->xResultSet.is())
                    pParam->xResultSet->first();
            }
            catch(const uno::Exception&)
            {}
        }
    }
}

SwDSParam* SwDBManager::FindDSData(const SwDBData& rData, bool bCreate)
{
    //prefer merge data if available
    if(m_pImpl->pMergeData &&
        ((rData.sDataSource == m_pImpl->pMergeData->sDataSource &&
        rData.sCommand == m_pImpl->pMergeData->sCommand) ||
        (rData.sDataSource.isEmpty() && rData.sCommand.isEmpty())) &&
        (rData.nCommandType == -1 || rData.nCommandType == m_pImpl->pMergeData->nCommandType ||
        (bCreate && m_pImpl->pMergeData->nCommandType == -1)))
    {
        return m_pImpl->pMergeData.get();
    }

    SwDSParam* pFound = nullptr;
    for (size_t nPos = m_DataSourceParams.size(); nPos; nPos--)
    {
        SwDSParam* pParam = m_DataSourceParams[nPos - 1].get();
        if(rData.sDataSource == pParam->sDataSource &&
            rData.sCommand == pParam->sCommand &&
            (rData.nCommandType == -1 || rData.nCommandType == pParam->nCommandType ||
            (bCreate && pParam->nCommandType == -1)))
        {
            // calls from the calculator may add a connection with an invalid commandtype
            //later added "real" data base connections have to re-use the already available
            //DSData and set the correct CommandType
            if(bCreate && pParam->nCommandType == -1)
                pParam->nCommandType = rData.nCommandType;
            pFound = pParam;
            break;
        }
    }
    if(bCreate && !pFound)
    {
        pFound = new SwDSParam(rData);
        m_DataSourceParams.push_back(std::unique_ptr<SwDSParam>(pFound));
        try
        {
            uno::Reference<lang::XComponent> xComponent(pFound->xConnection, uno::UNO_QUERY);
            if(xComponent.is())
                xComponent->addEventListener(m_pImpl->m_xDisposeListener);
        }
        catch(const uno::Exception&)
        {
        }
    }
    return pFound;
}

SwDSParam*  SwDBManager::FindDSConnection(const OUString& rDataSource, bool bCreate)
{
    //prefer merge data if available
    if(m_pImpl->pMergeData && rDataSource == m_pImpl->pMergeData->sDataSource )
    {
        SetAsUsed(rDataSource);
        return m_pImpl->pMergeData.get();
    }
    SwDSParam* pFound = nullptr;
    for (const auto & pParam : m_DataSourceParams)
    {
        if(rDataSource == pParam->sDataSource)
        {
            SetAsUsed(rDataSource);
            pFound = pParam.get();
            break;
        }
    }
    if(bCreate && !pFound)
    {
        SwDBData aData;
        aData.sDataSource = rDataSource;
        pFound = new SwDSParam(aData);
        SetAsUsed(rDataSource);
        m_DataSourceParams.push_back(std::unique_ptr<SwDSParam>(pFound));
        try
        {
            uno::Reference<lang::XComponent> xComponent(pFound->xConnection, uno::UNO_QUERY);
            if(xComponent.is())
                xComponent->addEventListener(m_pImpl->m_xDisposeListener);
        }
        catch(const uno::Exception&)
        {
        }
    }
    return pFound;
}

const SwDBData& SwDBManager::GetAddressDBName()
{
    return SwModule::get()->GetDBConfig()->GetAddressSource();
}

uno::Sequence<OUString> SwDBManager::GetExistingDatabaseNames()
{
    const uno::Reference<uno::XComponentContext>& xContext( ::comphelper::getProcessComponentContext() );
    uno::Reference<sdb::XDatabaseContext> xDBContext = sdb::DatabaseContext::create(xContext);
    return xDBContext->getElementNames();
}

namespace  sw
{
DBConnURIType GetDBunoType(const INetURLObject &rURL)
{
    OUString sExt(rURL.GetFileExtension());
    DBConnURIType type = DBConnURIType::UNKNOWN;

    if (sExt == "odb")
    {
        type = DBConnURIType::ODB;
    }
    else if (sExt.equalsIgnoreAsciiCase("sxc")
        || sExt.equalsIgnoreAsciiCase("ods")
        || sExt.equalsIgnoreAsciiCase("xls")
        || sExt.equalsIgnoreAsciiCase("xlsx"))
    {
        type = DBConnURIType::CALC;
    }
    else if (sExt.equalsIgnoreAsciiCase("sxw") || sExt.equalsIgnoreAsciiCase("odt") || sExt.equalsIgnoreAsciiCase("doc") || sExt.equalsIgnoreAsciiCase("docx"))
    {
        type = DBConnURIType::WRITER;
    }
    else if (sExt.equalsIgnoreAsciiCase("dbf"))
    {
        type = DBConnURIType::DBASE;
    }
    else if (sExt.equalsIgnoreAsciiCase("csv") || sExt.equalsIgnoreAsciiCase("txt"))
    {
        type = DBConnURIType::FLAT;
    }
#ifdef _WIN32
    else if (sExt.equalsIgnoreAsciiCase("accdb") || sExt.equalsIgnoreAsciiCase("accde")
             || sExt.equalsIgnoreAsciiCase("mdb") || sExt.equalsIgnoreAsciiCase("mde"))
    {
        type = DBConnURIType::MSACE;
    }
#endif
    return type;
}
}

namespace
{
uno::Any GetDBunoURI(const INetURLObject &rURL, DBConnURIType& rType)
{
    uno::Any aURLAny;

    if (rType == DBConnURIType::UNKNOWN)
        rType = GetDBunoType(rURL);

    switch (rType) {
    case DBConnURIType::UNKNOWN:
    case DBConnURIType::ODB:
        break;
    case DBConnURIType::CALC:
    {
        OUString sDBURL = "sdbc:calc:" +
            rURL.GetMainURL(INetURLObject::DecodeMechanism::NONE);
        aURLAny <<= sDBURL;
    }
    break;
    case DBConnURIType::WRITER:
    {
        OUString sDBURL = "sdbc:writer:" +
            rURL.GetMainURL(INetURLObject::DecodeMechanism::NONE);
        aURLAny <<= sDBURL;
    }
    break;
    case DBConnURIType::DBASE:
    {
        INetURLObject aUrlTmp(rURL);
        aUrlTmp.removeSegment();
        aUrlTmp.removeFinalSlash();
        OUString sDBURL = "sdbc:dbase:" +
            aUrlTmp.GetMainURL(INetURLObject::DecodeMechanism::NONE);
        aURLAny <<= sDBURL;
    }
    break;
    case DBConnURIType::FLAT:
    {
        INetURLObject aUrlTmp(rURL);
        aUrlTmp.removeSegment();
        aUrlTmp.removeFinalSlash();
        OUString sDBURL = "sdbc:flat:" +
            //only the 'path' has to be added
            aUrlTmp.GetMainURL(INetURLObject::DecodeMechanism::NONE);
        aURLAny <<= sDBURL;
    }
    break;
    case DBConnURIType::MSACE:
#ifdef _WIN32
    {
        OUString sDBURL("sdbc:ado:PROVIDER=Microsoft.ACE.OLEDB.12.0;DATA SOURCE=" + rURL.PathToFileName());
        aURLAny <<= sDBURL;
    }
#endif
    break;
    }
    return aURLAny;
}

/// Returns the URL of this SwDoc.
OUString getOwnURL(SfxObjectShell const * pDocShell)
{
    OUString aRet;

    if (!pDocShell)
        return aRet;

    const INetURLObject& rURLObject = pDocShell->GetMedium()->GetURLObject();
    aRet = rURLObject.GetMainURL(INetURLObject::DecodeMechanism::NONE);
    return aRet;
}

/**
Loads a data source from file and registers it.

In case of success it returns the registered name, otherwise an empty string.
Optionally add a prefix to the registered DB name.
*/
OUString LoadAndRegisterDataSource_Impl(DBConnURIType type, const uno::Reference< beans::XPropertySet > *pSettings,
    const INetURLObject &rURL, const OUString *pDestDir, SfxObjectShell* pDocShell)
{
    OUString sExt(rURL.GetFileExtension());
    uno::Any aTableFilterAny;
    uno::Any aSuppressVersionsAny;
    uno::Any aInfoAny;
    bool bStore = true;
    OUString sFind;

    uno::Any aURLAny = GetDBunoURI(rURL, type);
    switch (type) {
    case DBConnURIType::UNKNOWN:
    case DBConnURIType::CALC:
    case DBConnURIType::WRITER:
        break;
    case DBConnURIType::ODB:
        bStore = false;
        break;
    case DBConnURIType::FLAT:
    case DBConnURIType::DBASE:
        //set the filter to the file name without extension
        {
            uno::Sequence<OUString> aFilters { rURL.getBase(INetURLObject::LAST_SEGMENT, true, INetURLObject::DecodeMechanism::WithCharset) };
            aTableFilterAny <<= aFilters;
        }
        break;
    case DBConnURIType::MSACE:
        aSuppressVersionsAny <<= true;
        break;
    }

    try
    {
        const uno::Reference<uno::XComponentContext>& xContext(::comphelper::getProcessComponentContext());
        uno::Reference<sdb::XDatabaseContext> xDBContext = sdb::DatabaseContext::create(xContext);

        OUString sNewName = rURL.getName(
            INetURLObject::LAST_SEGMENT, true, INetURLObject::DecodeMechanism::Unambiguous);
        sal_Int32 nExtLen = sExt.getLength();
        sNewName = sNewName.replaceAt(sNewName.getLength() - nExtLen - 1, nExtLen + 1, u"");

        //find a unique name if sNewName already exists
        sFind = sNewName;
        sal_Int32 nIndex = 0;
        while (xDBContext->hasByName(sFind))
            sFind = sNewName + OUString::number(++nIndex);

        uno::Reference<uno::XInterface> xNewInstance;
        if (!bStore)
        {
            //odb-file
            uno::Any aDataSource = xDBContext->getByName(rURL.GetMainURL(INetURLObject::DecodeMechanism::NONE));
            aDataSource >>= xNewInstance;
        }
        else
        {
            xNewInstance = xDBContext->createInstance();
            uno::Reference<beans::XPropertySet> xDataProperties(xNewInstance, uno::UNO_QUERY);

            if (aURLAny.hasValue())
                xDataProperties->setPropertyValue(u"URL"_ustr, aURLAny);
            if (aTableFilterAny.hasValue())
                xDataProperties->setPropertyValue(u"TableFilter"_ustr, aTableFilterAny);
            if (aSuppressVersionsAny.hasValue())
                xDataProperties->setPropertyValue(u"SuppressVersionColumns"_ustr, aSuppressVersionsAny);
            if (aInfoAny.hasValue())
                xDataProperties->setPropertyValue(u"Info"_ustr, aInfoAny);

            if (DBConnURIType::FLAT == type && pSettings)
            {
                uno::Any aSettings = xDataProperties->getPropertyValue(u"Settings"_ustr);
                uno::Reference < beans::XPropertySet > xDSSettings;
                aSettings >>= xDSSettings;
                ::comphelper::copyProperties(*pSettings, xDSSettings);
                xDSSettings->setPropertyValue(u"Extension"_ustr, uno::Any(sExt));
            }

            uno::Reference<sdb::XDocumentDataSource> xDS(xNewInstance, uno::UNO_QUERY_THROW);
            uno::Reference<frame::XStorable> xStore(xDS->getDatabaseDocument(), uno::UNO_QUERY_THROW);
            OUString aOwnURL = getOwnURL(pDocShell);
            if (aOwnURL.isEmpty())
            {
                // Cannot embed, as embedded data source would need the URL of the parent document.
                OUString sHomePath(SvtPathOptions().GetWorkPath());
                const OUString sTmpName = utl::CreateTempURL(sNewName, true, u".odb", pDestDir ? pDestDir : &sHomePath);
                xStore->storeAsURL(sTmpName, uno::Sequence<beans::PropertyValue>());
            }
            else
            {
                // Embed.
                OUString aStreamRelPath = u"EmbeddedDatabase"_ustr;
                uno::Reference<embed::XStorage> xStorage = pDocShell->GetStorage();

                // Refer to the sub-storage name in the document settings, so
                // we can load it again next time the file is imported.
                uno::Reference<lang::XMultiServiceFactory> xFactory(pDocShell->GetModel(), uno::UNO_QUERY);
                uno::Reference<beans::XPropertySet> xPropertySet(xFactory->createInstance(u"com.sun.star.document.Settings"_ustr), uno::UNO_QUERY);
                xPropertySet->setPropertyValue(u"EmbeddedDatabaseName"_ustr, uno::Any(aStreamRelPath));

                // Store it only after setting the above property, so that only one data source gets registered.
                SwDBManager::StoreEmbeddedDataSource(xStore, xStorage, aStreamRelPath, aOwnURL);
            }
        }
        xDBContext->registerObject(sFind, xNewInstance);
    }
    catch (const uno::Exception&)
    {
        sFind.clear();
    }
    return sFind;
}
}

OUString SwDBManager::LoadAndRegisterDataSource(weld::Window* pParent, SwDocShell* pDocShell)
{
    sfx2::FileDialogHelper aDlgHelper(ui::dialogs::TemplateDescription::FILEOPEN_SIMPLE, FileDialogFlags::NONE, pParent);
    aDlgHelper.SetContext(sfx2::FileDialogHelper::WriterRegisterDataSource);
    uno::Reference < ui::dialogs::XFilePicker3 > xFP = aDlgHelper.GetFilePicker();

    OUString sFilterAll(SwResId(STR_FILTER_ALL));
    OUString sFilterAllData(SwResId(STR_FILTER_ALL_DATA));

    const std::vector<std::pair<OUString, OUString>> filters{
        { SwResId(STR_FILTER_SXB), u"*.odb"_ustr },
        { SwResId(STR_FILTER_SXC), u"*.ods;*.sxc"_ustr },
        { SwResId(STR_FILTER_SXW), u"*.odt;*.sxw"_ustr },
        { SwResId(STR_FILTER_DBF), u"*.dbf"_ustr },
        { SwResId(STR_FILTER_XLS), u"*.xls;*.xlsx"_ustr },
        { SwResId(STR_FILTER_DOC), u"*.doc;*.docx"_ustr },
        { SwResId(STR_FILTER_TXT), u"*.txt"_ustr },
        { SwResId(STR_FILTER_CSV), u"*.csv"_ustr },
#ifdef _WIN32
        { SwResId(STR_FILTER_ACCDB), u"*.accdb;*.accde;*.mdb;*.mde"_ustr },
#endif
    };

    OUStringBuffer sAllDataFilter;
    for (const auto& [name, filter] : filters)
    {
        (void)name;
        if (!sAllDataFilter.isEmpty())
            sAllDataFilter.append(';');
        sAllDataFilter.append(filter);
    }

    xFP->appendFilter( sFilterAll, u"*"_ustr );
    xFP->appendFilter( sFilterAllData, sAllDataFilter.makeStringAndClear());

    // Similar to sfx2::addExtension from sfx2/source/dialog/filtergrouping.cxx
    for (const auto& [name, filter] : filters)
        xFP->appendFilter(name + " (" + filter + ")", filter);

    xFP->setCurrentFilter( sFilterAll ) ;
    OUString sFind;
    if( ERRCODE_NONE == aDlgHelper.Execute() )
    {
        uno::Reference< beans::XPropertySet > aSettings;
        const INetURLObject aURL( xFP->getSelectedFiles()[0] );
        const DBConnURIType type = GetDBunoType( aURL );

        if( DBConnURIType::FLAT == type )
        {
            const uno::Reference<uno::XComponentContext>& xContext( ::comphelper::getProcessComponentContext() );
            uno::Reference < sdb::XTextConnectionSettings > xSettingsDlg = sdb::TextConnectionSettings::create(xContext);
            if( xSettingsDlg->execute() )
                aSettings.set( uno::Reference < beans::XPropertySet >( xSettingsDlg, uno::UNO_QUERY_THROW ) );
        }
        sFind = LoadAndRegisterDataSource_Impl( type, DBConnURIType::FLAT == type ? &aSettings : nullptr, aURL, nullptr, pDocShell );

        s_aUncommittedRegistrations.push_back(std::pair<SwDocShell*, OUString>(pDocShell, sFind));
    }
    return sFind;
}

void SwDBManager::StoreEmbeddedDataSource(const uno::Reference<frame::XStorable>& xStorable,
                                          const uno::Reference<embed::XStorage>& xStorage,
                                          const OUString& rStreamRelPath,
                                          const OUString& rOwnURL, bool bCopyTo)
{
    // Construct vnd.sun.star.pkg:// URL for later loading, and TargetStorage/StreamRelPath for storing.
    OUString const sTmpName = ConstructVndSunStarPkgUrl(rOwnURL, rStreamRelPath);

    uno::Sequence<beans::PropertyValue> aSequence = comphelper::InitPropertySequence(
    {
        {"TargetStorage", uno::Any(xStorage)},
        {"StreamRelPath", uno::Any(rStreamRelPath)},
        {"BaseURI", uno::Any(rOwnURL)}
    });
    if (bCopyTo)
        xStorable->storeToURL(sTmpName, aSequence);
    else
        xStorable->storeAsURL(sTmpName, aSequence);
}

OUString SwDBManager::LoadAndRegisterDataSource(std::u16string_view rURI, const OUString *pDestDir)
{
    return LoadAndRegisterDataSource_Impl( DBConnURIType::UNKNOWN, nullptr, INetURLObject(rURI), pDestDir, nullptr );
}

namespace
{
    // tdf#117824 switch the embedded database away from using its current storage and point it to temporary storage
    // which allows the original storage to be deleted
    void switchEmbeddedDatabaseStorage(const uno::Reference<sdb::XDatabaseContext>& rDatabaseContext, const OUString& rName)
    {
        uno::Reference<sdb::XDocumentDataSource> xDS(rDatabaseContext->getByName(rName), uno::UNO_QUERY);
        if (!xDS)
            return;
        uno::Reference<document::XStorageBasedDocument> xStorageDoc(xDS->getDatabaseDocument(), uno::UNO_QUERY);
        if (!xStorageDoc)
            return;
        xStorageDoc->switchToStorage(comphelper::OStorageHelper::GetTemporaryStorage());
    }
}

void SwDBManager::RevokeDataSource(const OUString& rName)
{
    uno::Reference<sdb::XDatabaseContext> xDatabaseContext = sdb::DatabaseContext::create(comphelper::getProcessComponentContext());
    if (xDatabaseContext->hasByName(rName))
    {
        switchEmbeddedDatabaseStorage(xDatabaseContext, rName);
        xDatabaseContext->revokeObject(rName);
    }
}

void SwDBManager::LoadAndRegisterEmbeddedDataSource(const SwDBData& rData, const SwDocShell& rDocShell)
{
    uno::Reference<sdb::XDatabaseContext> xDatabaseContext = sdb::DatabaseContext::create(comphelper::getProcessComponentContext());

    OUString sDataSource = rData.sDataSource;

    // Fallback, just in case the document would contain an embedded data source, but no DB fields.
    if (sDataSource.isEmpty())
        sDataSource = u"EmbeddedDatabase"_ustr;

    SwDBManager::RevokeDataSource( sDataSource );

    // Encode the stream name and the real path into a single URL.
    const INetURLObject& rURLObject = rDocShell.GetMedium()->GetURLObject();
    OUString const aURL = ConstructVndSunStarPkgUrl(
        rURLObject.GetMainURL(INetURLObject::DecodeMechanism::NONE),
        m_sEmbeddedName);

    uno::Reference<uno::XInterface> xDataSource(xDatabaseContext->getByName(aURL), uno::UNO_QUERY);
    xDatabaseContext->registerObject( sDataSource, xDataSource );

    // temp file - don't remember connection
    if (rData.sDataSource.isEmpty())
        s_aUncommittedRegistrations.push_back(std::pair<SwDocShell*, OUString>(nullptr, sDataSource));
}

void SwDBManager::ExecuteFormLetter( SwWrtShell& rSh,
                        const uno::Sequence<beans::PropertyValue>& rProperties)
{
    //prevent second call
    if(m_pImpl->pMergeDialog)
        return ;
    OUString sDataSource, sDataTableOrQuery;
    uno::Sequence<uno::Any> aSelection;

    sal_Int32 nCmdType = sdb::CommandType::TABLE;
    uno::Reference< sdbc::XConnection> xConnection;

    svx::ODataAccessDescriptor aDescriptor(rProperties);
    sDataSource = aDescriptor.getDataSource();
    OSL_VERIFY(aDescriptor[svx::DataAccessDescriptorProperty::Command]      >>= sDataTableOrQuery);
    OSL_VERIFY(aDescriptor[svx::DataAccessDescriptorProperty::CommandType]  >>= nCmdType);

    if ( aDescriptor.has(svx::DataAccessDescriptorProperty::Selection) )
        aDescriptor[svx::DataAccessDescriptorProperty::Selection] >>= aSelection;
    if ( aDescriptor.has(svx::DataAccessDescriptorProperty::Connection) )
        aDescriptor[svx::DataAccessDescriptorProperty::Connection] >>= xConnection;

    if(sDataSource.isEmpty() || sDataTableOrQuery.isEmpty())
    {
        OSL_FAIL("PropertyValues missing or unset");
        return;
    }

    //always create a connection for the dialog and dispose it after the dialog has been closed
    SwDSParam* pFound = nullptr;
    if(!xConnection.is())
    {
        xConnection = SwDBManager::RegisterConnection(sDataSource);
        pFound = FindDSConnection(sDataSource, true);
    }
    SwAbstractDialogFactory* pFact = SwAbstractDialogFactory::Create();
    m_pImpl->pMergeDialog = pFact->CreateMailMergeDlg(rSh.GetView().GetViewFrame().GetFrameWeld(), rSh,
                                                     sDataSource,
                                                     sDataTableOrQuery,
                                                     nCmdType,
                                                     xConnection);
    if(m_pImpl->pMergeDialog->Execute() == RET_OK)
    {
        aDescriptor[svx::DataAccessDescriptorProperty::Selection] <<= m_pImpl->pMergeDialog->GetSelection();

        uno::Reference<sdbc::XResultSet> xResSet = m_pImpl->pMergeDialog->GetResultSet();
        if(xResSet.is())
            aDescriptor[svx::DataAccessDescriptorProperty::Cursor] <<= xResSet;

        // SfxObjectShellRef is ok, since there should be no control over the document lifetime here
        SfxObjectShellRef xDocShell = rSh.GetView().GetViewFrame().GetObjectShell();

        lcl_emitEvent(SfxEventHintId::SwMailMerge, STR_SW_EVENT_MAIL_MERGE, xDocShell.get());

        // prepare mail merge descriptor
        SwMergeDescriptor aMergeDesc( m_pImpl->pMergeDialog->GetMergeType(), rSh, aDescriptor );
        aMergeDesc.sSaveToFilter = m_pImpl->pMergeDialog->GetSaveFilter();
        aMergeDesc.bCreateSingleFile = m_pImpl->pMergeDialog->IsSaveSingleDoc();
        aMergeDesc.bPrefixIsFilename = aMergeDesc.bCreateSingleFile;
        aMergeDesc.sPrefix = m_pImpl->pMergeDialog->GetTargetURL();

        if(!aMergeDesc.bCreateSingleFile)
        {
            if(m_pImpl->pMergeDialog->IsGenerateFromDataBase())
                aMergeDesc.sDBcolumn = m_pImpl->pMergeDialog->GetColumnName();

            if(m_pImpl->pMergeDialog->IsFileEncryptedFromDataBase())
                aMergeDesc.sDBPasswordColumn = m_pImpl->pMergeDialog->GetPasswordColumnName();
        }

        Merge( aMergeDesc );

        lcl_emitEvent(SfxEventHintId::SwMailMergeEnd, STR_SW_EVENT_MAIL_MERGE_END, xDocShell.get());

        // reset the cursor inside
        xResSet = nullptr;
        aDescriptor[svx::DataAccessDescriptorProperty::Cursor] <<= xResSet;
    }
    if(pFound)
    {
        for (const auto & pParam : m_DataSourceParams)
        {
            if (pParam.get() == pFound)
            {
                try
                {
                    uno::Reference<lang::XComponent> xComp(pParam->xConnection, uno::UNO_QUERY);
                    if(xComp.is())
                        xComp->dispose();
                }
                catch(const uno::RuntimeException&)
                {
                    //may be disposed already since multiple entries may have used the same connection
                }
                break;
            }
            //pFound doesn't need to be removed/deleted -
            //this has been done by the SwConnectionDisposedListener_Impl already
        }
    }
    m_pImpl->pMergeDialog.disposeAndClear();
}

void SwDBManager::InsertText(SwWrtShell& rSh,
                        const uno::Sequence< beans::PropertyValue>& rProperties)
{
    OUString sDataSource, sDataTableOrQuery;
    uno::Reference<sdbc::XResultSet>  xResSet;
    uno::Sequence<uno::Any> aSelection;
    sal_Int16 nCmdType = sdb::CommandType::TABLE;
    uno::Reference< sdbc::XConnection> xConnection;
    for(const beans::PropertyValue& rValue : rProperties)
    {
        if ( rValue.Name == "DataSourceName" )
            rValue.Value >>= sDataSource;
        else if ( rValue.Name == "Command" )
            rValue.Value >>= sDataTableOrQuery;
        else if ( rValue.Name == "Cursor" )
            rValue.Value >>= xResSet;
        else if ( rValue.Name == "Selection" )
            rValue.Value >>= aSelection;
        else if ( rValue.Name == "CommandType" )
            rValue.Value >>= nCmdType;
        else if ( rValue.Name == "ActiveConnection" )
            rValue.Value >>= xConnection;
    }
    if(sDataSource.isEmpty() || sDataTableOrQuery.isEmpty() || !xResSet.is())
    {
        OSL_FAIL("PropertyValues missing or unset");
        return;
    }
    const uno::Reference< uno::XComponentContext >& xContext( ::comphelper::getProcessComponentContext() );
    uno::Reference<sdbc::XDataSource> xSource;
    uno::Reference<container::XChild> xChild(xConnection, uno::UNO_QUERY);
    if(xChild.is())
        xSource.set(xChild->getParent(), uno::UNO_QUERY);
    if(!xSource.is())
        xSource = dbtools::getDataSource(sDataSource, xContext);
    uno::Reference< sdbcx::XColumnsSupplier > xColSupp( xResSet, uno::UNO_QUERY );
    SwDBData aDBData;
    aDBData.sDataSource = sDataSource;
    aDBData.sCommand = sDataTableOrQuery;
    aDBData.nCommandType = nCmdType;

    SwAbstractDialogFactory* pFact = SwAbstractDialogFactory::Create();
    VclPtr<AbstractSwInsertDBColAutoPilot> pDlg(pFact->CreateSwInsertDBColAutoPilot( rSh.GetView(),
                                                                                xSource,
                                                                                xColSupp,
                                                                                aDBData ));
    pDlg->StartExecuteAsync(
        [xConnection, xSource, pDlg, xResSet, aSelection] (sal_Int32 nResult)->void
        {
            if (nResult == RET_OK)
            {
                OUString sDummy;
                auto xTmpConnection = xConnection;
                if(!xTmpConnection.is())
                    xTmpConnection = xSource->getConnection(sDummy, sDummy);
                try
                {
                    pDlg->DataToDoc( aSelection , xSource, xTmpConnection, xResSet);
                }
                catch (const uno::Exception&)
                {
                    TOOLS_WARN_EXCEPTION("sw.mailmerge", "");
                }
                pDlg->disposeOnce();
            }
        }
    );

}

uno::Reference<sdbc::XDataSource> SwDBManager::getDataSourceAsParent(const uno::Reference< sdbc::XConnection>& _xConnection,const OUString& _sDataSourceName)
{
    uno::Reference<sdbc::XDataSource> xSource;
    try
    {
        uno::Reference<container::XChild> xChild(_xConnection, uno::UNO_QUERY);
        if ( xChild.is() )
            xSource.set(xChild->getParent(), uno::UNO_QUERY);
        if ( !xSource.is() )
            xSource = dbtools::getDataSource(_sDataSourceName, ::comphelper::getProcessComponentContext());
    }
    catch (const uno::Exception&)
    {
        TOOLS_WARN_EXCEPTION("sw.mailmerge", "getDataSourceAsParent()");
    }
    return xSource;
}

uno::Reference<sdbc::XResultSet> SwDBManager::createCursor(const OUString& _sDataSourceName,
                                       const OUString& _sCommand,
                                       sal_Int32 _nCommandType,
                                       const uno::Reference<sdbc::XConnection>& _xConnection,
                                       const SwView* pView)
{
    uno::Reference<sdbc::XResultSet> xResultSet;
    try
    {
        uno::Reference< lang::XMultiServiceFactory > xMgr( ::comphelper::getProcessServiceFactory() );
        if( xMgr.is() )
        {
            uno::Reference<uno::XInterface> xInstance = xMgr->createInstance(u"com.sun.star.sdb.RowSet"_ustr);
            uno::Reference<beans::XPropertySet> xRowSetPropSet(xInstance, uno::UNO_QUERY);
            if(xRowSetPropSet.is())
            {
                xRowSetPropSet->setPropertyValue(u"DataSourceName"_ustr, uno::Any(_sDataSourceName));
                xRowSetPropSet->setPropertyValue(u"ActiveConnection"_ustr, uno::Any(_xConnection));
                xRowSetPropSet->setPropertyValue(u"Command"_ustr, uno::Any(_sCommand));
                xRowSetPropSet->setPropertyValue(u"CommandType"_ustr, uno::Any(_nCommandType));

                uno::Reference< sdb::XCompletedExecution > xRowSet(xInstance, uno::UNO_QUERY);

                if ( xRowSet.is() )
                {
                    weld::Window* pWindow = pView ? pView->GetFrameWeld() : nullptr;
                    uno::Reference< task::XInteractionHandler > xHandler( task::InteractionHandler::createWithParent(comphelper::getComponentContext(xMgr), pWindow ? pWindow->GetXWindow() : nullptr), uno::UNO_QUERY_THROW );
                    xRowSet->executeWithCompletion(xHandler);
                }
                xResultSet.set(xRowSet, uno::UNO_QUERY);
            }
        }
    }
    catch (const uno::Exception&)
    {
        TOOLS_WARN_EXCEPTION("sw.mailmerge", "Caught exception while creating a new RowSet");
    }
    return xResultSet;
}

void SwDBManager::setEmbeddedName(const OUString& rEmbeddedName, SwDocShell& rDocShell)
{
    bool bLoad = m_sEmbeddedName != rEmbeddedName && !rEmbeddedName.isEmpty();
    bool bRegisterListener = m_sEmbeddedName.isEmpty() && !rEmbeddedName.isEmpty();

    m_sEmbeddedName = rEmbeddedName;

    if (bLoad)
    {
        uno::Reference<embed::XStorage> xStorage = rDocShell.GetStorage();
        // It's OK that we don't have the named sub-storage yet, in case
        // we're in the process of creating it.
        if (xStorage->hasByName(rEmbeddedName))
            LoadAndRegisterEmbeddedDataSource(rDocShell.GetDoc()->GetDBData(), rDocShell);
    }

    if (bRegisterListener)
        // Register a remove listener, so we know when the embedded data source is removed.
        m_pImpl->m_xDataSourceRemovedListener = new SwDataSourceRemovedListener(*this);
}

const OUString& SwDBManager::getEmbeddedName() const
{
    return m_sEmbeddedName;
}

SwDoc* SwDBManager::getDoc() const
{
    return m_pDoc;
}

void SwDBManager::releaseRevokeListener()
{
    if (m_pImpl->m_xDataSourceRemovedListener.is())
    {
        m_pImpl->m_xDataSourceRemovedListener->Dispose();
        m_pImpl->m_xDataSourceRemovedListener.clear();
    }
}

SwDBManager::ConnectionDisposedListener_Impl::ConnectionDisposedListener_Impl(SwDBManager& rManager)
    : m_pDBManager(&rManager)
{
}

void SwDBManager::ConnectionDisposedListener_Impl::disposing( const lang::EventObject& rSource )
{
    ::SolarMutexGuard aGuard;

    if (!m_pDBManager) return; // we're disposed too!

    uno::Reference<sdbc::XConnection> xSource(rSource.Source, uno::UNO_QUERY);
    for (size_t nPos = m_pDBManager->m_DataSourceParams.size(); nPos; nPos--)
    {
        SwDSParam* pParam = m_pDBManager->m_DataSourceParams[nPos - 1].get();
        if(pParam->xConnection.is() &&
                (xSource == pParam->xConnection))
        {
            m_pDBManager->m_DataSourceParams.erase(
                    m_pDBManager->m_DataSourceParams.begin() + nPos - 1);
        }
    }
}

std::shared_ptr<SwMailMergeConfigItem> SwDBManager::PerformMailMerge(SwView const * pView)
{
    std::shared_ptr<SwMailMergeConfigItem> xConfigItem = pView->GetMailMergeConfigItem();
    if (!xConfigItem)
        return xConfigItem;

    svx::ODataAccessDescriptor aDescriptor;
    aDescriptor.setDataSource(xConfigItem->GetCurrentDBData().sDataSource);
    aDescriptor[ svx::DataAccessDescriptorProperty::Connection ]  <<= xConfigItem->GetConnection().getTyped();
    aDescriptor[ svx::DataAccessDescriptorProperty::Cursor ]      <<= xConfigItem->GetResultSet();
    aDescriptor[ svx::DataAccessDescriptorProperty::Command ]     <<= xConfigItem->GetCurrentDBData().sCommand;
    aDescriptor[ svx::DataAccessDescriptorProperty::CommandType ] <<= xConfigItem->GetCurrentDBData().nCommandType;
    aDescriptor[ svx::DataAccessDescriptorProperty::Selection ]   <<= xConfigItem->GetSelection();

    SwWrtShell& rSh = pView->GetWrtShell();
    xConfigItem->SetTargetView(nullptr);

    SwMergeDescriptor aMergeDesc(DBMGR_MERGE_SHELL, rSh, aDescriptor);
    aMergeDesc.pMailMergeConfigItem = xConfigItem.get();
    aMergeDesc.bCreateSingleFile = true;
    rSh.GetDBManager()->Merge(aMergeDesc);

    return xConfigItem;
}

void SwDBManager::RevokeLastRegistrations()
{
    if (s_aUncommittedRegistrations.empty())
        return;

    SwView* pView = ( m_pDoc && m_pDoc->GetDocShell() ) ? m_pDoc->GetDocShell()->GetView() : nullptr;
    if (pView)
    {
        const std::shared_ptr<SwMailMergeConfigItem>& xConfigItem = pView->GetMailMergeConfigItem();
        if (xConfigItem)
        {
            xConfigItem->DisposeResultSet();
            xConfigItem->DocumentReloaded();
        }
    }

    for (auto it = s_aUncommittedRegistrations.begin(); it != s_aUncommittedRegistrations.end();)
    {
        if ((m_pDoc && it->first == m_pDoc->GetDocShell()) || it->first == nullptr)
        {
            RevokeDataSource(it->second);
            it = s_aUncommittedRegistrations.erase(it);
        }
        else
            ++it;
    }
}

void SwDBManager::CommitLastRegistrations()
{
    for (auto aIt = s_aUncommittedRegistrations.begin(); aIt != s_aUncommittedRegistrations.end();)
    {
        if (aIt->first == m_pDoc->GetDocShell() || aIt->first == nullptr)
        {
            m_aNotUsedConnections.push_back(aIt->second);
            aIt = s_aUncommittedRegistrations.erase(aIt);
        }
        else
            aIt++;
    }
}

void SwDBManager::SetAsUsed(const OUString& rName)
{
    auto aFound = std::find(m_aNotUsedConnections.begin(), m_aNotUsedConnections.end(), rName);
    if (aFound != m_aNotUsedConnections.end())
        m_aNotUsedConnections.erase(aFound);
}

void SwDBManager::RevokeNotUsedConnections()
{
    for (auto aIt = m_aNotUsedConnections.begin(); aIt != m_aNotUsedConnections.end();)
    {
        RevokeDataSource(*aIt);
        aIt = m_aNotUsedConnections.erase(aIt);
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
