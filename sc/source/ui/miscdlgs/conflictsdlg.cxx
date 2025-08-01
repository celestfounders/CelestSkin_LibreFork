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

#include <comphelper/string.hxx>
#include <osl/diagnose.h>

#include <conflictsdlg.hxx>
#include <o3tl/safeint.hxx>
#include <strings.hrc>
#include <scresid.hxx>
#include <viewdata.hxx>
#include <dbfunc.hxx>
#include <chgtrack.hxx>

// struct ScConflictsListEntry

bool ScConflictsListEntry::HasSharedAction( sal_uLong nSharedAction ) const
{
    auto aEnd = maSharedActions.cend();
    auto aItr = std::find(maSharedActions.cbegin(), aEnd, nSharedAction);

    return aItr != aEnd;
}

bool ScConflictsListEntry::HasOwnAction( sal_uLong nOwnAction ) const
{
    auto aEnd = maOwnActions.cend();
    auto aItr = std::find(maOwnActions.cbegin(), aEnd, nOwnAction);

    return aItr != aEnd;
}


bool ScConflictsListHelper::HasOwnAction( ScConflictsList& rConflictsList, sal_uLong nOwnAction )
{
    return std::any_of(rConflictsList.begin(), rConflictsList.end(),
                       [nOwnAction](ScConflictsListEntry& rConflict) { return rConflict.HasOwnAction( nOwnAction ); });
}

ScConflictsListEntry* ScConflictsListHelper::GetSharedActionEntry( ScConflictsList& rConflictsList, sal_uLong nSharedAction )
{
    auto aEnd = rConflictsList.end();
    auto aItr = std::find_if(rConflictsList.begin(), aEnd,
        [nSharedAction](ScConflictsListEntry& rConflict) { return rConflict.HasSharedAction( nSharedAction ); });

    if (aItr != aEnd)
        return &(*aItr);

    return nullptr;
}

ScConflictsListEntry* ScConflictsListHelper::GetOwnActionEntry( ScConflictsList& rConflictsList, sal_uLong nOwnAction )
{
    auto aEnd = rConflictsList.end();
    auto aItr = std::find_if(rConflictsList.begin(), aEnd,
        [nOwnAction](ScConflictsListEntry& rConflict) { return rConflict.HasOwnAction( nOwnAction ); });

    if (aItr != aEnd)
        return &(*aItr);

    return nullptr;
}

void ScConflictsListHelper::Transform_Impl( std::vector<sal_uLong>& rActionList, ScChangeActionMergeMap* pMergeMap )
{
    if ( !pMergeMap )
    {
        return;
    }

    for ( auto aItr = rActionList.begin(); aItr != rActionList.end(); )
    {
        ScChangeActionMergeMap::iterator aItrMap = pMergeMap->find( *aItr );
        if ( aItrMap != pMergeMap->end() )
        {
            *aItr = aItrMap->second;
            ++aItr;
        }
        else
        {
            aItr = rActionList.erase( aItr );
            OSL_FAIL( "ScConflictsListHelper::Transform_Impl: erased action from conflicts list!" );
        }
    }
}

void ScConflictsListHelper::TransformConflictsList( ScConflictsList& rConflictsList,
    ScChangeActionMergeMap* pSharedMap, ScChangeActionMergeMap* pOwnMap )
{
    for ( auto& rConflictEntry : rConflictsList )
    {
        if ( pSharedMap )
        {
            ScConflictsListHelper::Transform_Impl( rConflictEntry.maSharedActions, pSharedMap );
        }

        if ( pOwnMap )
        {
            ScConflictsListHelper::Transform_Impl( rConflictEntry.maOwnActions, pOwnMap );
        }
    }
}


ScConflictsFinder::ScConflictsFinder( ScChangeTrack* pTrack, sal_uLong nStartShared, sal_uLong nEndShared,
        sal_uLong nStartOwn, sal_uLong nEndOwn, ScConflictsList& rConflictsList )
    :mpTrack( pTrack )
    ,mnStartShared( nStartShared )
    ,mnEndShared( nEndShared )
    ,mnStartOwn( nStartOwn )
    ,mnEndOwn( nEndOwn )
    ,mrConflictsList( rConflictsList )
{
}

bool ScConflictsFinder::DoActionsIntersect( const ScChangeAction* pAction1, const ScChangeAction* pAction2 )
{
    return pAction1 && pAction2 && pAction1->GetBigRange().Intersects( pAction2->GetBigRange() );
}

ScConflictsListEntry* ScConflictsFinder::GetIntersectingEntry( const ScChangeAction* pAction ) const
{
    auto doActionsIntersect = [this, pAction](const sal_uLong& aAction) { return DoActionsIntersect( mpTrack->GetAction( aAction ), pAction ); };

    for ( auto& rConflict : mrConflictsList )
    {
        if (std::any_of( rConflict.maSharedActions.cbegin(), rConflict.maSharedActions.cend(), doActionsIntersect ))
            return &rConflict;

        if (std::any_of( rConflict.maOwnActions.cbegin(), rConflict.maOwnActions.cend(), doActionsIntersect ))
            return &rConflict;
    }

    return nullptr;
}

ScConflictsListEntry& ScConflictsFinder::GetEntry( sal_uLong nSharedAction, const std::vector<sal_uLong>& rOwnActions )
{
    // try to get a list entry which already contains the shared action
    ScConflictsListEntry* pEntry = ScConflictsListHelper::GetSharedActionEntry( mrConflictsList, nSharedAction );
    if ( pEntry )
    {
        return *pEntry;
    }

    // try to get a list entry for which the shared action intersects with any
    // other action of this entry
    pEntry = GetIntersectingEntry( mpTrack->GetAction( nSharedAction ) );
    if ( pEntry )
    {
        pEntry->maSharedActions.push_back( nSharedAction );
        return *pEntry;
    }

    // try to get a list entry for which any of the own actions intersects with
    // any other action of this entry
    for ( auto& rOwnAction : rOwnActions )
    {
        pEntry = GetIntersectingEntry( mpTrack->GetAction( rOwnAction ) );
        if ( pEntry )
        {
            pEntry->maSharedActions.push_back( nSharedAction );
            return *pEntry;
        }
    }

    // if no entry was found, create a new one
    ScConflictsListEntry aEntry;
    aEntry.meConflictAction = SC_CONFLICT_ACTION_NONE;
    aEntry.maSharedActions.push_back( nSharedAction );
    mrConflictsList.push_back(std::move(aEntry));
    return mrConflictsList.back();
}

bool ScConflictsFinder::Find()
{
    if ( !mpTrack )
    {
        return false;
    }

    bool bReturn = false;
    ScChangeAction* pSharedAction = mpTrack->GetAction( mnStartShared );
    while ( pSharedAction && pSharedAction->GetActionNumber() <= mnEndShared )
    {
        std::vector<sal_uLong> aOwnActions;
        ScChangeAction* pOwnAction = mpTrack->GetAction( mnStartOwn );
        while ( pOwnAction && pOwnAction->GetActionNumber() <= mnEndOwn )
        {
            if ( DoActionsIntersect( pSharedAction, pOwnAction ) )
            {
                aOwnActions.push_back( pOwnAction->GetActionNumber() );
            }
            pOwnAction = pOwnAction->GetNext();
        }

        if ( !aOwnActions.empty() )
        {
            ScConflictsListEntry& rEntry = GetEntry(pSharedAction->GetActionNumber(), aOwnActions);
            for ( const auto& aOwnAction : aOwnActions )
            {
                if (!ScConflictsListHelper::HasOwnAction(mrConflictsList, aOwnAction))
                {
                    rEntry.maOwnActions.push_back(aOwnAction);
                }
            }
            bReturn = true;
        }

        pSharedAction = pSharedAction->GetNext();
    }

    return bReturn;
}


ScConflictsResolver::ScConflictsResolver( ScChangeTrack* pTrack, ScConflictsList& rConflictsList )
    :mpTrack ( pTrack )
    ,mrConflictsList ( rConflictsList )
{
    OSL_ENSURE( mpTrack, "ScConflictsResolver CTOR: mpTrack is null!" );
}

void ScConflictsResolver::HandleAction( ScChangeAction* pAction, bool bIsSharedAction,
    bool bHandleContentAction, bool bHandleNonContentAction )
{
    if ( !mpTrack || !pAction )
    {
        return;
    }

    if ( bIsSharedAction )
    {
        ScConflictsListEntry* pConflictEntry = ScConflictsListHelper::GetSharedActionEntry(
            mrConflictsList, pAction->GetActionNumber() );
        if ( pConflictEntry )
        {
            ScConflictAction eConflictAction = pConflictEntry->meConflictAction;
            if ( eConflictAction == SC_CONFLICT_ACTION_KEEP_MINE )
            {
                if ( pAction->GetType() == SC_CAT_CONTENT )
                {
                    if ( bHandleContentAction )
                    {
                        mpTrack->Reject( pAction );
                    }
                }
                else
                {
                    if ( bHandleNonContentAction )
                    {
                        mpTrack->Reject( pAction );
                    }
                }
            }
        }
    }
    else
    {
        ScConflictsListEntry* pConflictEntry = ScConflictsListHelper::GetOwnActionEntry(
            mrConflictsList, pAction->GetActionNumber() );
        if ( pConflictEntry )
        {
            ScConflictAction eConflictAction = pConflictEntry->meConflictAction;
            if ( eConflictAction == SC_CONFLICT_ACTION_KEEP_MINE )
            {
                if ( pAction->GetType() == SC_CAT_CONTENT )
                {
                    if ( bHandleContentAction )
                    {
                        // do nothing
                        //mpTrack->SelectContent( pAction );
                    }
                }
                else
                {
                    if ( bHandleNonContentAction )
                    {
                        // do nothing
                        //mpTrack->Accept( pAction );
                    }
                }
            }
            else if ( eConflictAction == SC_CONFLICT_ACTION_KEEP_OTHER )
            {
                if ( pAction->GetType() == SC_CAT_CONTENT )
                {
                    if ( bHandleContentAction )
                    {
                        mpTrack->Reject( pAction );
                    }
                }
                else
                {
                    if ( bHandleNonContentAction )
                    {
                        mpTrack->Reject( pAction );
                    }
                }
            }
        }
    }
}


ScConflictsDlg::ScConflictsDlg(weld::Window* pParent, ScViewData& rViewData, ScDocument& rSharedDoc, ScConflictsList& rConflictsList)
    : GenericDialogController(pParent, u"modules/scalc/ui/conflictsdialog.ui"_ustr, u"ConflictsDialog"_ustr)
    , maStrUnknownUser   ( ScResId( STR_UNKNOWN_USER_CONFLICT ) )
    , mrViewData         ( rViewData )
    , mrOwnDoc           ( mrViewData.GetDocument() )
    , mrSharedDoc        ( rSharedDoc )
    , mrConflictsList    ( rConflictsList )
    , maSelectionIdle    ( "ScConflictsDlg maSelectionIdle" )
    , mbInSelectHdl      ( false )
    , m_xBtnKeepMine(m_xBuilder->weld_button(u"keepmine"_ustr))
    , m_xBtnKeepOther(m_xBuilder->weld_button(u"keepother"_ustr))
    , m_xBtnKeepAllMine(m_xBuilder->weld_button(u"keepallmine"_ustr))
    , m_xBtnKeepAllOthers(m_xBuilder->weld_button(u"keepallothers"_ustr))
    , m_xLbConflicts(new SvxRedlinTable(m_xBuilder->weld_tree_view(u"container"_ustr), nullptr,
                                        nullptr))
{

    weld::TreeView& rTreeView = m_xLbConflicts->GetWidget();

    auto nDigitWidth = rTreeView.get_approximate_digit_width();
    std::vector<int> aWidths
    {
        o3tl::narrowing<int>(nDigitWidth * 60),
        o3tl::narrowing<int>(nDigitWidth * 20)
    };
    rTreeView.set_column_fixed_widths(aWidths);

    rTreeView.set_selection_mode(SelectionMode::Multiple);
    rTreeView.set_size_request(-1, rTreeView.get_height_rows(16));

    maSelectionIdle.SetInvokeHandler( LINK( this, ScConflictsDlg, UpdateSelectionHdl ) );

    rTreeView.connect_selection_changed(LINK(this, ScConflictsDlg, SelectHandle));

    m_xBtnKeepMine->connect_clicked( LINK( this, ScConflictsDlg, KeepMineHandle ) );
    m_xBtnKeepOther->connect_clicked( LINK( this, ScConflictsDlg, KeepOtherHandle ) );
    m_xBtnKeepAllMine->connect_clicked( LINK( this, ScConflictsDlg, KeepAllMineHandle ) );
    m_xBtnKeepAllOthers->connect_clicked( LINK( this, ScConflictsDlg, KeepAllOthersHandle ) );

    UpdateView();

    std::unique_ptr<weld::TreeIter> xEntry(rTreeView.make_iterator());
    if (rTreeView.get_iter_first(*xEntry))
        rTreeView.select(*xEntry);
}

ScConflictsDlg::~ScConflictsDlg()
{
}

OUString ScConflictsDlg::GetConflictString( const ScConflictsListEntry& rConflictEntry )
{
    OUString aString;
    if ( ScChangeTrack* pOwnTrack = mrOwnDoc.GetChangeTrack() )
    {
        const ScChangeAction* pAction = pOwnTrack->GetAction( rConflictEntry.maOwnActions[ 0 ] );
        if ( pAction )
        {
            SCTAB nTab = pAction->GetBigRange().MakeRange( mrOwnDoc ).aStart.Tab();
            mrOwnDoc.GetName( nTab, aString );
        }
    }
    return aString;
}

void ScConflictsDlg::SetActionString(ScChangeAction& rAction, ScDocument& rDoc, const weld::TreeIter& rEntry)
{

    weld::TreeView& rTreeView = m_xLbConflicts->GetWidget();
    OUString aDesc = rAction.GetDescription(rDoc, true, false);
    rTreeView.set_text(rEntry, aDesc, 0);

    OUString aUser = comphelper::string::strip(rAction.GetUser(), ' ');
    if ( aUser.isEmpty() )
    {
        aUser = maStrUnknownUser;
    }
    rTreeView.set_text(rEntry, aUser, 1);

    DateTime aDateTime = rAction.GetDateTime();
    OUString aString = ScGlobal::getLocaleData().getDate( aDateTime ) + " " +
        ScGlobal::getLocaleData().getTime( aDateTime, false );
    rTreeView.set_text(rEntry, aString, 2);
}

void ScConflictsDlg::HandleListBoxSelection()
{
    weld::TreeView& rTreeView = m_xLbConflicts->GetWidget();
    std::unique_ptr<weld::TreeIter> xEntry(rTreeView.make_iterator());
    bool bSelEntry = rTreeView.get_cursor(xEntry.get());
    if (!bSelEntry)
        bSelEntry = rTreeView.get_selected(xEntry.get());
    if (!bSelEntry)
        return;

    bool bSelectHandle = rTreeView.is_selected(*xEntry);

    while (rTreeView.get_iter_depth(*xEntry))
        rTreeView.iter_parent(*xEntry);

    if (bSelectHandle)
        rTreeView.unselect_all();
    if (!rTreeView.is_selected(*xEntry))
        rTreeView.select(*xEntry);
    if (rTreeView.iter_children(*xEntry))
    {
        do
        {
            if (!rTreeView.is_selected(*xEntry))
                rTreeView.select(*xEntry);
        } while (rTreeView.iter_next_sibling(*xEntry));
    }
}

IMPL_LINK_NOARG(ScConflictsDlg, SelectHandle, weld::TreeView&, void)
{
    if (mbInSelectHdl)
        return;

    mbInSelectHdl = true;
    HandleListBoxSelection();
    maSelectionIdle.Start();
    mbInSelectHdl = false;
}

IMPL_LINK_NOARG(ScConflictsDlg, UpdateSelectionHdl, Timer *, void)
{
    ScTabView* pTabView = mrViewData.GetView();
    pTabView->DoneBlockMode();

    std::vector<const ScChangeAction*> aActions;

    weld::TreeView& rTreeView = m_xLbConflicts->GetWidget();
    rTreeView.selected_foreach([&rTreeView, &aActions](weld::TreeIter& rEntry){
        if (rTreeView.get_iter_depth(rEntry))
        {
            RedlinData* pUserData = weld::fromId<RedlinData*>(rTreeView.get_id(rEntry));
            if  (pUserData)
            {
                ScChangeAction* pAction = static_cast< ScChangeAction* >( pUserData->pData );
                if ( pAction && ( pAction->GetType() != SC_CAT_DELETE_TABS ) &&
                     ( pAction->IsClickable() || pAction->IsVisible() ) )
                {
                    aActions.push_back(pAction);
                }
            }
        }
        return false;
    });

    bool bContMark = false;
    for (size_t i = 0, nCount = aActions.size(); i < nCount; ++i)
    {
        const ScBigRange& rBigRange = aActions[i]->GetBigRange();
        if (rBigRange.IsValid(mrOwnDoc))
        {
            bool bSetCursor = i == nCount - 1;
            pTabView->MarkRange(rBigRange.MakeRange( mrOwnDoc ), bSetCursor, bContMark);
            bContMark = true;
        }
    }
}

void ScConflictsDlg::SetConflictAction(const weld::TreeIter& rRootEntry, ScConflictAction eConflictAction)
{
    weld::TreeView& rTreeView = m_xLbConflicts->GetWidget();
    RedlinData* pUserData = weld::fromId<RedlinData*>(rTreeView.get_id(rRootEntry));
    ScConflictsListEntry* pConflictEntry = static_cast< ScConflictsListEntry* >( pUserData ? pUserData->pData : nullptr );
    if ( pConflictEntry )
    {
        pConflictEntry->meConflictAction = eConflictAction;
    }
}

void ScConflictsDlg::KeepHandler(bool bMine)
{
    weld::TreeView& rTreeView = m_xLbConflicts->GetWidget();
    std::unique_ptr<weld::TreeIter> xEntry(rTreeView.make_iterator());
    if (!rTreeView.get_selected(xEntry.get()))
        return;

    while (rTreeView.get_iter_depth(*xEntry))
        rTreeView.iter_parent(*xEntry);

    m_xDialog->set_busy_cursor(true);
    ScConflictAction eConflictAction = ( bMine ? SC_CONFLICT_ACTION_KEEP_MINE : SC_CONFLICT_ACTION_KEEP_OTHER );
    SetConflictAction(*xEntry, eConflictAction);
    rTreeView.remove(*xEntry);
    m_xDialog->set_busy_cursor(false);
    if (rTreeView.n_children() == 0)
        m_xDialog->response(RET_OK);
}

void ScConflictsDlg::KeepAllHandler( bool bMine )
{
    weld::TreeView& rTreeView = m_xLbConflicts->GetWidget();
    std::unique_ptr<weld::TreeIter> xEntry(rTreeView.make_iterator());
    if (!rTreeView.get_iter_first(*xEntry))
        return;

    while (rTreeView.get_iter_depth(*xEntry))
        rTreeView.iter_parent(*xEntry);

    m_xDialog->set_busy_cursor(true);

    ScConflictAction eConflictAction = ( bMine ? SC_CONFLICT_ACTION_KEEP_MINE : SC_CONFLICT_ACTION_KEEP_OTHER );
    do
    {
        SetConflictAction(*xEntry, eConflictAction);
    } while (rTreeView.iter_next_sibling(*xEntry));

    rTreeView.freeze();
    rTreeView.clear();
    rTreeView.thaw();

    m_xDialog->set_busy_cursor(false);

    m_xDialog->response(RET_OK);
}

IMPL_LINK_NOARG(ScConflictsDlg, KeepMineHandle, weld::Button&, void)
{
    KeepHandler( true );
}

IMPL_LINK_NOARG(ScConflictsDlg, KeepOtherHandle, weld::Button&, void)
{
    KeepHandler( false );
}

IMPL_LINK_NOARG(ScConflictsDlg, KeepAllMineHandle, weld::Button&, void)
{
    KeepAllHandler( true );
}

IMPL_LINK_NOARG(ScConflictsDlg, KeepAllOthersHandle, weld::Button&, void)
{
    KeepAllHandler( false );
}

void ScConflictsDlg::UpdateView()
{
    weld::TreeView& rTreeView = m_xLbConflicts->GetWidget();
    for ( ScConflictsListEntry& rConflictEntry : mrConflictsList )
    {
        if (rConflictEntry.meConflictAction == SC_CONFLICT_ACTION_NONE)
        {
            std::unique_ptr<RedlinData> pRootUserData(new RedlinData());
            pRootUserData->pData = static_cast<void*>(&rConflictEntry);
            OUString sString(GetConflictString(rConflictEntry));
            OUString sId(weld::toId(pRootUserData.release()));
            std::unique_ptr<weld::TreeIter> xRootEntry(rTreeView.make_iterator());
            std::unique_ptr<weld::TreeIter> xEntry(rTreeView.make_iterator());
            rTreeView.insert(nullptr, -1, &sString, &sId, nullptr, nullptr, false, xRootEntry.get());

            for ( const auto& aSharedAction : rConflictEntry.maSharedActions )
            {
                if ( ScChangeTrack* pSharedTrack = mrSharedDoc.GetChangeTrack() )
                {
                    if (ScChangeAction* pAction = pSharedTrack->GetAction(aSharedAction))
                    {
                        // only display shared top content entries
                        if ( pAction->GetType() == SC_CAT_CONTENT )
                        {
                            ScChangeActionContent* pNextContent = dynamic_cast<ScChangeActionContent&>(*pAction).GetNextContent();
                            if ( pNextContent && rConflictEntry.HasSharedAction( pNextContent->GetActionNumber() ) )
                            {
                                continue;
                            }
                        }

                        rTreeView.insert(xRootEntry.get(), -1, nullptr, nullptr, nullptr, nullptr, false, xEntry.get());
                        SetActionString(*pAction, mrSharedDoc, *xEntry);
                    }
                }
            }

            for ( const auto& aOwnAction : rConflictEntry.maOwnActions )
            {
                if ( ScChangeTrack* pOwnTrack = mrOwnDoc.GetChangeTrack() )
                {
                    if (ScChangeAction* pAction = pOwnTrack->GetAction(aOwnAction))
                    {
                        // only display own top content entries
                        if ( pAction->GetType() == SC_CAT_CONTENT )
                        {
                            ScChangeActionContent* pNextContent = dynamic_cast<ScChangeActionContent&>(*pAction).GetNextContent();
                            if ( pNextContent && rConflictEntry.HasOwnAction( pNextContent->GetActionNumber() ) )
                            {
                                continue;
                            }
                        }

                        std::unique_ptr<RedlinData> pUserData(new RedlinData());
                        pUserData->pData = static_cast< void* >( pAction );
                        OUString aId(weld::toId(pUserData.release()));
                        rTreeView.insert(xRootEntry.get(), -1, nullptr, &aId, nullptr, nullptr, false, xEntry.get());
                        SetActionString(*pAction, mrOwnDoc, *xEntry);
                    }
                }
            }

            rTreeView.expand_row(*xRootEntry);
        }
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
