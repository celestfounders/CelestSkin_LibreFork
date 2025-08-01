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

#include <tools/datetime.hxx>
#include <tools/wldcrd.hxx>
#include <utility>
#include <vcl/commandevent.hxx>
#include <vcl/event.hxx>
#include <vcl/svapp.hxx>
#include <vcl/weld.hxx>
#include <vcl/graphicfilter.hxx>
#include <vcl/virdev.hxx>
#include <com/sun/star/ucb/ContentCreationException.hpp>
#include <sfx2/app.hxx>
#include <sfx2/viewfrm.hxx>
#include <sfx2/bindings.hxx>
#include <galobj.hxx>
#include <svx/svxids.hrc>
#include <svx/gallery1.hxx>
#include <svx/galtheme.hxx>
#include <svx/galmisc.hxx>
#include <svx/galctrl.hxx>
#include <galbrws1.hxx>
#include <svx/strings.hrc>
#include <algorithm>
#include <svx/dialmgr.hxx>
#include <svx/galleryitem.hxx>
#include <comphelper/dispatchcommand.hxx>
#include <comphelper/propertyvalue.hxx>
#include <com/sun/star/beans/PropertyValue.hpp>
#include <comphelper/processfactory.hxx>
#include <com/sun/star/frame/FrameSearchFlag.hpp>
#include <com/sun/star/frame/XDispatchProvider.hpp>
#include <com/sun/star/frame/XFrame.hpp>
#include <com/sun/star/gallery/GalleryItemType.hpp>
#include <com/sun/star/graphic/XGraphic.hpp>
#include <com/sun/star/lang/XComponent.hpp>
#include <com/sun/star/style/GraphicLocation.hpp>
#include <svx/svxdlg.hxx>
#include <memory>
#include <bitmaps.hlst>
#include <osl/diagnose.h>
#include <o3tl/string_view.hxx>

using namespace ::com::sun::star;

namespace
{

struct DispatchInfo
{
    css::util::URL                                  TargetURL;
    css::uno::Sequence< css::beans::PropertyValue > Arguments;
    css::uno::Reference< css::frame::XDispatch >    Dispatch;
};

struct CommandInfo
{
    css::util::URL                               URL;
    css::uno::Reference< css::frame::XDispatch > Dispatch;

    explicit CommandInfo( const OUString &rURL )
    {
        URL.Complete = rURL;
    }
};

class GalleryThemePopup : public ::cppu::WeakImplHelper< css::frame::XStatusListener >
{
private:
    const GalleryTheme* mpTheme;
    sal_uInt32          mnObjectPos;
    bool                mbPreview;
    std::unique_ptr<weld::Builder> mxBuilder;
    std::unique_ptr<weld::Menu> mxPopupMenu;
    std::unique_ptr<weld::Menu> mxBackgroundPopup;
    GalleryBrowser*  mpBrowser;

    typedef std::map< int, CommandInfo > CommandInfoMap;
    CommandInfoMap   m_aCommandInfo;

    static void Execute( const CommandInfo &rCmdInfo,
                  const css::uno::Sequence< css::beans::PropertyValue > &rArguments );

    void MenuSelectHdl(std::u16string_view rIdent);
    void BackgroundMenuSelectHdl(sal_uInt16 nId);
public:
    GalleryThemePopup(weld::Widget* pParent,
                      const GalleryTheme* pTheme,
                      sal_uInt32 nObjectPos,
                      bool bPreview,
                      GalleryBrowser* pBrowser);

    void ExecutePopup(weld::Widget* pParent, const ::Point &rPos);

    virtual void SAL_CALL statusChanged( const css::frame::FeatureStateEvent &rEvent) override;
    virtual void SAL_CALL disposing( const css::lang::EventObject &rSource) override;
};

}

GalleryBrowser::GalleryBrowser(
    weld::Builder& rBuilder,
    Gallery* pGallery)//,
    //std::function<void ()> aThemeSelectionHandler)
    :
    mxNewTheme(rBuilder.weld_button(u"insert"_ustr)),
    mxThemes(rBuilder.weld_tree_view(u"themelist"_ustr)),
    mxMoreGalleries(rBuilder.weld_button(u"btnMoreGalleries"_ustr)),
    mpGallery             ( pGallery ),
    mpExchangeData        ( new ExchangeData ),
    aImgNormal            ( RID_SVXBMP_THEME_NORMAL ),
    aImgDefault           ( RID_SVXBMP_THEME_DEFAULT ),
    aImgReadOnly          ( RID_SVXBMP_THEME_READONLY )
    //maThemeSelectionHandler(std::move(aThemeSelectionHandler))
    , mpCurTheme(nullptr)
    , mxIconView(new GalleryIconView(this, rBuilder.weld_scrolled_window(u"galleryscroll"_ustr, true)))
    , mxIconViewWin(new weld::CustomWeld(rBuilder, u"gallery"_ustr, *mxIconView))
    , mxListView(rBuilder.weld_tree_view(u"gallerylist"_ustr))
    , mxPreview(new GalleryPreview(this, rBuilder.weld_scrolled_window(u"previewscroll"_ustr)))
    , mxPreviewWin(new weld::CustomWeld(rBuilder, u"preview"_ustr, *mxPreview))
    , mxIconButton(rBuilder.weld_toggle_button(u"icon"_ustr))
    , mxListButton(rBuilder.weld_toggle_button(u"list"_ustr))
    , mxSearchField(rBuilder.weld_entry(u"search"_ustr))
    , mxInfoBar(rBuilder.weld_label(u"label"_ustr))
    , maPreviewSize(28, 28)
    , mnCurActionPos      ( 0xffffffff )
    , meMode              ( GALLERYBROWSERMODE_NONE )
    , meLastMode          ( GALLERYBROWSERMODE_NONE )
    , m_aCharacterClassficator( ::comphelper::getProcessComponentContext(), SvtSysLocale().GetLanguageTag() )
{
    mxNewTheme->connect_clicked( LINK( this, GalleryBrowser, ClickNewThemeHdl ) );

    mxThemes->make_sorted();
    mxThemes->connect_selection_changed(LINK(this, GalleryBrowser, SelectThemeHdl));
    mxThemes->connect_popup_menu(LINK(this, GalleryBrowser, PopupMenuHdl1));
    mxThemes->connect_key_press(LINK(this, GalleryBrowser, KeyInputHdl1));
    mxThemes->set_size_request(-1, mxThemes->get_height_rows(6));

    mxMoreGalleries->connect_clicked(LINK(this, GalleryBrowser, OnMoreGalleriesClick));

    // disable creation of new themes if a writable directory is not available
    if( mpGallery->GetUserURL().GetProtocol() == INetProtocol::NotValid )
        mxNewTheme->set_sensitive(false);

    StartListening( *mpGallery );

    for (size_t i = 0, nCount = mpGallery->GetThemeCount(); i < nCount; ++i)
        ImplInsertThemeEntry( mpGallery->GetThemeInfo( i ) );

    m_xContext.set( ::comphelper::getProcessComponentContext() );

    int nHeight = mxListView->get_height_rows(10);
    mxListView->set_size_request(-1, nHeight);
    mxIconView->set_size_request(-1, nHeight);

    m_xTransformer.set(
        m_xContext->getServiceManager()->createInstanceWithContext(
            u"com.sun.star.util.URLTransformer"_ustr, m_xContext ),
        css::uno::UNO_QUERY );

    mxIconButton->connect_toggled( LINK( this, GalleryBrowser, SelectTbxHdl ) );
    mxListButton->connect_toggled( LINK( this, GalleryBrowser, SelectTbxHdl ) );

    mxIconView->SetSelectHdl( LINK( this, GalleryBrowser, SelectObjectValueSetHdl ) );
    mxListView->connect_visible_range_changed(LINK(this, GalleryBrowser, VisRowsScrolledHdl));
    mxListView->connect_size_allocate(LINK(this, GalleryBrowser, SizeAllocHdl));
    mxListView->connect_selection_changed(LINK(this, GalleryBrowser, SelectObjectHdl));
    mxListView->connect_popup_menu(LINK(this, GalleryBrowser, PopupMenuHdl2));
    mxListView->connect_key_press(LINK(this, GalleryBrowser, KeyInputHdl2));
    mxListView->connect_row_activated(LINK(this, GalleryBrowser, RowActivatedHdl));
    mxDragDropTargetHelper.reset(new GalleryDragDrop(this, mxListView->get_drop_target()));
    mxListView->connect_drag_begin(LINK(this, GalleryBrowser, DragBeginHdl));
    mxSearchField->connect_changed( LINK( this, GalleryBrowser, SearchHdl));

    SetMode( ( GALLERYBROWSERMODE_PREVIEW != GalleryBrowser::meInitMode ) ? GalleryBrowser::meInitMode : GALLERYBROWSERMODE_ICON );

    FillThemeEntries();
}

GalleryBrowser::~GalleryBrowser()
{
    EndListening( *mpGallery );
    mpExchangeData.reset();
    if (mpCurTheme)
        mpGallery->ReleaseTheme( mpCurTheme, *this );
}

void GalleryBrowser::ImplInsertThemeEntry( const GalleryThemeEntry* pEntry )
{
    static const bool bShowHiddenThemes = ( getenv( "GALLERY_SHOW_HIDDEN_THEMES" ) != nullptr );

    if( !(pEntry && ( !pEntry->IsHidden() || bShowHiddenThemes )) )
        return;

    const OUString* pImage;

    if( pEntry->IsReadOnly() )
        pImage = &aImgReadOnly;
    else if( pEntry->IsDefault() )
        pImage = &aImgDefault;
    else
        pImage = &aImgNormal;

    mxThemes->append(u""_ustr, pEntry->GetThemeName(), *pImage);
}

void GalleryBrowser::ImplFillExchangeData(const GalleryTheme& rThm, ExchangeData& rData)
{
    rData.pTheme = const_cast<GalleryTheme*>(&rThm);
    rData.aEditedTitle = rThm.GetName();

    try
    {
        DateTime aDateTime(rThm.getModificationDate());

        rData.aThemeChangeDate = aDateTime;
        rData.aThemeChangeTime = aDateTime;
    }
    catch( const ucb::ContentCreationException& )
    {
    }
    catch( const uno::RuntimeException& )
    {
    }
    catch( const uno::Exception& )
    {
    }
}

void GalleryBrowser::ImplGetExecuteVector(std::vector<OUString>& o_aExec)
{
    GalleryTheme*           pTheme = mpGallery->AcquireTheme( GetSelectedTheme(), maLocalListener );

    if( !pTheme )
        return;

    bool                bUpdateAllowed, bRenameAllowed, bRemoveAllowed;
    static const bool   bIdDialog = ( getenv( "GALLERY_ENABLE_ID_DIALOG" ) != nullptr );

    if( pTheme->IsReadOnly() )
        bUpdateAllowed = bRenameAllowed = bRemoveAllowed = false;
    else if( pTheme->IsDefault() )
    {
        bUpdateAllowed = bRenameAllowed = true;
        bRemoveAllowed = false;
    }
    else
        bUpdateAllowed = bRenameAllowed = bRemoveAllowed = true;

    if( bUpdateAllowed && pTheme->GetObjectCount() )
        o_aExec.emplace_back("update");

    if( bRenameAllowed )
        o_aExec.emplace_back("rename");

    if( bRemoveAllowed )
        o_aExec.emplace_back("delete");

    if( bIdDialog && !pTheme->IsReadOnly() )
        o_aExec.emplace_back("assign");

    o_aExec.emplace_back("properties");

    mpGallery->ReleaseTheme( pTheme, maLocalListener );
}

void GalleryBrowser::ImplGalleryThemeProperties( std::u16string_view rThemeName, bool bCreateNew )
{
    DBG_ASSERT(!mpThemePropsDlgItemSet, "mpThemePropsDlgItemSet already set!");
    mpThemePropsDlgItemSet.reset(new SfxItemSet( SfxGetpApp()->GetPool() ));
    GalleryTheme* pTheme = mpGallery->AcquireTheme( rThemeName, maLocalListener );
    if (!pTheme)
    {
        SAL_WARN("svx", "failed to acquire theme: " << OUString(rThemeName));
        return;
    }

    ImplFillExchangeData(*pTheme, *mpExchangeData);

    SvxAbstractDialogFactory* pFact = SvxAbstractDialogFactory::Create();
    VclPtr<VclAbstractDialog> xThemePropertiesDialog = pFact->CreateGalleryThemePropertiesDialog(mxThemes.get(), mpExchangeData.get(), mpThemePropsDlgItemSet.get());

    if ( bCreateNew )
    {
        xThemePropertiesDialog->StartExecuteAsync([xThemePropertiesDialog, this](sal_Int32 nResult){
            EndNewThemePropertiesDlgHdl(nResult);
            xThemePropertiesDialog->disposeOnce();
        });
    }
    else
    {
        xThemePropertiesDialog->StartExecuteAsync([xThemePropertiesDialog, this](sal_Int32 nResult){
            EndThemePropertiesDlgHdl(nResult);
            xThemePropertiesDialog->disposeOnce();
        });
    }
}

void GalleryBrowser::ImplEndGalleryThemeProperties(bool bCreateNew, sal_Int32 nRet)
{
    if( nRet == RET_OK )
    {
        OUString aName( mpExchangeData->pTheme->GetName() );

        if( !mpExchangeData->aEditedTitle.isEmpty() && aName != mpExchangeData->aEditedTitle )
        {
            OUString            aTitle( mpExchangeData->aEditedTitle );
            sal_uInt16          nCount = 0;

            while( mpGallery->HasTheme( aTitle ) && ( nCount++ < 16000 ) )
            {
                aTitle = mpExchangeData->aEditedTitle + " " + OUString::number( nCount );
            }

            mpGallery->RenameTheme( aName, aTitle );
        }

        if ( bCreateNew )
        {
            mxThemes->select_text( mpExchangeData->pTheme->GetName() );
            SelectThemeHdl( *mxThemes );
        }
    }

    OUString aThemeName( mpExchangeData->pTheme->GetName() );
    mpGallery->ReleaseTheme( mpExchangeData->pTheme, maLocalListener );

    if ( bCreateNew && ( nRet != RET_OK ) )
    {
        mpGallery->RemoveTheme( aThemeName );
    }
}

void GalleryBrowser::EndNewThemePropertiesDlgHdl(sal_Int32 nResult)
{
    ImplEndGalleryThemeProperties(true, nResult);
}

void GalleryBrowser::EndThemePropertiesDlgHdl(sal_Int32 nResult)
{
    ImplEndGalleryThemeProperties(false, nResult);
}

void GalleryBrowser::ImplExecute(std::u16string_view rIdent)
{
    if (rIdent == u"update")
    {
        if (GalleryTheme* pTheme = mpGallery->AcquireTheme( GetSelectedTheme(), maLocalListener ))
        {
            SvxAbstractDialogFactory* pFact = SvxAbstractDialogFactory::Create();
            ScopedVclPtr<VclAbstractDialog> aActualizeProgress(pFact->CreateActualizeProgressDialog(mxThemes.get(), pTheme));

            aActualizeProgress->Execute();
            mpGallery->ReleaseTheme( pTheme, maLocalListener );
        }
    }
    else if (rIdent == u"delete")
    {
        std::unique_ptr<weld::Builder> xBuilder(Application::CreateBuilder(mxThemes.get(), u"svx/ui/querydeletethemedialog.ui"_ustr));
        std::unique_ptr<weld::MessageDialog> xQuery(xBuilder->weld_message_dialog(u"QueryDeleteThemeDialog"_ustr));
        if (xQuery->run() == RET_YES)
            mpGallery->RemoveTheme( mxThemes->get_selected_text() );
    }
    else if (rIdent == u"rename")
    {
        if (GalleryTheme* pTheme = mpGallery->AcquireTheme(GetSelectedTheme(), maLocalListener))
        {
            const OUString  aOldName( pTheme->GetName() );

            SvxAbstractDialogFactory* pFact = SvxAbstractDialogFactory::Create();
            ScopedVclPtr<AbstractTitleDialog> aDlg(pFact->CreateTitleDialog(mxThemes.get(), aOldName));

            if( aDlg->Execute() == RET_OK )
            {
                const OUString aNewName( aDlg->GetTitle() );

                if( !aNewName.isEmpty() && ( aNewName != aOldName ) )
                {
                    OUString  aName( aNewName );
                    sal_uInt16  nCount = 0;

                    while( mpGallery->HasTheme( aName ) && ( nCount++ < 16000 ) )
                    {
                        aName = aNewName + " " + OUString::number( nCount );
                    }

                    mpGallery->RenameTheme( aOldName, aName );
                }
            }
            mpGallery->ReleaseTheme( pTheme, maLocalListener );
        }
    }
    else if (rIdent == u"assign")
    {
        if (GalleryTheme* pTheme = mpGallery->AcquireTheme( GetSelectedTheme(), maLocalListener ))
        {
            if (!pTheme->IsReadOnly())
            {
                SvxAbstractDialogFactory* pFact = SvxAbstractDialogFactory::Create();
                ScopedVclPtr<AbstractGalleryIdDialog> aDlg(pFact->CreateGalleryIdDialog(mxThemes.get(), pTheme));
                if( aDlg->Execute() == RET_OK )
                    pTheme->SetId( aDlg->GetId(), true );
            }

            mpGallery->ReleaseTheme( pTheme, maLocalListener );
        }
    }
    else if (rIdent == u"properties")
    {
        ImplGalleryThemeProperties( GetSelectedTheme(), false );
    }
}

//TODO Duplicate method
void GalleryBrowser::Notify( SfxBroadcaster&, const SfxHint& rHint )
{
    const GalleryHint& rGalleryHint = static_cast<const GalleryHint&>(rHint);

    switch( rGalleryHint.GetType() )
    {
        case GalleryHintType::THEME_CREATED:
            ImplInsertThemeEntry( mpGallery->GetThemeInfo( rGalleryHint.GetThemeName() ) );
        break;

        case GalleryHintType::THEME_RENAMED:
        {
            const sal_Int32 nCurSelectPos = mxThemes->get_selected_index();
            const sal_Int32 nRenameEntryPos = mxThemes->find_text( rGalleryHint.GetThemeName() );

            mxThemes->remove_text( rGalleryHint.GetThemeName() );
            ImplInsertThemeEntry( mpGallery->GetThemeInfo( rGalleryHint.GetStringData() ) );

            if( nCurSelectPos == nRenameEntryPos )
            {
                mxThemes->select_text( rGalleryHint.GetStringData() );
                SelectThemeHdl( *mxThemes );
            }
        }
        break;

        case GalleryHintType::THEME_REMOVED:
        {
            mxThemes->remove_text( rGalleryHint.GetThemeName() );
        }
        break;

        case GalleryHintType::CLOSE_THEME:
        {
            const sal_Int32 nCurSelectPos = mxThemes->get_selected_index();
            const sal_Int32 nCloseEntryPos = mxThemes->find_text( rGalleryHint.GetThemeName() );

            if( nCurSelectPos == nCloseEntryPos )
            {
                if( nCurSelectPos < ( mxThemes->n_children() - 1 ) )
                    mxThemes->select( nCurSelectPos + 1 );
                else if( nCurSelectPos )
                    mxThemes->select( nCurSelectPos - 1 );
                else
                    mxThemes->select(-1);

                SelectThemeHdl( *mxThemes );
            }
        }
        break;

        case GalleryHintType::THEME_UPDATEVIEW:
        {
            if( GALLERYBROWSERMODE_PREVIEW == GetMode() )
                SetMode( meLastMode );

            ImplUpdateViews( reinterpret_cast<size_t>(rGalleryHint.GetData1()) + 1 );
        }
        break;
        case GalleryHintType::ADD_OBJECT:
        case GalleryHintType::CLOSE_OBJECT:
        {
            FillThemeEntries();
            ImplUpdateViews(0);
        }
        break;

        default:
        break;
    }

}

IMPL_STATIC_LINK_NOARG( GalleryBrowser, OnMoreGalleriesClick, weld::Button&, void)
{
    css::uno::Sequence<css::beans::PropertyValue> aArgs{
        comphelper::makePropertyValue(u"AdditionsTag"_ustr, u"Gallery"_ustr)
    };
    comphelper::dispatchCommand(u".uno:AdditionsDialog"_ustr, aArgs);
}

IMPL_LINK(GalleryBrowser, KeyInputHdl1, const KeyEvent&, rKEvt, bool)
{
    bool bRet = false;

    std::vector<OUString> aExecVector;
    ImplGetExecuteVector(aExecVector);
    OUString sExecuteIdent;
    bool bMod1 = rKEvt.GetKeyCode().IsMod1();

    switch( rKEvt.GetKeyCode().GetCode() )
    {
        case KEY_INSERT:
            ClickNewThemeHdl(*mxNewTheme);
        break;

        case KEY_I:
        {
            if( bMod1 )
               ClickNewThemeHdl(*mxNewTheme);
        }
        break;

        case KEY_U:
        {
            if( bMod1 )
                sExecuteIdent = "update";
        }
        break;

        case KEY_DELETE:
            sExecuteIdent = "delete";
        break;

        case KEY_D:
        {
            if( bMod1 )
                sExecuteIdent = "delete";
        }
        break;

        case KEY_R:
        {
            if( bMod1 )
                sExecuteIdent = "rename";
        }
        break;

        case KEY_RETURN:
        {
            if( bMod1 )
                sExecuteIdent = "properties";
        }
        break;
    }

    if (!sExecuteIdent.isEmpty() && (std::find( aExecVector.begin(), aExecVector.end(), sExecuteIdent) != aExecVector.end()))
    {
        ImplExecute(sExecuteIdent);
        bRet = true;
    }

    return bRet;
}

IMPL_LINK(GalleryBrowser, PopupMenuHdl1, const CommandEvent&, rCEvt, bool)
{
    if (rCEvt.GetCommand() != CommandEventId::ContextMenu)
        return false;

    std::vector<OUString> aExecVector;
    ImplGetExecuteVector(aExecVector);

    if (aExecVector.empty())
        return true;

    std::unique_ptr<weld::Builder> xBuilder(Application::CreateBuilder(mxThemes.get(), u"svx/ui/gallerymenu1.ui"_ustr));
    std::unique_ptr<weld::Menu> xMenu(xBuilder->weld_menu(u"menu"_ustr));

    xMenu->set_visible(u"update"_ustr, std::find( aExecVector.begin(), aExecVector.end(), "update" ) != aExecVector.end());
    xMenu->set_visible(u"rename"_ustr, std::find( aExecVector.begin(), aExecVector.end(), "rename" ) != aExecVector.end());
    xMenu->set_visible(u"delete"_ustr, std::find( aExecVector.begin(), aExecVector.end(), "delete" ) != aExecVector.end());
    xMenu->set_visible(u"assign"_ustr, std::find( aExecVector.begin(), aExecVector.end(), "assign" ) != aExecVector.end());
    xMenu->set_visible(u"properties"_ustr, std::find( aExecVector.begin(), aExecVector.end(), "properties" ) != aExecVector.end());

    OUString sCommand(xMenu->popup_at_rect(mxThemes.get(), tools::Rectangle(rCEvt.GetMousePosPixel(), Size(1,1))));
    ImplExecute(sCommand);

    return true;
}

IMPL_LINK_NOARG(GalleryBrowser, SelectThemeHdl, weld::TreeView&, void)
{
    SelectTheme(GetSelectedTheme());
}

IMPL_LINK_NOARG(GalleryBrowser, ClickNewThemeHdl, weld::Button&, void)
{
    OUString  aNewTheme( SvxResId(RID_SVXSTR_GALLERY_NEWTHEME) );
    OUString  aName( aNewTheme );
    sal_uInt16 nCount = 0;

    while( mpGallery->HasTheme( aName ) && ( nCount++ < 16000 ) )
    {
        aName = aNewTheme + " " + OUString::number( nCount );
    }

    if( !mpGallery->HasTheme( aName ) && mpGallery->CreateTheme( aName ) )
    {
        ImplGalleryThemeProperties( aName, true );
    }
}

GalleryBrowserMode GalleryBrowser::meInitMode = GALLERYBROWSERMODE_ICON;

IMPL_STATIC_LINK( GalleryBrowser, AsyncDispatch_Impl, void*, p, void )
{
    DispatchInfo* pDispatchInfo = static_cast<DispatchInfo*>(p);
    if ( pDispatchInfo && pDispatchInfo->Dispatch.is() )
    {
        try
        {
            pDispatchInfo->Dispatch->dispatch( pDispatchInfo->TargetURL,
                                               pDispatchInfo->Arguments );
        }
        catch ( const css::uno::Exception& )
        {
        }
    }

    delete pDispatchInfo;
}

IMPL_LINK(GalleryBrowser, PopupMenuHdl2, const CommandEvent&, rCEvt, bool)
{
    if (rCEvt.GetCommand() != CommandEventId::ContextMenu)
        return false;
    ShowContextMenu(rCEvt);
    return true;
}

IMPL_LINK(GalleryBrowser, KeyInputHdl2, const KeyEvent&, rKEvt, bool)
{
    return KeyInput(rKEvt);
}

IMPL_LINK_NOARG(GalleryBrowser, RowActivatedHdl, weld::TreeView&, bool)
{
    TogglePreview();
    return true;
}

sal_Int8 GalleryBrowser::AcceptDrop( const DropTargetHelper& rTarget )
{
    sal_Int8 nRet = DND_ACTION_NONE;

    if( mpCurTheme && !mpCurTheme->IsReadOnly() )
    {
        if( !mpCurTheme->IsDragging() )
        {
            if( rTarget.IsDropFormatSupported( SotClipboardFormatId::DRAWING ) ||
                rTarget.IsDropFormatSupported( SotClipboardFormatId::FILE_LIST ) ||
                rTarget.IsDropFormatSupported( SotClipboardFormatId::SIMPLE_FILE ) ||
                rTarget.IsDropFormatSupported( SotClipboardFormatId::SVXB ) ||
                rTarget.IsDropFormatSupported( SotClipboardFormatId::GDIMETAFILE ) ||
                rTarget.IsDropFormatSupported( SotClipboardFormatId::BITMAP ) )
            {
                nRet = DND_ACTION_COPY;
            }
        }
        else
            nRet = DND_ACTION_COPY;
    }

    return nRet;
}

sal_Int8 GalleryBrowser::ExecuteDrop( const ExecuteDropEvent& rEvt )
{
    sal_Int8 nRet = DND_ACTION_NONE;

    if( mpCurTheme )
    {
        Point aSelPos;
        const sal_uInt32 nItemId = ImplGetSelectedItemId( &rEvt.maPosPixel, aSelPos );
        const sal_uInt32 nInsertPos = (nItemId ? (nItemId - 1) : mpCurTheme->GetObjectCount());

        if( mpCurTheme->IsDragging() )
            mpCurTheme->ChangeObjectPos( mpCurTheme->GetDragPos(), nInsertPos );
        else
            nRet = mpCurTheme->InsertTransferable( rEvt.maDropEvent.Transferable, nInsertPos ) ? 1 : 0;
    }

    return nRet;
}

bool GalleryBrowser::StartDrag()
{
    if (!mpCurTheme)
        return true;
    return m_xHelper->StartDrag();
}

IMPL_LINK(GalleryBrowser, DragBeginHdl, bool&, rUnsetDragIcon, bool)
{
    rUnsetDragIcon = false;
    return StartDrag();
}

void GalleryBrowser::TogglePreview()
{
    SetMode( ( GALLERYBROWSERMODE_PREVIEW != GetMode() ) ? GALLERYBROWSERMODE_PREVIEW : meLastMode );
    GetViewWindow()->grab_focus();
}

bool GalleryBrowser::ShowContextMenu(const CommandEvent& rCEvt)
{
    Point aMousePos = rCEvt.GetMousePosPixel();
    Point aSelPos;
    const sal_uInt32 nItemId = ImplGetSelectedItemId( rCEvt.IsMouseEvent() ? &aMousePos : nullptr, aSelPos );

    if( !(mpCurTheme && nItemId && ( nItemId <= mpCurTheme->GetObjectCount() )) )
        return false;

    ImplSelectItemId( nItemId );

    css::uno::Reference< css::frame::XFrame > xFrame( GetFrame() );
    if ( !xFrame.is() )
        return false;

    weld::Widget* pParent = GetViewWindow();
    mnCurActionPos = nItemId - 1;
    rtl::Reference< GalleryThemePopup > xPopup(
        new GalleryThemePopup(
            pParent,
            mpCurTheme,
            mnCurActionPos,
            GALLERYBROWSERMODE_PREVIEW == GetMode(),
            this ) );
    xPopup->ExecutePopup(pParent, aSelPos);
    return true;
}

bool GalleryBrowser::ViewBoxHasFocus() const
{
    return mxIconButton->has_focus() || mxListButton->has_focus();
}

bool GalleryBrowser::KeyInput(const KeyEvent& rKEvt)
{
    Point       aSelPos;
    const sal_uInt32 nItemId = ImplGetSelectedItemId( nullptr, aSelPos );
    bool bRet = false;

    if (!ViewBoxHasFocus() && nItemId && mpCurTheme)
    {
        OUString sExecuteIdent;
        INetURLObject       aURL;

        mpCurTheme->GetURL( nItemId - 1, aURL );

        const bool  bValidURL = ( aURL.GetProtocol() != INetProtocol::NotValid );
        bool        bPreview = bValidURL;
        bool        bDelete = false;
        bool        bTitle = false;

        if( !mpCurTheme->IsReadOnly() && mpCurTheme->GetObjectCount() )
        {
            bDelete = ( GALLERYBROWSERMODE_PREVIEW != GetMode() );
            bTitle = true;
        }

        switch( rKEvt.GetKeyCode().GetCode() )
        {
            case KEY_SPACE:
            case KEY_RETURN:
            case KEY_P:
            {
                if( bPreview )
                {
                    TogglePreview();
                    bRet = true;
                }
            }
            break;

            case KEY_INSERT:
            case KEY_I:
            {
                // Inserting a gallery item in the document must be dispatched
                if( bValidURL )
                {
                    DispatchAdd(css::uno::Reference<css::frame::XDispatch>(), css::util::URL());
                    return true;
                }
            }
            break;

            case KEY_DELETE:
            case KEY_D:
            {
                if( bDelete )
                    sExecuteIdent = "delete";
            }
            break;

            case KEY_T:
            {
                if( bTitle )
                    sExecuteIdent = "title";
            }
            break;

            default:
            break;
        }

        if (!sExecuteIdent.isEmpty())
        {
            Execute(sExecuteIdent);
            bRet = true;
        }
    }

    return bRet;
}

void GalleryBrowser::SelectTheme( std::u16string_view rThemeName )
{
    if( mpCurTheme )
        mpGallery->ReleaseTheme( mpCurTheme, *this );

    if (rThemeName.empty())
    {
        mxIconButton->set_sensitive(false);
        mxListButton->set_sensitive(false);
        mxListView->clear();
        mxIconView->Clear();
        mpCurTheme = nullptr;
    }
    else
    {
        mpCurTheme = mpGallery->AcquireTheme( rThemeName, *this );

        m_xHelper.set(new GalleryTransferable(mpCurTheme, 0, true));
        rtl::Reference<TransferDataContainer> xHelper(m_xHelper);
        mxListView->enable_drag_source(xHelper, DND_ACTION_COPY | DND_ACTION_LINK);
        mxIconView->SetDragDataTransferable(xHelper, DND_ACTION_COPY | DND_ACTION_LINK);
        mxPreview->SetDragDataTransferable(xHelper, DND_ACTION_COPY | DND_ACTION_LINK);

        mxIconView->SetTheme(mpCurTheme);
        mxPreview->SetTheme(mpCurTheme);

        if( GALLERYBROWSERMODE_PREVIEW == GetMode() )
            meMode = meLastMode;

        ImplUpdateViews( 1 );

        bool bIconMode = (GALLERYBROWSERMODE_ICON == GetMode());
        mxIconButton->set_sensitive(true);
        mxListButton->set_sensitive(true);
        mxIconButton->set_active(bIconMode);
        mxListButton->set_active(!bIconMode);
    }
}

void GalleryBrowser::SetMode( GalleryBrowserMode eMode )
{
    if( GetMode() == eMode )
        return;

    meLastMode = GetMode();

    switch( eMode )
    {
        case GALLERYBROWSERMODE_ICON:
        {
            mxListView->hide();

            mxPreview->Hide();
            mxPreview->SetGraphic( Graphic() );
            GalleryPreview::PreviewMedia( INetURLObject() );

            mxIconView->Show();

            mxIconButton->set_sensitive(true);
            mxListButton->set_sensitive(true);

            mxIconButton->set_active(true);
            mxListButton->set_active(false);
        }
        break;

        case GALLERYBROWSERMODE_LIST:
        {
            mxIconView->Hide();

            mxPreview->Hide();
            mxPreview->SetGraphic( Graphic() );
            GalleryPreview::PreviewMedia( INetURLObject() );

            mxListView->show();
            UpdateRows(true);

            mxIconButton->set_sensitive(true);
            mxListButton->set_sensitive(true);

            mxIconButton->set_active(false);
            mxListButton->set_active(true);
        }
        break;

        case GALLERYBROWSERMODE_PREVIEW:
        {
            Graphic     aGraphic;
            Point       aSelPos;
            const sal_uInt32 nItemId = ImplGetSelectedItemId( nullptr, aSelPos );

            if( nItemId )
            {
                const sal_uInt32 nPos = nItemId - 1;

                mxIconView->Hide();
                mxListView->hide();

                if( mpCurTheme )
                    mpCurTheme->GetGraphic( nPos, aGraphic );

                mxPreview->SetGraphic( aGraphic );
                mxPreview->Show();

                if( mpCurTheme && mpCurTheme->GetObjectKind( nPos ) == SgaObjKind::Sound )
                    GalleryPreview::PreviewMedia( mpCurTheme->GetObjectURL( nPos ) );

                mxIconButton->set_sensitive(false);
                mxListButton->set_sensitive(false);
            }
        }
        break;

        default:
            break;
    }

    GalleryBrowser::meInitMode = meMode = eMode;
}

weld::Widget* GalleryBrowser::GetViewWindow() const
{
    weld::Widget* pRet;

    switch( GetMode() )
    {
        case GALLERYBROWSERMODE_LIST: pRet = mxListView.get(); break;
        case GALLERYBROWSERMODE_PREVIEW: pRet = mxPreview->GetDrawingArea(); break;

        default:
            pRet = mxIconView->GetDrawingArea();
        break;
    }

    return pRet;
}

void GalleryBrowser::Travel( GalleryBrowserTravel eTravel )
{
    if( !mpCurTheme )
        return;

    Point       aSelPos;
    const sal_uInt32 nItemId = ImplGetSelectedItemId( nullptr, aSelPos );

    if( !nItemId )
        return;

    sal_uInt32 nNewItemId = nItemId;

    switch( eTravel )
    {
        case GalleryBrowserTravel::First:     nNewItemId = 1; break;
        case GalleryBrowserTravel::Last:      nNewItemId = mpCurTheme->GetObjectCount(); break;
        case GalleryBrowserTravel::Previous:  nNewItemId--; break;
        case GalleryBrowserTravel::Next:      nNewItemId++; break;
        default:
            break;
    }

    if( nNewItemId < 1 )
        nNewItemId = 1;
    else if( nNewItemId > mpCurTheme->GetObjectCount() )
        nNewItemId = mpCurTheme->GetObjectCount();

    if( nNewItemId == nItemId )
        return;

    ImplSelectItemId( nNewItemId );
    ImplUpdateInfoBar();

    if( GALLERYBROWSERMODE_PREVIEW != GetMode() )
        return;

    Graphic     aGraphic;
    const sal_uInt32 nPos = nNewItemId - 1;

    mpCurTheme->GetGraphic( nPos, aGraphic );
    mxPreview->SetGraphic( aGraphic );

    if( SgaObjKind::Sound == mpCurTheme->GetObjectKind( nPos ) )
        GalleryPreview::PreviewMedia( mpCurTheme->GetObjectURL( nPos ) );

    mxPreview->Invalidate();
}

void GalleryBrowser::ImplUpdateViews( sal_uInt16 nSelectionId )
{
    mxIconView->Hide();
    mxListView->hide();
    mxPreview->Hide();

    mxIconView->Clear();
    mxListView->clear();

    if( mpCurTheme )
    {
        const int nAlwaysUpToDate = 15;

        mxListView->freeze();
        OUString aThemeName = mpCurTheme->GetName();
        sal_Int32 inserted = 0;
        for (GalleryThemeEntries::const_iterator aFoundIter = maFoundThemeEntries.begin(); aFoundIter != maFoundThemeEntries.end(); ++aFoundIter)
        {
            if (aFoundIter->maThemeName == aThemeName)
            {
               mxIconView->InsertItem(aFoundIter->mnIdInTheme + 1); // skip reserved id 0
               mxListView->append(OUString::number(aFoundIter->mnIdInTheme),u""_ustr); // create on-demand in VisRowsScrolledHdl

               if (inserted == nAlwaysUpToDate) // fill in the first block
                    UpdateRows(false);
               ++inserted;
            }
        }

/* sal_uInt32 nCount = mpCurTheme->GetObjectCount();
        for (sal_uInt32 i = 0; i < nCount; ++i)
        {
            mxIconView->InsertItem(i + 1); // skip reserved id 0
            mxListView->append(OUString::number(i), ""); // create on-demand in VisRowsScrolledHdl

            if (i == nAlwaysUpToDate) // fill in the first block
                UpdateRows(false);
        }*/

        if (inserted < nAlwaysUpToDate) // if less than block size, fill in all of them
            UpdateRows(false);

        mxListView->thaw();

        ImplSelectItemId( std::min<sal_uInt16>( nSelectionId, mpCurTheme->GetObjectCount() ) );
    }

    switch( GetMode() )
    {
        case GALLERYBROWSERMODE_ICON: mxIconView->Show(); break;
        case GALLERYBROWSERMODE_LIST:
            mxListView->show();
            UpdateRows(true);
            break;
        case GALLERYBROWSERMODE_PREVIEW: mxPreview->Show(); break;

        default:
        break;
    }

    ImplUpdateInfoBar();
}

void GalleryBrowser::UpdateRows(bool bVisibleOnly)
{
    auto lambda = [this](weld::TreeIter& rEntry){
        // id is non-null if the preview is pending creation
        OUString sId(mxListView->get_id(rEntry));
        if (sId.isEmpty())
            return false;

        // get the icon for the listview
        Bitmap aBitmap;
        Size aPreparedSize;

        OUString sItemTextTitle;
        OUString sItemTextPath;

        sal_Int32 i = sId.toUInt32();
        mpCurTheme->GetPreviewBitmapAndStrings(i, aBitmap, aPreparedSize, sItemTextTitle, sItemTextPath);

        bool bNeedToCreate(aBitmap.IsEmpty());
        if (!bNeedToCreate && (sItemTextTitle.isEmpty() || aPreparedSize != maPreviewSize))
            bNeedToCreate = true;

        if (bNeedToCreate)
        {
            std::unique_ptr<SgaObject> xObj = mpCurTheme->AcquireObject(i);
            if (xObj)
            {
                aBitmap = Bitmap(xObj->createPreviewBitmapEx(maPreviewSize));
                sItemTextTitle = GalleryBrowser::GetItemText(*xObj, GalleryItemFlags::Title);
                sItemTextPath = GalleryBrowser::GetItemText(*xObj, GalleryItemFlags::Path);

                mpCurTheme->SetPreviewBitmapAndStrings(i, aBitmap, maPreviewSize, sItemTextTitle, sItemTextPath);
            }
        }

        ScopedVclPtr<VirtualDevice> xDev(mxListView->create_virtual_device());
        xDev->SetOutputSizePixel(maPreviewSize);

        if (!aBitmap.IsEmpty())
        {
            const Size aBitmapSizePixel(aBitmap.GetSizePixel());
            const Point aPos(
                ((maPreviewSize.Width() - aBitmapSizePixel.Width()) >> 1),
                ((maPreviewSize.Height() - aBitmapSizePixel.Height()) >> 1));

            if (aBitmap.HasAlpha())
            {
                // draw checkered background
                GalleryIconView::drawTransparenceBackground(*xDev, aPos, aBitmapSizePixel);
            }

            xDev->DrawBitmapEx(aPos, aBitmap);
        }

        mxListView->set_text(rEntry, sItemTextTitle);
        mxListView->set_image(rEntry, *xDev);
        mxListView->set_id(rEntry, OUString());

        return false;
    };

    if (bVisibleOnly)
    {
        // ensure all visible entries are up to date
        mxListView->visible_foreach(lambda);
        // and ensure all selected entries are up to date
        mxListView->selected_foreach(lambda);
        return;
    }

    mxListView->all_foreach(lambda);
}

IMPL_LINK_NOARG(GalleryBrowser, VisRowsScrolledHdl, weld::TreeView&, void)
{
    UpdateRows(true);
}

IMPL_LINK_NOARG(GalleryBrowser, SizeAllocHdl, const Size&, void)
{
    UpdateRows(true);
}

void GalleryBrowser::ImplUpdateInfoBar()
{
    if (!mpCurTheme)
        return;
    mxInfoBar->set_label( mpCurTheme->GetName() );
}

void GalleryBrowser::ImplUpdateSelection()
{
    if (!mpCurTheme)
        return;
    auto nSelectedObject = (GALLERYBROWSERMODE_ICON == GetMode()) ? (mxIconView->GetSelectedItemId() - 1) : mxListView->get_selected_index();
    m_xHelper->SelectObject(nSelectedObject);
}

sal_uInt32 GalleryBrowser::ImplGetSelectedItemId( const Point* pSelPos, Point& rSelPos )
{
    sal_uInt32 nRet = 0;

    if( GALLERYBROWSERMODE_PREVIEW == GetMode() )
    {
        nRet = ( ( GALLERYBROWSERMODE_ICON == meLastMode ) ? mxIconView->GetSelectedItemId() : ( mxListView->get_selected_index() + 1 ) );

        if( pSelPos )
            rSelPos = *pSelPos;
        else
        {
            Size aOutputSizePixel(mxPreview->GetOutputSizePixel());
            rSelPos = Point( aOutputSizePixel.Width() >> 1, aOutputSizePixel.Height() >> 1 );
        }
    }
    else if (GALLERYBROWSERMODE_ICON == GetMode())
    {
        if (pSelPos)
        {
            nRet = mxIconView->GetItemId( *pSelPos );
            rSelPos = *pSelPos;
        }
        else
        {
            nRet = mxIconView->GetSelectedItemId();
            rSelPos = mxIconView->GetItemRect(nRet).Center();
        }
    }
    else
    {
        std::unique_ptr<weld::TreeIter> xIter = mxListView->make_iterator();
        if( pSelPos )
        {
            if (mxListView->get_dest_row_at_pos(*pSelPos, xIter.get(), false))
                nRet = mxListView->get_iter_index_in_parent(*xIter) + 1;
            rSelPos = *pSelPos;
        }
        else
        {
            if (mxListView->get_selected(xIter.get()))
            {
                nRet = mxListView->get_iter_index_in_parent(*xIter) + 1;
                rSelPos = mxListView->get_row_area(*xIter).Center();
            }
        }
    }

    if( nRet && ( !mpCurTheme || ( nRet > mpCurTheme->GetObjectCount() ) ) )
    {
        nRet = 0;
    }

    return nRet;
}

void GalleryBrowser::ImplSelectItemId(sal_uInt32 nItemId)
{
    if( nItemId )
    {
        mxIconView->SelectItem(nItemId);
        mxListView->select( nItemId - 1 );
        ImplUpdateSelection();
    }
}

css::uno::Reference< css::frame::XFrame >
GalleryBrowser::GetFrame()
{
    css::uno::Reference< css::frame::XFrame > xFrame;
    SfxViewFrame* pCurrentViewFrame = SfxViewFrame::Current();
    if ( pCurrentViewFrame )
    {
        SfxBindings& rBindings = pCurrentViewFrame->GetBindings();
        xFrame.set( rBindings.GetActiveFrame() );
    }

    return xFrame;
}

void GalleryBrowser::DispatchAdd(
    const css::uno::Reference< css::frame::XDispatch > &rxDispatch,
    const css::util::URL &rURL)
{
    Point aSelPos;
    const sal_uInt32 nItemId = ImplGetSelectedItemId( nullptr, aSelPos );

    if( !mpCurTheme || !nItemId )
        return;

    mnCurActionPos = nItemId - 1;

    css::uno::Reference< css::frame::XDispatch > xDispatch( rxDispatch );
    css::util::URL aURL = rURL;

    if ( !xDispatch.is() )
    {
        css::uno::Reference< css::frame::XDispatchProvider > xDispatchProvider(
            GetFrame(), css::uno::UNO_QUERY );
        if ( !xDispatchProvider.is() || !m_xTransformer.is() )
            return;

        aURL.Complete = ".uno:InsertGalleryPic";
        m_xTransformer->parseStrict( aURL );
        xDispatch = xDispatchProvider->queryDispatch(
            aURL,
            u"_self"_ustr,
            css::frame::FrameSearchFlag::SELF );
    }

    if ( !xDispatch.is() )
        return;

    sal_Int8 nType = 0;
    OUString aFilterName;
    css::uno::Reference< css::lang::XComponent > xDrawing;
    css::uno::Reference< css::graphic::XGraphic > xGraphic;

    aFilterName = GetFilterName();

    switch( mpCurTheme->GetObjectKind( mnCurActionPos ) )
    {
        case SgaObjKind::Bitmap:
        case SgaObjKind::Animation:
        case SgaObjKind::Inet:
        // TODO drawing objects are inserted as drawings only via drag&drop
        case SgaObjKind::SvDraw:
            nType = css::gallery::GalleryItemType::GRAPHIC;
        break;

        case SgaObjKind::Sound :
            nType = css::gallery::GalleryItemType::MEDIA;
        break;

        default:
            nType = css::gallery::GalleryItemType::EMPTY;
        break;
    }

    Graphic aGraphic;
    bool bGraphic = mpCurTheme->GetGraphic( mnCurActionPos, aGraphic );
    if ( bGraphic && !aGraphic.IsNone() )
        xGraphic.set( aGraphic.GetXGraphic() );
    OSL_ENSURE( xGraphic.is(), "gallery item is graphic, but the reference is invalid!" );

    css::uno::Sequence< css::beans::PropertyValue > aSeq{
        comphelper::makePropertyValue(SVXGALLERYITEM_TYPE, nType),
        comphelper::makePropertyValue(SVXGALLERYITEM_URL, OUString()),
        comphelper::makePropertyValue(SVXGALLERYITEM_FILTER, aFilterName),
        comphelper::makePropertyValue(SVXGALLERYITEM_DRAWING, xDrawing),
        comphelper::makePropertyValue(SVXGALLERYITEM_GRAPHIC, xGraphic)
    };
    assert(aSeq.getLength() == SVXGALLERYITEM_PARAMS);

    css::uno::Sequence< css::beans::PropertyValue > aArgs{ comphelper::makePropertyValue(
        SVXGALLERYITEM_ARGNAME, aSeq) };

    std::unique_ptr<DispatchInfo> pInfo(new DispatchInfo);
    pInfo->TargetURL = std::move(aURL);
    pInfo->Arguments = std::move(aArgs);
    pInfo->Dispatch = std::move(xDispatch);

    if ( Application::PostUserEvent(
            LINK( nullptr, GalleryBrowser, AsyncDispatch_Impl), pInfo.get() ) )
        pInfo.release();
}

void GalleryBrowser::Execute(std::u16string_view rIdent)
{
    Point       aSelPos;
    const sal_uInt32 nItemId = ImplGetSelectedItemId( nullptr, aSelPos );

    if( !(mpCurTheme && nItemId) )
        return;

    mnCurActionPos = nItemId - 1;

    if (rIdent == u"preview")
        SetMode( ( GALLERYBROWSERMODE_PREVIEW != GetMode() ) ? GALLERYBROWSERMODE_PREVIEW : meLastMode );
    else if (rIdent == u"delete")
    {
        if (!mpCurTheme->IsReadOnly())
        {
            std::unique_ptr<weld::Builder> xBuilder(Application::CreateBuilder(GetViewWindow(), u"svx/ui/querydeleteobjectdialog.ui"_ustr));
            std::unique_ptr<weld::MessageDialog> xQuery(xBuilder->weld_message_dialog(u"QueryDeleteObjectDialog"_ustr));
            if (xQuery->run() == RET_YES)
            {
                mpCurTheme->RemoveObject( mnCurActionPos );
            }
        }
    }
    else if (rIdent == u"title")
    {
        std::unique_ptr<SgaObject> pObj = mpCurTheme->AcquireObject( mnCurActionPos );

        if( pObj )
        {
            const OUString  aOldTitle( GetItemText( *pObj, GalleryItemFlags::Title ) );

            SvxAbstractDialogFactory* pFact = SvxAbstractDialogFactory::Create();
            ScopedVclPtr<AbstractTitleDialog> aDlg(pFact->CreateTitleDialog(GetViewWindow(), aOldTitle));
            if( aDlg->Execute() == RET_OK )
            {
                OUString aNewTitle( aDlg->GetTitle() );

                if( ( aNewTitle.isEmpty() && !pObj->GetTitle().isEmpty() ) || ( aNewTitle != aOldTitle ) )
                {
                    if( aNewTitle.isEmpty() )
                        aNewTitle = "__<empty>__";

                    pObj->SetTitle( aNewTitle );
                    mpCurTheme->InsertObject( *pObj );
                }
            }
        }
    }
    else if (rIdent == u"copy")
    {
        mpCurTheme->CopyToClipboard(*GetViewWindow(), mnCurActionPos);
    }
    else if (rIdent == u"paste")
    {
        if( !mpCurTheme->IsReadOnly() )
        {
            weld::Widget* pParent = GetViewWindow();
            TransferableDataHelper aDataHelper(TransferableDataHelper::CreateFromClipboard(pParent->get_clipboard()));
            mpCurTheme->InsertTransferable( aDataHelper.GetTransferable(), mnCurActionPos );
        }
    }
}

OUString GalleryBrowser::GetItemText( const SgaObject& rObj, GalleryItemFlags nItemTextFlags )
{
    OUString          aRet;

    const INetURLObject& aURL(rObj.GetURL());

    if( nItemTextFlags & GalleryItemFlags::Title )
    {
        OUString aTitle( rObj.GetTitle() );

        if( aTitle.isEmpty() )
            aTitle = aURL.getBase( INetURLObject::LAST_SEGMENT, true, INetURLObject::DecodeMechanism::Unambiguous );

        if( aTitle.isEmpty() )
        {
            aTitle = aURL.GetMainURL( INetURLObject::DecodeMechanism::Unambiguous );
            aTitle = aTitle.copy( aTitle.lastIndexOf('/')+1 );
        }

        aRet += aTitle;
    }

    if( nItemTextFlags & GalleryItemFlags::Path )
    {
        const OUString aPath( aURL.getFSysPath( FSysStyle::Detect ) );

        if( !aPath.isEmpty() && ( nItemTextFlags & GalleryItemFlags::Title ) )
            aRet += " (";

        aRet += aURL.getFSysPath( FSysStyle::Detect );

        if( !aPath.isEmpty() && ( nItemTextFlags & GalleryItemFlags::Title ) )
            aRet += ")";
    }

    return aRet;
}

INetURLObject GalleryBrowser::GetURL() const
{
    INetURLObject aURL;

    if( mpCurTheme && mnCurActionPos != 0xffffffff )
        aURL = mpCurTheme->GetObjectURL( mnCurActionPos );

    return aURL;
}

OUString GalleryBrowser::GetFilterName() const
{
    OUString aFilterName;

    if( mpCurTheme && mnCurActionPos != 0xffffffff )
    {
        const SgaObjKind eObjKind = mpCurTheme->GetObjectKind( mnCurActionPos );

        if( ( SgaObjKind::Bitmap == eObjKind ) || ( SgaObjKind::Animation == eObjKind ) )
        {
            GraphicFilter& rFilter = GraphicFilter::GetGraphicFilter();
            INetURLObject       aURL;
            mpCurTheme->GetURL( mnCurActionPos, aURL );
            sal_uInt16 nFilter = rFilter.GetImportFormatNumberForShortName(aURL.GetFileExtension());

            if( GRFILTER_FORMAT_DONTKNOW != nFilter )
                aFilterName = rFilter.GetImportFormatName( nFilter );
        }
    }

    return aFilterName;
}

IMPL_LINK_NOARG(GalleryBrowser, SelectObjectValueSetHdl, ValueSet*, void)
{
    ImplUpdateSelection();
}

IMPL_LINK_NOARG(GalleryBrowser, SelectObjectHdl, weld::TreeView&, void)
{
    ImplUpdateSelection();
}

IMPL_LINK(GalleryBrowser, SelectTbxHdl, weld::Toggleable&, rBox, void)
{
    if (&rBox == mxIconButton.get())
        SetMode(rBox.get_active() ? GALLERYBROWSERMODE_ICON : GALLERYBROWSERMODE_LIST);
    else if (&rBox == mxListButton.get())
        SetMode(rBox.get_active() ? GALLERYBROWSERMODE_LIST : GALLERYBROWSERMODE_ICON);
}

void GalleryBrowser::FillThemeEntries()
{
    maFoundThemeEntries.clear();
    maAllThemeEntries.clear();
    for (size_t i = 0, nCount = mpGallery->GetThemeCount(); i < nCount; ++i)
    {
        const GalleryThemeEntry* pThemeInfo = mpGallery->GetThemeInfo( i );
        OUString aThemeName = pThemeInfo->GetThemeName();
        //sal_uInt32 nId = pThemeInfo->GetId();
        if (GalleryTheme* pTheme = mpGallery->AcquireTheme(aThemeName, maLocalListener))
        {
            sal_uInt32 nObjectCount = pTheme->GetObjectCount();
            for (size_t nObject = 0; nObject < nObjectCount; ++nObject)
            {
                if (std::unique_ptr<SgaObject> xSgaObject = pTheme->AcquireObject(nObject))
                {
                    OUString aTitle = GetItemText(*xSgaObject, GalleryItemFlags::Title);
                    maAllThemeEntries.push_back(ThemeEntry(aThemeName, aTitle, nObject));
                }
            }
            mpGallery->ReleaseTheme(pTheme, maLocalListener);
        }
    }
    maFoundThemeEntries.assign(maAllThemeEntries.begin(), maAllThemeEntries.end());
}
IMPL_LINK(GalleryBrowser, SearchHdl, weld::Entry&, searchEdit, void)
{
    OUString search =   searchEdit.get_text().trim();
    OUString curThemeName;
    ::std::set<OUString> aFoundThemes;
    if (mpCurTheme)
    {
        curThemeName = mpCurTheme->GetName();
    }
    if (search.isEmpty())
    {
        maFoundThemeEntries.assign(maAllThemeEntries.begin(), maAllThemeEntries.end());
        if (maAllThemeEntries.begin() != maAllThemeEntries.end())
            curThemeName = maAllThemeEntries.begin()->maThemeName;
    }
    else
    {
        search = "*" + search + "*";
        WildCard aSearchExpression(m_aCharacterClassficator.lowercase(search));
        bool currentThemeFound = false;
        maFoundThemeEntries.clear();
        for (GalleryThemeEntries::const_iterator allIter = maAllThemeEntries.begin(); allIter != maAllThemeEntries.end(); ++allIter)
        {
            if (aSearchExpression.Matches(m_aCharacterClassficator.lowercase(allIter->maEntryTitle)))
            {
                maFoundThemeEntries.push_back(*allIter);
                aFoundThemes.insert(allIter->maThemeName);
                if (curThemeName == allIter->maThemeName)
                    currentThemeFound = true;
            }
        }
        if (!currentThemeFound)
        {
            if (maFoundThemeEntries.begin() != maFoundThemeEntries.end())
                curThemeName = maFoundThemeEntries.begin()->maThemeName;
            else
                curThemeName.clear();
        }
    }
    mxThemes->clear();
    if (search.isEmpty())
    {
        for (size_t i = 0, nCount = mpGallery->GetThemeCount(); i < nCount; ++i)
            ImplInsertThemeEntry( mpGallery->GetThemeInfo( i ) );
    }
    else
    {
        for(std::set<OUString>::iterator foundIter = aFoundThemes.begin(); foundIter != aFoundThemes.end(); ++foundIter)
            ImplInsertThemeEntry(mpGallery->GetThemeInfo(*foundIter));
    }
    mxThemes->select_text(curThemeName);
    SelectTheme(curThemeName);
}

namespace
{

GalleryThemePopup::GalleryThemePopup(
    weld::Widget* pParent,
    const GalleryTheme* pTheme,
    sal_uInt32 nObjectPos,
    bool bPreview,
    GalleryBrowser* pBrowser )
    : mpTheme( pTheme )
    , mnObjectPos( nObjectPos )
    , mbPreview( bPreview )
    , mxBuilder(Application::CreateBuilder(pParent, u"svx/ui/gallerymenu2.ui"_ustr))
    , mxPopupMenu(mxBuilder->weld_menu(u"menu"_ustr))
    , mxBackgroundPopup(mxBuilder->weld_menu(u"backgroundmenu"_ustr))
    , mpBrowser( pBrowser )
{
    // SID_GALLERY_ENABLE_ADDCOPY
    m_aCommandInfo.emplace(
            SID_GALLERY_ENABLE_ADDCOPY,
            CommandInfo( u".uno:GalleryEnableAddCopy"_ustr ));
    // SID_GALLERY_BG_BRUSH
    m_aCommandInfo.emplace(
            SID_GALLERY_BG_BRUSH,
            CommandInfo( u".uno:BackgroundImage"_ustr ));
    // SID_GALLERY_FORMATS
    m_aCommandInfo.emplace(
            SID_GALLERY_FORMATS,
            CommandInfo( u".uno:InsertGalleryPic"_ustr ));

}

void SAL_CALL GalleryThemePopup::statusChanged(
    const css::frame::FeatureStateEvent &rEvent )
{
    const OUString &rURL = rEvent.FeatureURL.Complete;
    if ( rURL == ".uno:GalleryEnableAddCopy" )
    {
        if ( !rEvent.IsEnabled )
        {
            mxPopupMenu->set_visible(u"add"_ustr, false);
        }
    }
    else if ( rURL == ".uno:BackgroundImage" )
    {
        mxBackgroundPopup->clear();
        if ( rEvent.IsEnabled )
        {
            OUString sItem;
            css::uno::Sequence< OUString > sItems;
            if ( ( rEvent.State >>= sItem ) && sItem.getLength() )
            {
                mxBackgroundPopup->append(OUString::number(1), sItem);
            }
            else if ( ( rEvent.State >>= sItems ) && sItems.hasElements() )
            {
                sal_uInt16 nId = 1;
                for (const OUString& rStr : sItems)
                {
                    mxBackgroundPopup->append(OUString::number(nId), rStr);
                    nId++;
                }
            }
        }
    }
}

void SAL_CALL GalleryThemePopup::disposing(
    const css::lang::EventObject &/*rSource*/)
{
}

void GalleryThemePopup::Execute(
    const CommandInfo &rCmdInfo,
    const css::uno::Sequence< css::beans::PropertyValue > &rArguments )
{
    if ( rCmdInfo.Dispatch.is() )
    {
        std::unique_ptr<DispatchInfo> pInfo(new DispatchInfo);
        pInfo->TargetURL = rCmdInfo.URL;
        pInfo->Arguments = rArguments;
        pInfo->Dispatch = rCmdInfo.Dispatch;

        if ( Application::PostUserEvent(
                LINK( nullptr, GalleryBrowser, AsyncDispatch_Impl), pInfo.get() ) )
            pInfo.release();
    }
}

void GalleryThemePopup::ExecutePopup(weld::Widget* pParent, const ::Point &rPos)
{
    css::uno::Reference< css::frame::XStatusListener > xThis( this );

    const SgaObjKind eObjKind = mpTheme->GetObjectKind( mnObjectPos );
    INetURLObject    aURL;

    mpTheme->GetURL(mnObjectPos, aURL);
    const bool bValidURL = ( aURL.GetProtocol() != INetProtocol::NotValid );

    mxPopupMenu->set_visible(u"add"_ustr, bValidURL && SgaObjKind::Sound != eObjKind);

    mxPopupMenu->set_visible(u"preview"_ustr, bValidURL);
    mxPopupMenu->set_active(u"preview"_ustr, mbPreview);

    if( mpTheme->IsReadOnly() || !mpTheme->GetObjectCount() )
    {
        mxPopupMenu->set_visible(u"delete"_ustr, false);
        mxPopupMenu->set_visible(u"title"_ustr, false);
        if (mpTheme->IsReadOnly())
            mxPopupMenu->set_visible(u"paste"_ustr, false);

        if (!mpTheme->GetObjectCount())
            mxPopupMenu->set_visible(u"copy"_ustr, false);
    }
    else
    {
        mxPopupMenu->set_visible(u"delete"_ustr, !mbPreview);
        mxPopupMenu->set_visible(u"title"_ustr, true);
        mxPopupMenu->set_visible(u"copy"_ustr, true);
        mxPopupMenu->set_visible(u"paste"_ustr, true);
    }

    // update status
    css::uno::Reference< css::frame::XDispatchProvider> xDispatchProvider(
        GalleryBrowser::GetFrame(), css::uno::UNO_QUERY );
    css::uno::Reference< css::util::XURLTransformer > xTransformer(
        mpBrowser->GetURLTransformer() );
    for ( auto& rInfo : m_aCommandInfo )
    {
        try
        {
            CommandInfo &rCmdInfo = rInfo.second;
            if ( xTransformer.is() )
                xTransformer->parseStrict( rCmdInfo.URL );

            if ( xDispatchProvider.is() )
            {
                rCmdInfo.Dispatch = xDispatchProvider->queryDispatch(
                    rCmdInfo.URL,
                    u"_self"_ustr,
                    css::frame::FrameSearchFlag::SELF );
            }

            if ( rCmdInfo.Dispatch.is() )
            {
                rCmdInfo.Dispatch->addStatusListener( this, rCmdInfo.URL );
                rCmdInfo.Dispatch->removeStatusListener( this, rCmdInfo.URL );
            }
        }
        catch ( ... )
        {}
    }

    if( !mxBackgroundPopup->n_children() || ( eObjKind == SgaObjKind::SvDraw ) || ( eObjKind == SgaObjKind::Sound ) )
        mxPopupMenu->set_visible(u"background"_ustr, false);
    else
        mxPopupMenu->set_visible(u"background"_ustr, true);

    MenuSelectHdl(mxPopupMenu->popup_at_rect(pParent, tools::Rectangle(rPos, Size(1,1))));
}

void GalleryThemePopup::MenuSelectHdl(std::u16string_view rIdent)
{
    if (rIdent.empty())
        return;

    sal_uInt16 nSubMenuId = o3tl::toUInt32(rIdent);
    if (nSubMenuId)
    {
        BackgroundMenuSelectHdl(nSubMenuId-1);
        return;
    }

    if (rIdent == u"add")
    {
        const CommandInfoMap::const_iterator it = m_aCommandInfo.find( SID_GALLERY_FORMATS );
        if (it != m_aCommandInfo.end())
            mpBrowser->DispatchAdd(it->second.Dispatch, it->second.URL);
    }
    else
        mpBrowser->Execute(rIdent);
}

void GalleryThemePopup::BackgroundMenuSelectHdl(sal_uInt16 nPos)
{
    OUString aURL( mpBrowser->GetURL().GetMainURL( INetURLObject::DecodeMechanism::NONE ) );
    OUString aFilterName( mpBrowser->GetFilterName() );

    css::uno::Sequence< css::beans::PropertyValue > aArgs{
        comphelper::makePropertyValue(u"Background.Transparent"_ustr, sal_Int32( 0 )), // 0 - 100
        comphelper::makePropertyValue(u"Background.BackColor"_ustr, sal_Int32( - 1 )),
        comphelper::makePropertyValue(u"Background.URL"_ustr, aURL),
        comphelper::makePropertyValue(u"Background.Filtername"_ustr, aFilterName), // FIXME name should be FilterName
        comphelper::makePropertyValue(u"Background.Position"_ustr, css::style::GraphicLocation_TILED),
        comphelper::makePropertyValue(u"Position"_ustr, nPos)
    };

    const CommandInfoMap::const_iterator it = m_aCommandInfo.find( SID_GALLERY_BG_BRUSH );
    if ( it != m_aCommandInfo.end() )
        Execute( it->second, aArgs );
}

} // end anonymous namespace


/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
