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

#include <com/sun/star/embed/XEmbeddedObject.hpp>
#include <com/sun/star/embed/XTransactedObject.hpp>
#include <com/sun/star/embed/XEmbedPersist.hpp>
#include <com/sun/star/embed/ElementModes.hpp>
#include <com/sun/star/lang/XComponent.hpp>
#include <comphelper/fileformat.h>
#include <unotools/ucbstreamhelper.hxx>
#include <unotools/tempfile.hxx>
#include <editeng/flditem.hxx>
#include <svx/svdpagv.hxx>
#include <svx/svdoole2.hxx>
#include <svx/svdograf.hxx>
#include <svx/svdotext.hxx>
#include <editeng/outlobj.hxx>
#include <sot/storage.hxx>
#include <editeng/editobj.hxx>
#include <o3tl/safeint.hxx>
#include <svx/svdobjkind.hxx>
#include <svx/svdouno.hxx>
#include <svx/ImageMapInfo.hxx>
#include <sot/formats.hxx>
#include <svl/urlbmk.hxx>
#include <comphelper/diagnose_ex.hxx>

#include <com/sun/star/form/FormButtonType.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <unotools/streamwrap.hxx>

#include <svx/svdotable.hxx>
#include <svx/unomodel.hxx>
#include <svx/svditer.hxx>
#include <sfx2/docfile.hxx>
#include <comphelper/storagehelper.hxx>
#include <comphelper/servicehelper.hxx>
#include <svtools/embedtransfer.hxx>
#include <DrawDocShell.hxx>
#include <View.hxx>
#include <sdmod.hxx>
#include <sdpage.hxx>
#include <drawdoc.hxx>
#include <stlpool.hxx>
#include <sdxfer.hxx>
#include <unomodel.hxx>
#include <vcl/virdev.hxx>
#include <vcl/svapp.hxx>

using namespace ::com::sun::star;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::io;
using namespace ::com::sun::star::datatransfer;

constexpr sal_uInt32 SDTRANSFER_OBJECTTYPE_DRAWMODEL = 1;
constexpr sal_uInt32 SDTRANSFER_OBJECTTYPE_DRAWOLE   = 2;

SdTransferable::SdTransferable( SdDrawDocument* pSrcDoc, ::sd::View* pWorkView, bool bInitOnGetData )
:   mpPageDocShell( nullptr )
,   mpSdView( pWorkView )
,   mpSdViewIntern( pWorkView )
,   mpSdDrawDocument( nullptr )
,   mpSdDrawDocumentIntern( nullptr )
,   mpSourceDoc( pSrcDoc )
,   mpVDev( nullptr )
,   mbInternalMove( false )
,   mbOwnDocument( false )
,   mbOwnView( false )
,   mbLateInit( bInitOnGetData )
,   mbPageTransferable( false )
,   mbPageTransferablePersistent( false )
{
    if( mpSourceDoc )
        StartListening( *mpSourceDoc );

    if( pWorkView )
        StartListening( *pWorkView );

    if( !mbLateInit )
        CreateData();
}

SdTransferable::~SdTransferable()
{
    SolarMutexGuard g;

    if( mpSourceDoc )
        EndListening( *mpSourceDoc );

    if( mpSdView )
        EndListening( *const_cast< sd::View *>( mpSdView) );

    ObjectReleased();

    if( mbOwnView )
        delete mpSdViewIntern;

    mpOLEDataHelper.reset();

    if( maDocShellRef.is() )
    {
        SfxObjectShell* pObj = maDocShellRef.get();
        ::sd::DrawDocShell* pDocSh = static_cast< ::sd::DrawDocShell*>(pObj);
        pDocSh->DoClose();
    }

    maDocShellRef.clear();

    if( mbOwnDocument )
        delete mpSdDrawDocumentIntern;

    moGraphic.reset();
    moBookmark.reset();
    mpImageMap.reset();

    mpVDev.disposeAndClear();
    mpObjDesc.reset();

    //call explicitly at end of dtor to be covered by above SolarMutex
    maUserData.clear();
}

void SdTransferable::CreateObjectReplacement( SdrObject* pObj )
{
    if( !pObj )
        return;

    mpOLEDataHelper.reset();
    moGraphic.reset();
    moBookmark.reset();
    mpImageMap.reset();

    if( auto pOleObj = dynamic_cast< SdrOle2Obj* >( pObj ) )
    {
        try
        {
            uno::Reference < embed::XEmbeddedObject > xObj = pOleObj->GetObjRef();
            uno::Reference < embed::XEmbedPersist > xPersist( xObj, uno::UNO_QUERY );
            if( xObj.is() && xPersist.is() && xPersist->hasEntry() )
            {
                mpOLEDataHelper.reset( new TransferableDataHelper( new SvEmbedTransferHelper( xObj, pOleObj->GetGraphic(), pOleObj->GetAspect() ) ) );

                // TODO/LATER: the standalone handling of the graphic should not be used any more in future
                // The EmbedDataHelper should bring the graphic in future
                const Graphic* pObjGr = pOleObj->GetGraphic();
                if ( pObjGr )
                    moGraphic.emplace(*pObjGr);
            }
        }
        catch( uno::Exception& )
        {}
    }
    else if( dynamic_cast< const SdrGrafObj *>( pObj ) !=  nullptr && (mpSourceDoc && !SdDrawDocument::GetAnimationInfo( pObj )) )
    {
        moGraphic.emplace( static_cast< SdrGrafObj* >( pObj )->GetTransformedGraphic() );
    }
    else if( pObj->IsUnoObj() && SdrInventor::FmForm == pObj->GetObjInventor() && ( pObj->GetObjIdentifier() == SdrObjKind::FormButton ) )
    {
        SdrUnoObj* pUnoCtrl = static_cast< SdrUnoObj* >( pObj );

        if (SdrInventor::FmForm == pUnoCtrl->GetObjInventor())
        {
            const Reference< css::awt::XControlModel >& xControlModel( pUnoCtrl->GetUnoControlModel() );

            if( !xControlModel.is() )
                return;

            Reference< css::beans::XPropertySet > xPropSet( xControlModel, UNO_QUERY );

            if( !xPropSet.is() )
                return;

            css::form::FormButtonType  eButtonType;
            Any                                     aTmp( xPropSet->getPropertyValue( u"ButtonType"_ustr ) );

            if( aTmp >>= eButtonType )
            {
                OUString aLabel, aURL;

                xPropSet->getPropertyValue( u"Label"_ustr ) >>= aLabel;
                xPropSet->getPropertyValue( u"TargetURL"_ustr ) >>= aURL;

                moBookmark.emplace( aURL, aLabel );
            }
        }
    }
    else if( auto pTextObj = DynCastSdrTextObj( pObj ) )
    {
        const OutlinerParaObject* pPara;

        if( (pPara = pTextObj->GetOutlinerParaObject()) != nullptr )
        {
            const SvxFieldItem* pField;

            if( (pField = pPara->GetTextObject().GetField()) != nullptr )
            {
                const SvxFieldData* pData = pField->GetField();

                if( auto pURL = dynamic_cast< const SvxURLField *>( pData ) )
                {
                    // #i63399# This special code identifies TextFrames which have just a URL
                    // as content and directly add this to the clipboard, probably to avoid adding
                    // an unnecessary DrawObject to the target where paste may take place. This is
                    // wanted only for SdrObjects with no fill and no line, else it is necessary to
                    // use the whole SdrObject. Test here for Line/FillStyle and take shortcut only
                    // when both are unused
                    if(!pObj->HasFillStyle() && !pObj->HasLineStyle())
                    {
                        moBookmark.emplace( pURL->GetURL(), pURL->GetRepresentation() );
                    }
                }
            }
        }
    }

    SvxIMapInfo* pInfo = SvxIMapInfo::GetIMapInfo( pObj );

    if( pInfo )
        mpImageMap.reset( new ImageMap( pInfo->GetImageMap() ) );
}

void SdTransferable::CreateData()
{
    if( mpSdDrawDocument && !mpSdViewIntern )
    {
        mbOwnView = true;

        SdPage* pPage = mpSdDrawDocument->GetSdPage(0, PageKind::Standard);

        if( pPage && 1 == pPage->GetObjCount() )
            CreateObjectReplacement( pPage->GetObj( 0 ) );

        mpVDev = VclPtr<VirtualDevice>::Create( *Application::GetDefaultDevice() );
        mpVDev->SetMapMode(MapMode(mpSdDrawDocumentIntern->GetScaleUnit()));
        mpSdViewIntern = new ::sd::View( *mpSdDrawDocumentIntern, mpVDev );
        mpSdViewIntern->EndListening(*mpSdDrawDocumentIntern );
        mpSdViewIntern->hideMarkHandles();
        SdrPageView* pPageView = mpSdViewIntern->ShowSdrPage(pPage);
        mpSdViewIntern->MarkAllObj(pPageView);
    }
    else if( mpSdView && !mpSdDrawDocumentIntern )
    {
        const SdrMarkList& rMarkList = mpSdView->GetMarkedObjectList();

        if( rMarkList.GetMarkCount() == 1 )
            CreateObjectReplacement( rMarkList.GetMark( 0 )->GetMarkedSdrObj() );

        if( mpSourceDoc )
            mpSourceDoc->CreatingDataObj(this);
        mpSdDrawDocumentIntern = static_cast<SdDrawDocument*>( mpSdView->CreateMarkedObjModel().release() );
        if( mpSourceDoc )
            mpSourceDoc->CreatingDataObj(nullptr);

        if( !maDocShellRef.is() && mpSdDrawDocumentIntern->GetDocSh() )
            maDocShellRef = mpSdDrawDocumentIntern->GetDocSh();

        if( !maDocShellRef.is() )
        {
            OSL_FAIL( "SdTransferable::CreateData(), failed to create a model with persist, clipboard operation will fail for OLE objects!" );
            mbOwnDocument = true;
        }

        // Use dimension of source page
        SdrPageView*        pPgView = mpSdView->GetSdrPageView();
        SdPage*             pOldPage = static_cast<SdPage*>( pPgView->GetPage() );
        SdStyleSheetPool*   pOldStylePool = static_cast<SdStyleSheetPool*>(mpSdView->GetModel().GetStyleSheetPool());
        SdStyleSheetPool*   pNewStylePool = static_cast<SdStyleSheetPool*>( mpSdDrawDocumentIntern->GetStyleSheetPool() );
        SdPage*             pPage = mpSdDrawDocumentIntern->GetSdPage( 0, PageKind::Standard );
        OUString            aOldLayoutName( pOldPage->GetLayoutName() );

        pPage->SetSize( pOldPage->GetSize() );
        pPage->SetLayoutName( aOldLayoutName );
        pNewStylePool->CopyGraphicSheets( *pOldStylePool );
        pNewStylePool->CopyCellSheets( *pOldStylePool );
        pNewStylePool->CopyTableStyles( *pOldStylePool );
        sal_Int32 nPos = aOldLayoutName.indexOf( SD_LT_SEPARATOR );
        if( nPos != -1 )
            aOldLayoutName = aOldLayoutName.copy( 0, nPos );
        StyleSheetCopyResultVector aCreatedSheets;
        pNewStylePool->CopyLayoutSheets( aOldLayoutName, *pOldStylePool, aCreatedSheets );
    }

    // set VisArea and adjust objects if necessary
    if( !(maVisArea.IsEmpty() &&
        mpSdDrawDocumentIntern && mpSdViewIntern &&
        mpSdDrawDocumentIntern->GetPageCount()) )
        return;

    SdPage* pPage = mpSdDrawDocumentIntern->GetSdPage( 0, PageKind::Standard );

    if( 1 == mpSdDrawDocumentIntern->GetPageCount() )
    {
        // #112978# need to use GetAllMarkedBoundRect instead of GetAllMarkedRect to get
        // fat lines correctly
        maVisArea = mpSdViewIntern->GetAllMarkedBoundRect();
        Point   aOrigin( maVisArea.TopLeft() );
        Size    aVector( -aOrigin.X(), -aOrigin.Y() );

        for (const rtl::Reference<SdrObject>& pObj : *pPage)
            pObj->NbcMove( aVector );
    }
    else
        maVisArea.SetSize( pPage->GetSize() );

    // output is at the zero point
    maVisArea.SetPos( Point() );
}

static bool lcl_HasOnlyControls( SdrModel* pModel )
{
    bool bOnlyControls = false;         // default if there are no objects

    if ( pModel )
    {
        SdrPage* pPage = pModel->GetPage(0);
        if (pPage)
        {
            SdrObjListIter aIter( pPage, SdrIterMode::DeepNoGroups );
            SdrObject* pObj = aIter.Next();
            if ( pObj )
            {
                bOnlyControls = true;   // only set if there are any objects at all
                while ( pObj )
                {
                    if (dynamic_cast< const SdrUnoObj *>( pObj ) ==  nullptr)
                    {
                        bOnlyControls = false;
                        break;
                    }
                    pObj = aIter.Next();
                }
            }
        }
    }

    return bOnlyControls;
}

static bool lcl_HasOnlyOneTable( SdrModel* pModel )
{
    if ( pModel )
    {
        SdrPage* pPage = pModel->GetPage(0);
        if (pPage && pPage->GetObjCount() == 1 )
        {
            if( dynamic_cast< sdr::table::SdrTableObj* >( pPage->GetObj(0) ) != nullptr )
                return true;
        }
    }
    return false;
}

void SdTransferable::AddSupportedFormats()
{
    if( mbPageTransferable && !mbPageTransferablePersistent )
        return;

    if( !mbLateInit )
        CreateData();

    if( mpObjDesc )
        AddFormat( SotClipboardFormatId::OBJECTDESCRIPTOR );

    if( mpOLEDataHelper )
    {
        AddFormat( SotClipboardFormatId::EMBED_SOURCE );

        DataFlavorExVector              aVector( mpOLEDataHelper->GetDataFlavorExVector() );

        for( const auto& rItem : aVector )
            AddFormat( rItem );
    }
    else if( moGraphic )
    {
        // #i25616#
        AddFormat( SotClipboardFormatId::DRAWING );

        AddFormat( SotClipboardFormatId::SVXB );

        if( moGraphic->GetType() == GraphicType::Bitmap )
        {
            AddFormat( SotClipboardFormatId::PNG );
            AddFormat( SotClipboardFormatId::BITMAP );
            AddFormat( SotClipboardFormatId::GDIMETAFILE );
        }
        else
        {
            AddFormat( SotClipboardFormatId::GDIMETAFILE );
            AddFormat( SotClipboardFormatId::PNG );
            AddFormat( SotClipboardFormatId::BITMAP );
        }
    }
    else if( moBookmark )
    {
        AddFormat( SotClipboardFormatId::NETSCAPE_BOOKMARK );
        AddFormat( SotClipboardFormatId::STRING );
    }
    else
    {
        AddFormat( SotClipboardFormatId::EMBED_SOURCE );
        AddFormat( SotClipboardFormatId::DRAWING );
        if( !mpSdDrawDocument || !lcl_HasOnlyControls( mpSdDrawDocument ) )
        {
            AddFormat( SotClipboardFormatId::GDIMETAFILE );
            AddFormat( SotClipboardFormatId::PNG );
            AddFormat( SotClipboardFormatId::BITMAP );
        }

        if( lcl_HasOnlyOneTable( mpSdDrawDocument ) ) {
            AddFormat( SotClipboardFormatId::RTF );
            AddFormat( SotClipboardFormatId::RICHTEXT );
        }
    }

    if( mpImageMap )
        AddFormat( SotClipboardFormatId::SVIM );
}

bool SdTransferable::GetData( const DataFlavor& rFlavor, const OUString& rDestDoc )
{
    if (SdModule::get() == nullptr)
        return false;

    SotClipboardFormatId nFormat = SotExchange::GetFormat( rFlavor );
    bool        bOK = false;

    CreateData();

    if( nFormat == SotClipboardFormatId::RTF && lcl_HasOnlyOneTable( mpSdDrawDocument ) )
    {
        bOK = SetTableRTF( mpSdDrawDocument );
    }
    else if( mpOLEDataHelper && mpOLEDataHelper->HasFormat( rFlavor ) )
    {
        // TODO/LATER: support all the graphical formats, the embedded object scenario should not have separated handling
        if( nFormat == SotClipboardFormatId::GDIMETAFILE && moGraphic )
            bOK = SetGDIMetaFile( moGraphic->GetGDIMetaFile() );
        else
            bOK = SetAny( mpOLEDataHelper->GetAny(rFlavor, rDestDoc) );
    }
    else if( HasFormat( nFormat ) )
    {
        if( ( nFormat == SotClipboardFormatId::LINKSRCDESCRIPTOR || nFormat == SotClipboardFormatId::OBJECTDESCRIPTOR ) && mpObjDesc )
        {
            bOK = SetTransferableObjectDescriptor( *mpObjDesc );
        }
        else if( nFormat == SotClipboardFormatId::DRAWING )
        {
            SfxObjectShellRef aOldRef( maDocShellRef );

            maDocShellRef.clear();

            if( mpSdViewIntern )
            {
                SdDrawDocument& rInternDoc = mpSdViewIntern->GetDoc();
                rInternDoc.CreatingDataObj(this);
                SdDrawDocument* pDoc = dynamic_cast< SdDrawDocument* >( mpSdViewIntern->CreateMarkedObjModel().release() );
                rInternDoc.CreatingDataObj(nullptr);

                bOK = SetObject( pDoc, SDTRANSFER_OBJECTTYPE_DRAWMODEL, rFlavor );

                if( maDocShellRef.is() )
                {
                    maDocShellRef->DoClose();
                }
                else
                {
                    delete pDoc;
                }
            }

            maDocShellRef = std::move(aOldRef);
        }
        else if( nFormat == SotClipboardFormatId::GDIMETAFILE )
        {
            if (mpSdViewIntern)
            {
                const bool bToggleOnlineSpell = mpSdDrawDocumentIntern && mpSdDrawDocumentIntern->GetOnlineSpell();
                if (bToggleOnlineSpell)
                    mpSdDrawDocumentIntern->SetOnlineSpell(false);
                bOK = SetGDIMetaFile( mpSdViewIntern->GetMarkedObjMetaFile( true ) );
                if (bToggleOnlineSpell)
                    mpSdDrawDocumentIntern->SetOnlineSpell(true);
            }
        }
        else if( SotClipboardFormatId::BITMAP == nFormat || SotClipboardFormatId::PNG == nFormat )
        {
            if (mpSdViewIntern)
            {
                const bool bToggleOnlineSpell = mpSdDrawDocumentIntern && mpSdDrawDocumentIntern->GetOnlineSpell();
                if (bToggleOnlineSpell)
                    mpSdDrawDocumentIntern->SetOnlineSpell(false);
                bOK = SetBitmapEx( BitmapEx(mpSdViewIntern->GetMarkedObjBitmap(true)), rFlavor );
                if (bToggleOnlineSpell)
                    mpSdDrawDocumentIntern->SetOnlineSpell(true);
            }
        }
        else if( ( nFormat == SotClipboardFormatId::STRING ) && moBookmark )
        {
            bOK = SetString( moBookmark->GetURL() );
        }
        else if( ( nFormat == SotClipboardFormatId::SVXB ) && moGraphic )
        {
            bOK = SetGraphic( *moGraphic );
        }
        else if( ( nFormat == SotClipboardFormatId::SVIM ) && mpImageMap )
        {
            bOK = SetImageMap( *mpImageMap );
        }
        else if( moBookmark )
        {
            bOK = SetINetBookmark( *moBookmark, rFlavor );
        }
        else if( nFormat == SotClipboardFormatId::EMBED_SOURCE )
        {
            if( mpSdDrawDocumentIntern )
            {
                if( !maDocShellRef.is() )
                {
                    maDocShellRef = new ::sd::DrawDocShell(
                        mpSdDrawDocumentIntern,
                        SfxObjectCreateMode::EMBEDDED,
                        true,
                        mpSdDrawDocumentIntern->GetDocumentType());
                    mbOwnDocument = false;
                    maDocShellRef->DoInitNew();
                }

                maDocShellRef->SetVisArea( maVisArea );
                bOK = SetObject( maDocShellRef.get(), SDTRANSFER_OBJECTTYPE_DRAWOLE, rFlavor );
            }
        }
    }

    return bOK;
}

bool SdTransferable::WriteObject( SvStream& rOStm, void* pObject, sal_uInt32 nObjectType, const DataFlavor& )
{
    bool bRet = false;

    switch( nObjectType )
    {
        case SDTRANSFER_OBJECTTYPE_DRAWMODEL:
        {
            try
            {
                static const bool bDontBurnInStyleSheet = ( getenv( "AVOID_BURN_IN_FOR_GALLERY_THEME" ) != nullptr );
                SdDrawDocument* pDoc = static_cast<SdDrawDocument*>(pObject);
                if ( !bDontBurnInStyleSheet )
                    pDoc->BurnInStyleSheetAttributes();
                rOStm.SetBufferSize( 16348 );

                rtl::Reference< SdXImpressDocument > xComponent( new SdXImpressDocument( pDoc, true ) );
                pDoc->setUnoModel( xComponent );

                {
                    css::uno::Reference<css::io::XOutputStream> xDocOut( new utl::OOutputStreamWrapper( rOStm ) );
                    SvxDrawingLayerExport( pDoc, xDocOut, xComponent, (pDoc->GetDocumentType() == DocumentType::Impress) ? "com.sun.star.comp.Impress.XMLClipboardExporter" : "com.sun.star.comp.DrawingLayer.XMLExporter" );
                }

                xComponent->dispose();
                bRet = ( rOStm.GetError() == ERRCODE_NONE );
            }
            catch( Exception& )
            {
                TOOLS_WARN_EXCEPTION( "sd", "sd::SdTransferable::WriteObject()" );
                bRet = false;
            }
        }
        break;

        case SDTRANSFER_OBJECTTYPE_DRAWOLE:
        {
            SfxObjectShell*   pEmbObj = static_cast<SfxObjectShell*>(pObject);
            ::utl::TempFileFast aTempFile;
            SvStream* pTempStream = aTempFile.GetStream(StreamMode::READWRITE);

            try
            {
                uno::Reference< embed::XStorage > xWorkStore =
                    ::comphelper::OStorageHelper::GetStorageFromStream( new utl::OStreamWrapper(*pTempStream), embed::ElementModes::READWRITE );

                // write document storage
                pEmbObj->SetupStorage( xWorkStore, SOFFICE_FILEFORMAT_CURRENT, false );
                // mba: no relative URLs for clipboard!
                SfxMedium aMedium( xWorkStore, OUString() );
                pEmbObj->DoSaveObjectAs( aMedium, false );
                pEmbObj->DoSaveCompleted();

                uno::Reference< embed::XTransactedObject > xTransact( xWorkStore, uno::UNO_QUERY );
                if ( xTransact.is() )
                    xTransact->commit();

                rOStm.SetBufferSize( 0xff00 );
                rOStm.WriteStream( *pTempStream );

                bRet = true;
            }
            catch ( Exception& )
            {}
        }

        break;

        default:
        break;
    }

    return bRet;
}

void SdTransferable::DragFinished( sal_Int8 nDropAction )
{
    if( mpSdView )
        const_cast< ::sd::View* >(mpSdView)->DragFinished( nDropAction );
}

void SdTransferable::ObjectReleased()
{
    SdModule* pModule = SdModule::get();
    if (!pModule)
        return;

    if( this == pModule->pTransferClip )
        pModule->pTransferClip = nullptr;

    if( this == pModule->pTransferDrag )
        pModule->pTransferDrag = nullptr;

    if( this == pModule->pTransferSelection )
        pModule->pTransferSelection = nullptr;
}

void SdTransferable::SetObjectDescriptor( std::unique_ptr<TransferableObjectDescriptor> pObjDesc )
{
    mpObjDesc = std::move(pObjDesc);
    PrepareOLE( *mpObjDesc );
}

void SdTransferable::SetPageBookmarks( std::vector<OUString> && rPageBookmarks, bool bPersistent, bool bMergeMasterPagesOnly )
{
    if( !mpSourceDoc )
        return;

    if( mpSdViewIntern )
        mpSdViewIntern->HideSdrPage();

    mpSdDrawDocument->ClearModel(false);

    mpPageDocShell = nullptr;

    maPageBookmarks.clear();

    if( bPersistent )
    {
        mpSdDrawDocument->CreateFirstPages(mpSourceDoc);
        mpSdDrawDocument->ImportDocumentPages(rPageBookmarks, 1, mpSourceDoc->GetDocSh(), bMergeMasterPagesOnly);
    }
    else
    {
        mpPageDocShell = mpSourceDoc->GetDocSh();
        maPageBookmarks = std::move(rPageBookmarks);
    }

    if( mpSdViewIntern )
    {
        SdPage* pPage = mpSdDrawDocument->GetSdPage( 0, PageKind::Standard );

        if( pPage )
        {
            mpSdViewIntern->MarkAllObj( mpSdViewIntern->ShowSdrPage( pPage ) );
        }
    }

    // set flags for page transferable; if ( mbPageTransferablePersistent == sal_False ),
    // don't offer any formats => it's just for internal purposes
    mbPageTransferable = true;
    mbPageTransferablePersistent = bPersistent;
}

void SdTransferable::AddUserData (const std::shared_ptr<UserData>& rpData)
{
    maUserData.push_back(rpData);
}

sal_Int32 SdTransferable::GetUserDataCount() const
{
    return maUserData.size();
}

std::shared_ptr<SdTransferable::UserData> SdTransferable::GetUserData (const sal_Int32 nIndex) const
{
    if (nIndex>=0 && o3tl::make_unsigned(nIndex)<maUserData.size())
        return maUserData[nIndex];
    else
        return std::shared_ptr<UserData>();
}

SdTransferable* SdTransferable::getImplementation( const Reference< XInterface >& rxData ) noexcept
{
    return dynamic_cast<SdTransferable*>(rxData.get());
}

void SdTransferable::Notify( SfxBroadcaster& rBC, const SfxHint& rHint )
{
    if (rHint.GetId() == SfxHintId::ThisIsAnSdrHint)
    {
        const SdrHint* pSdrHint = static_cast< const SdrHint* >( &rHint );
        if( SdrHintKind::ModelCleared == pSdrHint->GetKind() )
        {
            EndListening(*mpSourceDoc);
            mpSourceDoc = nullptr;
        }
    }
    else
    {
        if( rHint.GetId() == SfxHintId::Dying )
        {
            if( &rBC == mpSourceDoc )
                mpSourceDoc = nullptr;
            if( &rBC == mpSdViewIntern )
                mpSdViewIntern = nullptr;
            if( &rBC == mpSdView )
                mpSdView = nullptr;
        }
    }
}

void SdTransferable::SetView(const ::sd::View* pView)
{
    if (mpSdView)
        EndListening(*const_cast<sd::View*>(mpSdView));
    mpSdView = pView;
    if (mpSdView)
        StartListening(*const_cast<sd::View*>(mpSdView));
}

bool SdTransferable::SetTableRTF( SdDrawDocument* pModel )
{
    if ( pModel )
    {
        SdrPage* pPage = pModel->GetPage(0);
        if (pPage && pPage->GetObjCount() == 1 )
        {
            sdr::table::SdrTableObj* pTableObj = dynamic_cast< sdr::table::SdrTableObj* >( pPage->GetObj(0) );
            if( pTableObj )
            {
                SvMemoryStream aMemStm( 65535, 65535 );
                sdr::table::ExportAsRTF( aMemStm, *pTableObj );
                return SetAny( Any( Sequence< sal_Int8 >( static_cast< const sal_Int8* >( aMemStm.GetData() ), aMemStm.TellEnd() ) ) );
            }
        }
    }

    return false;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
