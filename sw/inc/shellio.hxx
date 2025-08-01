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
#ifndef INCLUDED_SW_INC_SHELLIO_HXX
#define INCLUDED_SW_INC_SHELLIO_HXX

#include <memory>

#include <com/sun/star/uno/Reference.h>
#include <sot/storage.hxx>
#include <tools/date.hxx>
#include <tools/time.hxx>
#include <tools/datetime.hxx>
#include <tools/ref.hxx>
#include <rtl/ref.hxx>
#include <osl/thread.h>
#include <o3tl/deleter.hxx>
#include <o3tl/typed_flags_set.hxx>
#include "swdllapi.h"
#include "docfac.hxx"
#include "unocrsr.hxx"

class SfxItemPool;
class SfxItemSet;
class SfxMedium;
class SvStream;
class SvxFontItem;
class SvxMacroTableDtor;
class SwContentNode;
class SwCursorShell;
class SwDoc;
class SwPaM;
class SwTextBlocks;
struct SwPosition;
struct Writer_Impl;
namespace sw::mark { class MarkBase; }
namespace com::sun::star::embed { class XStorage; }

// Defines the count of chars at which a paragraph read via ASCII/W4W-Reader
// is forced to wrap. It has to be always greater than 200!!!
// Note: since version 4.3, maximum character count changed to 4 GiB from 64 KiB
// in a paragraph, but because of the other limitations, we set a lower wrap value
// to get a working text editor e.g. without freezing and crash during loading of
// a 50 MB text line, or unusably slow editing of a 5 MB text line.
#define MAX_ASCII_PARA 250000

class SW_DLLPUBLIC SwAsciiOptions
{
    OUString m_sFont;
    rtl_TextEncoding m_eCharSet;
    LanguageType m_nLanguage;
    LineEnd m_eCRLF_Flag;
    bool m_bIncludeBOM;   // Whether to include a byte-order-mark in the output.
    bool m_bIncludeHidden; // Whether to include hidden paragraphs and text.

public:

    const OUString& GetFontName() const { return m_sFont; }
    void SetFontName( const OUString& rFont ) { m_sFont = rFont; }

    rtl_TextEncoding GetCharSet() const { return m_eCharSet; }
    void SetCharSet( rtl_TextEncoding nVal ) { m_eCharSet = nVal; }

    LanguageType GetLanguage() const { return m_nLanguage; }
    void SetLanguage( LanguageType nVal ) { m_nLanguage = nVal; }

    LineEnd GetParaFlags() const { return m_eCRLF_Flag; }
    void SetParaFlags( LineEnd eVal ) { m_eCRLF_Flag = eVal; }

    bool GetIncludeBOM() const { return m_bIncludeBOM; }
    void SetIncludeBOM( bool bVal ) { m_bIncludeBOM = bVal; }

    bool GetIncludeHidden() const { return m_bIncludeHidden; }
    void SetIncludeHidden( bool bVal ) { m_bIncludeHidden = bVal; }

    void Reset();

    // for the automatic conversion (mail/news/...)
    void ReadUserData( std::u16string_view );
    void WriteUserData( OUString& ) const;

    SwAsciiOptions() { Reset(); }
};

// Base class of possible options for a special reader.
class Reader;

class SwgReaderOption
{
    SwAsciiOptions m_aASCIIOpts;
    bool m_bFrameFormats;
    bool m_bPageDescs;
    bool m_bTextFormats;
    bool m_bNumRules;
    bool m_bMerge;
    css::uno::Reference<css::io::XInputStream> m_xInputStream;
public:
    void ResetAllFormatsOnly() { m_bFrameFormats = m_bPageDescs = m_bTextFormats = m_bNumRules = m_bMerge = false; }
    bool IsFormatsOnly() const { return m_bFrameFormats || m_bPageDescs || m_bTextFormats || m_bNumRules || m_bMerge; }

    bool IsFrameFormats() const { return m_bFrameFormats; }
    void SetFrameFormats( const bool bNew) { m_bFrameFormats = bNew; }

    bool IsPageDescs() const { return m_bPageDescs; }
    void SetPageDescs( const bool bNew) { m_bPageDescs = bNew; }

    bool IsTextFormats() const { return m_bTextFormats; }
    void SetTextFormats( const bool bNew) { m_bTextFormats = bNew; }

    bool IsNumRules() const { return m_bNumRules; }
    void SetNumRules( const bool bNew) { m_bNumRules = bNew; }

    bool IsMerge() const { return m_bMerge; }
    void SetMerge( const bool bNew ) { m_bMerge = bNew; }

    const SwAsciiOptions& GetASCIIOpts() const { return m_aASCIIOpts; }
    void SetASCIIOpts( const SwAsciiOptions& rOpts ) { m_aASCIIOpts = rOpts; }
    void ResetASCIIOpts() { m_aASCIIOpts.Reset(); }

    css::uno::Reference<css::io::XInputStream>& GetInputStream() { return m_xInputStream; }
    void SetInputStream(const css::uno::Reference<css::io::XInputStream>& xInputStream)
    {
        m_xInputStream = xInputStream;
    }

    SwgReaderOption()
        { ResetAllFormatsOnly(); m_aASCIIOpts.Reset(); }
};

// Calls reader with its options, document, cursor etc.
class SW_DLLPUBLIC SwReader: public SwDocFac
{
    SvStream* mpStrm;
    rtl::Reference<SotStorage> mpStg;
    css::uno::Reference < css::embed::XStorage > mxStg;
    SfxMedium* mpMedium;     // Who wants to obtain a Medium (W4W).

    SwPaM* mpCursor;
    OUString maFileName;
    OUString msBaseURL;
    bool mbSkipImages;
    bool mbSkipInvalidateNumRules = false;
    bool mbIsInPaste = false;

public:

    // Initial reading. Document is created only at Read(...)
    // or in case it is given, into that.
    // Special case for Load with Sw3Reader.
    SwReader( SfxMedium&, OUString aFilename, SwDoc *pDoc = nullptr );

    // Read into existing document.
    // Document and position in document are taken from SwPaM.
    SwReader( SvStream&, OUString aFilename, const OUString& rBaseURL, SwPaM& );
    SwReader( SfxMedium&, OUString aFilename, SwPaM& );
    SwReader( css::uno::Reference < css::embed::XStorage > , OUString aFilename, SwPaM& );

    // The only export interface is SwReader::Read(...)!!!
    ErrCodeMsg Read( const Reader& );

    // Ask for glossaries.
    bool HasGlossaries( const Reader& );
    bool ReadGlossaries( const Reader&, SwTextBlocks&, bool bSaveRelFiles );

    void SetSkipInvalidateNumRules(bool bSkipInvalidateNumRules)
    {
        mbSkipInvalidateNumRules = bSkipInvalidateNumRules;
    }
    void SetInPaste(bool val) { mbIsInPaste = val; }
    bool IsInPaste() const { return mbIsInPaste; }

protected:
    void                SetBaseURL( const OUString& rURL ) { msBaseURL = rURL; }
    void                SetSkipImages( bool bSkipImages ) { mbSkipImages = bSkipImages; }
};

typedef std::unique_ptr<SwReader, o3tl::default_delete<SwReader>> SwReaderPtr;

// Special Readers can be both!! (Excel, W4W, .. ).
enum class SwReaderType {
    NONE = 0x00,
    Stream = 0x01,
    Storage = 0x02
};
namespace o3tl {
    template<> struct typed_flags<SwReaderType> : is_typed_flags<SwReaderType, 0x03> {};
};

extern "C" SAL_DLLPUBLIC_EXPORT bool TestImportDOC(SvStream &rStream, const OUString &rFltName);
extern "C" SAL_DLLPUBLIC_EXPORT bool TestImportDOCX(SvStream &rStream);
extern "C" SAL_DLLPUBLIC_EXPORT bool TestImportRTF(SvStream &rStream);
extern "C" SAL_DLLPUBLIC_EXPORT bool TestPDFExportRTF(SvStream &rStream);
extern "C" SAL_DLLPUBLIC_EXPORT bool TestImportHTML(SvStream &rStream);
SAL_DLLPUBLIC_EXPORT void FlushFontCache();

class SW_DLLPUBLIC Reader
{
    friend class SwReader;
    friend bool TestImportDOC(SvStream &rStream, const OUString &rFltName);
    friend bool TestImportHTML(SvStream &rStream);
    rtl::Reference<SwDoc> mxTemplate;
    OUString m_aTemplateName;

    Date m_aDateStamp;
    tools::Time m_aTimeStamp;
    DateTime m_aCheckDateTime;

protected:
    SvStream* m_pStream;
    rtl::Reference<SotStorage> m_pStorage;
    css::uno::Reference < css::embed::XStorage > m_xStorage;
    SfxMedium* m_pMedium;     // Who wants to obtain a Medium (W4W).

    SwgReaderOption m_aOption;
    bool m_bInsertMode : 1;
    bool m_bTemplateBrowseMode : 1;
    bool m_bReadUTF8: 1;      // Interpret stream as UTF-8.
    bool m_bBlockMode: 1;
    bool m_bOrganizerMode : 1;
    bool m_bHasAskTemplateName : 1;
    bool m_bIgnoreHTMLComments : 1;
    bool m_bSkipImages : 1;
    bool m_bIsInPaste : 1 = false;

    virtual OUString GetTemplateName(SwDoc& rDoc) const;

public:
    Reader();
    virtual ~Reader();

    virtual SwReaderType GetReaderType();
    SwgReaderOption& GetReaderOpt() { return m_aOption; }

    virtual void SetFltName( const OUString& rFltNm );

    // Adapt item-set of a Frame-Format to the old format.
    static void ResetFrameFormatAttrs( SfxItemSet &rFrameSet );

    // Adapt Frame-/Graphics-/OLE- styles to the old format
    // (without borders etc.).
    static void ResetFrameFormats( SwDoc& rDoc );

    // Load filter template, set it and release it again.
    SwDoc* GetTemplateDoc(SwDoc& rDoc);
    bool SetTemplate( SwDoc& rDoc );
    void ClearTemplate();
    void SetTemplateName( const OUString& rDir );
    void MakeHTMLDummyTemplateDoc();

    bool IsReadUTF8() const { return m_bReadUTF8; }
    void SetReadUTF8( bool bSet ) { m_bReadUTF8 = bSet; }

    bool IsBlockMode() const { return m_bBlockMode; }
    void SetBlockMode( bool bSet ) { m_bBlockMode = bSet; }

    bool IsOrganizerMode() const { return m_bOrganizerMode; }
    void SetOrganizerMode( bool bSet ) { m_bOrganizerMode = bSet; }

    void SetIgnoreHTMLComments( bool bSet ) { m_bIgnoreHTMLComments = bSet; }

    bool IsInPaste() const { return m_bIsInPaste; }
    void SetInPaste(bool bSet) { m_bIsInPaste = bSet; }

    virtual bool HasGlossaries() const;
    virtual bool ReadGlossaries( SwTextBlocks&, bool bSaveRelFiles ) const;

    // Read the sections of the document, which is equal to the medium.
    // Returns the count of it
    virtual size_t GetSectionList( SfxMedium& rMedium,
                                   std::vector<OUString>& rStrings) const;

    const rtl::Reference<SotStorage>& getSotStorageRef() const { return m_pStorage; };
    void setSotStorageRef(const rtl::Reference<SotStorage>& pStgRef) { m_pStorage = pStgRef; };

private:
    virtual ErrCodeMsg Read(SwDoc &, const OUString& rBaseURL, SwPaM &, const OUString &)=0;

    // Everyone who does not need the streams / storages open
    // has to override the method (W4W!!).
    virtual bool SetStrmStgPtr();
};

class AsciiReader final : public Reader
{
    friend class SwReader;
    virtual ErrCodeMsg Read( SwDoc &, const OUString& rBaseURL, SwPaM &, const OUString &) override;
public:
    AsciiReader(): Reader() {}
};

class MarkdownReader final : public Reader
{
    friend class SwReader;
    virtual ErrCodeMsg Read( SwDoc &, const OUString& rBaseURL, SwPaM &, const OUString &) override;
public:
    MarkdownReader(): Reader() {}
};

class SW_DLLPUBLIC StgReader : public Reader
{
    OUString m_aFltName;

public:
    virtual SwReaderType GetReaderType() override;
    const OUString& GetFltName() const { return m_aFltName; }
    virtual void SetFltName( const OUString& r ) override;
};

// The given stream has to be created dynamically and must
// be requested via Stream() before the instance is deleted!

class SwImpBlocks;

/// Used for autotext handling. The list of autotexts get imported to a temporary document, and then
/// this class provides the actual glossary document which is the final target for autotext.
class SW_DLLPUBLIC SwTextBlocks
{
    std::unique_ptr<SwImpBlocks> m_pImp;
    ErrCode        m_nErr;

public:
    SwTextBlocks( const OUString& );
    ~SwTextBlocks();

    SwDoc* GetDoc();
    void   ClearDoc();                  // Delete Doc-contents.
    OUString GetName() const;
    void   SetName( const OUString& );
    ErrCode const & GetError() const { return m_nErr; }

    const OUString & GetBaseURL() const;
    void   SetBaseURL( const OUString& rURL );

    sal_uInt16 GetCount() const;                        // Get count text modules.
    sal_uInt16 GetIndex( const OUString& ) const;       // Get index of short names.
    sal_uInt16 GetLongIndex( std::u16string_view ) const;   // Get index of long names.
    const OUString & GetShortName( sal_uInt16 ) const;          // Get short name for index.
    const OUString & GetLongName( sal_uInt16 ) const;           // Get long name for index.

    bool   Delete( sal_uInt16 );
    void   Rename( sal_uInt16, const OUString*, const OUString* );
    ErrCode const & CopyBlock( SwTextBlocks const & rSource, OUString& rSrcShort,
                                    const OUString& rLong );

    bool   BeginGetDoc( sal_uInt16 );   // Read text modules.
    void   EndGetDoc();                     // Release text modules.

    bool   BeginPutDoc( const OUString&, const OUString& ); // Begin save.
    sal_uInt16 PutDoc();                                    // End save.

    sal_uInt16 PutText( const OUString&, const OUString&, const OUString& ); // Save (short name, text).

    bool IsOnlyTextBlock( sal_uInt16 ) const;
    bool IsOnlyTextBlock( const OUString& rShort ) const;

    OUString const & GetFileName() const;           // Filename of pImp.
    bool IsReadOnly() const;            // ReadOnly-flag of pImp.

    bool GetMacroTable( sal_uInt16 nIdx, SvxMacroTableDtor& rMacroTable );
    bool SetMacroTable( sal_uInt16 nIdx, const SvxMacroTableDtor& rMacroTable );

    bool StartPutMuchBlockEntries();
    void EndPutMuchBlockEntries();
};

// BEGIN source/filter/basflt/fltini.cxx

extern Reader *ReadAscii, *ReadHTML, *ReadXML, *ReadMarkdown;

SW_DLLPUBLIC Reader* SwGetReaderXML();

// END source/filter/basflt/fltini.cxx

extern bool SetHTMLTemplate( SwDoc &rDoc ); //For templates from HTML before loading shellio.cxx.

// Base-class of all writers.

class IDocumentSettingAccess;
class IDocumentStylePoolAccess;

class SW_DLLPUBLIC Writer
    : public SvRefBase
{
    SwAsciiOptions m_aAsciiOptions;
    OUString       m_sBaseURL;

    void AddFontItem( SfxItemPool& rPool, const SvxFontItem& rFont );
    void AddFontItems_( SfxItemPool& rPool, TypedWhichId<SvxFontItem> nWhichId );

    std::unique_ptr<Writer_Impl> m_pImpl;

    Writer(Writer const&) = delete;
    Writer& operator=(Writer const&) = delete;

protected:

    const OUString* m_pOrigFileName;

    void ResetWriter();
    bool CopyNextPam( SwPaM ** );

    void PutNumFormatFontsInAttrPool();
    void PutEditEngFontsInAttrPool();

    virtual ErrCode WriteStream() = 0;
    void                SetBaseURL( const OUString& rURL ) { m_sBaseURL = rURL; }

    IDocumentSettingAccess& getIDocumentSettingAccess();
    const IDocumentSettingAccess& getIDocumentSettingAccess() const;

    IDocumentStylePoolAccess& getIDocumentStylePoolAccess();
    const IDocumentStylePoolAccess& getIDocumentStylePoolAccess() const;

public:
    SwDoc* m_pDoc;
    SwPaM* m_pOrigPam;            // Last Pam that has to be processed.
    std::shared_ptr<SwUnoCursor> m_pCurrentPam;
    bool m_bWriteAll : 1;
    bool m_bShowProgress : 1;
    bool m_bWriteClipboardDoc : 1;
    bool m_bWriteOnlyFirstTable : 1;
    bool m_bASCII_ParaAsCR : 1;
    bool m_bASCII_ParaAsBlank : 1;
    bool m_bASCII_NoLastLineEnd : 1;
    bool m_bUCS2_WithStartChar : 1;
    bool m_bExportParagraphNumbering : 1;

    bool m_bBlock : 1;
    bool m_bOrganizerMode : 1;
    bool m_bHideDeleteRedlines : 1;

    Writer();
    virtual ~Writer() override;

    virtual ErrCodeMsg Write( SwPaM&, SfxMedium&, const OUString* );
            ErrCodeMsg Write( SwPaM&, SvStream&,  const OUString* );
    virtual ErrCodeMsg Write( SwPaM&, const css::uno::Reference < css::embed::XStorage >&, const OUString*, SfxMedium* = nullptr );
    virtual ErrCodeMsg Write( SwPaM&, SotStorage&, const OUString* );

    virtual void SetupFilterOptions(SfxMedium& rMedium);

    virtual bool IsStgWriter() const;

    void SetShowProgress( bool bFlag )  { m_bShowProgress = bFlag; }

    const OUString* GetOrigFileName() const       { return m_pOrigFileName; }

    const SwAsciiOptions& GetAsciiOptions() const { return m_aAsciiOptions; }
    void SetAsciiOptions( const SwAsciiOptions& rOpt ) { m_aAsciiOptions = rOpt; }

    const OUString& GetBaseURL() const { return m_sBaseURL;}

    // Look up next bookmark position from bookmark-table.
    sal_Int32 FindPos_Bkmk( const SwPosition& rPos ) const;
    // Build a bookmark table, which is sort by the node position. The
    // OtherPos of the bookmarks also inserted.
    void CreateBookmarkTable();
    // Search all Bookmarks in the range and return it in the Array.
    bool GetBookmarks( const SwContentNode& rNd,
                        sal_Int32 nStt, sal_Int32 nEnd,
                        std::vector< const ::sw::mark::MarkBase* >& rArr );

    // Create new PaM at position.
    static std::shared_ptr<SwUnoCursor> NewUnoCursor(SwDoc & rDoc,
                            SwNodeOffset const nStartIdx, SwNodeOffset const nEndIdx);

    // If applicable copy a local file into internet.
    bool CopyLocalFileToINet( OUString& rFileNm );

    // Stream-specific routines. Do not use in storage-writer!

    void SetStream(SvStream *const pStream);
    SvStream& Strm();

    void SetOrganizerMode( bool bSet ) { m_bOrganizerMode = bSet; }
};

typedef tools::SvRef<Writer> WriterRef;

// Base class for all storage writers.
class SW_DLLPUBLIC StgWriter : public Writer
{
protected:
    rtl::Reference<SotStorage> m_pStg;
    css::uno::Reference < css::embed::XStorage > m_xStg;

    // Create error at call.
    virtual ErrCode WriteStream() override;
    virtual ErrCodeMsg WriteStorage() = 0;
    virtual ErrCodeMsg WriteMedium( SfxMedium& ) = 0;

    using Writer::Write;

public:
    StgWriter() : Writer() {}

    virtual bool IsStgWriter() const override;

    virtual ErrCodeMsg Write( SwPaM&, const css::uno::Reference < css::embed::XStorage >&, const OUString*, SfxMedium* = nullptr ) override;
    virtual ErrCodeMsg Write( SwPaM&, SotStorage&, const OUString* ) override;

    SotStorage& GetStorage() const       { return *m_pStg; }
};

// Interface class for general access on special writers.

class SW_DLLPUBLIC SwWriter
{
    SvStream* m_pStrm;
    css::uno::Reference < css::embed::XStorage > m_xStg;
    SfxMedium* m_pMedium;

    SwPaM* m_pOutPam;
    SwCursorShell *m_pShell;
    SwDoc &m_rDoc;

    bool m_bWriteAll;

public:
    ErrCodeMsg Write( WriterRef const & rxWriter, const OUString* = nullptr);

    SwWriter( SvStream&, SwCursorShell &, bool bWriteAll = false );
    SwWriter( SvStream&, SwDoc & );
    SwWriter( SvStream&, SwPaM &, bool bWriteAll = false );

    SwWriter( css::uno::Reference < css::embed::XStorage > , SwDoc& );

    SwWriter( SfxMedium&, SwCursorShell &, bool bWriteAll );
    SwWriter( SfxMedium&, SwDoc & );
};

typedef Reader* (*FnGetReader)();
typedef void (*FnGetWriter)(std::u16string_view, const OUString& rBaseURL, WriterRef&);
ErrCode SaveOrDelMSVBAStorage( SfxObjectShell&, SotStorage&, bool, const OUString& );
ErrCode GetSaveWarningOfMSVBAStorage( SfxObjectShell &rDocS );

struct SwReaderWriterEntry
{
    Reader* pReader;
    FnGetReader fnGetReader;
    FnGetWriter fnGetWriter;
    bool bDelReader;

    SwReaderWriterEntry( const FnGetReader fnReader, const FnGetWriter fnWriter, bool bDel )
        : pReader( nullptr ), fnGetReader( fnReader ), fnGetWriter( fnWriter ), bDelReader( bDel )
    {}

    /// Get access to the reader.
    Reader* GetReader();

    /// Get access to the writer.
    void GetWriter( std::u16string_view rNm, const OUString& rBaseURL, WriterRef& xWrt ) const;
};

namespace SwReaderWriter
{
    SW_DLLPUBLIC Reader* GetRtfReader();
    SW_DLLPUBLIC Reader* GetDOCXReader();

    /// Return reader based on the name.
    Reader* GetReader( const OUString& rFltName );

    /// Return writer based on the name.
    SW_DLLPUBLIC void GetWriter( std::u16string_view rFltName, const OUString& rBaseURL, WriterRef& xWrt );
}

void GetRTFWriter( std::u16string_view, const OUString&, WriterRef& );
void GetASCWriter( std::u16string_view, const OUString&, WriterRef&);
void GetHTMLWriter( std::u16string_view, const OUString&, WriterRef& );
void GetXMLWriter( std::u16string_view, const OUString&, WriterRef& );
void GetMDWriter(std::u16string_view, const OUString&, WriterRef&);

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
