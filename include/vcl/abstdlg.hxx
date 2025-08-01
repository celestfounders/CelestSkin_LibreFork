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
#ifndef INCLUDED_VCL_ABSTDLG_HXX
#define INCLUDED_VCL_ABSTDLG_HXX

#include <sal/types.h>
#include <rtl/ustring.hxx>
#include <tools/color.hxx>
#include <vcl/ColorDialog.hxx>
#include <vcl/dllapi.h>
#include <vcl/vclptr.hxx>
#include <vcl/vclreferencebase.hxx>
#include <vector>
#include <functional>
#include <memory>

namespace com::sun::star::uno { template <class interface_type> class Reference; }

namespace com::sun::star::frame { class XModel; }

class Bitmap;
class SdrObjGroup;
namespace weld
{
    class Dialog;
    class DialogController;
    class Window;
}

/**
* Some things multiple-inherit from VclAbstractDialog and OutputDevice,
* so we need to use virtual inheritance to keep the referencing counting
* OK.
*/
class VCL_DLLPUBLIC VclAbstractDialog : public virtual VclReferenceBase
{
protected:
    virtual             ~VclAbstractDialog() override;
public:
    virtual short       Execute() = 0;

    struct AsyncContext {
        // for the case where the owner is the dialog itself, and the dialog is an unwelded VclPtr based dialog
        VclPtr<VclReferenceBase> mxOwner;
        // for the case where the dialog is welded, and owned by a DialogController
        std::shared_ptr<weld::DialogController> mxOwnerDialogController;
        // for the case where the dialog is welded, and is running async without a DialogController
        std::shared_ptr<weld::Dialog> mxOwnerSelf;
        std::function<void(sal_Int32)> maEndDialogFn;
        bool isSet() const { return !!maEndDialogFn; }
    };

    /**
    * Usual codepath for modal dialogs. Some uno command decides to open a dialog,
      we call StartExecuteAsync() with a callback to handle the dialog result
      and that handler will be executed at some stage in the future,
      instead of right now.
    */
    bool StartExecuteAsync(const std::function<void(sal_Int32)> &rEndDialogFn)
    {
        AsyncContext aCtx;
        aCtx.mxOwner = this;
        aCtx.maEndDialogFn = rEndDialogFn;
        return StartExecuteAsync(aCtx);
    }

    /// Commence execution of a modal dialog.
    virtual bool StartExecuteAsync(AsyncContext &);

    // Screenshot interface
    virtual std::vector<OUString> getAllPageUIXMLDescriptions() const;
    virtual bool selectPageByUIXMLDescription(const OUString& rUIXMLDescription);
    virtual Bitmap createScreenshot() const;
    virtual OUString GetScreenshotId() const { return {}; };
};

class VCL_DLLPUBLIC VclAbstractTerminatedDialog : public VclAbstractDialog
{
protected:
    virtual             ~VclAbstractTerminatedDialog() override = default;
public:
    virtual void        EndDialog(sal_Int32 nResult) = 0;
};

class AbstractColorPickerDialog : virtual public VclAbstractDialog
{
protected:
    virtual ~AbstractColorPickerDialog() override = default;

public:
    virtual void SetColor(const Color& rColor) = 0;
    virtual Color GetColor() const = 0;

    virtual weld::Dialog* GetDialog() const = 0;
};

class VCL_DLLPUBLIC AbstractPasswordToOpenModifyDialog : public VclAbstractDialog
{
protected:
    virtual             ~AbstractPasswordToOpenModifyDialog() override = default;
public:
    virtual OUString    GetPasswordToOpen() const   = 0;
    virtual OUString    GetPasswordToModify() const = 0;
    virtual bool        IsRecommendToOpenReadonly() const = 0;
    virtual void        Response(sal_Int32) = 0;
    virtual void        AllowEmpty() = 0;
};

class VCL_DLLPUBLIC AbstractSecurityOptionsDialog : public VclAbstractDialog
{
protected:
    virtual             ~AbstractSecurityOptionsDialog() override = default;
public:
    virtual bool        SetSecurityOptions() = 0;
};

class VCL_DLLPUBLIC AbstractScreenshotAnnotationDlg : public VclAbstractDialog
{
protected:
    virtual             ~AbstractScreenshotAnnotationDlg() override = default;
};

class VCL_DLLPUBLIC AbstractSignatureLineDialog : public VclAbstractDialog
{
protected:
    virtual ~AbstractSignatureLineDialog() override = default;
public:
    virtual void Apply() = 0;
};

class VCL_DLLPUBLIC AbstractSignSignatureLineDialog : public VclAbstractDialog
{
protected:
    virtual ~AbstractSignSignatureLineDialog() override = default;
public:
    virtual void Apply() = 0;
};

class VCL_DLLPUBLIC AbstractQrCodeGenDialog : public VclAbstractDialog
{
protected:
    virtual ~AbstractQrCodeGenDialog() override = default;
};

class VCL_DLLPUBLIC AbstractAdditionsDialog : public VclAbstractDialog
{
protected:
    virtual ~AbstractAdditionsDialog() override = default;
};

/** Edit Diagram dialog */
class VCL_DLLPUBLIC AbstractDiagramDialog : public VclAbstractDialog
{
protected:
    virtual ~AbstractDiagramDialog() override = default;
};

class VCL_DLLPUBLIC AbstractQueryDialog : public VclAbstractDialog
{
protected:
    virtual ~AbstractQueryDialog() override = default;
public:
    virtual bool ShowAgain() const = 0;
    virtual void SetYesLabel(const OUString& sLabel) = 0;
    virtual void SetNoLabel(const OUString& sLabel) = 0;
};

class VCL_DLLPUBLIC VclAbstractDialogFactory
{
public:
    virtual             ~VclAbstractDialogFactory();    // needed for export of vtable
    static VclAbstractDialogFactory* Create();
    // The Id is an implementation detail of the factory
    virtual VclPtr<VclAbstractDialog> CreateVclDialog(weld::Window* pParent, sal_uInt32 nId) = 0;

    virtual VclPtr<AbstractColorPickerDialog>
    CreateColorPickerDialog(weld::Window* pParent, Color nColor, vcl::ColorPickerMode eMode) = 0;

    // creates instance of PasswordToOpenModifyDialog from cui
    virtual VclPtr<AbstractPasswordToOpenModifyDialog> CreatePasswordToOpenModifyDialog(weld::Window * pParent, sal_uInt16 nMaxPasswdLen, bool bIsPasswordToModify) = 0;

    // creates instance of SignatureDialog from cui
    virtual VclPtr<AbstractSignatureLineDialog>
    CreateSignatureLineDialog(weld::Window* pParent,
                              const css::uno::Reference<css::frame::XModel> xModel,
                              bool bEditExisting)
        = 0;

    // creates instance of SignSignatureDialog from cui
    virtual VclPtr<AbstractSignSignatureLineDialog>
    CreateSignSignatureLineDialog(weld::Window* pParent,
                                  const css::uno::Reference<css::frame::XModel> xModel)
        = 0;

    // creates instance of QrCodeDialog from cui
    virtual VclPtr<AbstractQrCodeGenDialog>
    CreateQrCodeGenDialog(weld::Window* pParent,
                              const css::uno::Reference<css::frame::XModel> xModel,
                              bool bEditExisting)
        = 0;

    // creates instance of ScreenshotAnnotationDlg from cui
    virtual VclPtr<AbstractScreenshotAnnotationDlg> CreateScreenshotAnnotationDlg(
        weld::Dialog& rParentDialog) = 0;

    // create additions dialog
    virtual VclPtr<AbstractAdditionsDialog>
        CreateAdditionsDialog(weld::Window* pParent, const OUString& sAdditionsTag) = 0;

    virtual VclPtr<AbstractDiagramDialog> CreateDiagramDialog(
        weld::Window* pParent,
        SdrObjGroup& rDiagram) = 0;

    virtual VclPtr<AbstractQueryDialog> CreateQueryDialog(
        weld::Window* pParent,
        const OUString& sTitle, const OUString& sText, const OUString& sQuestion,
        bool bShowAgain) = 0;

#ifdef _WIN32
    virtual VclPtr<VclAbstractDialog>
    CreateFileExtCheckDialog(weld::Window* _pParent, const OUString& sTitle, const OUString& sMsg)
        = 0;
#endif
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
