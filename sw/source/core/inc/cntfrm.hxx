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

#ifndef INCLUDED_SW_SOURCE_CORE_INC_CNTFRM_HXX
#define INCLUDED_SW_SOURCE_CORE_INC_CNTFRM_HXX

#include <swdllapi.h>
#include "frame.hxx"
#include "flowfrm.hxx"
#include <cshtyp.hxx>

class SwLayoutFrame;
class SwContentNode;
class SwBorderAttrs;
class SwAttrSetChg;
class SwTextFrame;

// implemented in cntfrm.cxx, used in cntfrm.cxx and crsrsh.cxx
extern bool GetFrameInPage( const SwContentFrame*, SwWhichPage, SwPosPage, SwPaM* );

enum class SwContentFrameInvFlags : sal_uInt8
{
    NONE = 0x00,
    SetCompletePaint = 0x01,
    InvalidatePos = 0x02,
    InvalidateSize = 0x04,
    InvalidateSectPrt = 0x08,
    InvalidateNextPrt = 0x10,
    InvalidatePrevPrt = 0x20,
    InvalidateNextPos = 0x40,
    SetNextCompletePaint = 0x80,
};

namespace o3tl {
    template<> struct typed_flags<SwContentFrameInvFlags> : is_typed_flags<SwContentFrameInvFlags, 0xff> {};
}

/**
 * SwContentFrame is the layout for content nodes: a common base class for text (paragraph) and
 * non-text (e.g. graphic) frames.
 */
class SW_DLLPUBLIC SwContentFrame: public SwFrame, public SwFlowFrame
{
    friend void MakeNxt( SwFrame *pFrame, SwFrame *pNxt );    // calls MakePrtArea

    // parameter <bObjsInNewUpper>  indicates that objects exist in remaining
    // area of new upper
    bool WouldFit_( SwTwips nSpace,
                    SwLayoutFrame *pNewUpper,
                    bool bTstMove,
                    const bool bObjsInNewUpper );

    virtual void MakeAll(vcl::RenderContext* pRenderContext) override;

    void UpdateAttr_( const SfxPoolItem*, const SfxPoolItem*, SwContentFrameInvFlags &,
                      SwAttrSetChg *pa = nullptr, SwAttrSetChg *pb = nullptr );
    void UpdateAttrForFormatChange( SwFormat*, SwFormat*, SwContentFrameInvFlags & );

    virtual bool ShouldBwdMoved( SwLayoutFrame *pNewUpper, bool& ) override;

    const SwContentFrame* ImplGetNextContentFrame( bool bFwd ) const;

protected:
    void MakePrtArea( const SwBorderAttrs & );

    virtual void SwClientNotify(const SwModify&, const SfxHint&) override;
    virtual SwTwips ShrinkFrame( SwTwips, bool bTst = false, bool bInfo = false ) override;
    using SwFrame::GrowFrame;
    virtual SwTwips GrowFrame(SwTwips, SwResizeLimitReason&, bool bTst, bool bInfo) override;

    SwContentFrame( SwContentNode * const, SwFrame* );

    virtual void DestroyImpl() override;
    virtual ~SwContentFrame() override;

public:

    virtual void Cut() override;
    virtual void Paste( SwFrame* pParent, SwFrame* pSibling = nullptr ) override;

    inline const SwContentFrame *GetFollow() const;
    inline       SwContentFrame *GetFollow();
    SwTextFrame* FindMaster() const;

    // layout dependent cursor travelling
    virtual bool LeftMargin(SwPaM *) const = 0;
    virtual bool RightMargin(SwPaM *, bool bAPI = false) const = 0;
    virtual bool UnitUp( SwPaM *, const SwTwips nOffset,
                         bool bSetInReadOnly ) const;
    virtual bool UnitDown( SwPaM *, const SwTwips nOffset,
                           bool bSetInReadOnly ) const;

    // nMaxHeight is the required height
    // bSplit indicates that the paragraph has to be split
    // bTst indicates that we are currently doing a test formatting
    virtual bool WouldFit(SwTwips &nMaxHeight, bool &bSplit, bool bTst, bool);

    bool MoveFootnoteCntFwd( bool, SwFootnoteBossFrame* ); // called by MoveFwd if content

    inline  SwContentFrame* GetNextContentFrame() const;
    inline  SwContentFrame* GetPrevContentFrame() const;
    static bool CalcLowers(SwLayoutFrame & rLay, SwLayoutFrame const& rDontLeave,
            tools::Long nBottom, bool bSkipRowSpanCells);

    /// Returns bValue as is, unless this frame has split fly draw objects.
    bool IgnoringSplitFlyAnchor(bool bValue) const;
};

inline SwContentFrame* SwContentFrame::GetNextContentFrame() const
{
    if ( GetNext() && GetNext()->IsContentFrame() )
        return const_cast<SwContentFrame*>(static_cast<const SwContentFrame*>(GetNext()));
    else
        return const_cast<SwContentFrame*>(ImplGetNextContentFrame( true ));
}

inline SwContentFrame* SwContentFrame::GetPrevContentFrame() const
{
    if ( GetPrev() && GetPrev()->IsContentFrame() )
        return const_cast<SwContentFrame*>(static_cast<const SwContentFrame*>(GetPrev()));
    else
        return const_cast<SwContentFrame*>(ImplGetNextContentFrame( false ));
}

inline const SwContentFrame *SwContentFrame::GetFollow() const
{
    return static_cast<const SwContentFrame*>(SwFlowFrame::GetFollow());
}
inline SwContentFrame *SwContentFrame::GetFollow()
{
    return static_cast<SwContentFrame*>(SwFlowFrame::GetFollow());
}

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
