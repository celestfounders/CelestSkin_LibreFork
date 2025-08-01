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

#ifndef INCLUDED_REPORTDESIGN_SOURCE_FILTER_XML_XMLEXPORT_HXX
#define INCLUDED_REPORTDESIGN_SOURCE_FILTER_XML_XMLEXPORT_HXX

#include "ReportDefinition.hxx"

#include <com/sun/star/container/XNamed.hpp>
#include <com/sun/star/document/XFilter.hpp>
#include <com/sun/star/document/XImporter.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/report/XImageControl.hpp>
#include <com/sun/star/report/XReportDefinition.hpp>
#include <com/sun/star/report/XSection.hpp>
#include <com/sun/star/report/XReportControlModel.hpp>
#include <com/sun/star/report/XFormattedField.hpp>
#include <com/sun/star/lang/XComponent.hpp>
#include <xmloff/xmlexp.hxx>
#include <xmloff/xmlexppr.hxx>
#include <xmloff/prhdlfac.hxx>
#include <xmloff/xmlprmap.hxx>
#include <com/sun/star/uno/XComponentContext.hpp>
#include <memory>

namespace rptxml
{
using namespace ::xmloff::token;
using namespace css::uno;
using namespace css::container;
using namespace css::lang;
using namespace css::beans;
using namespace css::report;


class ORptExport : public SvXMLExport
{
public:
    struct TCell
    {
        sal_Int32 nColSpan;
        sal_Int32 nRowSpan;
        Reference<XReportComponent> xElement;
        bool      bSet;
        TCell(  sal_Int32 _nColSpan,
                sal_Int32 _nRowSpan,
                Reference<XReportComponent> const & _xElement = Reference<XReportComponent>()) :
        nColSpan(_nColSpan)
        ,nRowSpan(_nRowSpan)
        ,xElement(_xElement)
        ,bSet(xElement.is())
        {}

        TCell( ) :
        nColSpan(1)
        ,nRowSpan(1)
        ,bSet(true)
        {}
    };
    typedef ::std::pair< OUString ,OUString> TStringPair;
    typedef ::std::map< Reference<XPropertySet> ,OUString >  TPropertyStyleMap;
    typedef ::std::map< Reference<XPropertySet> ,  std::vector<OUString>>      TGridStyleMap;
    typedef ::std::vector< TCell >                                  TRow;
    typedef ::std::vector< ::std::pair< bool, TRow > >              TGrid;
    typedef ::std::map< Reference<XPropertySet> ,TGrid >            TSectionsGrid;
    typedef ::std::map< Reference<XGroup> ,Reference<XFunction> >   TGroupFunctionMap;
private:
    TSectionsGrid                                   m_aSectionsGrid;

    TPropertyStyleMap                               m_aAutoStyleNames;
    TGridStyleMap                                   m_aColumnStyleNames;
    TGridStyleMap                                   m_aRowStyleNames;
    TGroupFunctionMap                               m_aGroupFunctionMap;

    OUString                                 m_sTableStyle;
    OUString                                 m_sCellStyle;
    rtl::Reference < SvXMLExportPropertyMapper>       m_xTableStylesExportPropertySetMapper;
    rtl::Reference < SvXMLExportPropertyMapper>       m_xCellStylesExportPropertySetMapper;
    rtl::Reference < SvXMLExportPropertyMapper>       m_xColumnStylesExportPropertySetMapper;
    rtl::Reference < SvXMLExportPropertyMapper>       m_xRowStylesExportPropertySetMapper;
    rtl::Reference < SvXMLExportPropertyMapper >      m_xParaPropMapper;
    rtl::Reference < XMLPropertyHandlerFactory >      m_xPropHdlFactory;

    rtl::Reference<XMLPropertySetMapper> m_xCellStylesPropertySetMapper;
    rtl::Reference<reportdesign::OReportDefinition> m_pReportDefinition;
    bool                                        m_bAllreadyFilled;

    void                    exportReport(const Reference<XReportDefinition>& _xReportDefinition); /// <element name="office:report">
    void                    exportReportAttributes(const Reference<XReportDefinition>& _xReport);
    void                    exportFunctions(const Reference<XIndexAccess>& _xFunctions); /// <ref name="rpt-function"/>
    void                    exportFunction(const Reference< XFunction>& _xFunction);
    void                    exportMasterDetailFields(const Reference<XReportComponent>& _xReportComponent);
    void                    exportComponent(const Reference<XReportComponent>& _xReportComponent);
    void                    exportGroup(const Reference<XReportDefinition>& _xReportDefinition,sal_Int32 _nPos,bool _bExportAutoStyle = false);
    void                    exportStyleName(XPropertySet* _xProp,comphelper::AttributeList& _rAtt,const OUString& _sName);
    void                    exportSection(const Reference<XSection>& _xProp,bool bHeader = false);
    void                    exportContainer(const Reference< XSection>& _xSection);
    void                    exportShapes(const Reference< XSection>& _xSection,bool _bAddParagraph = true);
    void                    exportTableColumns(const Reference< XSection>& _xSection);
    void                    exportSectionAutoStyle(const Reference<XSection>& _xProp);
    void                    exportReportElement(const Reference<XReportControlModel>& _xReportElement);
    void                    exportFormatConditions(const Reference<XReportControlModel>& _xReportElement);
    void                    exportAutoStyle(XPropertySet* _xProp,const Reference<XFormattedField>& _xParentFormattedField = Reference<XFormattedField>());
    void                    exportAutoStyle(const Reference<XSection>& _xProp);
    void                    exportReportComponentAutoStyles(const Reference<XSection>& _xProp);
    void                    collectComponentStyles();
    void                    collectStyleNames(XmlStyleFamily _nFamily,const ::std::vector< sal_Int32>& _aSize, std::vector<OUString>& _rStyleNames);
    void                    collectStyleNames(XmlStyleFamily _nFamily,const ::std::vector< sal_Int32>& _aSize, const ::std::vector< sal_Int32>& _aSizeAutoGrow, std::vector<OUString>& _rStyleNames);
    void                    exportParagraph(const Reference< XReportControlModel >& _xReportElement);
    bool                    exportFormula(enum ::xmloff::token::XMLTokenEnum eName,const OUString& _sFormula);
    void                    exportGroupsExpressionAsFunction(const Reference< XGroups>& _xGroups);
    static OUString  convertFormula(const OUString& _sFormula);
    void                    handleTextElement(const Reference<XServiceInfo>& xElement, bool bShapeHandled,
                                              const Reference< XSection>& _xSection);
    void                    handleNumberFormat(const Reference<XFormattedField>& xFormattedField);
    void                    handleEToken(const Reference<XReportControlModel>& xReportElement,
                                         const Reference<XReportDefinition>& xReportDefinition, const Reference<XSection>& xSection,
                                         const XMLTokenEnum& eToken);

    virtual void                    SetBodyAttributes() override;

protected:

    virtual void                    ExportStyles_( bool bUsed ) override;
    virtual void                    ExportAutoStyles_() override;
    virtual void                    ExportContent_() override;
    virtual void                    ExportMasterStyles_() override;
    virtual void                    ExportFontDecls_() override;
    virtual SvXMLAutoStylePoolP*    CreateAutoStylePool() override;
    virtual XMLShapeExport*         CreateShapeExport() override;

    virtual                 ~ORptExport() override {};
public:

    ORptExport(const Reference< XComponentContext >& _rxContext, OUString const & implementationName, SvXMLExportFlags nExportFlag);

    // XExporter
    virtual void SAL_CALL setSourceDocument( const css::uno::Reference< css::lang::XComponent >& xDoc ) override;

    const rtl::Reference < XMLPropertySetMapper >& GetCellStylePropertyMapper() const { return m_xCellStylesPropertySetMapper;}

    // Helper methods to create exporters
    static rtl::Reference<ORptExport>
    createSettingsExporter(const Reference<XComponentContext>& rxContext);
    static rtl::Reference<ORptExport>
    createStylesExporter(const Reference<XComponentContext>& rxContext);
    static rtl::Reference<ORptExport>
    createMetaExporter(const Reference<XComponentContext>& rxContext);
    static rtl::Reference<ORptExport>
    createExportFilter(const Reference<XComponentContext>& rxContext);
};


} // rptxml

#endif // INCLUDED_REPORTDESIGN_SOURCE_FILTER_XML_XMLEXPORT_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
