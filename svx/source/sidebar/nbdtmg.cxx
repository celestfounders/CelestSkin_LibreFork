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
#include <svx/nbdtmg.hxx>
#include <svx/svxids.hrc>
#include <vcl/svapp.hxx>
#include <svl/itemset.hxx>
#include <sfx2/request.hxx>
#include <svl/stritem.hxx>
#include <svtools/ctrltool.hxx>
#include <sfx2/objsh.hxx>
#include <editeng/flstitem.hxx>
#include <svl/itempool.hxx>
#include <vcl/outdev.hxx>
#include <editeng/brushitem.hxx>
#include <svx/dialmgr.hxx>
#include <svx/strings.hrc>
#include <vcl/graph.hxx>
#include <vcl/settings.hxx>

#include <i18nlangtag/languagetag.hxx>
#include <o3tl/temporary.hxx>
#include <tools/debug.hxx>
#include <tools/urlobj.hxx>
#include <unotools/ucbstreamhelper.hxx>
#include <unotools/pathoptions.hxx>
#include <editeng/eeitem.hxx>
#include <officecfg/Office/Common.hxx>

#include <com/sun/star/text/VertOrientation.hpp>
#include <com/sun/star/style/NumberingType.hpp>
#include <com/sun/star/container/XIndexAccess.hpp>
#include <com/sun/star/text/DefaultNumberingProvider.hpp>
#include <com/sun/star/beans/PropertyValue.hpp>
#include <comphelper/processfactory.hxx>
#include <memory>

using namespace com::sun::star;
using namespace com::sun::star::uno;
using namespace com::sun::star::beans;
using namespace com::sun::star::lang;
using namespace com::sun::star::text;
using namespace com::sun::star::container;

namespace svx::sidebar {

namespace {

vcl::Font& lcl_GetDefaultBulletFont()
{
    static vcl::Font aDefBulletFont = []()
    {
        static vcl::Font tmp(u"OpenSymbol"_ustr, u""_ustr, Size(0, 14));
        tmp.SetCharSet( RTL_TEXTENCODING_SYMBOL );
        tmp.SetFamily( FAMILY_DONTKNOW );
        tmp.SetPitch( PITCH_DONTKNOW );
        tmp.SetWeight( WEIGHT_DONTKNOW );
        tmp.SetTransparent( true );
        return tmp;
    }();
    return aDefBulletFont;
}

NumSettings_Impl* lcl_CreateNumberingSettingsPtr(const Sequence<PropertyValue>& rLevelProps)
{
    NumSettings_Impl* pNew = new NumSettings_Impl;
    for(const PropertyValue& rValue : rLevelProps)
    {
        if(rValue.Name == "NumberingType")
        {
            sal_Int16 nTmp;
            if (rValue.Value >>= nTmp)
                pNew->nNumberType = static_cast<SvxNumType>(nTmp);
        }
        else if(rValue.Name == "Prefix")
            rValue.Value >>= pNew->sPrefix;
        else if(rValue.Name == "Suffix")
            rValue.Value >>= pNew->sSuffix;
        else if (rValue.Name == "Adjust")
        {
            sal_Int16 nTmp;
            if (rValue.Value >>= nTmp)
                pNew->eNumAlign = static_cast<SvxAdjust>(nTmp);
        }
        else if(rValue.Name == "ParentNumbering")
            rValue.Value >>= pNew->nParentNumbering;
        else if(rValue.Name == "BulletChar")
            rValue.Value >>= pNew->sBulletChar;
        else if(rValue.Name == "BulletFontName")
            rValue.Value >>= pNew->sBulletFont;
    }
    const sal_Unicode cLocalPrefix = pNew->sPrefix.getLength() ? pNew->sPrefix[0] : 0;
    const sal_Unicode cLocalSuffix = pNew->sSuffix.getLength() ? pNew->sSuffix[0] : 0;
    if( cLocalPrefix == ' ') pNew->sPrefix.clear();
    if( cLocalSuffix == ' ') pNew->sSuffix.clear();
    return pNew;
}

}

sal_uInt16 NBOTypeMgrBase:: IsSingleLevel(sal_uInt16 nCurLevel)
{
    sal_uInt16 nLv = sal_uInt16(0xFFFF);
    sal_uInt16 nCount = 0;
    sal_uInt16 nMask = 1;
    for( sal_uInt16 i = 0; i < SVX_MAX_NUM; i++ )
    {
        if(nCurLevel & nMask)
        {
            nCount++;
            nLv=i;
        }
        nMask <<= 1 ;
    }

    if ( nCount == 1)
        return nLv;
    else
        return sal_uInt16(0xFFFF);
}

void NBOTypeMgrBase::SetItems(const SfxItemSet* pArg) {
    pSet = pArg;
    if ( !pSet )
        return;

    SfxAllItemSet aSet(*pSet);

    const SfxStringItem* pBulletCharFmt = aSet.GetItem<SfxStringItem>(SID_BULLET_CHAR_FMT, false);
    if (pBulletCharFmt)
        aBulletCharFmtName = pBulletCharFmt->GetValue();

    const SfxStringItem* pNumCharFmt = aSet.GetItem<SfxStringItem>(SID_NUM_CHAR_FMT, false);
    if (pNumCharFmt)
        aNumCharFmtName = pNumCharFmt->GetValue();

    const SfxPoolItem* pItem;
    SfxItemState eState = pSet->GetItemState(SID_ATTR_NUMBERING_RULE, false, &pItem);
    if(eState == SfxItemState::SET)
    {
        eCoreUnit = pSet->GetPool()->GetMetric(pSet->GetPool()->GetWhichIDFromSlotID(SID_ATTR_NUMBERING_RULE));
    } else {
        //sd use different sid for numbering rule
        eState = pSet->GetItemState(EE_PARA_NUMBULLET, false, &pItem);
        if(eState == SfxItemState::SET)
        {
            eCoreUnit = pSet->GetPool()->GetMetric(pSet->GetPool()->GetWhichIDFromSlotID(EE_PARA_NUMBULLET));
        }
    }
}

void NBOTypeMgrBase::ImplLoad(std::u16string_view filename)
{
    bIsLoading = true;
    MapUnit      eOldCoreUnit=eCoreUnit;
    eCoreUnit = MapUnit::Map100thMM;
    INetURLObject aFile( SvtPathOptions().GetUserConfigPath() );
    aFile.Append( filename);
    std::unique_ptr<SvStream> xIStm(::utl::UcbStreamHelper::CreateStream( aFile.GetMainURL( INetURLObject::DecodeMechanism::NONE ), StreamMode::READ ));
    if( xIStm ) {
        sal_uInt32                  nVersion = 0;
        sal_Int32                   nNumIndex = 0;
        xIStm->ReadUInt32( nVersion );
        if (nVersion==DEFAULT_NUMBERING_CACHE_FORMAT_VERSION) //first version
        {
            xIStm->ReadInt32( nNumIndex );
            while (nNumIndex>=0 && nNumIndex<DEFAULT_NUM_VALUSET_COUNT) {
                SvxNumRule aNum(*xIStm);
                //bullet color in font properties is not stored correctly. Need set transparency bits manually
                for(sal_uInt16 i = 0; i < aNum.GetLevelCount(); i++)
                {
                    SvxNumberFormat aFmt(aNum.GetLevel(i));
                    if (aFmt.GetBulletFont()) {
                        vcl::Font aFont(*aFmt.GetBulletFont());
                        Color c=aFont.GetColor();
                        c.SetAlpha(0);
                        aFont.SetColor(c);
                        aFmt.SetBulletFont(&aFont);
                        aNum.SetLevel(i, aFmt);
                    }
                }
                ReplaceNumRule(aNum,nNumIndex,0x1/*nLevel*/);
                xIStm->ReadInt32( nNumIndex );
            }
        }
    }
    eCoreUnit = eOldCoreUnit;
    bIsLoading = false;
}
void NBOTypeMgrBase::ImplStore(std::u16string_view filename)
{
    if (bIsLoading) return;
    MapUnit      eOldCoreUnit=eCoreUnit;
    eCoreUnit = MapUnit::Map100thMM;
    INetURLObject aFile( SvtPathOptions().GetUserConfigPath() );
    aFile.Append( filename);
    std::unique_ptr<SvStream> xOStm(::utl::UcbStreamHelper::CreateStream( aFile.GetMainURL( INetURLObject::DecodeMechanism::NONE ), StreamMode::WRITE ));
    if( xOStm ) {
        sal_uInt32                      nVersion;
        sal_Int32                       nNumIndex;
        nVersion = DEFAULT_NUMBERING_CACHE_FORMAT_VERSION;
        xOStm->WriteUInt32( nVersion );
        for(sal_Int32 nItem = 0; nItem < DEFAULT_NUM_VALUSET_COUNT; nItem++ ) {
            if (IsCustomized(nItem)) {
                SvxNumRule aDefNumRule( SvxNumRuleFlags::BULLET_REL_SIZE | SvxNumRuleFlags::CONTINUOUS | SvxNumRuleFlags::BULLET_COLOR,
                    10, false,
                    SvxNumRuleType::NUMBERING, SvxNumberFormat::LABEL_ALIGNMENT);
                xOStm->WriteInt32( nItem );
                ApplyNumRule(aDefNumRule,nItem,0x1/*nLevel*/,false,true);
                aDefNumRule.Store(*xOStm);
            }
        }
        nNumIndex = -1;
        xOStm->WriteInt32( nNumIndex );  //write end flag
    }
    eCoreUnit = eOldCoreUnit;
}

// Character Bullet Type lib
BulletsSettings* BulletsTypeMgr::pActualBullets[] ={nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr};
const sal_Unicode BulletsTypeMgr::aDynamicBulletTypes[]={' ',' ',' ',' ',' ',' ',' ',' '};
const sal_Unicode BulletsTypeMgr::aDynamicRTLBulletTypes[]={' ',' ',' ',' ',' ',' ',' ',' '};

BulletsTypeMgr::BulletsTypeMgr()
{
    Init();
}

BulletsTypeMgr& BulletsTypeMgr::GetInstance()
{
    static BulletsTypeMgr theBulletsTypeMgr;
    return theBulletsTypeMgr;
}

void BulletsTypeMgr::Init()
{
    css::uno::Sequence< OUString > aBulletSymbols(officecfg::Office::Common::BulletsNumbering::DefaultBullets::get());
    css::uno::Sequence< OUString > aBulletSymbolsFonts(officecfg::Office::Common::BulletsNumbering::DefaultBulletsFonts::get());

    vcl::Font& rActBulletFont = lcl_GetDefaultBulletFont();

    for (sal_uInt16 i=0;i<DEFAULT_BULLET_TYPES;i++)
    {
        pActualBullets[i] = new BulletsSettings;
        pActualBullets[i]->cBulletChar = aBulletSymbols[i].toChar();
        rActBulletFont.SetFamilyName(aBulletSymbolsFonts[i]);
        pActualBullets[i]->aFont = rActBulletFont;
    }
}

sal_uInt16 BulletsTypeMgr::GetNBOIndexForNumRule(SvxNumRule& aNum,sal_uInt16 mLevel,sal_uInt16 nFromIndex)
{
    if ( mLevel == sal_uInt16(0xFFFF) || mLevel == 0)
        return sal_uInt16(0xFFFF);
    //if ( !lcl_IsNumFmtSet(pNR, mLevel) ) return (sal_uInt16)0xFFFF;

    sal_uInt16 nActLv = IsSingleLevel(mLevel);

    if ( nActLv == sal_uInt16(0xFFFF) )
        return sal_uInt16(0xFFFF);

    const SvxNumberFormat& aFmt(aNum.GetLevel(nActLv));
    sal_UCS4 cChar = aFmt.GetBulletChar();

    css::uno::Sequence<OUString> aBulletSymbols(officecfg::Office::Common::BulletsNumbering::DefaultBullets::get());
    for(sal_uInt16 i = nFromIndex; i < DEFAULT_BULLET_TYPES; i++)
    {
        if ( (cChar == aBulletSymbols[i].toChar()) ||
             (cChar == 9830 && 57356 == pActualBullets[i]->cBulletChar) ||
             (cChar == 9632 && 57354 == pActualBullets[i]->cBulletChar)   )
        {
            return i+1;
        }
    }

    return sal_uInt16(0xFFFF);
}

void BulletsTypeMgr::ReplaceNumRule(SvxNumRule& aNum, sal_uInt16 nIndex, sal_uInt16 mLevel)
{
    if ( nIndex >= DEFAULT_BULLET_TYPES )
        return;

    if ( mLevel == sal_uInt16(0xFFFF) || mLevel == 0)
        return;

    if ( GetNBOIndexForNumRule(aNum,mLevel) != sal_uInt16(0xFFFF) )
        return;

    sal_uInt16 nActLv = IsSingleLevel(mLevel);

    if ( nActLv == sal_uInt16(0xFFFF) )
        return;

    SvxNumberFormat aFmt(aNum.GetLevel(nActLv));
    sal_UCS4 cChar = aFmt.GetBulletChar();
    const std::optional<vcl::Font>& pFont = aFmt.GetBulletFont();

    pActualBullets[nIndex]->cBulletChar = cChar;
    if ( pFont )
        pActualBullets[nIndex]->aFont = *pFont;
    pActualBullets[nIndex]->bIsCustomized = true;
}

void BulletsTypeMgr::ApplyNumRule(SvxNumRule& aNum, sal_uInt16 nIndex, sal_uInt16 mLevel, bool /*isDefault*/, bool isResetSize)
{
    if ( nIndex >= DEFAULT_BULLET_TYPES )
        return;

    css::uno::Sequence<OUString> aBulletSymbols(officecfg::Office::Common::BulletsNumbering::DefaultBullets::get());
    css::uno::Sequence<OUString> aBulletFontSymbols(officecfg::Office::Common::BulletsNumbering::DefaultBulletsFonts::get());

    sal_UCS4 cChar = aBulletSymbols[nIndex].toChar();
    vcl::Font& rActBulletFont = pActualBullets[nIndex]->aFont;
    rActBulletFont.SetFamilyName(aBulletFontSymbols[nIndex]);

    sal_uInt16 nMask = 1;
    OUString sBulletCharFormatName = GetBulletCharFmtName();
    for(sal_uInt16 i = 0; i < aNum.GetLevelCount(); i++)
    {
        if(mLevel & nMask)
        {
            SvxNumberFormat aFmt(aNum.GetLevel(i));
            aFmt.SetNumberingType( SVX_NUM_CHAR_SPECIAL );
            aFmt.SetBulletFont(&rActBulletFont);
            aFmt.SetBulletChar(cChar);
            aFmt.SetCharFormatName(sBulletCharFormatName);
            aFmt.SetListFormat( "" );
            if (isResetSize) aFmt.SetBulletRelSize(45);
            aNum.SetLevel(i, aFmt);
        }
        nMask <<= 1;
    }
}

void BulletsTypeMgr::ApplyCustomRule(SvxNumRule& aNum, std::u16string_view sBullet,
                                     const OUString& sFont, sal_uInt16 mLevel)
{
    sal_uInt16 nMask = 1;
    OUString sBulletCharFormatName = GetBulletCharFmtName();
    const vcl::Font aFont(sFont, Size(1, 1));
    for (sal_uInt16 i = 0; i < aNum.GetLevelCount(); i++)
    {
        if (mLevel & nMask)
        {
            SvxNumberFormat aFmt(aNum.GetLevel(i));
            aFmt.SetNumberingType(SVX_NUM_CHAR_SPECIAL);
            aFmt.SetBulletFont(&aFont);
            aFmt.SetBulletChar(sBullet[0]);
            aFmt.SetCharFormatName(sBulletCharFormatName);
            aFmt.SetListFormat("");
            aNum.SetLevel(i, aFmt);
        }
        nMask <<= 1;
    }
}

OUString BulletsTypeMgr::GetDescription(sal_uInt16 /*nIndex*/, bool /*isDefault*/)
{
    return OUString();
}

bool BulletsTypeMgr::IsCustomized(sal_uInt16 nIndex)
{
    bool bRet = false;

    if ( nIndex >= DEFAULT_BULLET_TYPES )
        bRet = false;
    else
        bRet = pActualBullets[nIndex]->bIsCustomized;

    return bRet;
}

// Numbering Type lib
NumberingTypeMgr::NumberingTypeMgr()
{
    Init();
    maDefaultNumberSettingsArr = maNumberSettingsArr;
    ImplLoad(u"standard.syb");
}

NumberingTypeMgr::~NumberingTypeMgr()
{
}

const TranslateId RID_SVXSTR_SINGLENUM_DESCRIPTIONS[] =
{
    RID_SVXSTR_SINGLENUM_DESCRIPTION_0,
    RID_SVXSTR_SINGLENUM_DESCRIPTION_1,
    RID_SVXSTR_SINGLENUM_DESCRIPTION_2,
    RID_SVXSTR_SINGLENUM_DESCRIPTION_3,
    RID_SVXSTR_SINGLENUM_DESCRIPTION_4,
    RID_SVXSTR_SINGLENUM_DESCRIPTION_5,
    RID_SVXSTR_SINGLENUM_DESCRIPTION_6,
    RID_SVXSTR_SINGLENUM_DESCRIPTION_7
};

NumberingTypeMgr& NumberingTypeMgr::GetInstance()
{
    static NumberingTypeMgr theNumberingTypeMgr;
    return theNumberingTypeMgr;
}

void NumberingTypeMgr::Init()
{
    const Reference< XComponentContext >& xContext = ::comphelper::getProcessComponentContext();
    Reference<XDefaultNumberingProvider> xDefNum = DefaultNumberingProvider::create( xContext );

    Sequence< Sequence< PropertyValue > > aNumberings;
    Locale aLocale(Application::GetSettings().GetLanguageTag().getLocale());
    try
    {
        aNumberings = xDefNum->getDefaultContinuousNumberingLevels( aLocale );

        sal_Int32 nLength = aNumberings.getLength();

        const Sequence<PropertyValue>* pValuesArr = aNumberings.getConstArray();
        for(sal_Int32 i = 0; i < nLength; i++)
        {
            NumSettings_Impl* pNew = lcl_CreateNumberingSettingsPtr(pValuesArr[i]);
            std::shared_ptr<NumberSettings_Impl> pNumEntry = std::make_shared<NumberSettings_Impl>();
            pNumEntry->pNumSetting = pNew;
            if ( i < 8 )
                pNumEntry->sDescription = SvxResId(RID_SVXSTR_SINGLENUM_DESCRIPTIONS[i]);
            maNumberSettingsArr.push_back(pNumEntry);
        }
    }
    catch(Exception&)
    {
    }
}

sal_uInt16 NumberingTypeMgr::GetNBOIndexForNumRule(SvxNumRule& aNum,sal_uInt16 mLevel,sal_uInt16 nFromIndex)
{
    if ( mLevel == sal_uInt16(0xFFFF) || mLevel > aNum.GetLevelCount() || mLevel == 0)
        return sal_uInt16(0xFFFF);

    sal_uInt16 nActLv = IsSingleLevel(mLevel);

    if ( nActLv == sal_uInt16(0xFFFF) )
        return sal_uInt16(0xFFFF);

    const SvxNumberFormat& aFmt(aNum.GetLevel(nActLv));
    //sal_Unicode cPrefix = OUString(aFmt.GetPrefix())[0];
    //sal_Unicode cSuffix = :OUString(aFmt.GetSuffix())[0];
    const OUString& sPrefix = aFmt.GetPrefix();
    const OUString& sLclSuffix = aFmt.GetSuffix();
    sal_Int16 eNumType = aFmt.GetNumberingType();

    sal_uInt16 nCount = maNumberSettingsArr.size();
    for(sal_uInt16 i = nFromIndex; i < nCount; ++i)
    {
        NumberSettings_Impl* _pSet = maNumberSettingsArr[i].get();
        sal_Int16 eNType = _pSet->pNumSetting->nNumberType;
        OUString sLocalPrefix = _pSet->pNumSetting->sPrefix;
        OUString sLocalSuffix = _pSet->pNumSetting->sSuffix;
        if (sPrefix == sLocalPrefix &&
            sLclSuffix == sLocalSuffix &&
            eNumType == eNType )
        {
            return i+1;
        }
    }


    return sal_uInt16(0xFFFF);
}

void NumberingTypeMgr::ReplaceNumRule(SvxNumRule& aNum, sal_uInt16 nIndex, sal_uInt16 mLevel)
{
    sal_uInt16 nActLv = IsSingleLevel(mLevel);

    if ( nActLv == sal_uInt16(0xFFFF) )
        return;

    const SvxNumberFormat& aFmt(aNum.GetLevel(nActLv));
    SvxNumType eNumType = aFmt.GetNumberingType();

    sal_uInt16 nCount = maNumberSettingsArr.size();
    if ( nIndex >= nCount )
        return;

    NumberSettings_Impl* _pSet = maNumberSettingsArr[nIndex].get();

    _pSet->pNumSetting->sPrefix = aFmt.GetPrefix();
    _pSet->pNumSetting->sSuffix = aFmt.GetSuffix();
    _pSet->pNumSetting->nNumberType = eNumType;
    _pSet->bIsCustomized = true;

    SvxNumRule aTmpRule1(aNum);
    SvxNumRule aTmpRule2(aNum);
    ApplyNumRule(aTmpRule1,nIndex,mLevel,true);
    ApplyNumRule(aTmpRule2,nIndex,mLevel);
    if (aTmpRule1==aTmpRule2) _pSet->bIsCustomized=false;
    if (!_pSet->bIsCustomized) {
        _pSet->sDescription = GetDescription(nIndex,true);
    }
    ImplStore(u"standard.syb");
}

void NumberingTypeMgr::ApplyNumRule(SvxNumRule& aNum, sal_uInt16 nIndex, sal_uInt16 mLevel, bool isDefault, bool isResetSize)
{
    if(maNumberSettingsArr.size() <= nIndex)
        return;
    NumberSettingsArr_Impl*     pCurrentNumberSettingsArr = &maNumberSettingsArr;
    if (isDefault) pCurrentNumberSettingsArr = &maDefaultNumberSettingsArr;
    NumberSettings_Impl* _pSet = (*pCurrentNumberSettingsArr)[nIndex].get();
    SvxNumType eNewType = _pSet->pNumSetting->nNumberType;

    sal_uInt16 nMask = 1;
    OUString sNumCharFmtName = GetNumCharFmtName();
    for(sal_uInt16 i = 0; i < aNum.GetLevelCount(); i++)
    {
        if(mLevel & nMask)
        {
            SvxNumberFormat aFmt(aNum.GetLevel(i));
            if (eNewType!=aFmt.GetNumberingType()) isResetSize=true;
            aFmt.SetNumberingType(eNewType);
            aFmt.SetListFormat(_pSet->pNumSetting->sPrefix, _pSet->pNumSetting->sSuffix, i);
            aFmt.SetCharFormatName(sNumCharFmtName);
            if (isResetSize) aFmt.SetBulletRelSize(100);
            aNum.SetLevel(i, aFmt);
        }
        nMask <<= 1 ;
    }
}

OUString NumberingTypeMgr::GetDescription(sal_uInt16 nIndex, bool isDefault)
{
    OUString sRet;
    sal_uInt16 nLength = maNumberSettingsArr.size();

    if ( nIndex >= nLength )
        return sRet;
    else
        sRet = maNumberSettingsArr[nIndex]->sDescription;
    if (isDefault) sRet = maDefaultNumberSettingsArr[nIndex]->sDescription;

    return sRet;
}

bool NumberingTypeMgr::IsCustomized(sal_uInt16 nIndex)
{
    bool bRet = false;
    sal_uInt16 nLength = maNumberSettingsArr.size();

    if ( nIndex >= nLength )
        bRet = false;
    else
        bRet = maNumberSettingsArr[nIndex]->bIsCustomized;

    return bRet;
}
// Multi-level /Outline Type lib
OutlineTypeMgr::OutlineTypeMgr()
{
    Init();
    for(sal_Int32 nItem = 0; nItem < DEFAULT_NUM_VALUSET_COUNT; nItem++ )
    {
        pDefaultOutlineSettingsArrs[nItem] = pOutlineSettingsArrs[nItem];
    }
    //Initial the first time to store the default value. Then do it again for customized value
    Init();
    ImplLoad(u"standard.syc");
}

OutlineTypeMgr& OutlineTypeMgr::GetInstance()
{
    static OutlineTypeMgr theOutlineTypeMgr;
    return theOutlineTypeMgr;
}

void OutlineTypeMgr::Init()
{
    const Reference< XComponentContext >& xContext = ::comphelper::getProcessComponentContext();
    Reference<XDefaultNumberingProvider> xDefNum = DefaultNumberingProvider::create( xContext );

    Sequence<Reference<XIndexAccess> > aOutlineAccess;
    Locale aLocale(Application::GetSettings().GetLanguageTag().getLocale());
    try
    {
        aOutlineAccess = xDefNum->getDefaultOutlineNumberings( aLocale );

        SvxNumRule aDefNumRule( SvxNumRuleFlags::BULLET_REL_SIZE | SvxNumRuleFlags::CONTINUOUS | SvxNumRuleFlags::BULLET_COLOR,
            10, false,
            SvxNumRuleType::NUMBERING, SvxNumberFormat::LABEL_ALIGNMENT);

        auto nSize = std::min<sal_Int32>(aOutlineAccess.getLength(), DEFAULT_NUM_VALUSET_COUNT);
        for(sal_Int32 nItem = 0; nItem < nSize; nItem++ )
        {
            pOutlineSettingsArrs[ nItem ] = new OutlineSettings_Impl;
            OutlineSettings_Impl* pItemArr = pOutlineSettingsArrs[ nItem ];
            OString id = OString::Concat(RID_SVXSTR_OUTLINENUM_DESCRIPTION_0.getId()) + OString::number(nItem);
            pItemArr->sDescription = SvxResId( TranslateId(RID_SVXSTR_OUTLINENUM_DESCRIPTION_0.mpContext, id.getStr()) );
            pItemArr->pNumSettingsArr = new NumSettingsArr_Impl;
            Reference<XIndexAccess> xLevel = aOutlineAccess.getConstArray()[nItem];
            for(sal_Int32 nLevel = 0; nLevel < SVX_MAX_NUM; nLevel++)
            {
                // use the last locale-defined level for all remaining levels.
                sal_Int32 nLocaleLevel = std::min(nLevel, xLevel->getCount() - 1);
                Sequence<PropertyValue> aLevelProps;
                if (nLocaleLevel >= 0)
                    xLevel->getByIndex(nLocaleLevel) >>= aLevelProps;

                NumSettings_Impl* pNew = lcl_CreateNumberingSettingsPtr(aLevelProps);
                const SvxNumberFormat& aNumFmt( aDefNumRule.GetLevel( nLevel) );
                assert(aNumFmt.GetNumAdjust() == SvxAdjust::Left && "new entry was previously defined by default, now defaults to Left");
                pNew->eLabelFollowedBy = aNumFmt.GetLabelFollowedBy();
                pNew->nTabValue = aNumFmt.GetListtabPos();
                if (pNew->eNumAlign == SvxAdjust::Right)
                    pNew->nNumAlignAt = -174; // number borrowed from RES_POOLNUMRULE_NUM4
                else
                    pNew->nNumAlignAt = aNumFmt.GetFirstLineIndent();
                pNew->nNumIndentAt = aNumFmt.GetIndentAt();
                pItemArr->pNumSettingsArr->push_back(std::shared_ptr<NumSettings_Impl>(pNew));
            }
        }
    }
    catch(Exception&)
    {
    }
}

sal_uInt16 OutlineTypeMgr::GetNBOIndexForNumRule(SvxNumRule& aNum,sal_uInt16 /*mLevel*/,sal_uInt16 nFromIndex)
{
    sal_uInt16 const nLength = SAL_N_ELEMENTS(pOutlineSettingsArrs);
    for(sal_uInt16 iDex = nFromIndex; iDex < nLength; iDex++)
    {
        bool bNotMatch = false;
        OutlineSettings_Impl* pItemArr = pOutlineSettingsArrs[iDex];
        sal_uInt16 nCount = pItemArr ? pItemArr->pNumSettingsArr->size() : 0;
        for (sal_uInt16 iLevel=0;iLevel < nCount;iLevel++)
        {
            NumSettings_Impl* _pSet = (*pItemArr->pNumSettingsArr)[iLevel].get();
            sal_Int16 eNType = _pSet->nNumberType;

            const SvxNumberFormat& aFmt(aNum.GetLevel(iLevel));
            const OUString& sPrefix = aFmt.GetPrefix();
            const OUString& sLclSuffix = aFmt.GetSuffix();
            sal_Int16 eNumType = aFmt.GetNumberingType();
            if( eNumType == SVX_NUM_CHAR_SPECIAL)
            {
                sal_UCS4 cChar = aFmt.GetBulletChar();

                sal_UCS4 ccChar
                    = _pSet->sBulletChar.isEmpty()
                          ? 0
                          : _pSet->sBulletChar.iterateCodePoints(&o3tl::temporary(sal_Int32(0)));

                if ( !((cChar == ccChar) &&
                    _pSet->eLabelFollowedBy == aFmt.GetLabelFollowedBy() &&
                    _pSet->nTabValue == aFmt.GetListtabPos() &&
                    _pSet->eNumAlign == aFmt.GetNumAdjust() &&
                    _pSet->nNumAlignAt == aFmt.GetFirstLineIndent() &&
                    _pSet->nNumIndentAt == aFmt.GetIndentAt()))
                {
                    bNotMatch = true;
                    break;
                }
            }
            else if ((eNumType&(~LINK_TOKEN)) == SVX_NUM_BITMAP )
            {
                const SvxBrushItem* pBrsh1 = aFmt.GetBrush();
                const SvxBrushItem* pBrsh2 = _pSet->pBrushItem;
                bool bIsMatch = false;
                if (SfxPoolItem::areSame(pBrsh1,pBrsh2)) bIsMatch = true;
                if (pBrsh1 && pBrsh2) {
                    const Graphic* pGrf1 = pBrsh1->GetGraphic();
                    const Graphic* pGrf2 = pBrsh2->GetGraphic();
                    if (pGrf1==pGrf2) bIsMatch = true;
                    if (pGrf1 && pGrf2) {
                        if ( pGrf1->GetBitmapEx() == pGrf2->GetBitmapEx() &&
                                _pSet->aSize == aFmt.GetGraphicSize())
                            bIsMatch = true;
                    }
                }
                if (!bIsMatch) {
                    bNotMatch = true;
                    break;
                }
            }
            else
            {
                if (!(sPrefix == _pSet->sPrefix &&
                      sLclSuffix == _pSet->sSuffix &&
                      eNumType == eNType &&
                      _pSet->eLabelFollowedBy == aFmt.GetLabelFollowedBy() &&
                      _pSet->nTabValue == aFmt.GetListtabPos() &&
                      _pSet->eNumAlign == aFmt.GetNumAdjust() &&
                      _pSet->nNumAlignAt == aFmt.GetFirstLineIndent() &&
                      _pSet->nNumIndentAt == aFmt.GetIndentAt()))
                {
                    bNotMatch = true;
                    break;
                }
            }
        }
        if ( !bNotMatch )
            return iDex+1;
    }


    return sal_uInt16(0xFFFF);
}

void OutlineTypeMgr::ReplaceNumRule(SvxNumRule& aNum, sal_uInt16 nIndex, sal_uInt16 mLevel)
{
    sal_uInt16 const nLength = SAL_N_ELEMENTS(pOutlineSettingsArrs);
    if ( nIndex >= nLength )
        return;

    OutlineSettings_Impl* pItemArr = pOutlineSettingsArrs[nIndex];
    sal_uInt16 nCount = pItemArr->pNumSettingsArr->size();
    for (sal_uInt16 iLevel=0;iLevel < nCount;iLevel++)
    {
        const SvxNumberFormat& aFmt(aNum.GetLevel(iLevel));
        SvxNumType eNumType = aFmt.GetNumberingType();

        NumSettings_Impl* _pSet = (*pItemArr->pNumSettingsArr)[iLevel].get();

        _pSet->eLabelFollowedBy = aFmt.GetLabelFollowedBy();
        _pSet->nTabValue = aFmt.GetListtabPos();
        _pSet->eNumAlign = aFmt.GetNumAdjust();
        _pSet->nNumAlignAt = aFmt.GetFirstLineIndent();
        _pSet->nNumIndentAt = aFmt.GetIndentAt();

        if( eNumType == SVX_NUM_CHAR_SPECIAL)
        {
            sal_UCS4 cChar = aFmt.GetBulletChar();
            _pSet->sBulletChar = OUString(&cChar, 1);
            if ( aFmt.GetBulletFont() )
                _pSet->sBulletFont = aFmt.GetBulletFont()->GetFamilyName();
            _pSet->nNumberType = eNumType;
            pItemArr->bIsCustomized = true;
        }else if ((eNumType&(~LINK_TOKEN)) == SVX_NUM_BITMAP ) {
            if (_pSet->pBrushItem) {
                delete _pSet->pBrushItem;
                _pSet->pBrushItem=nullptr;
            }
            if (aFmt.GetBrush())
                _pSet->pBrushItem = new SvxBrushItem(*aFmt.GetBrush());
            _pSet->aSize = aFmt.GetGraphicSize();
            _pSet->nNumberType = eNumType;
        } else
        {
            _pSet->sPrefix = aFmt.GetPrefix();
            _pSet->sSuffix = aFmt.GetSuffix();
            _pSet->nNumberType = eNumType;
            if ( aFmt.GetBulletFont() )
                _pSet->sBulletFont = aFmt.GetBulletFont()->GetFamilyName();
            pItemArr->bIsCustomized = true;
         }
    }
    SvxNumRule aTmpRule1(aNum);
    SvxNumRule aTmpRule2(aNum);
    ApplyNumRule(aTmpRule1,nIndex,mLevel,true);
    ApplyNumRule(aTmpRule2,nIndex,mLevel);
    if (aTmpRule1==aTmpRule2) pItemArr->bIsCustomized=false;
    if (!pItemArr->bIsCustomized) {
        pItemArr->sDescription = GetDescription(nIndex,true);
    }
    ImplStore(u"standard.syc");
}

void OutlineTypeMgr::ApplyNumRule(SvxNumRule& aNum, sal_uInt16 nIndex, sal_uInt16 /*mLevel*/, bool isDefault, bool isResetSize)
{
    DBG_ASSERT(DEFAULT_NUM_VALUSET_COUNT > nIndex, "wrong index");
    if(DEFAULT_NUM_VALUSET_COUNT <= nIndex)
        return;

    const FontList* pList = nullptr;

    OutlineSettings_Impl* pItemArr = pOutlineSettingsArrs[nIndex];
    if (isDefault) pItemArr=pDefaultOutlineSettingsArrs[nIndex];

    NumSettingsArr_Impl *pNumSettingsArr=pItemArr->pNumSettingsArr;

    NumSettings_Impl* pLevelSettings = nullptr;
    for(sal_uInt16 i = 0; i < aNum.GetLevelCount(); i++)
    {
        if(pNumSettingsArr->size() > i)
            pLevelSettings = (*pNumSettingsArr)[i].get();

        if(!pLevelSettings)
            break;

        SvxNumberFormat aFmt(aNum.GetLevel(i));
        const vcl::Font& rActBulletFont = lcl_GetDefaultBulletFont();
        if (pLevelSettings->nNumberType !=aFmt.GetNumberingType()) isResetSize=true;
        aFmt.SetNumberingType( pLevelSettings->nNumberType );
        sal_uInt16 nUpperLevelOrChar = static_cast<sal_uInt16>(pLevelSettings->nParentNumbering);
        if(aFmt.GetNumberingType() == SVX_NUM_CHAR_SPECIAL)
        {
            if( pLevelSettings->sBulletFont.getLength() &&
                pLevelSettings->sBulletFont != rActBulletFont.GetFamilyName() )
            {
                //search for the font
                if(!pList)
                {
                    if (SfxObjectShell* pCurDocShell = SfxObjectShell::Current())
                    {
                        const SvxFontListItem* pFontListItem = static_cast<const SvxFontListItem*>(pCurDocShell->GetItem(SID_ATTR_CHAR_FONTLIST));
                        pList = pFontListItem ? pFontListItem->GetFontList() : nullptr;
                    }
                }
                if(pList && pList->IsAvailable( pLevelSettings->sBulletFont ) )
                {
                    vcl::Font aFont(pList->Get(pLevelSettings->sBulletFont,WEIGHT_NORMAL, ITALIC_NONE));
                    aFmt.SetBulletFont(&aFont);
                }
                else
                {
                    //if it cannot be found then create a new one
                    vcl::Font aCreateFont( pLevelSettings->sBulletFont, OUString(), Size( 0, 14 ) );
                    aCreateFont.SetCharSet( RTL_TEXTENCODING_DONTKNOW );
                    aCreateFont.SetFamily( FAMILY_DONTKNOW );
                    aCreateFont.SetPitch( PITCH_DONTKNOW );
                    aCreateFont.SetWeight( WEIGHT_DONTKNOW );
                    aCreateFont.SetTransparent( true );
                    aFmt.SetBulletFont( &aCreateFont );
                }
            }else
                aFmt.SetBulletFont( &rActBulletFont );

            sal_UCS4 cChar = 0;
            if( !pLevelSettings->sBulletChar.isEmpty() )
            {
                cChar
                    = pLevelSettings->sBulletChar.iterateCodePoints(&o3tl::temporary(sal_Int32(0)));
            }
            if( AllSettings::GetLayoutRTL() )
            {
                if( 0 == i && cChar == BulletsTypeMgr::aDynamicBulletTypes[5] )
                    cChar = BulletsTypeMgr::aDynamicRTLBulletTypes[5];
                else if( 1 == i )
                {
                    const SvxNumberFormat& numberFmt = aNum.GetLevel(0);
                    if( numberFmt.GetBulletChar() == BulletsTypeMgr::aDynamicRTLBulletTypes[5] )
                        cChar = BulletsTypeMgr::aDynamicRTLBulletTypes[4];
                }
            }

            aFmt.SetBulletChar(cChar);
            aFmt.SetCharFormatName( GetBulletCharFmtName() );
            if (isResetSize) aFmt.SetBulletRelSize(45);
        }else if ((aFmt.GetNumberingType()&(~LINK_TOKEN)) == SVX_NUM_BITMAP ) {
            if (pLevelSettings->pBrushItem) {
                    const Graphic* pGrf = pLevelSettings->pBrushItem->GetGraphic();
                    Size aSize = pLevelSettings->aSize;
                    sal_Int16 eOrient = text::VertOrientation::LINE_CENTER;
                    if (!isResetSize  && aFmt.GetGraphicSize()!=Size(0,0))
                        aSize = aFmt.GetGraphicSize();
                    else if (aSize.IsEmpty() && pGrf)
                        aSize = SvxNumberFormat::GetGraphicSizeMM100( pGrf );
                    aSize = OutputDevice::LogicToLogic(aSize, MapMode(MapUnit::Map100thMM), MapMode(GetMapUnit()));
                    aFmt.SetGraphicBrush( pLevelSettings->pBrushItem, &aSize, &eOrient );
            }
        } else
        {
            aFmt.SetIncludeUpperLevels(sal::static_int_cast< sal_uInt8 >(0 != nUpperLevelOrChar ? aNum.GetLevelCount() : 1));
            aFmt.SetCharFormatName(GetNumCharFmtName());
            if (isResetSize) aFmt.SetBulletRelSize(100);
        }
        if(pNumSettingsArr->size() > i) {
            aFmt.SetLabelFollowedBy(pLevelSettings->eLabelFollowedBy);
            aFmt.SetListtabPos(pLevelSettings->nTabValue);
            aFmt.SetNumAdjust(pLevelSettings->eNumAlign);
            aFmt.SetFirstLineIndent(pLevelSettings->nNumAlignAt);
            aFmt.SetIndentAt(pLevelSettings->nNumIndentAt);
        }
        aFmt.SetListFormat(pLevelSettings->sPrefix, pLevelSettings->sSuffix, i);
        aNum.SetLevel(i, aFmt);
    }
}

OUString OutlineTypeMgr::GetDescription(sal_uInt16 nIndex, bool isDefault)
{
    OUString sRet;

    if ( nIndex >= SAL_N_ELEMENTS(pOutlineSettingsArrs) )
        return sRet;
    else
    {
        OutlineSettings_Impl* pItemArr = pOutlineSettingsArrs[nIndex];
        if (isDefault) pItemArr = pDefaultOutlineSettingsArrs[nIndex];
        if ( pItemArr )
        {
            sRet = pItemArr->sDescription;
        }
    }
    return sRet;
}

bool OutlineTypeMgr::IsCustomized(sal_uInt16 nIndex)
{
    bool bRet = false;

    if ( nIndex >= SAL_N_ELEMENTS(pOutlineSettingsArrs) )
        return bRet;
    else
    {
        OutlineSettings_Impl* pItemArr = pOutlineSettingsArrs[nIndex];
        if ( pItemArr )
        {
            bRet = pItemArr->bIsCustomized;
        }
    }

    return bRet;
}


}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
