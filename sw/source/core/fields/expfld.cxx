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

#include <limits>

#include <UndoTable.hxx>
#include <hintids.hxx>
#include <o3tl/any.hxx>
#include <osl/diagnose.h>
#include <unotools/collatorwrapper.hxx>
#include <unotools/charclass.hxx>
#include <editeng/langitem.hxx>
#include <editeng/fontitem.hxx>
#include <com/sun/star/text/SetVariableType.hpp>
#include <unofield.hxx>
#include <frmfmt.hxx>
#include <fmtfld.hxx>
#include <txtfld.hxx>
#include <fmtanchr.hxx>
#include <txtftn.hxx>
#include <doc.hxx>
#include <IDocumentFieldsAccess.hxx>
#include <layfrm.hxx>
#include <pagefrm.hxx>
#include <cntfrm.hxx>
#include <txtfrm.hxx>
#include <rootfrm.hxx>
#include <tabfrm.hxx>
#include <flyfrm.hxx>
#include <ftnfrm.hxx>
#include <rowfrm.hxx>
#include <expfld.hxx>
#include <usrfld.hxx>
#include <ndtxt.hxx>
#include <calc.hxx>
#include <pam.hxx>
#include <docfld.hxx>
#include <swtable.hxx>
#include <breakit.hxx>
#include <SwStyleNameMapper.hxx>
#include <unofldmid.h>
#include <numrule.hxx>
#include <names.hxx>
#include <utility>

using namespace ::com::sun::star;
using namespace ::com::sun::star::text;

static sal_Int16 lcl_SubTypeToAPI(SwGetSetExpType nSubType)
{
        sal_Int16 nRet = 0;
        switch(nSubType)
        {
            case SwGetSetExpType::Expr:
                nRet = SetVariableType::VAR;      // 0
                break;
            case SwGetSetExpType::Sequence:
                nRet = SetVariableType::SEQUENCE; // 1
                break;
            case SwGetSetExpType::Formula:
                nRet = SetVariableType::FORMULA;  // 2
                break;
            case SwGetSetExpType::String:
                nRet = SetVariableType::STRING;   // 3
                break;
            default: break;
        }
        return nRet;
}

static std::optional<SwGetSetExpType> lcl_APIToSubType(const uno::Any& rAny)
{
        sal_Int16 nVal = 0;
        rAny >>= nVal;
        std::optional<SwGetSetExpType> nSet;
        switch(nVal)
        {
            case SetVariableType::VAR:      nSet = SwGetSetExpType::Expr;  break;
            case SetVariableType::SEQUENCE: nSet = SwGetSetExpType::Sequence;  break;
            case SetVariableType::FORMULA:  nSet = SwGetSetExpType::Formula; break;
            case SetVariableType::STRING:   nSet = SwGetSetExpType::String;   break;
            default:
                OSL_FAIL("wrong value");
        }
        return nSet;
}

OUString ReplacePoint( const OUString& rTmpName, bool bWithCommandType )
{
    // replace first and last (if bWithCommandType: last two) dot
    // since table names may contain dots

    sal_Int32 nIndex = rTmpName.lastIndexOf('.');
    if (nIndex<0)
    {
        return rTmpName;
    }

    OUString sRes = rTmpName.replaceAt(nIndex, 1, rtl::OUStringChar(DB_DELIM));

    if (bWithCommandType)
    {
        nIndex = sRes.lastIndexOf('.', nIndex);
        if (nIndex<0)
        {
            return sRes;
        }
        sRes = sRes.replaceAt(nIndex, 1, rtl::OUStringChar(DB_DELIM));
    }

    nIndex = sRes.indexOf('.');
    if (nIndex>=0)
    {
        sRes = sRes.replaceAt(nIndex, 1, rtl::OUStringChar(DB_DELIM));
    }
    return sRes;
}

static SwTextNode* GetFirstTextNode( const SwDoc& rDoc, SwPosition& rPos,
                            const SwContentFrame *pCFrame, Point &rPt )
{
    SwTextNode* pTextNode = nullptr;
    if ( !pCFrame )
    {
        const SwNodes& rNodes = rDoc.GetNodes();
        rPos.Assign( *rNodes.GetEndOfContent().StartOfSectionNode() );
        SwContentNode* pCNd;
        while( nullptr != (pCNd = SwNodes::GoNext( &rPos ) ) &&
                nullptr == ( pTextNode = pCNd->GetTextNode() ) )
                        ;
        OSL_ENSURE( pTextNode, "Where is the 1. TextNode?" );
    }
    else if ( !pCFrame->isFrameAreaDefinitionValid() )
    {
        assert(pCFrame->IsTextFrame());
        rPos = static_cast<SwTextFrame const*>(pCFrame)->MapViewToModelPos(TextFrameIndex(0));
    }
    else
    {
        pCFrame->GetModelPositionForViewPoint( &rPos, rPt );
        pTextNode = rPos.GetNode().GetTextNode();
    }
    return pTextNode;
}

const SwTextNode* GetBodyTextNode( const SwDoc& rDoc, SwPosition& rPos,
                                const SwFrame& rFrame )
{
    const SwLayoutFrame* pLayout = rFrame.GetUpper();
    const SwTextNode* pTextNode = nullptr;

    while( pLayout )
    {
        if( pLayout->IsFlyFrame() )
        {
            // get the FlyFormat
            const SwFrameFormat* pFlyFormat = static_cast<const SwFlyFrame*>(pLayout)->GetFormat();
            assert(pFlyFormat && "Could not find FlyFormat, where is the field?");

            const SwFormatAnchor &rAnchor = pFlyFormat->GetAnchor();

            if( RndStdIds::FLY_AT_FLY == rAnchor.GetAnchorId() )
            {
                // the fly needs to be attached somewhere, so ask it
                pLayout = static_cast<const SwLayoutFrame*>(static_cast<const SwFlyFrame*>(pLayout)->GetAnchorFrame());
                continue;
            }
            else if ((RndStdIds::FLY_AT_PARA == rAnchor.GetAnchorId()) ||
                     (RndStdIds::FLY_AT_CHAR == rAnchor.GetAnchorId()) ||
                     (RndStdIds::FLY_AS_CHAR == rAnchor.GetAnchorId()))
            {
                OSL_ENSURE( rAnchor.GetContentAnchor(), "no valid position" );
                rPos = *rAnchor.GetContentAnchor();
                pTextNode = rPos.GetNode().GetTextNode();
                if ( RndStdIds::FLY_AT_PARA == rAnchor.GetAnchorId() )
                {
                    rPos.AssignStartIndex(*pTextNode);
                }

                // do not break yet, might be as well in Header/Footer/Footnote/Fly
                pLayout = static_cast<const SwFlyFrame*>(pLayout)->GetAnchorFrame()
                            ? static_cast<const SwFlyFrame*>(pLayout)->GetAnchorFrame()->GetUpper() : nullptr;
                continue;
            }
            else
            {
                pLayout->FindPageFrame()->GetContentPosition(
                                                pLayout->getFrameArea().Pos(), rPos );
                pTextNode = rPos.GetNode().GetTextNode();
            }
        }
        else if( pLayout->IsFootnoteFrame() )
        {
            // get the anchor's node
            const SwTextFootnote* pFootnote = static_cast<const SwFootnoteFrame*>(pLayout)->GetAttr();
            pTextNode = &pFootnote->GetTextNode();
            rPos.Assign( *pTextNode, pFootnote->GetStart() );
        }
        else if( pLayout->IsHeaderFrame() || pLayout->IsFooterFrame() )
        {
            const SwContentFrame* pContentFrame;
            const SwPageFrame* pPgFrame = pLayout->FindPageFrame();
            if( pLayout->IsHeaderFrame() )
            {
                const SwTabFrame *pTab;
                if( nullptr != ( pContentFrame = pPgFrame->FindFirstBodyContent()) &&
                    nullptr != (pTab = pContentFrame->FindTabFrame()) && pTab->IsFollow() &&
                    pTab->GetTable()->GetRowsToRepeat() > 0 &&
                    pTab->IsInHeadline( *pContentFrame ) )
                {
                    // take the next line
                    const SwLayoutFrame* pRow = pTab->GetFirstNonHeadlineRow();
                    pContentFrame = pRow->ContainsContent();
                }
            }
            else
                pContentFrame = pPgFrame->FindLastBodyContent();

            if( pContentFrame && !pContentFrame->IsInFootnote() )
            {
                assert(pContentFrame->IsTextFrame());
                SwTextFrame const*const pFrame(static_cast<SwTextFrame const*>(pContentFrame));
                rPos = pFrame->MapViewToModelPos(TextFrameIndex(pFrame->GetText().getLength()));
                pTextNode = rPos.GetNode().GetTextNode();
                assert(pTextNode);
            }
            else
            {
                Point aPt( pLayout->getFrameArea().Pos() );
                aPt.AdjustY( 1 );      // get out of the header
                pContentFrame = pPgFrame->GetContentPos( aPt, false, true );
                pTextNode = GetFirstTextNode( rDoc, rPos, pContentFrame, aPt );
            }
        }
        else
        {
            pLayout = pLayout->GetUpper();
            continue;
        }
        break; // found, so finish loop
    }
    return pTextNode;
}

SwGetExpFieldType::SwGetExpFieldType(SwDoc* pDc)
    : SwValueFieldType( pDc, SwFieldIds::GetExp )
{
}

std::unique_ptr<SwFieldType> SwGetExpFieldType::Copy() const
{
    return std::make_unique<SwGetExpFieldType>(GetDoc());
}

void SwGetExpFieldType::SwClientNotify(const SwModify&, const SfxHint&)
{
    // do not expand anything (else)
}

SwGetExpField::SwGetExpField(SwGetExpFieldType* pTyp, const OUString& rFormel,
                            SwGetSetExpType nSub, sal_uLong nFormat)
    : SwFormulaField( pTyp, nFormat, 0.0 )
    , m_fValueRLHidden(0.0)
    ,
    m_bIsInBodyText( true ),
    m_nSubType(nSub),
    m_bLateInitialization( false )
{
    SetFormula( rFormel );
}

void SwGetExpField::ChgExpStr(const OUString& rExpand, SwRootFrame const*const pLayout)
{
    if (!pLayout || pLayout->IsHideRedlines())
    {
        m_sExpandRLHidden = rExpand;
    }
    if (!pLayout || !pLayout->IsHideRedlines())
    {
        m_sExpand = rExpand;
    }
}

OUString SwGetExpField::ExpandImpl(SwRootFrame const*const pLayout) const
{
    if(m_nSubType & SwGetSetExpType::Command)
        return GetFormula();

    return (pLayout && pLayout->IsHideRedlines()) ? m_sExpandRLHidden : m_sExpand;
}

OUString SwGetExpField::GetFieldName() const
{
    const SwFieldTypesEnum nType =
        (SwGetSetExpType::Formula & m_nSubType)
        ? SwFieldTypesEnum::Formel
        : SwFieldTypesEnum::Get;

    return SwFieldType::GetTypeStr(nType) + " " + GetFormula();
}

std::unique_ptr<SwField> SwGetExpField::Copy() const
{
    std::unique_ptr<SwGetExpField> pTmp(new SwGetExpField(static_cast<SwGetExpFieldType*>(GetTyp()),
                                            GetFormula(), m_nSubType, GetFormat()));
    pTmp->SetLanguage(GetLanguage());
    pTmp->m_fValueRLHidden = m_fValueRLHidden;
    pTmp->SwValueField::SetValue(GetValue());
    pTmp->m_sExpand       = m_sExpand;
    pTmp->m_sExpandRLHidden = m_sExpandRLHidden;
    pTmp->m_bIsInBodyText  = m_bIsInBodyText;
    pTmp->SetAutomaticLanguage(IsAutomaticLanguage());
    if( m_bLateInitialization )
        pTmp->SetLateInitialization();

    return std::unique_ptr<SwField>(pTmp.release());
}

void SwGetExpField::ChangeExpansion( const SwFrame& rFrame, const SwTextField& rField )
{
    if( m_bIsInBodyText ) // only fields in Footer, Header, FootNote, Flys
        return;

    OSL_ENSURE( !rFrame.IsInDocBody(), "Flag incorrect, frame is in DocBody" );

    // determine document (or is there an easier way?)
    const SwTextNode* pTextNode = &rField.GetTextNode();
    SwDoc& rDoc = const_cast<SwDoc&>(pTextNode->GetDoc());

    // create index for determination of the TextNode
    SwPosition aPos( rDoc.GetNodes() );
    pTextNode = GetBodyTextNode( rDoc, aPos, rFrame );

    // If no layout exists, ChangeExpansion is called for header and
    // footer lines via layout formatting without existing TextNode.
    if(!pTextNode)
        return;
    // #i82544#
    if( m_bLateInitialization )
    {
        SwFieldType* pSetExpField = rDoc.getIDocumentFieldsAccess().GetFieldType(SwFieldIds::SetExp, GetFormula(), false);
        if( pSetExpField )
        {
            m_bLateInitialization = false;
            if( !(GetSubType() & SwGetSetExpType::String) &&
                static_cast< SwSetExpFieldType* >(pSetExpField)->GetType() == SwGetSetExpType::String )
                SetSubType( SwGetSetExpType::String );
        }
    }

    SwRootFrame const& rLayout(*rFrame.getRootFrame());
    OUString & rExpand(rLayout.IsHideRedlines() ? m_sExpandRLHidden : m_sExpand);
    // here a page number is needed to sort correctly
    SetGetExpField aEndField(aPos.GetNode(), &rField, aPos.GetContentIndex(), rFrame.GetPhyPageNum());
    if(GetSubType() & SwGetSetExpType::String)
    {
        std::unordered_map<OUString, OUString> aHashTable;
        rDoc.getIDocumentFieldsAccess().FieldsToExpand(aHashTable, aEndField, rLayout);
        rExpand = LookString( aHashTable, GetFormula() );
    }
    else
    {
        // fill calculator with values
        SwCalc aCalc( rDoc );
        rDoc.getIDocumentFieldsAccess().FieldsToCalc(aCalc, aEndField, &rLayout);

        // calculate value
        SetValue(aCalc.Calculate(GetFormula()).GetDouble(), &rLayout);

        // analyse based on format
        rExpand = static_cast<SwValueFieldType*>(GetTyp())->ExpandValue(
                                GetValue(&rLayout), GetFormat(), GetLanguage());
    }
}

OUString SwGetExpField::GetPar2() const
{
    return GetFormula();
}

void SwGetExpField::SetPar2(const OUString& rStr)
{
    SetFormula(rStr);
}

SwGetSetExpType SwGetExpField::GetSubType() const
{
    return m_nSubType;
}

void SwGetExpField::SetSubType(SwGetSetExpType nType)
{
    m_nSubType = nType;
}

void SwGetExpField::SetLanguage(LanguageType nLng)
{
    if (m_nSubType & SwGetSetExpType::Command)
        SwField::SetLanguage(nLng);
    else
        SwValueField::SetLanguage(nLng);
}

bool SwGetExpField::QueryValue( uno::Any& rAny, sal_uInt16 nWhichId ) const
{
    switch( nWhichId )
    {
    case FIELD_PROP_DOUBLE:
        rAny <<= GetValue();
        break;
    case FIELD_PROP_FORMAT:
        rAny <<= static_cast<sal_Int32>(GetFormat());
        break;
    case FIELD_PROP_USHORT1:
         rAny <<= static_cast<sal_Int16>(m_nSubType);
        break;
    case FIELD_PROP_PAR1:
         rAny <<= GetFormula();
        break;
    case FIELD_PROP_SUBTYPE:
        {
            sal_Int16 nRet = lcl_SubTypeToAPI(GetSubType() & SwGetSetExpType::LowerMask);
            rAny <<= nRet;
        }
        break;
    case FIELD_PROP_BOOL2:
        rAny <<= bool(m_nSubType & SwGetSetExpType::Command);
        break;
    case FIELD_PROP_PAR4:
        rAny <<= m_sExpand;
        break;
    default:
        return SwField::QueryValue(rAny, nWhichId);
    }
    return true;
}

bool SwGetExpField::PutValue( const uno::Any& rAny, sal_uInt16 nWhichId )
{
    sal_Int32 nTmp = 0;
    switch( nWhichId )
    {
    case FIELD_PROP_DOUBLE:
        SwValueField::SetValue(*o3tl::doAccess<double>(rAny));
        m_fValueRLHidden = *o3tl::doAccess<double>(rAny);
        break;
    case FIELD_PROP_FORMAT:
        rAny >>= nTmp;
        SetFormat(nTmp);
        break;
    case FIELD_PROP_USHORT1:
         rAny >>= nTmp;
         m_nSubType = static_cast<SwGetSetExpType>(nTmp);
        break;
    case FIELD_PROP_PAR1:
    {
        OUString sTmp;
        rAny >>= sTmp;
        SetFormula(sTmp);
        break;
    }
    case FIELD_PROP_SUBTYPE:
    {
        std::optional<SwGetSetExpType> nTmpSub = lcl_APIToSubType(rAny);
        if( nTmpSub )
            SetSubType( (GetSubType() & SwGetSetExpType::UpperMask) | *nTmpSub );
        break;
    }
    case FIELD_PROP_BOOL2:
        if(*o3tl::doAccess<bool>(rAny))
            m_nSubType |= SwGetSetExpType::Command;
        else
            m_nSubType &= ~SwGetSetExpType::Command;
        break;
    case FIELD_PROP_PAR4:
    {
        OUString sTmp;
        rAny >>= sTmp;
        ChgExpStr(sTmp, nullptr);
        break;
    }
    default:
        return SwField::PutValue(rAny, nWhichId);
    }
    return true;
}

SwSetExpFieldType::SwSetExpFieldType( SwDoc* pDc, UIName aName, SwGetSetExpType nTyp )
    : SwValueFieldType( pDc, SwFieldIds::SetExp ),
    m_sName( std::move(aName) ),
    m_sDelim( u"."_ustr ),
    m_nType(nTyp), m_nLevel( UCHAR_MAX ),
    m_bDeleted( false )
{
    if( ( SwGetSetExpType::Sequence | SwGetSetExpType::String ) & m_nType )
        EnableFormat(false);    // do not use Numberformatter
}

std::unique_ptr<SwFieldType> SwSetExpFieldType::Copy() const
{
    std::unique_ptr<SwSetExpFieldType> pNew(new SwSetExpFieldType(GetDoc(), m_sName, m_nType));
    pNew->m_bDeleted = m_bDeleted;
    pNew->m_sDelim = m_sDelim;
    pNew->m_nLevel = m_nLevel;

    return pNew;
}

UIName SwSetExpFieldType::GetName() const
{
    return m_sName;
}

const OUString& SwSetExpField::GetExpStr(SwRootFrame const*const pLayout) const
{
    return (pLayout && pLayout->IsHideRedlines()) ? msExpandRLHidden : msExpand;
}

void SwSetExpField::ChgExpStr(const OUString& rExpand, SwRootFrame const*const pLayout)
{
    if (!pLayout || pLayout->IsHideRedlines())
    {
        msExpandRLHidden = rExpand;
    }
    if (!pLayout || !pLayout->IsHideRedlines())
    {
        msExpand = rExpand;
    }
}

void SwSetExpFieldType::SwClientNotify(const SwModify&, const SfxHint&)
{
    // do not expand further
}

void SwSetExpFieldType::SetSeqFormat(sal_uInt32 nFormat)
{
    std::vector<SwFormatField*> vFields;
    GatherFields(vFields, false);
    for(auto pFormatField: vFields)
        pFormatField->GetField()->SetUntypedFormat(nFormat);
}

sal_uInt32 SwSetExpFieldType::GetSeqFormat() const
{
    if( !HasWriterListeners() )
        return SVX_NUM_ARABIC;

    std::vector<SwFormatField*> vFields;
    GatherFields(vFields, false);
    return vFields.front()->GetField()->GetUntypedFormat();
}

void SwSetExpFieldType::SetSeqRefNo( SwSetExpField& rField )
{
    if( !HasWriterListeners() || !(SwGetSetExpType::Sequence & m_nType) )
        return;

    std::vector<sal_uInt16> aArr;

    // check if number is already used and if a new one needs to be created
    std::vector<SwFormatField*> vFields;
    GatherFields(vFields);
    for(SwFormatField* pF: vFields)
        if(pF->GetField() != &rField)
            InsertSort(aArr, static_cast<SwSetExpField*>(pF->GetField())->GetSeqNumber());

    // check first if number already exists
    sal_uInt16 nNum = rField.GetSeqNumber();
    if( USHRT_MAX != nNum )
    {
        std::vector<sal_uInt16>::size_type n {0};

        for( n = 0; n < aArr.size(); ++n )
            if( aArr[ n ] >= nNum )
                break;

        if( n == aArr.size() || aArr[ n ] > nNum )
            return;            // no -> use it
    }

    // flagged all numbers, so determine the right number
    std::vector<sal_uInt16>::size_type n = aArr.size();
    OSL_ENSURE( n <= std::numeric_limits<sal_uInt16>::max(), "Array is too big for using a sal_uInt16 index" );

    if ( n > 0 && aArr[ n-1 ] != n-1 )
    {
        for( n = 0; n < aArr.size(); ++n )
            if( n != aArr[ n ] )
                break;
    }

    rField.SetSeqNumber( n );
}

size_t SwSetExpFieldType::GetSeqFieldList(SwSeqFieldList& rList,
        SwRootFrame const*const pLayout)
{
    rList.Clear();

    IDocumentRedlineAccess const& rIDRA(GetDoc()->getIDocumentRedlineAccess());

    std::vector<SwFormatField*> vFields;
    GatherFields(vFields);
    for(SwFormatField* pF: vFields)
    {
        const SwTextNode* pNd;
        if( nullptr != ( pNd = pF->GetTextField()->GetpTextNode() )
            && (!pLayout || !pLayout->IsHideRedlines()
                || !sw::IsFieldDeletedInModel(rIDRA, *pF->GetTextField())))
        {
            SeqFieldLstElem aNew(
                    pNd->GetExpandText(pLayout),
                    static_cast<SwSetExpField*>(pF->GetField())->GetSeqNumber() );
            rList.InsertSort( std::move(aNew) );
        }
    }
    return rList.Count();
}

void SwSetExpFieldType::SetChapter(SwSetExpField& rField, const SwNode& rNd,
        SwRootFrame const*const pLayout)
{
    const SwTextNode* pTextNd = rNd.FindOutlineNodeOfLevel(m_nLevel, pLayout);
    if( !pTextNd )
        return;

    SwNumRule * pRule = pTextNd->GetNumRule();

    if (!pRule)
        return;

    // --> OD 2005-11-02 #i51089 - TUNING#
    if (SwNodeNum const*const pNum = pTextNd->GetNum(pLayout))
    {
        // only get the number, without pre-/post-fixstrings
        OUString const sNumber(pRule->MakeNumString(*pNum, false));

        if( !sNumber.isEmpty() )
            rField.ChgExpStr(sNumber + m_sDelim + rField.GetExpStr(pLayout), pLayout);
    }
    else
    {
        OSL_ENSURE(pTextNd->GetNum(nullptr), "<SwSetExpFieldType::SetChapter(..)> - text node with numbering rule, but without number. This is a serious defect");
    }
}

void SwSetExpFieldType::QueryValue( uno::Any& rAny, sal_uInt16 nWhichId ) const
{
    switch( nWhichId )
    {
    case FIELD_PROP_SUBTYPE:
        {
            sal_Int16 nRet = lcl_SubTypeToAPI(GetType());
            rAny <<= nRet;
        }
        break;
    case FIELD_PROP_PAR2:
        rAny <<= GetDelimiter();
        break;
    case FIELD_PROP_SHORT1:
        {
            sal_Int8 nRet = m_nLevel < MAXLEVEL? m_nLevel : -1;
            rAny <<= nRet;
        }
        break;
    default:
        assert(false);
    }
}

void SwSetExpFieldType::PutValue( const uno::Any& rAny, sal_uInt16 nWhichId )
{
    switch( nWhichId )
    {
    case FIELD_PROP_SUBTYPE:
        {
            std::optional<SwGetSetExpType> nSet = lcl_APIToSubType(rAny);
            if(nSet)
                SetType(*nSet);
        }
        break;
    case FIELD_PROP_PAR2:
        {
            OUString sTmp;
            rAny >>= sTmp;
            if( !sTmp.isEmpty() )
                SetDelimiter( sTmp );
            else
                SetDelimiter( u" "_ustr );
        }
        break;
    case FIELD_PROP_SHORT1:
        {
            sal_Int8 nLvl = 0;
            rAny >>= nLvl;
            if(nLvl < 0 || nLvl >= MAXLEVEL)
                SetOutlineLvl(UCHAR_MAX);
            else
                SetOutlineLvl(nLvl);
        }
        break;
    default:
        assert(false);
    }
}

bool SwSeqFieldList::InsertSort( SeqFieldLstElem aNew )
{
    OUStringBuffer aBuf(aNew.sDlgEntry);
    const sal_Int32 nLen = aBuf.getLength();
    for (sal_Int32 i = 0; i < nLen; ++i)
    {
        if (aBuf[i]<' ')
        {
            aBuf[i]=' ';
        }
    }
    aNew.sDlgEntry = aBuf.makeStringAndClear();

    size_t nPos = 0;
    bool bRet = SeekEntry( aNew, &nPos );
    if( !bRet )
        maData.insert( maData.begin() + nPos, aNew );
    return bRet;
}

bool SwSeqFieldList::SeekEntry( const SeqFieldLstElem& rNew, size_t* pP ) const
{
    size_t nO = maData.size();
    size_t nU = 0;
    if( nO > 0 )
    {
        CollatorWrapper & rCaseColl = ::GetAppCaseCollator(),
                        & rColl = ::GetAppCollator();
        const CharClass& rCC = GetAppCharClass();

        //#59900# Sorting should sort number correctly (e.g. "10" after "9" not after "1")
        const OUString rTmp2 = rNew.sDlgEntry;
        sal_Int32 nFndPos2 = 0;
        const OUString sNum2( rTmp2.getToken( 0, ' ', nFndPos2 ));
        bool bIsNum2IsNumeric = CharClass::isAsciiNumeric( sNum2 );
        sal_Int32 nNum2 = bIsNum2IsNumeric ? sNum2.toInt32() : 0;

        nO--;
        while( nU <= nO )
        {
            const size_t nM = nU + ( nO - nU ) / 2;

            //#59900# Sorting should sort number correctly (e.g. "10" after "9" not after "1")
            const OUString rTmp1 = maData[nM].sDlgEntry;
            sal_Int32 nFndPos1 = 0;
            const OUString sNum1( rTmp1.getToken( 0, ' ', nFndPos1 ));
            sal_Int32 nCmp;

            if( bIsNum2IsNumeric && rCC.isNumeric( sNum1 ) )
            {
                sal_Int32 nNum1 = sNum1.toInt32();
                nCmp = nNum2 - nNum1;
                if( 0 == nCmp )
                {
                    OUString aTmp1 = nFndPos1 != -1 ? rTmp1.copy(nFndPos1) : OUString();
                    OUString aTmp2 = nFndPos2 != -1 ? rTmp2.copy(nFndPos2) : OUString();
                    nCmp = rCaseColl.compareString(aTmp2, aTmp1);
                }
            }
            else
                nCmp = rColl.compareString( rTmp2, rTmp1 );

            if( 0 == nCmp )
            {
                if( pP ) *pP = nM;
                return true;
            }
            else if( 0 < nCmp )
                nU = nM + 1;
            else if( nM == 0 )
                break;
            else
                nO = nM - 1;
        }
    }
    if( pP ) *pP = nU;
    return false;
}

SwSetExpField::SwSetExpField(SwSetExpFieldType* pTyp, const OUString& rFormel,
                                        sal_uLong nFormat)
    : SwFormulaField( pTyp, nFormat, 0.0 )
    , m_fValueRLHidden(0.0)
    , mnSeqNo( USHRT_MAX )
    , mnSubType(SwGetSetExpType::None)
    , mpFormatField(nullptr)
{
    SetFormula(rFormel);
    // ignore SubType
    mbInput = false;
    if( IsSequenceField() )
    {
        SwValueField::SetValue(1.0);
        m_fValueRLHidden = 1.0;
        if( rFormel.isEmpty() )
        {
            SetFormula(pTyp->GetName().toString() + "+1");
        }
    }
}

void SwSetExpField::SetFormatField(SwFormatField & rFormatField)
{
    mpFormatField = &rFormatField;
}

OUString SwSetExpField::ExpandImpl(SwRootFrame const*const pLayout) const
{
    if (mnSubType & SwGetSetExpType::Command)
    {   // we need the CommandString
        return GetTyp()->GetName().toString() + " = " + GetFormula();
    }
    if(!(mnSubType & SwGetSetExpType::Invisible))
    {   // value is visible
        return (pLayout && pLayout->IsHideRedlines()) ? msExpandRLHidden : msExpand;
    }
    return OUString();
}

/// @return the field name
OUString SwSetExpField::GetFieldName() const
{
    SwFieldTypesEnum const nStrType( (IsSequenceField())
                            ? SwFieldTypesEnum::Sequence
                            : mbInput
                                ? SwFieldTypesEnum::SetInput
                                : SwFieldTypesEnum::Set   );

    OUString aStr(
        SwFieldType::GetTypeStr( nStrType )
        + " "
        + GetTyp()->GetName().toString() );

    // Sequence: without formula
    if (SwFieldTypesEnum::Sequence != nStrType)
    {
        aStr += " = " + GetFormula();
    }
    return aStr;
}

std::unique_ptr<SwField> SwSetExpField::Copy() const
{
    std::unique_ptr<SwSetExpField> pTmp(new SwSetExpField(static_cast<SwSetExpFieldType*>(GetTyp()),
                                            GetFormula(), GetFormat()));
    pTmp->SwValueField::SetValue(GetValue());
    pTmp->m_fValueRLHidden = m_fValueRLHidden;
    pTmp->msExpand       = msExpand;
    pTmp->msExpandRLHidden = msExpandRLHidden;
    pTmp->SetAutomaticLanguage(IsAutomaticLanguage());
    pTmp->SetLanguage(GetLanguage());
    pTmp->maPText        = maPText;
    pTmp->mbInput        = mbInput;
    pTmp->mnSeqNo        = mnSeqNo;
    pTmp->SetSubType(GetSubType());

    return std::unique_ptr<SwField>(pTmp.release());
}

void SwSetExpField::SetSubType(SwGetSetExpType nSub)
{
    assert((nSub & SwGetSetExpType::LowerMask) != (SwGetSetExpType::String | SwGetSetExpType::Expr) && "SubType is illegal!");
    static_cast<SwSetExpFieldType*>(GetTyp())->SetType(nSub & SwGetSetExpType::LowerMask);
    mnSubType = nSub & SwGetSetExpType::UpperMask;
}

SwGetSetExpType SwSetExpField::GetSubType() const
{
    return static_cast<SwSetExpFieldType*>(GetTyp())->GetType() | mnSubType;
}

void SwSetExpField::SetValue( const double& rAny )
{
    SwValueField::SetValue(rAny);

    if( IsSequenceField() )
        msExpand = FormatNumber( GetValue(), static_cast<SvxNumType>(GetFormat()), GetLanguage() );
    else
        msExpand = static_cast<SwValueFieldType*>(GetTyp())->ExpandValue( rAny,
                                                GetFormat(), GetLanguage());
}

void SwSetExpField::SetValue(const double& rValue, SwRootFrame const*const pLayout)
{
    if (!pLayout || !pLayout->IsHideRedlines())
    {
        SetValue(rValue);
    }
    if (pLayout && !pLayout->IsHideRedlines())
        return;

    m_fValueRLHidden = rValue;
    if (IsSequenceField())
    {
        msExpandRLHidden = FormatNumber(rValue, static_cast<SvxNumType>(GetFormat()), GetLanguage());
    }
    else
    {
        msExpandRLHidden = static_cast<SwValueFieldType*>(GetTyp())->ExpandValue(
                rValue, GetFormat(), GetLanguage());
    }
}

double SwSetExpField::GetValue(SwRootFrame const* pLayout) const
{
    return (pLayout && pLayout->IsHideRedlines()) ? m_fValueRLHidden : GetValue();
}

void SwGetExpField::SetValue( const double& rAny )
{
    SwValueField::SetValue(rAny);
    m_sExpand = static_cast<SwValueFieldType*>(GetTyp())->ExpandValue( rAny, GetFormat(),
                                                            GetLanguage());
}

void SwGetExpField::SetValue(const double& rValue, SwRootFrame const*const pLayout)
{
    if (!pLayout || !pLayout->IsHideRedlines())
    {
        SetValue(rValue);
    }
    if (!pLayout || pLayout->IsHideRedlines())
    {
        m_fValueRLHidden = rValue;
        m_sExpandRLHidden = static_cast<SwValueFieldType*>(GetTyp())->ExpandValue(
                rValue, GetFormat(), GetLanguage());
    }
}

double SwGetExpField::GetValue(SwRootFrame const* pLayout) const
{
    return (pLayout && pLayout->IsHideRedlines()) ? m_fValueRLHidden : GetValue();
}

/** Find the index of the reference text following the current field
 *
 * @param rFormat
 * @param rDoc
 * @param nHint search starting position after the current field (or 0 if default)
 * @return
 */
sal_Int32 SwGetExpField::GetReferenceTextPos( const SwFormatField& rFormat, SwDoc& rDoc, sal_Int32 nHint)
{

    const SwTextField* pTextField = rFormat.GetTextField();
    const SwTextNode& rTextNode = pTextField->GetTextNode();

    sal_Int32 nRet = nHint ? nHint : pTextField->GetStart() + 1;
    OUString sNodeText = rTextNode.GetText();

    if(nRet<sNodeText.getLength())
    {
        sNodeText = sNodeText.copy(nRet);

        // now check if sNodeText starts with a non-alphanumeric character plus blanks
        sal_uInt16 nSrcpt = g_pBreakIt->GetRealScriptOfText( sNodeText, 0 );

        static const WhichRangesContainer nIds(svl::Items<
            RES_CHRATR_FONT, RES_CHRATR_FONT,
            RES_CHRATR_LANGUAGE, RES_CHRATR_LANGUAGE,
            RES_CHRATR_CJK_FONT, RES_CHRATR_CJK_FONT,
            RES_CHRATR_CJK_LANGUAGE, RES_CHRATR_CJK_LANGUAGE,
            RES_CHRATR_CTL_FONT, RES_CHRATR_CTL_FONT,
            RES_CHRATR_CTL_LANGUAGE, RES_CHRATR_CTL_LANGUAGE
        >);
        SwAttrSet aSet(rDoc.GetAttrPool(), nIds);
        rTextNode.GetParaAttr(aSet, nRet, nRet+1);

        TypedWhichId<SvxFontItem> nFontWhich = GetWhichOfScript( RES_CHRATR_FONT, nSrcpt );
        if( RTL_TEXTENCODING_SYMBOL != aSet.Get( nFontWhich ).GetCharSet() )
        {
            TypedWhichId<SvxLanguageItem> nLangWhich = GetWhichOfScript( RES_CHRATR_LANGUAGE, nSrcpt ) ;
            LanguageType eLang = aSet.Get(nLangWhich).GetLanguage();
            CharClass aCC(( LanguageTag(eLang) ));
            sal_Unicode c0 = sNodeText[0];
            bool bIsAlphaNum = aCC.isAlphaNumeric( sNodeText, 0 );
            if( !bIsAlphaNum ||
                (c0 == ' ' || c0 == '\t'))
            {
                // ignoring blanks
                nRet++;
                const sal_Int32 nLen = sNodeText.getLength();
                for (sal_Int32 i = 1;
                     i<nLen && (sNodeText[i]==' ' || sNodeText[i]=='\t');
                     ++i
                )
                    ++nRet;
            }
        }
    }
    return nRet;
}

OUString SwSetExpField::GetPar1() const
{
    return static_cast<const SwSetExpFieldType*>(GetTyp())->GetName().toString();
}

OUString SwSetExpField::GetPar2() const
{
    SwGetSetExpType nType = static_cast<SwSetExpFieldType*>(GetTyp())->GetType();

    if (nType & SwGetSetExpType::String)
        return GetFormula();
    return GetExpandedFormula();
}

void SwSetExpField::SetPar2(const OUString& rStr)
{
    SwGetSetExpType nType = static_cast<SwSetExpFieldType*>(GetTyp())->GetType();

    if( !(nType & SwGetSetExpType::Sequence) || !rStr.isEmpty() )
    {
        if (nType & SwGetSetExpType::String)
            SetFormula(rStr);
        else
            SetExpandedFormula(rStr);
    }
}

bool SwSetExpField::PutValue( const uno::Any& rAny, sal_uInt16 nWhichId )
{
    sal_Int32 nTmp32 = 0;
    sal_Int16 nTmp16 = 0;
    switch( nWhichId )
    {
    case FIELD_PROP_BOOL2:
        if(*o3tl::doAccess<bool>(rAny))
            mnSubType &= ~SwGetSetExpType::Invisible;
        else
            mnSubType |= SwGetSetExpType::Invisible;
        break;
    case FIELD_PROP_FORMAT:
        rAny >>= nTmp32;
        SetFormat(nTmp32);
        break;
    case FIELD_PROP_USHORT2:
        {
            rAny >>= nTmp16;
            if(nTmp16 <= css::style::NumberingType::NUMBER_NONE )
                SetFormat(nTmp16);
            else {
                //exception(wrong_value)
                ;
            }
        }
        break;
    case FIELD_PROP_USHORT1:
        rAny >>= nTmp16;
        mnSeqNo = nTmp16;
        break;
    case FIELD_PROP_PAR1:
        {
            OUString sTmp;
            rAny >>= sTmp;
            SetPar1( SwStyleNameMapper::GetUIName( ProgName(sTmp), SwGetPoolIdFromName::TxtColl ).toString() );
        }
        break;
    case FIELD_PROP_PAR2:
        {
            OUString uTmp;
            rAny >>= uTmp;
            //I18N - if the formula contains only "TypeName+1"
            //and it's one of the initially created sequence fields
            //then the localized names has to be replaced by a programmatic name
            OUString sMyFormula = SwXFieldMaster::LocalizeFormula(*this, uTmp, false);
            SetFormula( sMyFormula );
        }
        break;
    case FIELD_PROP_DOUBLE:
        {
            double fVal = 0.0;
            rAny >>= fVal;
            SetValue(fVal);
            m_fValueRLHidden = fVal;
        }
        break;
    case FIELD_PROP_SUBTYPE:
        {
            std::optional<SwGetSetExpType> oTmpSub = lcl_APIToSubType(rAny);
            if (oTmpSub && *oTmpSub != (GetSubType() & SwGetSetExpType::LowerMask))
            {
                SwGetSetExpType const subType((GetSubType() & SwGetSetExpType::UpperMask) | *oTmpSub);
                if (((*oTmpSub & SwGetSetExpType::String) != (GetSubType() & SwGetSetExpType::String))
                    && GetInputFlag())
                {
                    SwXTextField::TransmuteLeadToInputField(*this, subType);
                }
                else
                {
                    SetSubType(subType);
                }
            }
        }
        break;
    case FIELD_PROP_PAR3:
        rAny >>= maPText;
        break;
    case FIELD_PROP_BOOL3:
        if(*o3tl::doAccess<bool>(rAny))
            mnSubType |= SwGetSetExpType::Command;
        else
            mnSubType &= ~SwGetSetExpType::Command;
        break;
    case FIELD_PROP_BOOL1:
        {
            bool newInput(*o3tl::doAccess<bool>(rAny));
            if (newInput != GetInputFlag())
            {
                if (static_cast<SwSetExpFieldType*>(GetTyp())->GetType()
                        & SwGetSetExpType::String)
                {
                    SwXTextField::TransmuteLeadToInputField(*this, std::nullopt);
                }
                else
                {
                    SetInputFlag(newInput);
                }
            }
        }
        break;
    case FIELD_PROP_PAR4:
        {
            OUString sTmp;
            rAny >>= sTmp;
            ChgExpStr(sTmp, nullptr);
        }
        break;
    default:
        return SwField::PutValue(rAny, nWhichId);
    }
    return true;
}

bool SwSetExpField::QueryValue( uno::Any& rAny, sal_uInt16 nWhichId ) const
{
    switch( nWhichId )
    {
    case FIELD_PROP_BOOL2:
        rAny <<= !(mnSubType & SwGetSetExpType::Invisible);
        break;
    case FIELD_PROP_FORMAT:
        rAny <<= static_cast<sal_Int32>(GetFormat());
        break;
    case FIELD_PROP_USHORT2:
        rAny <<= static_cast<sal_Int16>(GetFormat());
        break;
    case FIELD_PROP_USHORT1:
        rAny <<= static_cast<sal_Int16>(mnSeqNo);
        break;
    case FIELD_PROP_PAR1:
        rAny <<= SwStyleNameMapper::GetProgName(UIName(GetPar1()), SwGetPoolIdFromName::TxtColl ).toString();
        break;
    case FIELD_PROP_PAR2:
        {
            //I18N - if the formula contains only "TypeName+1"
            //and it's one of the initially created sequence fields
            //then the localized names has to be replaced by a programmatic name
            OUString sMyFormula = SwXFieldMaster::LocalizeFormula(*this, GetFormula(), true);
            rAny <<= sMyFormula;
        }
        break;
    case FIELD_PROP_DOUBLE:
        rAny <<= GetValue();
        break;
    case FIELD_PROP_SUBTYPE:
        {
            sal_Int16 nRet = lcl_SubTypeToAPI(GetSubType() & SwGetSetExpType::LowerMask);
            rAny <<= nRet;
        }
        break;
    case FIELD_PROP_PAR3:
        rAny <<= maPText;
        break;
    case FIELD_PROP_BOOL3:
        rAny <<= bool(mnSubType & SwGetSetExpType::Command);
        break;
    case FIELD_PROP_BOOL1:
        rAny <<= GetInputFlag();
        break;
    case FIELD_PROP_PAR4:
        rAny <<= GetExpStr(nullptr);
        break;
    default:
        return SwField::QueryValue(rAny, nWhichId);
    }
    return true;
}

SwInputFieldType::SwInputFieldType( SwDoc* pD )
    : SwFieldType( SwFieldIds::Input )
    , mpDoc( pD )
{
}

std::unique_ptr<SwFieldType> SwInputFieldType::Copy() const
{
    return std::make_unique<SwInputFieldType>( mpDoc );
}

SwInputField::SwInputField( SwInputFieldType* pFieldType,
                            OUString aContent,
                            OUString aPrompt,
                            SwInputFieldSubType nSub,
                            bool bIsFormField )
    : SwField( pFieldType, LANGUAGE_SYSTEM, false )
    , maContent(std::move(aContent))
    , maPText(std::move(aPrompt))
    , mnSubType(nSub)
    , mbIsFormField( bIsFormField )
    , mpFormatField( nullptr )
{
}

SwInputField::~SwInputField()
{
}

void SwInputField::SetFormatField( SwFormatField& rFormatField )
{
    mpFormatField = &rFormatField;
}


void SwInputField::applyFieldContent( const OUString& rNewFieldContent )
{
    if ( (mnSubType & SwInputFieldSubType::LowerMask) == SwInputFieldSubType::Text )
    {
        maContent = rNewFieldContent;
    }
    else if( (mnSubType & SwInputFieldSubType::LowerMask) == SwInputFieldSubType::User )
    {
        SwUserFieldType* pUserTyp = static_cast<SwUserFieldType*>(
            static_cast<SwInputFieldType*>(GetTyp())->GetDoc()->getIDocumentFieldsAccess().GetFieldType( SwFieldIds::User, getContent(), false ) );
        if( pUserTyp )
        {
            pUserTyp->SetContent( rNewFieldContent );
            if (!pUserTyp->IsModifyLocked())
            {
                // trigger update of the corresponding User Fields and other
                // related Input Fields
                bool bUnlock(false);
                if (GetFormatField() != nullptr)
                {
                    SwTextInputField *const pTextInputField =
                        dynamic_cast<SwTextInputField*>(GetFormatField()->GetTextField());
                    if (pTextInputField != nullptr)
                    {
                        bUnlock = pTextInputField->LockNotifyContentChange();
                    }
                }
                pUserTyp->UpdateFields();
                if (bUnlock)
                {
                    SwTextInputField *const pTextInputField =
                        dynamic_cast<SwTextInputField*>(GetFormatField()->GetTextField());
                    if (pTextInputField != nullptr)
                    {
                        pTextInputField->UnlockNotifyContentChange();
                    }
                }
            }
        }
    }
}

OUString SwInputField::GetFieldName() const
{
    OUString aStr(SwField::GetFieldName());
    if ((mnSubType & SwInputFieldSubType::LowerMask) == SwInputFieldSubType::User)
    {
        aStr += GetTyp()->GetName().toString() + " " + getContent();
    }
    return aStr;
}

std::unique_ptr<SwField> SwInputField::Copy() const
{
    std::unique_ptr<SwInputField> pField(
        new SwInputField(
            static_cast<SwInputFieldType*>(GetTyp()),
            getContent(),
            maPText,
            GetSubType(),
            mbIsFormField ));

    pField->SetHelp( maHelp );
    pField->SetToolTip( maToolTip );
    pField->maGrabBag = maGrabBag;

    pField->SetAutomaticLanguage(IsAutomaticLanguage());
    return std::unique_ptr<SwField>(pField.release());
}

OUString SwInputField::ExpandImpl(SwRootFrame const*const) const
{
    if((mnSubType & SwInputFieldSubType::LowerMask) == SwInputFieldSubType::Text)
    {
        return getContent();
    }

    if( (mnSubType & SwInputFieldSubType::LowerMask) == SwInputFieldSubType::User )
    {
        SwUserFieldType* pUserTyp = static_cast<SwUserFieldType*>(
            static_cast<SwInputFieldType*>(GetTyp())->GetDoc()->getIDocumentFieldsAccess().GetFieldType( SwFieldIds::User, getContent(), false ) );
        if( pUserTyp )
            return pUserTyp->GetContent();
    }

    return OUString();
}

bool SwInputField::isFormField() const
{
    return mbIsFormField
           || !maHelp.isEmpty()
           || !maToolTip.isEmpty();
}

bool SwInputField::QueryValue( uno::Any& rAny, sal_uInt16 nWhichId ) const
{
    switch( nWhichId )
    {
    case FIELD_PROP_PAR1:
        rAny <<= getContent();
        break;
    case FIELD_PROP_PAR2:
        rAny <<= maPText;
        break;
    case FIELD_PROP_PAR3:
        rAny <<= maHelp;
        break;
    case FIELD_PROP_PAR4:
        rAny <<= maToolTip;
        break;
    case FIELD_PROP_GRABBAG:
        rAny <<= maGrabBag;
        break;
    case FIELD_PROP_TITLE:
        break;
    default:
        assert(false);
    }
    return true;
}

bool SwInputField::PutValue( const uno::Any& rAny, sal_uInt16 nWhichId )
{
    switch( nWhichId )
    {
    case FIELD_PROP_PAR1:
        rAny >>= maContent;
        break;
    case FIELD_PROP_PAR2:
        rAny >>= maPText;
        break;
    case FIELD_PROP_PAR3:
        rAny >>= maHelp;
        break;
    case FIELD_PROP_PAR4:
        rAny >>= maToolTip;
        break;
    case FIELD_PROP_GRABBAG:
        rAny >>= maGrabBag;
        break;
    case FIELD_PROP_TITLE:
        break;
    default:
        assert(false);
    }
    return true;
}

/// set condition
void SwInputField::SetPar1(const OUString& rStr)
{
    maContent = rStr;
}

OUString SwInputField::GetPar1() const
{
    return getContent();
}

void SwInputField::SetPar2(const OUString& rStr)
{
    maPText = rStr;
}

OUString SwInputField::GetPar2() const
{
    return maPText;
}

void SwInputField::SetHelp(const OUString & rStr)
{
    maHelp = rStr;
}

const OUString& SwInputField::GetHelp() const
{
    return maHelp;
}

void SwInputField::SetToolTip(const OUString & rStr)
{
    maToolTip = rStr;
}

const OUString& SwInputField::GetToolTip() const
{
    return maToolTip;
}

SwInputFieldSubType SwInputField::GetSubType() const
{
    return mnSubType;
}

void SwInputField::SetSubType(SwInputFieldSubType nSub)
{
    mnSubType = nSub;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
