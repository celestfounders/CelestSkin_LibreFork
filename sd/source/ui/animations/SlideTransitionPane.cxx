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

#include <com/sun/star/drawing/XDrawView.hpp>
#include <SlideTransitionPane.hxx>

#include <TransitionPreset.hxx>
#include <sdresid.hxx>
#include <ViewShellBase.hxx>
#include <DrawDocShell.hxx>
#include <SlideSorterViewShell.hxx>
#include <unomodel.hxx>
#include <drawdoc.hxx>
#include <sdmod.hxx>
#include <sdpage.hxx>
#include <filedlg.hxx>
#include <strings.hrc>
#include <EventMultiplexer.hxx>

#include <comphelper/lok.hxx>
#include <sal/log.hxx>
#include <tools/debug.hxx>
#include <svx/gallery.hxx>
#include <utility>
#include <vcl/stdtext.hxx>
#include <vcl/svapp.hxx>
#include <vcl/weld.hxx>
#include <tools/urlobj.hxx>
#include <slideshow.hxx>
#include <sdundogr.hxx>
#include <undoanim.hxx>
#include <optsitem.hxx>

#include <o3tl/safeint.hxx>

#include <algorithm>

using namespace ::com::sun::star;

using ::com::sun::star::uno::Reference;


namespace sd::impl
{
struct TransitionEffect
{
    TransitionEffect() :
            mnType( 0 ),
            mnSubType( 0 ),
            mbDirection( true ),
            mnFadeColor( 0 )
    {
        init();
    }
    explicit TransitionEffect( const ::sd::TransitionPreset & rPreset ) :
            mnType( rPreset.getTransition()),
            mnSubType( rPreset.getSubtype()),
            mbDirection( rPreset.getDirection()),
            mnFadeColor( rPreset.getFadeColor())
    {
        init();
    }
    explicit TransitionEffect( const SdPage & rPage ) :
            mnType( rPage.getTransitionType() ),
            mnSubType( rPage.getTransitionSubtype() ),
            mbDirection( rPage.getTransitionDirection() ),
            mnFadeColor( rPage.getTransitionFadeColor() )
    {
        init();

        mfDuration = rPage.getTransitionDuration();
        mfTime = rPage.GetTime();
        mePresChange = rPage.GetPresChange();
        mbSoundOn = rPage.IsSoundOn();
        maSound = rPage.GetSoundFile();
        mbLoopSound = rPage.IsLoopSound();
        mbStopSound = rPage.IsStopSound();
    }

    void init()
    {
        mfDuration = 2.0;
        mfTime = 0.0;
        mePresChange = PresChange::Manual;
        mbSoundOn = false;
        mbLoopSound = false;
        mbStopSound = false;

        mbEffectAmbiguous = false;
        mbDurationAmbiguous = false;
        mbTimeAmbiguous = false;
        mbPresChangeAmbiguous = false;
        mbSoundAmbiguous = false;
        mbLoopSoundAmbiguous = false;
    }

    void setAllAmbiguous()
    {
        mbEffectAmbiguous = true;
        mbDurationAmbiguous = true;
        mbTimeAmbiguous = true;
        mbPresChangeAmbiguous = true;
        mbSoundAmbiguous = true;
        mbLoopSoundAmbiguous = true;
    }

    bool operator == ( const ::sd::TransitionPreset & rPreset ) const
    {
        return
            (mnType == rPreset.getTransition()) &&
            (mnSubType == rPreset.getSubtype()) &&
            (mbDirection == rPreset.getDirection()) &&
            (mnFadeColor ==  rPreset.getFadeColor());
    }

    void applyTo( SdPage & rOutPage ) const
    {
        if( ! mbEffectAmbiguous )
        {
            rOutPage.setTransitionType( mnType );
            rOutPage.setTransitionSubtype( mnSubType );
            rOutPage.setTransitionDirection( mbDirection );
            rOutPage.setTransitionFadeColor( mnFadeColor );
        }

        if( ! mbDurationAmbiguous )
            rOutPage.setTransitionDuration( mfDuration );
        if( ! mbTimeAmbiguous )
            rOutPage.SetTime( mfTime );
        if( ! mbPresChangeAmbiguous )
            rOutPage.SetPresChange( mePresChange );
        if( ! mbSoundAmbiguous )
        {
            if( mbStopSound )
            {
                rOutPage.SetStopSound( true );
                rOutPage.SetSound( false );
            }
            else
            {
                rOutPage.SetStopSound( false );
                rOutPage.SetSound( mbSoundOn );
                rOutPage.SetSoundFile( maSound );
            }
        }
        if( ! mbLoopSoundAmbiguous )
            rOutPage.SetLoopSound( mbLoopSound );
    }

    void compareWith( const SdPage & rPage )
    {
        TransitionEffect aOtherEffect( rPage );
        mbEffectAmbiguous = mbEffectAmbiguous || aOtherEffect.mbEffectAmbiguous
                                              || (mnType != aOtherEffect.mnType)
                                              || (mnSubType != aOtherEffect.mnSubType)
                                              || (mbDirection != aOtherEffect.mbDirection)
                                              || (mnFadeColor != aOtherEffect.mnFadeColor);

        mbDurationAmbiguous = mbDurationAmbiguous || aOtherEffect.mbDurationAmbiguous || mfDuration != aOtherEffect.mfDuration;
        mbTimeAmbiguous = mbTimeAmbiguous || aOtherEffect.mbTimeAmbiguous || mfTime != aOtherEffect.mfTime;
        mbPresChangeAmbiguous = mbPresChangeAmbiguous || aOtherEffect.mbPresChangeAmbiguous || mePresChange != aOtherEffect.mePresChange;
        mbSoundAmbiguous = mbSoundAmbiguous || aOtherEffect.mbSoundAmbiguous || mbSoundOn != aOtherEffect.mbSoundOn;
#if 0
                        // Weird leftover isolated expression with no effect, introduced in 2007 in
                        // CWS impress122. Ifdeffed out to avoid compiler warning, kept here in case
                        // somebody who understands this code notices and understands what the
                        // "right" thing to do might be.
                        (!mbStopSound && !aOtherEffect.mbStopSound && maSound != aOtherEffect.maSound) || (mbStopSound != aOtherEffect.mbStopSound);
#endif
        mbLoopSoundAmbiguous = mbLoopSoundAmbiguous || aOtherEffect.mbLoopSoundAmbiguous || mbLoopSound != aOtherEffect.mbLoopSound;
    }

    // effect
    sal_Int16 mnType;
    sal_Int16 mnSubType;
    bool  mbDirection;
    sal_Int32 mnFadeColor;

    // other settings
    double      mfDuration;
    double       mfTime;
    PresChange  mePresChange;
    bool        mbSoundOn;
    OUString    maSound;
    bool        mbLoopSound;
    bool        mbStopSound;

    bool mbEffectAmbiguous;
    bool mbDurationAmbiguous;
    bool mbTimeAmbiguous;
    bool mbPresChangeAmbiguous;
    bool mbSoundAmbiguous;
    bool mbLoopSoundAmbiguous;
};

} // namespace sd::impl

namespace sd
{

// Local Helper Functions
namespace
{

void lcl_ApplyToPages(
    const ::sd::slidesorter::SharedPageSelection& rpPages,
    const ::sd::impl::TransitionEffect & rEffect )
{
    for( const auto& rpPage : *rpPages )
    {
        rEffect.applyTo( *rpPage );
    }
}

void lcl_CreateUndoForPages(
    const ::sd::slidesorter::SharedPageSelection& rpPages,
    ::sd::ViewShellBase const & rBase )
{
    ::sd::DrawDocShell* pDocSh      = rBase.GetDocShell();
    if (!pDocSh)
        return;
    SfxUndoManager* pManager   = pDocSh->GetUndoManager();
    if (!pManager)
        return;
    SdDrawDocument* pDoc            = pDocSh->GetDoc();
    if (!pDoc)
        return;

    OUString aComment( SdResId(STR_UNDO_SLIDE_PARAMS) );
    pManager->EnterListAction(aComment, aComment, 0, rBase.GetViewShellId());
    std::unique_ptr<SdUndoGroup> pUndoGroup(new SdUndoGroup( *pDoc ));
    pUndoGroup->SetComment( aComment );

    for( const auto& rpPage : *rpPages )
    {
        pUndoGroup->AddAction( new sd::UndoTransition( *pDoc, rpPage ) );
    }

    pManager->AddUndoAction( std::move(pUndoGroup) );
    pManager->LeaveListAction();
}

struct lcl_EqualsSoundFileName
{
    explicit lcl_EqualsSoundFileName( OUString aStr ) :
            maStr(std::move( aStr ))
    {}

    bool operator() ( const OUString & rStr ) const
    {
        // note: formerly this was a case insensitive search for all
        // platforms. It seems more sensible to do this platform-dependent
        INetURLObject aURL(rStr);
#if defined(_WIN32)
        return maStr.equalsIgnoreAsciiCase( aURL.GetBase() );
#else
        return maStr == aURL.GetBase();
#endif
    }

private:
    OUString maStr;
};

// returns -1 if no object was found
bool lcl_findSoundInList( const ::std::vector< OUString > & rSoundList,
                          std::u16string_view rFileName,
                          ::std::vector< OUString >::size_type & rOutPosition )
{
    INetURLObject aURL(rFileName);
    ::std::vector< OUString >::const_iterator aIt =
          ::std::find_if( rSoundList.begin(), rSoundList.end(),
                          lcl_EqualsSoundFileName( aURL.GetBase()));
    if( aIt != rSoundList.end())
    {
        rOutPosition = ::std::distance( rSoundList.begin(), aIt );
        return true;
    }

    return false;
}

OUString lcl_getSoundFileURL(
    const ::std::vector< OUString > & rSoundList,
    const weld::ComboBox& rListBox )
{
    sal_Int32 nPos = rListBox.get_active();
    // the first three entries are no actual sounds
    if( nPos >= 3 )
    {
        DBG_ASSERT( static_cast<sal_uInt32>(rListBox.get_count() - 3) == rSoundList.size(),
                    "Sound list-box is not synchronized to sound list" );
        nPos -= 3;
        if( rSoundList.size() > o3tl::make_unsigned(nPos) )
            return rSoundList[ nPos ];
    }

    return OUString();
}

struct lcl_AppendSoundToListBox
{
    explicit lcl_AppendSoundToListBox(weld::ComboBox& rListBox)
        : mrListBox( rListBox )
    {}

    void operator() ( std::u16string_view rString ) const
    {
        INetURLObject aURL( rString );
        mrListBox.append_text( aURL.GetBase() );
    }

private:
    weld::ComboBox&  mrListBox;
};

void lcl_FillSoundListBox(
    const ::std::vector< OUString > & rSoundList,
    weld::ComboBox& rOutListBox )
{
    sal_Int32 nCount = rOutListBox.get_count();

    // keep first three entries
    for( sal_Int32 i=nCount - 1; i>=3; --i )
        rOutListBox.remove( i );

    ::std::for_each( rSoundList.begin(), rSoundList.end(),
                     lcl_AppendSoundToListBox( rOutListBox ));
}

/// Returns an offset into the list of transition presets
sd::TransitionPresetPtr getPreset(const sd::impl::TransitionEffect &rEffect)
{
    const sd::TransitionPresetList& rPresetList = sd::TransitionPreset::getTransitionPresetList();

    for (const auto& pPreset: rPresetList)
    {
        if (rEffect.operator==(*pPreset))
            return pPreset;
    }
    return sd::TransitionPresetPtr();
}

} // anonymous namespace

// SlideTransitionPane
SlideTransitionPane::SlideTransitionPane(
    weld::Widget* pParent,
    ViewShellBase & rBase) :
        PanelLayout( pParent, u"SlideTransitionsPanel"_ustr, u"modules/simpress/ui/slidetransitionspanel.ui"_ustr ),
        mrBase( rBase ),
        mpDrawDoc( rBase.GetDocShell() ? rBase.GetDocShell()->GetDoc() : nullptr ),
        mxTransitionsIconView(m_xBuilder->weld_icon_view("transitions_icons")),
        mxTransitionsScrollWindow(m_xBuilder->weld_scrolled_window("transitions_icons_scrolled_window")),
        mxRepeatAutoFrame(m_xBuilder->weld_frame("repeat_after_frame")),
        mbHasSelection( false ),
        mbUpdatingControls( false ),
        mbIsMainViewChangePending( false ),
        maLateInitTimer("sd SlideTransitionPane maLateInitTimer")
{
    Initialize(mpDrawDoc);
}

css::ui::LayoutSize SlideTransitionPane::GetHeightForWidth(const sal_Int32 /*nWidth*/)
{
    sal_Int32 nMinimumHeight = get_preferred_size().Height();
    return css::ui::LayoutSize(nMinimumHeight, -1, nMinimumHeight);
}

void SlideTransitionPane::Initialize(SdDrawDocument* pDoc)
{
    mxLB_VARIANT = m_xBuilder->weld_combo_box(u"variant_list"_ustr);
    mxCBX_duration = m_xBuilder->weld_metric_spin_button(u"transition_duration"_ustr, FieldUnit::SECOND);
    mxFT_SOUND = m_xBuilder->weld_label(u"sound_label"_ustr);
    mxLB_SOUND = m_xBuilder->weld_combo_box(u"sound_list"_ustr);
    mxCB_LOOP_SOUND = m_xBuilder->weld_check_button(u"loop_sound"_ustr);
    mxRB_ADVANCE_ON_MOUSE = m_xBuilder->weld_radio_button(u"rb_mouse_click"_ustr);
    mxRB_ADVANCE_AUTO = m_xBuilder->weld_radio_button(u"rb_auto_after"_ustr);
    mxMF_ADVANCE_AUTO_AFTER  = m_xBuilder->weld_metric_spin_button(u"auto_after_value"_ustr, FieldUnit::SECOND);
    mxRB_REPEAT_DISABLED = m_xBuilder->weld_radio_button(u"rb_disabled"_ustr);
    mxRB_REPEAT_AUTO = m_xBuilder->weld_radio_button(u"rb_auto_repeat"_ustr);
    mxMF_REPEAT_AUTO_AFTER  = m_xBuilder->weld_metric_spin_button(u"rb_auto_repeat_value"_ustr, FieldUnit::SECOND);
    mxPB_APPLY_TO_ALL = m_xBuilder->weld_button(u"apply_to_all"_ustr);
    mxPB_PLAY = m_xBuilder->weld_button(u"play"_ustr);
    mxCB_AUTO_PREVIEW = m_xBuilder->weld_check_button(u"auto_preview"_ustr);

    auto nMax = mxMF_ADVANCE_AUTO_AFTER->get_max(FieldUnit::SECOND);
    mxMF_ADVANCE_AUTO_AFTER->set_max(99, FieldUnit::SECOND);
    int nWidthChars = mxMF_ADVANCE_AUTO_AFTER->get_width_chars();
    mxMF_ADVANCE_AUTO_AFTER->set_max(nMax, FieldUnit::SECOND);
    mxMF_ADVANCE_AUTO_AFTER->set_width_chars(nWidthChars);
    mxMF_REPEAT_AUTO_AFTER->set_max(nMax, FieldUnit::SECOND);
    mxMF_REPEAT_AUTO_AFTER->set_width_chars(nWidthChars);
    mxCBX_duration->set_width_chars(nWidthChars);

    if( pDoc )
        mxModel = pDoc->getUnoModel();
    // TODO: get correct view
    if( mxModel.is())
        mxView.set( mxModel->getCurrentController(), uno::UNO_QUERY );

    // set defaults
    mxCB_AUTO_PREVIEW->set_active(true);      // automatic preview on

    // update control states before adding handlers
    updateControls();

    // set handlers
    mxPB_APPLY_TO_ALL->connect_clicked( LINK( this, SlideTransitionPane, ApplyToAllButtonClicked ));
    mxPB_PLAY->connect_clicked( LINK( this, SlideTransitionPane, PlayButtonClicked ));

    mxTransitionsIconView->connect_item_activated(LINK(this, SlideTransitionPane, TransitionSelected));

    mxLB_VARIANT->connect_changed( LINK( this, SlideTransitionPane, VariantListBoxSelected ));
    mxCBX_duration->connect_value_changed(LINK( this, SlideTransitionPane, DurationModifiedHdl));
    mxCBX_duration->connect_focus_out(LINK( this, SlideTransitionPane, DurationLoseFocusHdl));
    mxLB_SOUND->connect_changed( LINK( this, SlideTransitionPane, SoundListBoxSelected ));
    mxCB_LOOP_SOUND->connect_toggled( LINK( this, SlideTransitionPane, LoopSoundBoxChecked ));

    mxRB_ADVANCE_ON_MOUSE->connect_toggled( LINK( this, SlideTransitionPane, AdvanceSlideRadioButtonToggled ));
    mxRB_ADVANCE_AUTO->connect_toggled( LINK( this, SlideTransitionPane, AdvanceSlideRadioButtonToggled ));
    mxMF_ADVANCE_AUTO_AFTER->connect_value_changed( LINK( this, SlideTransitionPane, AdvanceTimeModified ));
    mxRB_REPEAT_DISABLED->connect_toggled( LINK( this, SlideTransitionPane, RepeatAfterRadioButtonToggled ));
    mxRB_REPEAT_AUTO->connect_toggled( LINK( this, SlideTransitionPane, RepeatAfterRadioButtonToggled ));
    mxMF_REPEAT_AUTO_AFTER->connect_value_changed( LINK( this, SlideTransitionPane, RepeatAfterTimeModified ));
    mxCB_AUTO_PREVIEW->connect_toggled( LINK( this, SlideTransitionPane, AutoPreviewClicked ));
    addListener();

    mxRB_REPEAT_DISABLED->set_active( true );

    maLateInitTimer.SetTimeout(200);
    maLateInitTimer.SetInvokeHandler(LINK(this, SlideTransitionPane, LateInitCallback));
    maLateInitTimer.Start();
}

SlideTransitionPane::~SlideTransitionPane()
{
    maLateInitTimer.Stop();
    removeListener();
    mxTransitionsScrollWindow.reset();
    mxTransitionsIconView.reset();
    mxRepeatAutoFrame.reset();
    mxLB_VARIANT.reset();
    mxCBX_duration.reset();
    mxFT_SOUND.reset();
    mxLB_SOUND.reset();
    mxCB_LOOP_SOUND.reset();
    mxRB_ADVANCE_ON_MOUSE.reset();
    mxRB_ADVANCE_AUTO.reset();
    mxMF_ADVANCE_AUTO_AFTER.reset();
    mxRB_REPEAT_DISABLED.reset();
    mxRB_REPEAT_AUTO.reset();
    mxMF_REPEAT_AUTO_AFTER.reset();
    mxPB_APPLY_TO_ALL.reset();
    mxPB_PLAY.reset();
    mxCB_AUTO_PREVIEW.reset();
}

void SlideTransitionPane::onSelectionChanged()
{
    updateControls();
}

void SlideTransitionPane::onChangeCurrentPage()
{
    updateControls();
}

::sd::slidesorter::SharedPageSelection SlideTransitionPane::getSelectedPages() const
{
    ::sd::slidesorter::SlideSorterViewShell * pSlideSorterViewShell
        = ::sd::slidesorter::SlideSorterViewShell::GetSlideSorter(mrBase);
    std::shared_ptr<sd::slidesorter::SlideSorterViewShell::PageSelection> pSelection;

    if( pSlideSorterViewShell )
    {
        pSelection = pSlideSorterViewShell->GetPageSelection();
    }
    else
    {
        pSelection = std::make_shared<sd::slidesorter::SlideSorterViewShell::PageSelection>();
        if( mxView.is() )
        {
            SdPage* pPage = SdPage::getImplementation( mxView->getCurrentPage() );
            if( pPage )
                pSelection->push_back(pPage);
        }
    }

    return pSelection;
}

void SlideTransitionPane::updateControls()
{
    ::sd::slidesorter::SharedPageSelection pSelectedPages(getSelectedPages());
    if( pSelectedPages->empty())
    {
        mbHasSelection = false;
        return;
    }
    mbHasSelection = true;

    DBG_ASSERT( ! mbUpdatingControls, "Multiple Control Updates" );
    mbUpdatingControls = true;

    // get model data for first page
    SdPage * pFirstPage = pSelectedPages->front();
    DBG_ASSERT( pFirstPage, "Invalid Page" );

    impl::TransitionEffect aEffect( *pFirstPage );

    // merge with other pages

    // start with second page (note aIt != aEndIt, because ! aSelectedPages.empty())
    for( const auto& rpPage : *pSelectedPages )
    {
        if( rpPage )
            aEffect.compareWith( *rpPage );
    }

    // detect current slide effect
    if( aEffect.mbEffectAmbiguous )
    {
        SAL_WARN( "sd.transitions", "Unusual, ambiguous transition effect" );
        mxTransitionsIconView->select(0);
    }
    else
    {
         // ToDo: That 0 is "no transition" is documented nowhere except in the
        // CTOR of sdpage
        if( aEffect.mnType == 0 )
            mxTransitionsIconView->select(0);
        else
            updateVariants(getPreset(aEffect));
    }

    if( aEffect.mbDurationAmbiguous )
    {
        mxCBX_duration->set_text(u""_ustr);
//TODO        mxCBX_duration->SetNoSelection();
    }
    else
    {
        mxCBX_duration->set_value( (aEffect.mfDuration)*100.0, FieldUnit::SECOND );
    }

    if( aEffect.mbSoundAmbiguous )
    {
        mxLB_SOUND->set_active(-1);
        maCurrentSoundFile.clear();
    }
    else
    {
        maCurrentSoundFile.clear();
        if( aEffect.mbStopSound )
        {
            mxLB_SOUND->set_active( 1 );
        }
        else if( aEffect.mbSoundOn && !aEffect.maSound.isEmpty() )
        {
            std::vector<OUString>::size_type nPos = 0;
            if( lcl_findSoundInList( maSoundList, aEffect.maSound, nPos ))
            {
                mxLB_SOUND->set_active( nPos + 3 );
                maCurrentSoundFile = aEffect.maSound;
            }
        }
        else
        {
            mxLB_SOUND->set_active( 0 );
        }
    }

    if( aEffect.mbLoopSoundAmbiguous )
    {
        mxCB_LOOP_SOUND->set_state(TRISTATE_INDET);
    }
    else
    {
        mxCB_LOOP_SOUND->set_active(aEffect.mbLoopSound);
    }

    if( aEffect.mbPresChangeAmbiguous )
    {
        mxRB_ADVANCE_ON_MOUSE->set_active( false );
        mxRB_ADVANCE_AUTO->set_active( false );
    }
    else
    {
        mxRB_ADVANCE_ON_MOUSE->set_active( aEffect.mePresChange == PresChange::Manual );
        mxRB_ADVANCE_AUTO->set_active( aEffect.mePresChange == PresChange::Auto );
        mxMF_ADVANCE_AUTO_AFTER->set_value(aEffect.mfTime * 100.0, FieldUnit::SECOND);
    }

    sd::PresentationSettings& rSettings = mpDrawDoc->getPresentationSettings();

    if ( !rSettings.mbEndless )
    {
        mxRB_REPEAT_DISABLED->set_active( true );
        mxRB_REPEAT_AUTO->set_active( false );
    }
    else
    {
        mxRB_REPEAT_DISABLED->set_active( false );
        mxRB_REPEAT_AUTO->set_active( true );
        mxMF_REPEAT_AUTO_AFTER->set_value(rSettings.mnPauseTimeout * 100.0, FieldUnit::SECOND);
    }

    if (comphelper::LibreOfficeKit::isActive())
    {
        mxPB_PLAY->hide();
        mxCB_AUTO_PREVIEW->set_active(false);
        mxCB_AUTO_PREVIEW->hide();
        mxFT_SOUND->hide();
        mxLB_SOUND->hide();
        mxCB_LOOP_SOUND->hide();
    }
    else
    {
        mxRepeatAutoFrame->hide();
        SdOptions* pOptions = SdModule::get()->GetSdOptions(DocumentType::Impress);
        mxCB_AUTO_PREVIEW->set_active( pOptions->IsPreviewTransitions() );
    }

    mbUpdatingControls = false;

    updateControlState();
}

void SlideTransitionPane::updateControlState()
{
    if (mxTransitionsScrollWindow)
        mxTransitionsScrollWindow->set_sensitive(mbHasSelection);
    mxLB_VARIANT->set_sensitive( mbHasSelection && mxLB_VARIANT->get_count() > 0 );
    mxCBX_duration->set_sensitive( mbHasSelection );
    mxLB_SOUND->set_sensitive( mbHasSelection );
    mxCB_LOOP_SOUND->set_sensitive( mbHasSelection && (mxLB_SOUND->get_active() > 2));
    mxRB_ADVANCE_ON_MOUSE->set_sensitive( mbHasSelection );
    mxRB_ADVANCE_AUTO->set_sensitive( mbHasSelection );
    mxMF_ADVANCE_AUTO_AFTER->set_sensitive( mbHasSelection && mxRB_ADVANCE_AUTO->get_active());
    mxRB_REPEAT_DISABLED->set_sensitive( mbHasSelection );
    mxRB_REPEAT_AUTO->set_sensitive( mbHasSelection );
    mxMF_REPEAT_AUTO_AFTER->set_sensitive( mbHasSelection && mxRB_REPEAT_AUTO->get_active());

    mxPB_APPLY_TO_ALL->set_sensitive( mbHasSelection );
    mxPB_PLAY->set_sensitive( mbHasSelection );
    mxCB_AUTO_PREVIEW->set_sensitive( mbHasSelection );
}

void SlideTransitionPane::updateSoundList()
{
    maSoundList.clear();

    GalleryExplorer::FillObjList( GALLERY_THEME_SOUNDS, maSoundList );
    GalleryExplorer::FillObjList( GALLERY_THEME_USERSOUNDS, maSoundList );

    lcl_FillSoundListBox( maSoundList, *mxLB_SOUND );
}

void SlideTransitionPane::openSoundFileDialog()
{
    if( ! mxLB_SOUND->get_sensitive())
        return;

    weld::Window* pDialogParent(GetFrameWeld());
    SdOpenSoundFileDialog aFileDialog(pDialogParent);

    DBG_ASSERT( mxLB_SOUND->get_active() == 2,
                "Dialog should only open when \"Other sound\" is selected" );

    bool bValidSoundFile( false );
    bool bQuitLoop( false );

    while( ! bQuitLoop &&
           aFileDialog.Execute() == ERRCODE_NONE )
    {
        OUString aFile = aFileDialog.GetPath();
        std::vector<OUString>::size_type nPos = 0;
        bValidSoundFile = lcl_findSoundInList( maSoundList, aFile, nPos );

        if( bValidSoundFile )
        {
            bQuitLoop = true;
        }
        else // not in sound list
        {
            // try to insert into gallery
            if( GalleryExplorer::InsertURL( GALLERY_THEME_USERSOUNDS, aFile ) )
            {
                updateSoundList();
                bValidSoundFile = lcl_findSoundInList( maSoundList, aFile, nPos );
                DBG_ASSERT( bValidSoundFile, "Adding sound to gallery failed" );

                bQuitLoop = true;
            }
            else
            {
                OUString aStrWarning(SdResId(STR_WARNING_NOSOUNDFILE));
                aStrWarning = aStrWarning.replaceFirst("%", aFile);
                std::unique_ptr<weld::MessageDialog> xWarn(Application::CreateMessageDialog(pDialogParent,
                                                           VclMessageType::Warning, VclButtonsType::NONE,
                                                           aStrWarning));
                xWarn->add_button(GetStandardText(StandardButtonType::Retry), RET_RETRY);
                xWarn->add_button(GetStandardText(StandardButtonType::Cancel), RET_CANCEL);
                bQuitLoop = (xWarn->run() != RET_RETRY);

                bValidSoundFile = false;
            }
        }

        if( bValidSoundFile )
            // skip first three entries in list
            mxLB_SOUND->set_active( nPos + 3 );
    }

    if(  bValidSoundFile )
        return;

    if( !maCurrentSoundFile.isEmpty() )
    {
        std::vector<OUString>::size_type nPos = 0;
        if( lcl_findSoundInList( maSoundList, maCurrentSoundFile, nPos ))
            mxLB_SOUND->set_active( nPos + 3 );
        else
            mxLB_SOUND->set_active( 0 );  // NONE
    }
    else
        mxLB_SOUND->set_active( 0 );  // NONE
}

impl::TransitionEffect SlideTransitionPane::getTransitionEffectFromControls() const
{
    impl::TransitionEffect aResult;
    aResult.setAllAmbiguous();

    OUString sSelectedId = mxTransitionsIconView->get_selected_id();
    auto* pTransitionEntry = weld::fromId<TransitionEntry*>(sSelectedId);
    if (!pTransitionEntry)
        return aResult;

    const sd::TransitionPresetList& rPresetList = sd::TransitionPreset::getTransitionPresetList();

    if (pTransitionEntry->mpPreset)
    {
        auto pSelectedPreset = pTransitionEntry->mpPreset;

        if (mxLB_VARIANT->get_active() == -1)
        {
            // Transition with just one effect.
            aResult = impl::TransitionEffect(*pSelectedPreset);
            aResult.setAllAmbiguous();
        }
        else
        {
            int nVariant = 0;
            bool bFound = false;
            for(const auto& rPreset: rPresetList)
            {
                if (rPreset->getSetId() == pSelectedPreset->getSetId() )
                {
                    if( mxLB_VARIANT->get_active() == nVariant)
                    {
                        aResult = impl::TransitionEffect(*rPreset);
                        aResult.setAllAmbiguous();
                        bFound = true;
                        break;
                    }
                    else
                    {
                        nVariant++;
                    }
                }
            }
            if( !bFound )
            {
                aResult.mnType = 0;
            }
        }
        aResult.mbEffectAmbiguous = false;
    }
    else
    {
        aResult.mbEffectAmbiguous = false;
    }

    //duration

    if( mxCBX_duration->get_sensitive() && (!(mxCBX_duration->get_text()).isEmpty()) )
    {
        aResult.mfDuration = static_cast<double>(mxCBX_duration->get_value(FieldUnit::SECOND))/100.0;
        aResult.mbDurationAmbiguous = false;
    }

    // slide-advance mode
    if( mxRB_ADVANCE_ON_MOUSE->get_sensitive() && mxRB_ADVANCE_AUTO->get_sensitive() &&
        (mxRB_ADVANCE_ON_MOUSE->get_active() || mxRB_ADVANCE_AUTO->get_active()))
    {
        if( mxRB_ADVANCE_ON_MOUSE->get_active())
            aResult.mePresChange = PresChange::Manual;
        else
        {
            aResult.mePresChange = PresChange::Auto;
            if( mxMF_ADVANCE_AUTO_AFTER->get_sensitive())
            {
                aResult.mfTime = static_cast<double>(mxMF_ADVANCE_AUTO_AFTER->get_value(FieldUnit::SECOND) ) / 100.0 ;
                aResult.mbTimeAmbiguous = false;
            }
        }

        aResult.mbPresChangeAmbiguous = false;
    }

    // transition repeat after
    if (mxRB_REPEAT_DISABLED->get_sensitive() && mxRB_REPEAT_AUTO->get_sensitive()
            && (mxRB_REPEAT_DISABLED->get_active() || mxRB_REPEAT_AUTO->get_active()))
    {
        sd::PresentationSettings& rSettings = mpDrawDoc->getPresentationSettings();

        if ( mxRB_REPEAT_DISABLED->get_active() )
        {
            rSettings.mbEndless = false;
            rSettings.mnPauseTimeout = 0;
        }
        else
        {
            if ( mxMF_REPEAT_AUTO_AFTER->get_sensitive() )
            {
                rSettings.mbEndless = true;
                rSettings.mnPauseTimeout = static_cast<sal_uInt32>(mxMF_REPEAT_AUTO_AFTER->get_value(FieldUnit::SECOND) / 100.0) ;
            }
        }
    }

    // sound
    if( mxLB_SOUND->get_sensitive())
    {
        maCurrentSoundFile.clear();
        sal_Int32 nPos = mxLB_SOUND->get_active();
        if (nPos != -1)
        {
            aResult.mbStopSound = nPos == 1;
            aResult.mbSoundOn = nPos > 1;
            if( aResult.mbStopSound )
            {
                aResult.maSound.clear();
                aResult.mbSoundAmbiguous = false;
            }
            else
            {
                aResult.maSound = lcl_getSoundFileURL(maSoundList, *mxLB_SOUND);
                aResult.mbSoundAmbiguous = false;
                maCurrentSoundFile = aResult.maSound;
            }
        }
    }

    // sound loop
    if( mxCB_LOOP_SOUND->get_sensitive() )
    {
        aResult.mbLoopSound = mxCB_LOOP_SOUND->get_active();
        aResult.mbLoopSoundAmbiguous = false;
    }

    return aResult;
}

void SlideTransitionPane::applyToSelectedPages(bool bPreview = true)
{
    if(  mbUpdatingControls )
        return;

    vcl::Window *pFocusWindow = Application::GetFocusWindow();

    ::sd::slidesorter::SharedPageSelection pSelectedPages( getSelectedPages());
    impl::TransitionEffect aEffect = getTransitionEffectFromControls();
    if( ! pSelectedPages->empty())
    {
        lcl_CreateUndoForPages( pSelectedPages, mrBase );
        lcl_ApplyToPages( pSelectedPages, aEffect );
        mrBase.GetDocShell()->SetModified();
    }
    if( mxCB_AUTO_PREVIEW->get_sensitive() &&
        mxCB_AUTO_PREVIEW->get_active() && bPreview)
    {
        if (aEffect.mnType) // mnType = 0 denotes no transition
            playCurrentEffect();
        else if( mxView.is() && !SlideShow::IsInteractiveSlideshow(&mrBase)) // IASS
            SlideShow::Stop( mrBase );
    }

    if (pFocusWindow)
        pFocusWindow->GrabFocus();
}

void SlideTransitionPane::playCurrentEffect()
{
    if( mxView.is() )
    {

        Reference< css::animations::XAnimationNode > xNode;
        SlideShow::StartPreview( mrBase, mxView->getCurrentPage(), xNode );
    }
}

void SlideTransitionPane::addListener()
{
    Link<tools::EventMultiplexerEvent&,void> aLink( LINK(this,SlideTransitionPane,EventMultiplexerListener) );
    mrBase.GetEventMultiplexer()->AddEventListener( aLink );
}

void SlideTransitionPane::removeListener()
{
    Link<tools::EventMultiplexerEvent&,void> aLink( LINK(this,SlideTransitionPane,EventMultiplexerListener) );
    mrBase.GetEventMultiplexer()->RemoveEventListener( aLink );
}

IMPL_LINK(SlideTransitionPane,EventMultiplexerListener,
    tools::EventMultiplexerEvent&, rEvent, void)
{
    switch (rEvent.meEventId)
    {
        case EventMultiplexerEventId::EditViewSelection:
            onSelectionChanged();
            break;

        case EventMultiplexerEventId::CurrentPageChanged:
        case EventMultiplexerEventId::SlideSortedSelection:
            onChangeCurrentPage();
            break;

        case EventMultiplexerEventId::MainViewRemoved:
            mxView.clear();
            onSelectionChanged();
            onChangeCurrentPage();
            break;

        case EventMultiplexerEventId::MainViewAdded:
            mbIsMainViewChangePending = true;
            break;

        case EventMultiplexerEventId::ConfigurationUpdated:
            if (mbIsMainViewChangePending)
            {
                mbIsMainViewChangePending = false;

                // At this moment the controller may not yet been set at
                // model or ViewShellBase.  Take it from the view shell
                // passed with the event.
                if (mrBase.GetMainViewShell() != nullptr)
                {
                    mxView.set(mrBase.GetController(), css::uno::UNO_QUERY);
                    onSelectionChanged();
                    onChangeCurrentPage();
                }
            }
            break;

        default:
            if (rEvent.meEventId != EventMultiplexerEventId::Disposing)
            {
                onSelectionChanged();
                onChangeCurrentPage();
            }
            break;
    }
}

IMPL_LINK_NOARG(SlideTransitionPane, ApplyToAllButtonClicked, weld::Button&, void)
{
    DBG_ASSERT( mpDrawDoc, "Invalid Draw Document!" );
    if( !mpDrawDoc )
        return;

    ::sd::slidesorter::SharedPageSelection pPages =
        std::make_shared<::sd::slidesorter::SlideSorterViewShell::PageSelection>();

    sal_uInt16 nPageCount = mpDrawDoc->GetSdPageCount( PageKind::Standard );
    pPages->reserve( nPageCount );
    for( sal_uInt16 i=0; i<nPageCount; ++i )
    {
        SdPage * pPage = mpDrawDoc->GetSdPage( i, PageKind::Standard );
        if( pPage )
            pPages->push_back( pPage );
    }

    if( ! pPages->empty())
    {
        lcl_CreateUndoForPages( pPages, mrBase );
        lcl_ApplyToPages( pPages, getTransitionEffectFromControls() );
    }
}

IMPL_LINK_NOARG(SlideTransitionPane, PlayButtonClicked, weld::Button&, void)
{
    playCurrentEffect();
}

IMPL_LINK_NOARG(SlideTransitionPane, TransitionSelected, weld::IconView&, bool)
{
    OUString sSelectedId = mxTransitionsIconView->get_selected_id();
    auto* pTransitionEntry = weld::fromId<TransitionEntry*>(sSelectedId);
    updateVariants(pTransitionEntry->mpPreset);
    applyToSelectedPages();
    return true;
}

/// we use an integer offset into the list of transition presets
void SlideTransitionPane::updateVariants(TransitionPresetPtr const& pPreset)
{
    mxLB_VARIANT->clear();
    mxLB_VARIANT->set_sensitive(false);
    mxLB_VARIANT->set_active(0);

    if (!pPreset)
    {
        mxTransitionsIconView->select(0);
    }
    else
    {
        auto iterator = maTranstionMap.find(pPreset->getSetId());
        if (iterator != maTranstionMap.end())
        {
            auto& pTransitionEntry = iterator->second;
            if (!pTransitionEntry->mnVariants.empty())
            {
                for (OUString const& rCurrentVariant : pTransitionEntry->mnVariants)
                {
                    mxLB_VARIANT->append_text(rCurrentVariant);
                    if (pPreset->getVariantLabel() == rCurrentVariant)
                        mxLB_VARIANT->set_active(mxLB_VARIANT->get_count() - 1);
                }
                mxLB_VARIANT->set_sensitive(true);
            }

            mxTransitionsIconView->select(pTransitionEntry->mnIndex);
        }
    }
}

IMPL_LINK_NOARG(SlideTransitionPane, AdvanceSlideRadioButtonToggled, weld::Toggleable&, void)
{
    updateControlState();
    applyToSelectedPages(false);
}

IMPL_LINK_NOARG(SlideTransitionPane, RepeatAfterRadioButtonToggled, weld::Toggleable&, void)
{
    updateControlState();
    applyToSelectedPages(false);
}

IMPL_LINK_NOARG(SlideTransitionPane, AdvanceTimeModified, weld::MetricSpinButton&, void)
{
    applyToSelectedPages(false);
}

IMPL_LINK_NOARG(SlideTransitionPane, RepeatAfterTimeModified, weld::MetricSpinButton&, void)
{
    applyToSelectedPages(false);
}

IMPL_LINK_NOARG(SlideTransitionPane, VariantListBoxSelected, weld::ComboBox&, void)
{
    applyToSelectedPages();
}

IMPL_LINK_NOARG(SlideTransitionPane, DurationModifiedHdl, weld::MetricSpinButton&, void)
{
    double duration_value = static_cast<double>(mxCBX_duration->get_value(FieldUnit::SECOND));
    if (duration_value <= 0.0)
        mxCBX_duration->set_value(0, FieldUnit::SECOND);
    else
        mxCBX_duration->set_value(duration_value, FieldUnit::SECOND);

    applyToSelectedPages();
}

IMPL_LINK_NOARG(SlideTransitionPane, DurationLoseFocusHdl, weld::Widget&, void)
{
    applyToSelectedPages();
}

IMPL_LINK_NOARG(SlideTransitionPane, SoundListBoxSelected, weld::ComboBox&, void)
{
    sal_Int32 nPos = mxLB_SOUND->get_active();
    if( nPos == 2 )
    {
        // other sound...
        openSoundFileDialog();
    }
    updateControlState();
    applyToSelectedPages();
}

IMPL_LINK_NOARG(SlideTransitionPane, LoopSoundBoxChecked, weld::Toggleable&, void)
{
    applyToSelectedPages();
}

IMPL_LINK_NOARG(SlideTransitionPane, AutoPreviewClicked, weld::Toggleable&, void)
{
    SdOptions* pOptions = SdModule::get()->GetSdOptions(DocumentType::Impress);
    pOptions->SetPreviewTransitions( mxCB_AUTO_PREVIEW->get_active() );
}

IMPL_LINK_NOARG(SlideTransitionPane, LateInitCallback, Timer *, void)
{
    mxTransitionsIconView->freeze();

    {
        auto pTransition = std::make_unique<TransitionEntry>();
        const OUString aId = weld::toId(pTransition.get());
        pTransition->msIcon = u"sd/cmd/transition-none.png"_ustr;
        pTransition->msLabel = SdResId(STR_SLIDETRANSITION_NONE);
        pTransition->mnIndex = 0;
        mxTransitionsIconView->append(aId, pTransition->msLabel, pTransition->msIcon);
        maTranstionMap.emplace(u"None"_ustr, std::move(pTransition));
    }

    const TransitionPresetList& rPresetList = TransitionPreset::getTransitionPresetList();
    size_t nIndex = 1;
    for (TransitionPresetPtr const& pPreset: rPresetList)
    {
        const OUString aLabel = pPreset->getSetLabel();
        if (!aLabel.isEmpty())
        {
            auto aIterator = maTranstionMap.find(pPreset->getSetId());
            if (aIterator == maTranstionMap.end())
            {
                auto pTransition = std::make_unique<TransitionEntry>();
                const OUString aId = weld::toId(pTransition.get());

                pTransition->msIcon = u"sd/cmd/transition-"_ustr + pPreset->getSetId() + u".png"_ustr;
                pTransition->msLabel = aLabel;
                pTransition->mpPreset = pPreset;
                if (!pPreset->getVariantLabel().isEmpty())
                    pTransition->mnVariants.push_back(pPreset->getVariantLabel());
                pTransition->mnIndex = nIndex;
                nIndex++;

                mxTransitionsIconView->append(aId, pTransition->msLabel, pTransition->msIcon);
                maTranstionMap.emplace(pPreset->getSetId(), std::move(pTransition));
            }
            else
            {
                auto& pTransition = aIterator->second;
                pTransition->mnVariants.push_back(pPreset->getVariantLabel());
            }
        }
    }
    mxTransitionsIconView->set_size_request(0, 0);
    mxTransitionsIconView->thaw();

    updateSoundList();
    updateControls();
}

} //  namespace sd

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
