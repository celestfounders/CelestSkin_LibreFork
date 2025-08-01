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

#include "datasource.hxx"
#include "commandcontainer.hxx"
#include <stringconstants.hxx>
#include <core_resource.hxx>
#include <strings.hrc>
#include <strings.hxx>
#include <connection.hxx>
#include "SharedConnection.hxx"
#include "databasedocument.hxx"
#include <OAuthenticationContinuation.hxx>

#include <hsqlimport.hxx>
#include <migrwarndlg.hxx>

#include <com/sun/star/beans/NamedValue.hpp>
#include <com/sun/star/beans/PropertyAttribute.hpp>
#include <com/sun/star/beans/PropertyState.hpp>
#include <com/sun/star/document/XDocumentSubStorageSupplier.hpp>
#include <com/sun/star/lang/DisposedException.hpp>
#include <com/sun/star/reflection/ProxyFactory.hpp>
#include <com/sun/star/sdb/DatabaseContext.hpp>
#include <com/sun/star/sdb/SQLContext.hpp>
#include <com/sun/star/sdbc/ConnectionPool.hpp>
#include <com/sun/star/sdbc/XDriverAccess.hpp>
#include <com/sun/star/sdbc/XDriverManager.hpp>
#include <com/sun/star/sdbc/DriverManager.hpp>
#include <com/sun/star/ucb/AuthenticationRequest.hpp>
#include <com/sun/star/ucb/XInteractionSupplyAuthentication.hpp>

#include <cppuhelper/implbase.hxx>
#include <comphelper/interaction.hxx>
#include <comphelper/property.hxx>
#include <comphelper/sequence.hxx>
#include <comphelper/types.hxx>
#include <cppuhelper/supportsservice.hxx>
#include <connectivity/dbexception.hxx>
#include <connectivity/dbtools.hxx>
#include <cppuhelper/typeprovider.hxx>
#include <officecfg/Office/Common.hxx>
#include <comphelper/diagnose_ex.hxx>
#include <o3tl/environment.hxx>
#include <osl/diagnose.h>
#include <osl/process.h>
#include <sal/log.hxx>
#include <tools/urlobj.hxx>
#include <unotools/sharedunocomponent.hxx>

#include <algorithm>
#include <iterator>
#include <set>

#include <config_firebird.h>

using namespace ::com::sun::star::sdbc;
using namespace ::com::sun::star::sdbcx;
using namespace ::com::sun::star::sdb;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::embed;
using namespace ::com::sun::star::container;
using namespace ::com::sun::star::util;
using namespace ::com::sun::star::io;
using namespace ::com::sun::star::task;
using namespace ::com::sun::star::ucb;
using namespace ::com::sun::star::frame;
using namespace ::com::sun::star::reflection;
using namespace ::cppu;
using namespace ::osl;
using namespace ::dbtools;
using namespace ::comphelper;

namespace dbaccess
{

namespace {

/** helper class which implements a XFlushListener, and forwards all
    notification events to another XFlushListener

    The speciality is that the foreign XFlushListener instance, to which
    the notifications are forwarded, is held weak.

    Thus, the class can be used with XFlushable instance which hold
    their listeners with a hard reference, if you simply do not *want*
    to be held hard-ref-wise.
*/
class FlushNotificationAdapter : public ::cppu::WeakImplHelper< XFlushListener >
{
private:
    WeakReference< XFlushable >     m_aBroadcaster;
    WeakReference< XFlushListener > m_aListener;

public:
    static void installAdapter( const Reference< XFlushable >& _rxBroadcaster, const Reference< XFlushListener >& _rxListener )
    {
        new FlushNotificationAdapter( _rxBroadcaster, _rxListener );
    }

protected:
    FlushNotificationAdapter( const Reference< XFlushable >& _rxBroadcaster, const Reference< XFlushListener >& _rxListener );
    virtual ~FlushNotificationAdapter() override;

    void impl_dispose();

protected:
    // XFlushListener
    virtual void SAL_CALL flushed( const css::lang::EventObject& rEvent ) override;
    // XEventListener
    virtual void SAL_CALL disposing( const css::lang::EventObject& Source ) override;
};

}

FlushNotificationAdapter::FlushNotificationAdapter( const Reference< XFlushable >& _rxBroadcaster, const Reference< XFlushListener >& _rxListener )
    :m_aBroadcaster( _rxBroadcaster )
    ,m_aListener( _rxListener )
{
    OSL_ENSURE( _rxBroadcaster.is(), "FlushNotificationAdapter::FlushNotificationAdapter: invalid flushable!" );

    osl_atomic_increment( &m_refCount );
    {
        if ( _rxBroadcaster.is() )
            _rxBroadcaster->addFlushListener( this );
    }
    osl_atomic_decrement( &m_refCount );
    OSL_ENSURE( m_refCount == 1, "FlushNotificationAdapter::FlushNotificationAdapter: broadcaster isn't holding by hard ref!?" );
}

FlushNotificationAdapter::~FlushNotificationAdapter()
{
}

void FlushNotificationAdapter::impl_dispose()
{
    Reference< XFlushListener > xKeepAlive( this );

    Reference< XFlushable > xFlushable( m_aBroadcaster );
    if ( xFlushable.is() )
        xFlushable->removeFlushListener( this );

    m_aListener.clear();
    m_aBroadcaster.clear();
}

void SAL_CALL FlushNotificationAdapter::flushed( const EventObject& rEvent )
{
    Reference< XFlushListener > xListener( m_aListener );
    if ( xListener.is() )
        xListener->flushed( rEvent );
    else
        impl_dispose();
}

void SAL_CALL FlushNotificationAdapter::disposing( const EventObject& Source )
{
    Reference< XFlushListener > xListener( m_aListener );
    if ( xListener.is() )
        xListener->disposing( Source );

    impl_dispose();
}

OAuthenticationContinuation::OAuthenticationContinuation()
    :m_bRememberPassword(true),   // TODO: a meaningful default
    m_bCanSetUserName(true)
{
}

sal_Bool SAL_CALL OAuthenticationContinuation::canSetRealm(  )
{
    return false;
}

void SAL_CALL OAuthenticationContinuation::setRealm( const OUString& /*Realm*/ )
{
    SAL_WARN("dbaccess","OAuthenticationContinuation::setRealm: not supported!");
}

sal_Bool SAL_CALL OAuthenticationContinuation::canSetUserName(  )
{
    // we always allow this, even if the database document is read-only. In this case,
    // it's simply that the user cannot store the new user name.
    return m_bCanSetUserName;
}

void SAL_CALL OAuthenticationContinuation::setUserName( const OUString& _rUser )
{
    m_sUser = _rUser;
}

sal_Bool SAL_CALL OAuthenticationContinuation::canSetPassword(  )
{
    return true;
}

void SAL_CALL OAuthenticationContinuation::setPassword( const OUString& _rPassword )
{
    m_sPassword = _rPassword;
}

Sequence< RememberAuthentication > SAL_CALL OAuthenticationContinuation::getRememberPasswordModes( RememberAuthentication& _reDefault )
{
    _reDefault = RememberAuthentication_SESSION;
    return { _reDefault };
}

void SAL_CALL OAuthenticationContinuation::setRememberPassword( RememberAuthentication _eRemember )
{
    m_bRememberPassword = (RememberAuthentication_NO != _eRemember);
}

sal_Bool SAL_CALL OAuthenticationContinuation::canSetAccount(  )
{
    return false;
}

void SAL_CALL OAuthenticationContinuation::setAccount( const OUString& )
{
    SAL_WARN("dbaccess","OAuthenticationContinuation::setAccount: not supported!");
}

Sequence< RememberAuthentication > SAL_CALL OAuthenticationContinuation::getRememberAccountModes( RememberAuthentication& _reDefault )
{
    _reDefault = RememberAuthentication_NO;
    return { RememberAuthentication_NO };
}

void SAL_CALL OAuthenticationContinuation::setRememberAccount( RememberAuthentication /*Remember*/ )
{
    SAL_WARN("dbaccess","OAuthenticationContinuation::setRememberAccount: not supported!");
}

OSharedConnectionManager::OSharedConnectionManager(const Reference< XComponentContext >& _rxContext)
{
    m_xProxyFactory.set( ProxyFactory::create( _rxContext ) );
}

OSharedConnectionManager::~OSharedConnectionManager()
{
}

void SAL_CALL OSharedConnectionManager::disposing( const css::lang::EventObject& Source )
{
    MutexGuard aGuard(m_aMutex);
    Reference<XConnection> xConnection(Source.Source,UNO_QUERY);
    TSharedConnectionMap::const_iterator aFind = m_aSharedConnection.find(xConnection);
    if ( m_aSharedConnection.end() != aFind )
    {
        osl_atomic_decrement(&aFind->second->second.nALiveCount);
        if ( !aFind->second->second.nALiveCount )
        {
            ::comphelper::disposeComponent(aFind->second->second.xMasterConnection);
            m_aConnections.erase(aFind->second);
        }
        m_aSharedConnection.erase(aFind);
    }
}

Reference<XConnection> OSharedConnectionManager::getConnection( const OUString& url,
                                        const OUString& user,
                                        const OUString& password,
                                        const Sequence< PropertyValue >& _aInfo,
                                        ODatabaseSource* _pDataSource)
{
    MutexGuard aGuard(m_aMutex);
    TConnectionMap::key_type nId;
    Sequence< PropertyValue > aInfoCopy(_aInfo);
    sal_Int32 nPos = aInfoCopy.getLength();
    aInfoCopy.realloc( nPos + 2 );
    auto pInfoCopy = aInfoCopy.getArray();
    pInfoCopy[nPos].Name      = "TableFilter";
    pInfoCopy[nPos++].Value <<= _pDataSource->m_pImpl->m_aTableFilter;
    pInfoCopy[nPos].Name      = "TableTypeFilter";
    pInfoCopy[nPos++].Value <<= _pDataSource->m_pImpl->m_aTableTypeFilter;

    OUString sUser = user;
    OUString sPassword = password;
    if ((sUser.isEmpty()) && (sPassword.isEmpty()) && (!_pDataSource->m_pImpl->m_sUser.isEmpty()))
    {   // ease the usage of this method. data source which are intended to have a user automatically
        // fill in the user/password combination if the caller of this method does not specify otherwise
        sUser = _pDataSource->m_pImpl->m_sUser;
        if (!_pDataSource->m_pImpl->m_aPassword.isEmpty())
            sPassword = _pDataSource->m_pImpl->m_aPassword;
    }

    ::connectivity::OConnectionWrapper::createUniqueId(url,aInfoCopy,nId.m_pBuffer,sUser,sPassword);
    TConnectionMap::iterator aIter = m_aConnections.find(nId);

    if ( m_aConnections.end() == aIter )
    {
        TConnectionHolder aHolder;
        aHolder.nALiveCount = 0; // will be incremented by addListener
        aHolder.xMasterConnection = _pDataSource->buildIsolatedConnection(user,password);
        aIter = m_aConnections.emplace(nId,aHolder).first;
    }

    rtl::Reference<OSharedConnection> xRet;
    if ( aIter->second.xMasterConnection.is() )
    {
        Reference< XAggregation > xConProxy = m_xProxyFactory->createProxy(cppu::getXWeak(aIter->second.xMasterConnection.get()));
        xRet = new OSharedConnection(xConProxy);
        m_aSharedConnection.emplace(xRet,aIter);
        addEventListener(xRet,aIter);
    }

    return xRet;
}

void OSharedConnectionManager::addEventListener(const Reference<XConnection>& _rxConnection, TConnectionMap::iterator const & _rIter)
{
    Reference<XComponent> xComp(_rxConnection,UNO_QUERY);
    xComp->addEventListener(this);
    OSL_ENSURE( m_aConnections.end() != _rIter , "Iterator is end!");
    osl_atomic_increment(&_rIter->second.nALiveCount);
}

namespace
{
    Sequence< PropertyValue > lcl_filterDriverProperties( const Reference< XDriver >& _xDriver, const OUString& _sUrl,
        const Sequence< PropertyValue >& _rDataSourceSettings )
    {
        if ( _xDriver.is() )
        {
            Sequence< DriverPropertyInfo > aDriverInfo(_xDriver->getPropertyInfo(_sUrl,_rDataSourceSettings));

            std::vector< PropertyValue > aRet;

            for (auto& dataSourceSetting : _rDataSourceSettings)
            {
                auto knownSettings = dbaccess::ODatabaseModelImpl::getDefaultDataSourceSettings();
                bool isSettingKnown = std::any_of(knownSettings.begin(), knownSettings.end(),
                                                  [name = dataSourceSetting.Name](auto& setting)
                                                  { return name == setting.Name; });
                // Allow if the particular data source setting is unknown or allowed by the driver
                bool bAllowSetting = !isSettingKnown
                                     || std::any_of(aDriverInfo.begin(), aDriverInfo.end(),
                                                    [name = dataSourceSetting.Name](auto& setting)
                                                    { return name == setting.Name; });
                if (bAllowSetting)
                {   // if the driver allows this particular setting, or if the setting is completely unknown,
                    // we pass it to the driver
                    aRet.push_back(dataSourceSetting);
                }
            }
            if ( !aRet.empty() )
                return comphelper::containerToSequence(aRet);
        }
        return Sequence< PropertyValue >();
    }

    typedef std::map< OUString, sal_Int32 > PropertyAttributeCache;

    struct IsDefaultAndNotRemoveable
    {
    private:
        const PropertyAttributeCache& m_rAttribs;

    public:
        explicit IsDefaultAndNotRemoveable( const PropertyAttributeCache& _rAttribs ) : m_rAttribs( _rAttribs ) { }

        bool operator()( const PropertyValue& _rProp )
        {
            if ( _rProp.State != PropertyState_DEFAULT_VALUE )
                return false;

            bool bRemoveable = true;

            PropertyAttributeCache::const_iterator pos = m_rAttribs.find( _rProp.Name );
            OSL_ENSURE( pos != m_rAttribs.end(), "IsDefaultAndNotRemoveable: illegal property name!" );
            if ( pos != m_rAttribs.end() )
                bRemoveable = ( ( pos->second & PropertyAttribute::REMOVABLE ) != 0 );

            return !bRemoveable;
        }
    };
}


ODatabaseSource::ODatabaseSource(const ::rtl::Reference<ODatabaseModelImpl>& _pImpl)
            :ModelDependentComponent( _pImpl )
            ,ODatabaseSource_Base( getMutex() )
            ,OPropertySetHelper( ODatabaseSource_Base::rBHelper )
            , m_Bookmarks(*this, getMutex())
            ,m_aFlushListeners( getMutex() )
{
    // some kind of default
    SAL_INFO("dbaccess", "DS: ctor: " << std::hex << this << ": " << std::hex << m_pImpl.get() );
}

ODatabaseSource::~ODatabaseSource()
{
    SAL_INFO("dbaccess", "DS: dtor: " << std::hex << this << ": " << std::hex << m_pImpl.get() );
    if ( !ODatabaseSource_Base::rBHelper.bInDispose && !ODatabaseSource_Base::rBHelper.bDisposed )
    {
        acquire();
        dispose();
    }
}

void ODatabaseSource::setName( const Reference< XDocumentDataSource >& _rxDocument, const OUString& _rNewName, DBContextAccess )
{
    ODatabaseSource& rModelImpl = dynamic_cast< ODatabaseSource& >( *_rxDocument );

    SolarMutexGuard g;
    if ( rModelImpl.m_pImpl.is() )
        rModelImpl.m_pImpl->m_sName = _rNewName;
}

// css::lang::XTypeProvider
Sequence< Type > ODatabaseSource::getTypes()
{
    OTypeCollection aPropertyHelperTypes(   cppu::UnoType<XFastPropertySet>::get(),
                                            cppu::UnoType<XPropertySet>::get(),
                                            cppu::UnoType<XMultiPropertySet>::get());

    return ::comphelper::concatSequences(
        ODatabaseSource_Base::getTypes(),
        aPropertyHelperTypes.getTypes()
    );
}

Sequence< sal_Int8 > ODatabaseSource::getImplementationId()
{
    return css::uno::Sequence<sal_Int8>();
}

// css::uno::XInterface
Any ODatabaseSource::queryInterface( const Type & rType )
{
    Any aIface = ODatabaseSource_Base::queryInterface( rType );
    if ( !aIface.hasValue() )
        aIface = ::cppu::OPropertySetHelper::queryInterface( rType );
    return aIface;
}

void ODatabaseSource::acquire() noexcept
{
    ODatabaseSource_Base::acquire();
}

void ODatabaseSource::release() noexcept
{
    ODatabaseSource_Base::release();
}

void SAL_CALL ODatabaseSource::disposing( const css::lang::EventObject& Source )
{
    if ( m_pImpl.is() )
        m_pImpl->disposing(Source);
}

// XServiceInfo
OUString ODatabaseSource::getImplementationName(  )
{
    return u"com.sun.star.comp.dba.ODatabaseSource"_ustr;
}

Sequence< OUString > ODatabaseSource::getSupportedServiceNames(  )
{
    return { SERVICE_SDB_DATASOURCE, u"com.sun.star.sdb.DocumentDataSource"_ustr };
}

sal_Bool ODatabaseSource::supportsService( const OUString& _rServiceName )
{
    return cppu::supportsService(this, _rServiceName);
}

// OComponentHelper
void ODatabaseSource::disposing()
{
    SAL_INFO("dbaccess", "DS: disp: " << std::hex << this << ", " << std::hex << m_pImpl.get() );
    ODatabaseSource_Base::WeakComponentImplHelperBase::disposing();
    OPropertySetHelper::disposing();

    EventObject aDisposeEvent(static_cast<XWeak*>(this));
    m_aFlushListeners.disposeAndClear( aDisposeEvent );

    ODatabaseDocument::clearObjectContainer(m_pImpl->m_xCommandDefinitions);
    ODatabaseDocument::clearObjectContainer(m_pImpl->m_xTableDefinitions);
    m_pImpl.clear();
}

weld::Window* ODatabaseModelImpl::GetFrameWeld()
{
    if (m_xDialogParent.is())
        return Application::GetFrameWeld(m_xDialogParent);

    rtl::Reference<ODatabaseDocument> xModel = getModel_noCreate();
    if (!xModel.is())
        return nullptr;
    Reference<XController> xController(xModel->getCurrentController());
    if (!xController.is())
        return nullptr;
    Reference<XFrame> xFrame(xController->getFrame());
    if (!xFrame.is())
        return nullptr;
    Reference<css::awt::XWindow> xWindow(xFrame->getContainerWindow());
    return Application::GetFrameWeld(xWindow);
}

Reference< XConnection > ODatabaseSource::buildLowLevelConnection(const OUString& _rUid, const OUString& _rPwd)
{
    Reference< XConnection > xReturn;

    Reference< XDriverManager > xManager;

#if ENABLE_FIREBIRD_SDBC
    bool bIgnoreMigration = false;
    bool bNeedMigration = false;
    rtl::Reference< ODatabaseDocument > xModel = m_pImpl->getModel_noCreate();
    if ( xModel)
    {
        //See ODbTypeWizDialogSetup::SaveDatabaseDocument
        ::comphelper::NamedValueCollection::get(xModel->getArgs(), u"IgnoreFirebirdMigration") >>= bIgnoreMigration;
    }
    else
    {
        //ignore when we don't have a model. E.g. Mailmerge, data sources, fields...
        bIgnoreMigration = true;
    }

    if (!officecfg::Office::Common::Misc::ExperimentalMode::get())
        bIgnoreMigration = true;

    if(!bIgnoreMigration && m_pImpl->m_sConnectURL == "sdbc:embedded:hsqldb")
    {
        Reference<XStorage> const xRootStorage = m_pImpl->getOrCreateRootStorage();
        if (!o3tl::getEnvironment(u"DBACCESS_HSQL_MIGRATION"_ustr).isEmpty())
            bNeedMigration = true;
        else
        {
            Reference<XPropertySet> const xPropSet(xRootStorage, UNO_QUERY_THROW);
            sal_Int32 nOpenMode(0);
            if ((xPropSet->getPropertyValue(u"OpenMode"_ustr) >>= nOpenMode)
                && (nOpenMode & css::embed::ElementModes::WRITE)
                && (!Application::IsHeadlessModeEnabled()))
            {
                MigrationWarnDialog aWarnDlg(m_pImpl->GetFrameWeld());
                bNeedMigration = aWarnDlg.run() == RET_OK;
            }
        }
        if (bNeedMigration)
        {
            // back up content xml file if migration was successful
            static constexpr OUString BACKUP_XML_NAME = u"content_before_migration.xml"_ustr;
            try
            {
                if(xRootStorage->isStreamElement(BACKUP_XML_NAME))
                    xRootStorage->removeElement(BACKUP_XML_NAME);
            }
            catch (NoSuchElementException&)
            {
                SAL_INFO("dbaccess", "No file content_before_migration.xml found" );
            }
            xRootStorage->copyElementTo(u"content.xml"_ustr, xRootStorage,
                BACKUP_XML_NAME);

            m_pImpl->m_sConnectURL = "sdbc:embedded:firebird";
        }
    }
#endif

    try {
        xManager.set( ConnectionPool::create( m_pImpl->m_aContext ), UNO_QUERY_THROW );
    } catch( const Exception& ) {  }
    if ( !xManager.is() )
        // no connection pool installed, fall back to driver manager
        xManager.set( DriverManager::create(m_pImpl->m_aContext ), UNO_QUERY_THROW );

    OUString sUser(_rUid);
    OUString sPwd(_rPwd);
    if ((sUser.isEmpty()) && (sPwd.isEmpty()) && (!m_pImpl->m_sUser.isEmpty()))
    {   // ease the usage of this method. data source which are intended to have a user automatically
        // fill in the user/password combination if the caller of this method does not specify otherwise
        sUser = m_pImpl->m_sUser;
        if (!m_pImpl->m_aPassword.isEmpty())
            sPwd = m_pImpl->m_aPassword;
    }

    TranslateId pExceptionMessageId = RID_STR_COULDNOTCONNECT_UNSPECIFIED;
    if (xManager.is())
    {
        sal_Int32 nAdditionalArgs(0);
        if (!sUser.isEmpty()) ++nAdditionalArgs;
        if (!sPwd.isEmpty()) ++nAdditionalArgs;

        Sequence< PropertyValue > aUserPwd(nAdditionalArgs);
        auto aUserPwdRange = asNonConstRange(aUserPwd);
        sal_Int32 nArgPos = 0;
        if (!sUser.isEmpty())
        {
            aUserPwdRange[ nArgPos ].Name = "user";
            aUserPwdRange[ nArgPos ].Value <<= sUser;
            ++nArgPos;
        }
        if (!sPwd.isEmpty())
        {
            aUserPwdRange[ nArgPos ].Name = "password";
            aUserPwdRange[ nArgPos ].Value <<= sPwd;
        }
        Reference< XDriver > xDriver;
        try
        {

            // choose driver
            Reference< XDriverAccess > xAccessDrivers( xManager, UNO_QUERY );
            if ( xAccessDrivers.is() )
                xDriver = xAccessDrivers->getDriverByURL( m_pImpl->m_sConnectURL );
        }
        catch( const Exception& )
        {
            TOOLS_WARN_EXCEPTION("dbaccess",  "ODatabaseSource::buildLowLevelConnection: got a strange exception while analyzing the error" );
        }
        if ( !xDriver.is() || !xDriver->acceptsURL( m_pImpl->m_sConnectURL ) )
        {
            // Nowadays, it's allowed for a driver to be registered for a given URL, but actually not to accept it.
            // This is because registration nowadays happens at compile time (by adding respective configuration data),
            // but acceptance is decided at runtime.
            pExceptionMessageId = RID_STR_COULDNOTCONNECT_NODRIVER;
        }
        else
        {
            Sequence< PropertyValue > aDriverInfo = lcl_filterDriverProperties(
                xDriver,
                m_pImpl->m_sConnectURL,
                m_pImpl->m_xSettings->getPropertyValues()
            );

            if ( m_pImpl->isEmbeddedDatabase() )
            {
                sal_Int32 nCount = aDriverInfo.getLength();
                aDriverInfo.realloc(nCount + 3 );
                auto pDriverInfo = aDriverInfo.getArray();

                pDriverInfo[nCount].Name = "URL";
                pDriverInfo[nCount++].Value <<= m_pImpl->getURL();

                pDriverInfo[nCount].Name = "Storage";
                Reference< css::document::XDocumentSubStorageSupplier> xDocSup( m_pImpl->getDocumentSubStorageSupplier() );
                pDriverInfo[nCount++].Value <<= xDocSup->getDocumentSubStorage(u"database"_ustr,ElementModes::READWRITE);

                pDriverInfo[nCount].Name = "Document";
                pDriverInfo[nCount++].Value <<= getDatabaseDocument();
            }
            if (nAdditionalArgs)
                xReturn = xManager->getConnectionWithInfo(m_pImpl->m_sConnectURL, ::comphelper::concatSequences(aUserPwd,aDriverInfo));
            else
                xReturn = xManager->getConnectionWithInfo(m_pImpl->m_sConnectURL,aDriverInfo);

            if ( m_pImpl->isEmbeddedDatabase() )
            {
                // see ODatabaseSource::flushed for comment on why we register as FlushListener
                // at the connection
                Reference< XFlushable > xFlushable( xReturn, UNO_QUERY );
                if ( xFlushable.is() )
                    FlushNotificationAdapter::installAdapter( xFlushable, this );
            }
        }
    }
    else
        pExceptionMessageId = RID_STR_COULDNOTLOAD_MANAGER;

    if ( !xReturn.is() )
    {
        OUString sMessage = DBA_RES(pExceptionMessageId)
            .replaceAll("$name$", m_pImpl->m_sConnectURL);

        SQLContext aContext(
            DBA_RES(RID_STR_CONNECTION_REQUEST).replaceFirst("$name$", m_pImpl->m_sConnectURL),
            {}, {}, 0, {}, {});

        throwGenericSQLException( sMessage, static_cast< XDataSource* >( this ), Any( aContext ) );
    }

#if ENABLE_FIREBIRD_SDBC
    if( bNeedMigration )
    {
        Reference< css::document::XDocumentSubStorageSupplier> xDocSup(
                m_pImpl->getDocumentSubStorageSupplier() );
        dbahsql::HsqlImporter importer(xReturn,
                xDocSup->getDocumentSubStorage(u"database"_ustr,ElementModes::READWRITE) );
        importer.importHsqlDatabase(m_pImpl->GetFrameWeld());
    }
#endif

    return xReturn;
}

// OPropertySetHelper
Reference< XPropertySetInfo >  ODatabaseSource::getPropertySetInfo()
{
    return createPropertySetInfo( getInfoHelper() ) ;
}

// comphelper::OPropertyArrayUsageHelper
::cppu::IPropertyArrayHelper* ODatabaseSource::createArrayHelper( ) const
{
    return new ::cppu::OPropertyArrayHelper
    {
        {
            // a change here means a change should also been done in OApplicationController::disposing()
            { PROPERTY_INFO, PROPERTY_ID_INFO, cppu::UnoType<Sequence< PropertyValue >>::get(), css::beans::PropertyAttribute::BOUND },
            { PROPERTY_ISPASSWORDREQUIRED, PROPERTY_ID_ISPASSWORDREQUIRED, cppu::UnoType<bool>::get(), css::beans::PropertyAttribute::BOUND },
            { PROPERTY_ISREADONLY, PROPERTY_ID_ISREADONLY, cppu::UnoType<bool>::get(), css::beans::PropertyAttribute::READONLY },
            { PROPERTY_LAYOUTINFORMATION, PROPERTY_ID_LAYOUTINFORMATION, cppu::UnoType<Sequence< PropertyValue >>::get(), css::beans::PropertyAttribute::BOUND },
            { PROPERTY_NAME, PROPERTY_ID_NAME, cppu::UnoType<OUString>::get(), css::beans::PropertyAttribute::READONLY },
            { PROPERTY_NUMBERFORMATSSUPPLIER, PROPERTY_ID_NUMBERFORMATSSUPPLIER, cppu::UnoType<XNumberFormatsSupplier>::get(),
              css::beans::PropertyAttribute::READONLY | css::beans::PropertyAttribute::TRANSIENT },
            { PROPERTY_PASSWORD, PROPERTY_ID_PASSWORD, cppu::UnoType<OUString>::get(), css::beans::PropertyAttribute::TRANSIENT },
            { PROPERTY_SETTINGS, PROPERTY_ID_SETTINGS, cppu::UnoType<XPropertySet>::get(), css::beans::PropertyAttribute::BOUND | css::beans::PropertyAttribute::READONLY },
            { PROPERTY_SUPPRESSVERSIONCL, PROPERTY_ID_SUPPRESSVERSIONCL, cppu::UnoType<bool>::get(), css::beans::PropertyAttribute::BOUND },
            { PROPERTY_TABLEFILTER, PROPERTY_ID_TABLEFILTER, cppu::UnoType<Sequence< OUString >>::get(), css::beans::PropertyAttribute::BOUND },
            { PROPERTY_TABLETYPEFILTER, PROPERTY_ID_TABLETYPEFILTER, cppu::UnoType<Sequence< OUString >>::get(), css::beans::PropertyAttribute::BOUND },
            { PROPERTY_URL, PROPERTY_ID_URL, cppu::UnoType<OUString>::get(), css::beans::PropertyAttribute::BOUND },
            { PROPERTY_USER, PROPERTY_ID_USER, cppu::UnoType<OUString>::get(), css::beans::PropertyAttribute::BOUND }
        }
    };
}

// cppu::OPropertySetHelper
::cppu::IPropertyArrayHelper& ODatabaseSource::getInfoHelper()
{
    return *getArrayHelper();
}

sal_Bool ODatabaseSource::convertFastPropertyValue(Any & rConvertedValue, Any & rOldValue, sal_Int32 nHandle, const Any& rValue )
{
    if ( m_pImpl.is() )
    {
        switch (nHandle)
        {
            case PROPERTY_ID_TABLEFILTER:
                return ::comphelper::tryPropertyValue(rConvertedValue, rOldValue, rValue, m_pImpl->m_aTableFilter);
            case PROPERTY_ID_TABLETYPEFILTER:
                return ::comphelper::tryPropertyValue(rConvertedValue, rOldValue, rValue, m_pImpl->m_aTableTypeFilter);
            case PROPERTY_ID_USER:
                return ::comphelper::tryPropertyValue(rConvertedValue, rOldValue, rValue, m_pImpl->m_sUser);
            case PROPERTY_ID_PASSWORD:
                return ::comphelper::tryPropertyValue(rConvertedValue, rOldValue, rValue, m_pImpl->m_aPassword);
            case PROPERTY_ID_ISPASSWORDREQUIRED:
                return ::comphelper::tryPropertyValue(rConvertedValue, rOldValue, rValue, m_pImpl->m_bPasswordRequired);
            case PROPERTY_ID_SUPPRESSVERSIONCL:
                return ::comphelper::tryPropertyValue(rConvertedValue, rOldValue, rValue, m_pImpl->m_bSuppressVersionColumns);
            case PROPERTY_ID_LAYOUTINFORMATION:
                return ::comphelper::tryPropertyValue(rConvertedValue, rOldValue, rValue, m_pImpl->m_aLayoutInformation);
            case PROPERTY_ID_URL:
                return ::comphelper::tryPropertyValue(rConvertedValue, rOldValue, rValue, m_pImpl->m_sConnectURL);
            case PROPERTY_ID_INFO:
            {
                Sequence<PropertyValue> aValues;
                if (!(rValue >>= aValues))
                    throw IllegalArgumentException();

                for (auto const& checkName : aValues)
                {
                    if ( checkName.Name.isEmpty() )
                        throw IllegalArgumentException();
                }

                Sequence< PropertyValue > aSettings = m_pImpl->m_xSettings->getPropertyValues();

                rConvertedValue = rValue;
                rOldValue <<= aSettings;

                if (aSettings.getLength() != aValues.getLength())
                    return true;

                for (sal_Int32 i = 0; i < aSettings.getLength(); ++i)
                {
                    if (aValues[i].Name != aSettings[i].Name
                        || aValues[i].Value != aSettings[i].Value)
                        return true;
                }
            }
            break;
            default:
                SAL_WARN("dbaccess", "ODatabaseSource::convertFastPropertyValue: unknown or readonly Property!" );
        }
    }
    return false;
}

namespace
{
    struct SelectPropertyName
    {
    public:
        const OUString& operator()( const PropertyValue& _lhs )
        {
            return _lhs.Name;
        }
    };

    /** sets a new set of property values for a given property bag instance

        The method takes a property bag, and a sequence of property values to set for this bag.
        Upon return, every property which is not part of the given sequence is
        <ul><li>removed from the bag, if it's a removable property</li>
            <li><em>or</em>reset to its default value, if it's not a removable property</li>
        </ul>.

        @param  _rxPropertyBag
            the property bag to operate on
        @param  _rAllNewPropertyValues
            the new property values to set for the bag
    */
    void lcl_setPropertyValues_resetOrRemoveOther( const Reference< XPropertyBag >& _rxPropertyBag, const Sequence< PropertyValue >& _rAllNewPropertyValues )
    {
        // sequences are ugly to operate on
        std::set<OUString> aToBeSetPropertyNames;
        std::transform(
            _rAllNewPropertyValues.begin(),
            _rAllNewPropertyValues.end(),
            std::inserter( aToBeSetPropertyNames, aToBeSetPropertyNames.end() ),
            SelectPropertyName()
        );

        try
        {
            // obtain all properties currently known at the bag
            Reference< XPropertySetInfo > xPSI( _rxPropertyBag->getPropertySetInfo(), UNO_SET_THROW );
            const Sequence< Property > aAllExistentProperties( xPSI->getProperties() );

            Reference< XPropertyState > xPropertyState( _rxPropertyBag, UNO_QUERY_THROW );

            // loop through them, and reset resp. default properties which are not to be set
            for ( auto const & existentProperty : aAllExistentProperties )
            {
                if ( aToBeSetPropertyNames.find( existentProperty.Name ) != aToBeSetPropertyNames.end() )
                    continue;

                // this property is not to be set, but currently exists in the bag.
                // -> Remove it, or reset it to the default.
                if ( ( existentProperty.Attributes & PropertyAttribute::REMOVABLE ) != 0 )
                    _rxPropertyBag->removeProperty( existentProperty.Name );
                else
                    xPropertyState->setPropertyToDefault( existentProperty.Name );
            }

            // finally, set the new property values
            _rxPropertyBag->setPropertyValues( _rAllNewPropertyValues );
        }
        catch( const Exception& )
        {
            DBG_UNHANDLED_EXCEPTION("dbaccess");
        }
    }
}

void ODatabaseSource::setFastPropertyValue_NoBroadcast( sal_Int32 nHandle, const Any& rValue )
{
    if ( !m_pImpl.is() )
        return;

    switch(nHandle)
    {
        case PROPERTY_ID_TABLEFILTER:
            rValue >>= m_pImpl->m_aTableFilter;
            break;
        case PROPERTY_ID_TABLETYPEFILTER:
            rValue >>= m_pImpl->m_aTableTypeFilter;
            break;
        case PROPERTY_ID_USER:
            rValue >>= m_pImpl->m_sUser;
            // if the user name has changed, reset the password
            m_pImpl->m_aPassword.clear();
            break;
        case PROPERTY_ID_PASSWORD:
            rValue >>= m_pImpl->m_aPassword;
            break;
        case PROPERTY_ID_ISPASSWORDREQUIRED:
            m_pImpl->m_bPasswordRequired = any2bool(rValue);
            break;
        case PROPERTY_ID_SUPPRESSVERSIONCL:
            m_pImpl->m_bSuppressVersionColumns = any2bool(rValue);
            break;
        case PROPERTY_ID_URL:
            rValue >>= m_pImpl->m_sConnectURL;
            break;
        case PROPERTY_ID_INFO:
        {
            Sequence< PropertyValue > aInfo;
            OSL_VERIFY( rValue >>= aInfo );
            lcl_setPropertyValues_resetOrRemoveOther( m_pImpl->m_xSettings, aInfo );
        }
        break;
        case PROPERTY_ID_LAYOUTINFORMATION:
            rValue >>= m_pImpl->m_aLayoutInformation;
            break;
    }
    m_pImpl->setModified(true);
}

void ODatabaseSource::getFastPropertyValue( Any& rValue, sal_Int32 nHandle ) const
{
    if ( !m_pImpl.is() )
        return;

    switch (nHandle)
    {
        case PROPERTY_ID_TABLEFILTER:
            rValue <<= m_pImpl->m_aTableFilter;
            break;
        case PROPERTY_ID_TABLETYPEFILTER:
            rValue <<= m_pImpl->m_aTableTypeFilter;
            break;
        case PROPERTY_ID_USER:
            rValue <<= m_pImpl->m_sUser;
            break;
        case PROPERTY_ID_PASSWORD:
            rValue <<= m_pImpl->m_aPassword;
            break;
        case PROPERTY_ID_ISPASSWORDREQUIRED:
            rValue <<= m_pImpl->m_bPasswordRequired;
            break;
        case PROPERTY_ID_SUPPRESSVERSIONCL:
            rValue <<= m_pImpl->m_bSuppressVersionColumns;
            break;
        case PROPERTY_ID_ISREADONLY:
            rValue <<= m_pImpl->m_bReadOnly;
            break;
        case PROPERTY_ID_INFO:
        {
            try
            {
                // collect the property attributes of all current settings
                Reference< XPropertySet > xSettingsAsProps( m_pImpl->m_xSettings, UNO_QUERY_THROW );
                Reference< XPropertySetInfo > xPST( xSettingsAsProps->getPropertySetInfo(), UNO_SET_THROW );
                const Sequence< Property > aSettings( xPST->getProperties() );
                std::map< OUString, sal_Int32 > aPropertyAttributes;
                for ( auto const & setting : aSettings )
                {
                    aPropertyAttributes[ setting.Name ] = setting.Attributes;
                }

                // get all current settings with their values
                Sequence< PropertyValue > aValues( m_pImpl->m_xSettings->getPropertyValues() );

                // transform them so that only property values which fulfill certain
                // criteria survive
                Sequence< PropertyValue > aNonDefaultOrUserDefined( aValues.getLength() );
                auto [begin, end] = asNonConstRange(aValues);
                auto pCopyStart = aNonDefaultOrUserDefined.getArray();
                const PropertyValue* pCopyEnd = std::remove_copy_if(
                    begin,
                    end,
                    pCopyStart,
                    IsDefaultAndNotRemoveable( aPropertyAttributes )
                );
                aNonDefaultOrUserDefined.realloc( pCopyEnd - pCopyStart );
                rValue <<= aNonDefaultOrUserDefined;
            }
            catch( const Exception& )
            {
                DBG_UNHANDLED_EXCEPTION("dbaccess");
            }
        }
        break;
        case PROPERTY_ID_SETTINGS:
            rValue <<= m_pImpl->m_xSettings;
            break;
        case PROPERTY_ID_URL:
            rValue <<= m_pImpl->m_sConnectURL;
            break;
        case PROPERTY_ID_NUMBERFORMATSSUPPLIER:
            rValue <<= m_pImpl->getNumberFormatsSupplier();
            break;
        case PROPERTY_ID_NAME:
            rValue <<= m_pImpl->m_sName;
            break;
        case PROPERTY_ID_LAYOUTINFORMATION:
            rValue <<= m_pImpl->m_aLayoutInformation;
            break;
        default:
            SAL_WARN("dbaccess","unknown Property");
    }
}

// XDataSource
void ODatabaseSource::setLoginTimeout(sal_Int32 seconds)
{
    ModelMethodGuard aGuard( *this );
    m_pImpl->m_nLoginTimeout = seconds;
}

sal_Int32 ODatabaseSource::getLoginTimeout()
{
    ModelMethodGuard aGuard( *this );
    return m_pImpl->m_nLoginTimeout;
}

// XCompletedConnection
Reference< XConnection > SAL_CALL ODatabaseSource::connectWithCompletion( const Reference< XInteractionHandler >& _rxHandler )
{
    return connectWithCompletion(_rxHandler,false);
}

Reference< XConnection > ODatabaseSource::getConnection(const OUString& user, const OUString& password)
{
    return getConnection(user,password,false);
}

Reference< XConnection > SAL_CALL ODatabaseSource::getIsolatedConnection( const OUString& user, const OUString& password )
{
    return getConnection(user,password,true);
}

Reference< XConnection > SAL_CALL ODatabaseSource::getIsolatedConnectionWithCompletion( const Reference< XInteractionHandler >& _rxHandler )
{
    return connectWithCompletion(_rxHandler,true);
}

Reference< XConnection > ODatabaseSource::connectWithCompletion( const Reference< XInteractionHandler >& _rxHandler,bool _bIsolated )
{
    ModelMethodGuard aGuard( *this );

    if (!_rxHandler.is())
    {
        SAL_WARN("dbaccess","ODatabaseSource::connectWithCompletion: invalid interaction handler!");
        return getConnection(m_pImpl->m_sUser, m_pImpl->m_aPassword,_bIsolated);
    }

    OUString sUser(m_pImpl->m_sUser), sPassword(m_pImpl->m_aPassword);
    bool bNewPasswordGiven = false;

    if (m_pImpl->m_bPasswordRequired && sPassword.isEmpty())
    {   // we need a password, but don't have one yet.
        // -> ask the user

        // build an interaction request
        // two continuations (Ok and Cancel)
        rtl::Reference<OInteractionAbort> pAbort = new OInteractionAbort;
        rtl::Reference<OAuthenticationContinuation> pAuthenticate = new OAuthenticationContinuation;

        // the name which should be referred in the login dialog
        OUString sServerName( m_pImpl->m_sName );
        INetURLObject aURLCheck( sServerName );
        if ( aURLCheck.GetProtocol() != INetProtocol::NotValid )
            sServerName = aURLCheck.getBase( INetURLObject::LAST_SEGMENT, true, INetURLObject::DecodeMechanism::Unambiguous );

        // the request
        AuthenticationRequest aRequest;
        aRequest.ServerName = sServerName;
        aRequest.HasRealm = aRequest.HasAccount = false;
        aRequest.HasUserName = aRequest.HasPassword = true;
        aRequest.UserName = m_pImpl->m_sUser;
        aRequest.Password = m_pImpl->m_sFailedPassword.isEmpty() ?  m_pImpl->m_aPassword : m_pImpl->m_sFailedPassword;
        rtl::Reference<OInteractionRequest> pRequest = new OInteractionRequest(Any(aRequest));
        // some knittings
        pRequest->addContinuation(pAbort);
        pRequest->addContinuation(pAuthenticate);

        // handle the request
        try
        {
            _rxHandler->handle(pRequest);
        }
        catch(Exception&)
        {
            DBG_UNHANDLED_EXCEPTION("dbaccess");
        }

        if (!pAuthenticate->wasSelected())
            return Reference< XConnection >();

        // get the result
        sUser = m_pImpl->m_sUser = pAuthenticate->getUser();
        sPassword = pAuthenticate->getPassword();

        if (pAuthenticate->getRememberPassword())
        {
            m_pImpl->m_aPassword = pAuthenticate->getPassword();
            bNewPasswordGiven = true;
        }
        m_pImpl->m_sFailedPassword.clear();
    }

    try
    {
        return getConnection(sUser, sPassword,_bIsolated);
    }
    catch(Exception&)
    {
        if (bNewPasswordGiven)
        {
            m_pImpl->m_sFailedPassword = m_pImpl->m_aPassword;
            // assume that we had an authentication problem. Without this we may, after an unsuccessful connect, while
            // the user gave us a password and the order to remember it, never allow a password input again (at least
            // not without restarting the session)
            m_pImpl->m_aPassword.clear();
        }
        throw;
    }
}

rtl::Reference< OConnection > ODatabaseSource::buildIsolatedConnection(const OUString& user, const OUString& password)
{
    Reference< XConnection > xSdbcConn = buildLowLevelConnection(user, password);
    OSL_ENSURE( xSdbcConn.is(), "ODatabaseSource::buildIsolatedConnection: invalid return value of buildLowLevelConnection!" );
    // buildLowLevelConnection is expected to always succeed
    if ( !xSdbcConn.is() )
        return nullptr;

    // build a connection server and return it (no stubs)
    return new OConnection(*this, xSdbcConn, m_pImpl->m_aContext);
}

Reference< XConnection > ODatabaseSource::getConnection(const OUString& user, const OUString& password,bool _bIsolated)
{
    ModelMethodGuard aGuard( *this );

    Reference< XConnection > xConn;
    if ( _bIsolated )
    {
        xConn = buildIsolatedConnection(user,password);
    }
    else
    { // create a new proxy for the connection
        if ( !m_pImpl->m_xSharedConnectionManager.is() )
        {
            m_pImpl->m_xSharedConnectionManager = new OSharedConnectionManager( m_pImpl->m_aContext );
        }
        xConn = m_pImpl->m_xSharedConnectionManager->getConnection(
            m_pImpl->m_sConnectURL, user, password, m_pImpl->m_xSettings->getPropertyValues(), this );
    }

    if ( xConn.is() )
    {
        Reference< XComponent> xComp(xConn,UNO_QUERY);
        if ( xComp.is() )
            xComp->addEventListener( static_cast< XContainerListener* >( this ) );
        m_pImpl->m_aConnections.emplace_back(xConn);
    }

    return xConn;
}

Reference< XNameAccess > SAL_CALL ODatabaseSource::getBookmarks(  )
{
    ModelMethodGuard aGuard( *this );
    // tdf#114596 this may look nutty but see OBookmarkContainer::acquire()
    return static_cast<XNameContainer*>(&m_Bookmarks);
}

Reference< XNameAccess > SAL_CALL ODatabaseSource::getQueryDefinitions( )
{
    ModelMethodGuard aGuard( *this );

    Reference< XNameAccess > xContainer = m_pImpl->m_xCommandDefinitions;
    if ( !xContainer.is() )
    {
        Any aValue;
        css::uno::Reference< css::uno::XInterface > xMy(*this);
        if (dbtools::getDataSourceSetting(xMy, u"CommandDefinitions"_ustr, aValue))
        {
            OUString sSupportService;
            aValue >>= sSupportService;
            if ( !sSupportService.isEmpty() )
            {
                Sequence<Any> aArgs{ Any(NamedValue(u"DataSource"_ustr,Any(xMy))) };
                xContainer.set( m_pImpl->m_aContext->getServiceManager()->createInstanceWithArgumentsAndContext(sSupportService, aArgs, m_pImpl->m_aContext), UNO_QUERY);
            }
        }
        if ( !xContainer.is() )
        {
            TContentPtr& rContainerData( m_pImpl->getObjectContainer( ODatabaseModelImpl::ObjectType::Query ) );
            xContainer = new OCommandContainer( m_pImpl->m_aContext, *this, rContainerData, false );
        }
        m_pImpl->m_xCommandDefinitions = xContainer;
    }
    return xContainer;
}

// XTablesSupplier
Reference< XNameAccess >  ODatabaseSource::getTables()
{
    ModelMethodGuard aGuard( *this );

    rtl::Reference< OCommandContainer > xContainer = m_pImpl->m_xTableDefinitions;
    if ( !xContainer.is() )
    {
        TContentPtr& rContainerData( m_pImpl->getObjectContainer( ODatabaseModelImpl::ObjectType::Table ) );
        xContainer = new OCommandContainer( m_pImpl->m_aContext, *this, rContainerData, true );
        m_pImpl->m_xTableDefinitions = xContainer.get();
    }
    return xContainer;
}

void SAL_CALL ODatabaseSource::flush(  )
{
    try
    {
        // SYNCHRONIZED ->
        {
            ModelMethodGuard aGuard( *this );

            typedef ::utl::SharedUNOComponent< XModel, ::utl::CloseableComponent > SharedModel;
            SharedModel xModel( m_pImpl->getModel_noCreate(), SharedModel::NoTakeOwnership );

            if ( !xModel.is() )
                xModel.reset( m_pImpl->createNewModel_deliverOwnership(), SharedModel::TakeOwnership );

            Reference< css::frame::XStorable> xStorable( xModel, UNO_QUERY_THROW );
            xStorable->store();
        }
        // <- SYNCHRONIZED

        css::lang::EventObject aFlushedEvent(*this);
        m_aFlushListeners.notifyEach( &XFlushListener::flushed, aFlushedEvent );
    }
    catch( const Exception& )
    {
        DBG_UNHANDLED_EXCEPTION("dbaccess");
    }
}

void SAL_CALL ODatabaseSource::flushed( const EventObject& /*rEvent*/ )
{
    ModelMethodGuard aGuard( *this );

    // Okay, this is some hack.
    //
    // In general, we have the problem that embedded databases write into their underlying storage, which
    // logically is one of our sub storage, and practically is a temporary file maintained by the
    // package implementation. As long as we did not commit this storage and our main storage,
    // the changes made by the embedded database engine are not really reflected in the database document
    // file. This is Bad (TM) for a "real" database application - imagine somebody entering some
    // data, and then crashing: For a database application, you would expect that the data still is present
    // when you connect to the database next time.
    //
    // Since this is a conceptual problem as long as we do use those ZIP packages (in fact, we *cannot*
    // provide the desired functionality as long as we do not have a package format which allows O(1) writes),
    // we cannot completely fix this. However, we can relax the problem by committing more often - often
    // enough so that data loss is more seldom, and seldom enough so that there's no noticeable performance
    // decrease.
    //
    // For this, we introduced a few places which XFlushable::flush their connections, and register as
    // XFlushListener at the embedded connection (which needs to provide the XFlushable functionality).
    // Then, when the connection is flushed, we commit both the database storage and our main storage.
    //
    // #i55274#

    OSL_ENSURE( m_pImpl->isEmbeddedDatabase(), "ODatabaseSource::flushed: no embedded database?!" );
    bool bWasModified = m_pImpl->m_bModified;
    m_pImpl->commitEmbeddedStorage();
    m_pImpl->setModified( bWasModified );
}

void SAL_CALL ODatabaseSource::addFlushListener( const Reference< css::util::XFlushListener >& _xListener )
{
    m_aFlushListeners.addInterface(_xListener);
}

void SAL_CALL ODatabaseSource::removeFlushListener( const Reference< css::util::XFlushListener >& _xListener )
{
    m_aFlushListeners.removeInterface(_xListener);
}

void SAL_CALL ODatabaseSource::elementInserted( const ContainerEvent& /*Event*/ )
{
    ModelMethodGuard aGuard( *this );
    if ( m_pImpl.is() )
        m_pImpl->setModified(true);
}

void SAL_CALL ODatabaseSource::elementRemoved( const ContainerEvent& /*Event*/ )
{
    ModelMethodGuard aGuard( *this );
    if ( m_pImpl.is() )
        m_pImpl->setModified(true);
}

void SAL_CALL ODatabaseSource::elementReplaced( const ContainerEvent& /*Event*/ )
{
    ModelMethodGuard aGuard( *this );
    if ( m_pImpl.is() )
        m_pImpl->setModified(true);
}

// XDocumentDataSource
Reference< XOfficeDatabaseDocument > SAL_CALL ODatabaseSource::getDatabaseDocument()
{
    ModelMethodGuard aGuard( *this );

    rtl::Reference< ODatabaseDocument > xModel( m_pImpl->getModel_noCreate() );
    if ( !xModel.is() )
        xModel = m_pImpl->createNewModel_deliverOwnership();

    return Reference< XOfficeDatabaseDocument >( static_cast<cppu::OWeakObject*>(xModel.get()), UNO_QUERY_THROW );
}

void SAL_CALL ODatabaseSource::initialize( css::uno::Sequence< css::uno::Any > const & rArguments)
{
    ::comphelper::NamedValueCollection aProperties( rArguments );
    if (aProperties.has(u"ParentWindow"_ustr))
        aProperties.get(u"ParentWindow"_ustr) >>= m_pImpl->m_xDialogParent;
}

Reference< XInterface > ODatabaseSource::getThis() const
{
    return *const_cast< ODatabaseSource* >( this );
}

}   // namespace dbaccess

extern "C" SAL_DLLPUBLIC_EXPORT css::uno::XInterface*
com_sun_star_comp_dba_ODatabaseSource(css::uno::XComponentContext* context,
        css::uno::Sequence<css::uno::Any> const &)
{
    css::uno::Reference<XInterface> inst(
        DatabaseContext::create(context)->createInstance());
    inst->acquire();
    return inst.get();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
