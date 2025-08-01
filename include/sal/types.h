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

/*
 * This file is part of LibreOffice published API.
 */

#ifndef INCLUDED_SAL_TYPES_H
#define INCLUDED_SAL_TYPES_H

#include "sal/config.h"

#include <stddef.h>

#include "sal/macros.h"
#include "sal/typesizes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char sal_Bool;
#   define sal_False ((sal_Bool)0)
#   define sal_True  ((sal_Bool)1)

/* char is assumed to always be 1 byte long */
typedef signed char         sal_Int8;
typedef unsigned char       sal_uInt8;

#if SAL_TYPES_SIZEOFSHORT == 2
    typedef signed short      sal_Int16;
    typedef unsigned short    sal_uInt16;
#else
     #error "Could not find 16-bit type, add support for your architecture"
#endif

#if SAL_TYPES_SIZEOFLONG == 4
    typedef signed long       sal_Int32;
    typedef unsigned long     sal_uInt32;
    #define SAL_PRIdINT32 "ld"
    #define SAL_PRIuUINT32 "lu"
    #define SAL_PRIxUINT32 "lx"
    #define SAL_PRIXUINT32 "lX"
#elif SAL_TYPES_SIZEOFINT == 4
    typedef signed int        sal_Int32;
    typedef unsigned int      sal_uInt32;
    #define SAL_PRIdINT32 "d"
    #define SAL_PRIuUINT32 "u"
    #define SAL_PRIxUINT32 "x"
    #define SAL_PRIXUINT32 "X"
#else
     #error "Could not find 32-bit type, add support for your architecture"
#endif

#ifdef _MSC_VER
    typedef __int64                  sal_Int64;
    typedef unsigned __int64         sal_uInt64;

    /*  The following are macros that will add the 64 bit constant suffix. */
    #define SAL_CONST_INT64(x)       x##i64
    #define SAL_CONST_UINT64(x)      x##ui64

    #define SAL_PRIdINT64 "I64d"
    #define SAL_PRIuUINT64 "I64u"
    #define SAL_PRIxUINT64 "I64x"
    #define SAL_PRIXUINT64 "I64X"
#elif defined (__GNUC__)
    #if SAL_TYPES_SIZEOFLONG == 8
        typedef signed long int         sal_Int64;
        typedef unsigned long int       sal_uInt64;


        /*  The following are macros that will add the 64 bit constant suffix. */
        #define SAL_CONST_INT64(x)       x##l
        #define SAL_CONST_UINT64(x)      x##ul

        #define SAL_PRIdINT64 "ld"
        #define SAL_PRIuUINT64 "lu"
        #define SAL_PRIxUINT64 "lx"
        #define SAL_PRIXUINT64 "lX"
    #elif SAL_TYPES_SIZEOFLONGLONG == 8
        typedef signed long long    sal_Int64;
        typedef unsigned long long  sal_uInt64;

        /*  The following are macros that will add the 64 bit constant suffix. */
        #define SAL_CONST_INT64(x)       x##ll
        #define SAL_CONST_UINT64(x)      x##ull

        #define SAL_PRIdINT64 "lld"
        #define SAL_PRIuUINT64 "llu"
        #define SAL_PRIxUINT64 "llx"
        #define SAL_PRIXUINT64 "llX"
    #else
        #error "Could not find 64-bit type, add support for your architecture"
    #endif
#else
    #error "Please define the 64-bit types for your architecture/compiler in include/sal/types.h"
#endif

#if defined LIBO_INTERNAL_ONLY && defined __cplusplus
    #define SAL_UNICODE_NOTEQUAL_WCHAR_T
    typedef char16_t sal_Unicode;
#elif defined(_WIN32)
    typedef wchar_t sal_Unicode;
#else
    #define SAL_UNICODE_NOTEQUAL_WCHAR_T
    typedef sal_uInt16 sal_Unicode;
#endif

typedef void *                   sal_Handle;

/* sal_Size should currently be the native width of the platform */
#if SAL_TYPES_SIZEOFPOINTER == 4
    typedef sal_uInt32          sal_Size;
    typedef sal_Int32           sal_sSize;
#elif SAL_TYPES_SIZEOFPOINTER == 8
    typedef sal_uInt64          sal_Size;
    typedef sal_Int64           sal_sSize;
#else
    #error "Please make sure SAL_TYPES_SIZEOFPOINTER is defined for your architecture/compiler"
#endif

/* sal_PtrDiff holds the result of a pointer subtraction */
#if SAL_TYPES_SIZEOFPOINTER == 4
    typedef sal_Int32           sal_PtrDiff;
#elif SAL_TYPES_SIZEOFPOINTER == 8
    typedef sal_Int64           sal_PtrDiff;
#else
    #error "Please make sure SAL_TYPES_SIZEOFPOINTER is defined for your architecture/compiler"
#endif

/* printf-style conversion specification length modifiers for size_t and
   ptrdiff_t (most platforms support C99, MSC has its own extension) */
#if defined(_MSC_VER)
    #define SAL_PRI_SIZET "I"
    #define SAL_PRI_PTRDIFFT "I"
#else
    #define SAL_PRI_SIZET "z"
    #define SAL_PRI_PTRDIFFT "t"
#endif

/* sal_IntPtr, sal_uIntPtr are integer types designed to hold pointers so that any valid
 * pointer to void can be converted to this type and back to a pointer to void and the
 * result will compare to the original pointer */
#if SAL_TYPES_SIZEOFPOINTER == 4
    typedef sal_Int32           sal_IntPtr;
    typedef sal_uInt32          sal_uIntPtr;
    #define SAL_PRIdINTPTR SAL_PRIdINT32
    #define SAL_PRIuUINTPTR SAL_PRIuUINT32
    #define SAL_PRIxUINTPTR SAL_PRIxUINT32
    #define SAL_PRIXUINTPTR SAL_PRIXUINT32
#elif SAL_TYPES_SIZEOFPOINTER == 8
    typedef sal_Int64           sal_IntPtr;
    typedef sal_uInt64          sal_uIntPtr;
    #define SAL_PRIdINTPTR SAL_PRIdINT64
    #define SAL_PRIuUINTPTR SAL_PRIuUINT64
    #define SAL_PRIxUINTPTR SAL_PRIxUINT64
    #define SAL_PRIXUINTPTR SAL_PRIXUINT64
#else
    #error "Please make sure SAL_TYPES_SIZEOFPOINTER is defined for your architecture/compiler"
#endif

/* The following SAL_MIN_INTn defines codify the assumption that the signed
 * sal_Int types use two's complement representation.  Defining them as
 * "-0x7F... - 1" instead of as "-0x80..." prevents warnings about applying the
 * unary minus operator to unsigned quantities.
 */
#define SAL_MIN_INT8          ((sal_Int8)   (-0x7F - 1))
#define SAL_MAX_INT8          ((sal_Int8)   0x7F)
#define SAL_MAX_UINT8         ((sal_uInt8)  0xFF)
#define SAL_MIN_INT16         ((sal_Int16)  (-0x7FFF - 1))
#define SAL_MAX_INT16         ((sal_Int16)  0x7FFF)
#define SAL_MAX_UINT16        ((sal_uInt16) 0xFFFF)
#define SAL_MIN_INT32         ((sal_Int32)  (-0x7FFFFFFF - 1))
#define SAL_MAX_INT32         ((sal_Int32)  0x7FFFFFFF)
#define SAL_MAX_UINT32        ((sal_uInt32) 0xFFFFFFFF)
#define SAL_MIN_INT64         ((sal_Int64)  (SAL_CONST_INT64(-0x7FFFFFFFFFFFFFFF) - 1))
#define SAL_MAX_INT64         ((sal_Int64)  SAL_CONST_INT64(0x7FFFFFFFFFFFFFFF))
#define SAL_MAX_UINT64        ((sal_uInt64) SAL_CONST_UINT64(0xFFFFFFFFFFFFFFFF))

#if SAL_TYPES_SIZEOFPOINTER == 4
#define SAL_MAX_SSIZE       SAL_MAX_INT32
#define SAL_MAX_SIZE        SAL_MAX_UINT32
#elif SAL_TYPES_SIZEOFPOINTER == 8
#define SAL_MAX_SSIZE       SAL_MAX_INT64
#define SAL_MAX_SIZE        SAL_MAX_UINT64
#endif

#define SAL_MAX_ENUM 0x7fffffff

#if defined(_MSC_VER)
#   define SAL_DLLPUBLIC_EXPORT    __declspec(dllexport)
#   define SAL_JNI_EXPORT          __declspec(dllexport)
#   define SAL_DLLPUBLIC_IMPORT    __declspec(dllimport)
#   define SAL_DLLPRIVATE
#   define SAL_DLLPUBLIC_TEMPLATE
#   define SAL_DLLPUBLIC_RTTI
#   define SAL_CALL         __cdecl
#elif defined SAL_UNX
#   if defined(__GNUC__)
#     if defined(DISABLE_DYNLOADING)
#       define SAL_DLLPUBLIC_EXPORT  __attribute__ ((visibility("hidden")))
#       define SAL_JNI_EXPORT        __attribute__ ((visibility("default")))
#       define SAL_DLLPUBLIC_IMPORT  __attribute__ ((visibility("hidden")))
#       define SAL_DLLPRIVATE        __attribute__ ((visibility("hidden")))
#       define SAL_DLLPUBLIC_TEMPLATE __attribute__ ((visibility("hidden")))
#       define SAL_DLLPUBLIC_RTTI     __attribute__ ((visibility("default")))
#     else
#       define SAL_DLLPUBLIC_EXPORT  __attribute__ ((visibility("default")))
#       define SAL_JNI_EXPORT        __attribute__ ((visibility("default")))
#       define SAL_DLLPUBLIC_IMPORT  __attribute__ ((visibility("default")))
#       define SAL_DLLPRIVATE        __attribute__ ((visibility("hidden")))
#       define SAL_DLLPUBLIC_TEMPLATE __attribute__ ((visibility("default")))
#       if defined __clang__
#         if __has_attribute(type_visibility)
#           define SAL_DLLPUBLIC_RTTI  __attribute__ ((type_visibility("default")))
#         else
#           define SAL_DLLPUBLIC_RTTI  __attribute__ ((visibility("default")))
#         endif
#       else
// GCC does not have currently have equivalent functionality to clang's type_visibility
// but I have a feature request for that at https://gcc.gnu.org/bugzilla/show_bug.cgi?id=113958
// Until that is implemented, just make the whole class visible, which is what I would need to
// do anyhow if something wants to import the typeinfo symbol.
#         define SAL_DLLPUBLIC_RTTI  __attribute__ ((visibility("default")))
#       endif
#     endif
#   else
#     define SAL_DLLPUBLIC_EXPORT
#     define SAL_JNI_EXPORT
#     define SAL_DLLPUBLIC_IMPORT
#     define SAL_DLLPRIVATE
#     define SAL_DLLPUBLIC_TEMPLATE
#     define SAL_DLLPUBLIC_RTTI
#   endif
#   define SAL_CALL
#else
#   error("unknown platform")
#endif

/**
   Exporting the symbols necessary for exception handling on GCC.

   These macros are used for inline declarations of exception classes, as in
   rtl/malformeduriexception.hxx.
*/
#if defined(__GNUC__)
#   if defined(DISABLE_DYNLOADING)
#      define SAL_EXCEPTION_DLLPUBLIC_EXPORT __attribute__((visibility("default")))
#    else
#      define SAL_EXCEPTION_DLLPUBLIC_EXPORT SAL_DLLPUBLIC_EXPORT
#    endif
#    define SAL_EXCEPTION_DLLPRIVATE SAL_DLLPRIVATE
#else
#    define SAL_EXCEPTION_DLLPUBLIC_EXPORT
#    define SAL_EXCEPTION_DLLPRIVATE
#endif

/** Use this as markup for functions and methods whose return value must be
    used.

    Compilers that support a construct of this nature will emit a compile
    time warning on unused return value.
*/
#if defined LIBO_INTERNAL_ONLY && defined __cplusplus
#define SAL_WARN_UNUSED_RESULT [[nodiscard]]
#elif (defined __GNUC__ \
     && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1))) \
    || defined __clang__
#   define SAL_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#   define SAL_WARN_UNUSED_RESULT
#endif

#if defined LIBO_INTERNAL_ONLY
/** Use this as markup for functions and methods whose return value may be
    null and should not be dereferenced unconditionally.

    Compilers that support a construct of this nature will emit a compile
    time warning on unconditional dereference of returned pointer.
*/
#ifdef _MSC_VER
#   define SAL_RET_MAYBENULL _Ret_maybenull_
#else
#   define SAL_RET_MAYBENULL
#endif
#endif

/** Use this for pure virtual classes, e.g. class SAL_NO_VTABLE Foo { ...
    This hinders the compiler from setting a generic vtable stating that
    a pure virtual function was called and thus slightly reduces code size.
*/
#ifdef _MSC_VER
#   define SAL_NO_VTABLE __declspec(novtable)
#else
#   define SAL_NO_VTABLE
#endif

#ifdef _WIN32
#   pragma pack(push, 8)
#endif

/** This is the binary specification of a SAL sequence.
*/
typedef struct _sal_Sequence
{
    /** reference count of sequence<br>
    */
    sal_Int32           nRefCount;
    /** element count<br>
    */
    sal_Int32           nElements;
    /** elements array<br>
    */
    char                elements[1];
} sal_Sequence;

#define SAL_SEQUENCE_HEADER_SIZE ((sal_Size) offsetof(sal_Sequence,elements))

#if defined( _WIN32)
#pragma pack(pop)
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** Nothrow specification for C functions.

    This is a macro so it can expand to nothing in C code.
*/
#if defined __cplusplus
#if __cplusplus >= 201103L
#define SAL_THROW_EXTERN_C() noexcept
#else
#define SAL_THROW_EXTERN_C() throw ()
#endif
#else
#define SAL_THROW_EXTERN_C()
#endif

#ifdef __cplusplus

/** To markup destructors that coverity warns might throw exceptions
    which won't throw in practice, or where std::terminate is
    an acceptable response if they do
*/
#if defined(LIBO_INTERNAL_ONLY) && defined(__COVERITY__) && __COVERITY_MAJOR__ <= 2024
#   define COVERITY_NOEXCEPT_FALSE noexcept(false)
#else
#   define COVERITY_NOEXCEPT_FALSE
#endif

enum __sal_NoAcquire
{
    /** definition of a no acquire enum for ctors
    */
    SAL_NO_ACQUIRE
};

namespace com { namespace sun { namespace star { } } }

/** short-circuit extra-verbose API namespaces

 @since LibreOffice 4.0
*/
namespace css = ::com::sun::star;

/** C++11 "= delete" feature.

    For LIBO_INTERNAL_ONLY, calling a deleted function will cause a compile-time
    error, while otherwise it will only cause a link-time error as the declared
    function is not defined.

    @since LibreOffice 4.1
*/
#if defined LIBO_INTERNAL_ONLY
#define SAL_DELETED_FUNCTION = delete
#else
#define SAL_DELETED_FUNCTION
#endif

/** C++11 "override" feature.

    For LIBO_INTERNAL_ONLY, force the method to override an existing method in
    parent, error out if the method with the correct signature does not exist.

    @since LibreOffice 4.1
*/
#if defined LIBO_INTERNAL_ONLY
#define SAL_OVERRIDE override
#else
#define SAL_OVERRIDE
#endif

/** C++11 "constexpr" feature.

    For LIBO_INTERNAL_ONLY, declare that it's possible to evaluate the value
    at compile time.

    @since LibreOffice 7.2
*/
#if defined LIBO_INTERNAL_ONLY
#define SAL_CONSTEXPR constexpr
#else
#define SAL_CONSTEXPR
#endif

/** Macro for C++11 "noexcept" vs. "throw ()" exception specification.

    The latter has been removed completely from C++20.

    @since LibreOffice 7.2
 */
#if __cplusplus >= 201103L
#define SAL_NOEXCEPT noexcept
#else
#define SAL_NOEXCEPT throw ()
#endif

#endif /* __cplusplus */

#ifdef __cplusplus

namespace sal {

/**
   A static_cast between integral types, to avoid C++ compiler warnings.

   In C++ source code, use sal::static_int_cast<T>(n) instead of
   static_cast<T>(n) whenever a compiler warning about integral type problems
   shall be silenced.  That way, source code that needs to be modified when the
   type of any of the expressions involved in the compiler warning is changed
   can be found more easily.

   Both template arguments T1 and T2 must be integral types.
*/
template< typename T1, typename T2 > inline T1 static_int_cast(T2 n) {
    return static_cast< T1 >(n);
}

}

#else /* __cplusplus */

/**
   A cast between integer types, to avoid C compiler warnings.

   In C source code, use SAL_INT_CAST(type, expr) instead of ((type) (expr))
   whenever a compiler warning about integer type problems shall be silenced.
   That way, source code that needs to be modified when the type of any of the
   expressions involved in the compiler warning is changed can be found more
   easily.

   The argument 'type' must be an integer type and the argument 'expr' must be
   an integer expression.  Both arguments are evaluated exactly once.
*/
#define SAL_INT_CAST(type, expr) ((type) (expr))

#endif /* __cplusplus */

/**
    Use as follows:
        SAL_DEPRECATED("Don't use, it's evil.") void doit(int nPara);
*/

#if defined __GNUC__ || defined __clang__
#if defined LIBO_INTERNAL_ONLY
#    define SAL_DEPRECATED(message) __attribute__((deprecated(message)))
#else
#    define SAL_DEPRECATED(message) __attribute__((deprecated))
#endif
#elif defined(_MSC_VER)
#    define SAL_DEPRECATED(message) __declspec(deprecated(message))
#else
#    define SAL_DEPRECATED(message)
#endif

/**
   This macro is used in cppumaker-generated include files, to tag entities that
   are marked as @deprecated in UNOIDL.

   It causes deprecation warnings to be generated in external code, but for now
   is silenced in internal code.  It would need some substantial clean-up of
   internal code to fix all warnings/errors generated by it.  (Once that is
   done, it can become a synonym for SAL_DEPRECATED under LIBO_INTERNAL_ONLY,
   too.  Completely removing the macro then would be incompatible, in case there
   are include files still around generated with a cppumaker that emitted it.)
 */
#ifdef LIBO_INTERNAL_ONLY
#    define SAL_DEPRECATED_INTERNAL(message)
#else
#    define SAL_DEPRECATED_INTERNAL(message) SAL_DEPRECATED(message)
#endif

/**
    Use as follows:
        SAL_WNODEPRECATED_DECLARATIONS_PUSH
        \::std::auto_ptr<X> ...
        SAL_WNODEPRECATED_DECLARATIONS_POP
*/

#if defined LIBO_INTERNAL_ONLY && defined __GNUC__
#define SAL_WNODEPRECATED_DECLARATIONS_PUSH \
    _Pragma(SAL_STRINGIFY_ARG(GCC diagnostic push)) \
    _Pragma(SAL_STRINGIFY_ARG(GCC diagnostic ignored "-Wdeprecated-declarations"))
#define SAL_WNODEPRECATED_DECLARATIONS_POP \
    _Pragma(SAL_STRINGIFY_ARG(GCC diagnostic pop))
#elif defined LIBO_INTERNAL_ONLY && defined _MSC_VER
#define SAL_WNODEPRECATED_DECLARATIONS_PUSH \
    _Pragma(SAL_STRINGIFY_ARG(warning(push))) \
    _Pragma(SAL_STRINGIFY_ARG(warning(disable : 4996)))
#define SAL_WNODEPRECATED_DECLARATIONS_POP \
    _Pragma(SAL_STRINGIFY_ARG(warning(pop)))
#else
#   define SAL_WNODEPRECATED_DECLARATIONS_PUSH
#   define SAL_WNODEPRECATED_DECLARATIONS_POP
#endif

/**
   Use as follows:

   SAL_WNOUNREACHABLE_CODE_PUSH

   function definition

   SAL_WNOUNREACHABLE_CODE_POP

   Useful in cases where the compiler is "too clever" like when doing
   link-time code generation, and noticing that a called function
   always throws, and fixing the problem cleanly so that it produces
   no warnings in normal non-LTO compilations either is not easy.

*/

#ifdef _MSC_VER
#define SAL_WNOUNREACHABLE_CODE_PUSH \
    __pragma(warning(push)) \
    __pragma(warning(disable:4702)) \
    __pragma(warning(disable:4722))
#define SAL_WNOUNREACHABLE_CODE_POP \
    __pragma(warning(pop))
#else
/* Add definitions for GCC and Clang if needed */
#define SAL_WNOUNREACHABLE_CODE_PUSH
#define SAL_WNOUNREACHABLE_CODE_POP
#endif

/** Annotate unused but required C++ function parameters.

    An unused parameter is required if the function needs to adhere to a given
    type (e.g., if its address is assigned to a function pointer of a specific
    type, or if it is called from template code).  This annotation helps static
    analysis tools suppress false warnings.  In the case of virtual functions
    (where unused required parameters are common, too), the annotation is not
    required (as static analysis tools can themselves derive the information
    whether a function is virtual).

    Use the annotation in function definitions like

      void f(SAL_UNUSED_PARAMETER int) {}

    C does not allow unnamed parameters, anyway, so a function definition like
    the above needs to be written there as

      void f(int dummy) { (void) dummy; / * avoid warnings * / }

    without a SAL_UNUSED_PARAMETER annotation.

    @since LibreOffice 3.6
 */
#if defined __cplusplus
#if defined __GNUC__ || defined __clang__
#define SAL_UNUSED_PARAMETER __attribute__ ((unused))
#else
#define SAL_UNUSED_PARAMETER
#endif
#endif

/**

 Annotate classes where a compiler should warn if an instance is unused.

 The compiler cannot warn about unused instances if they have non-trivial
 or external constructors or destructors. Classes marked with SAL_WARN_UNUSED
 will be warned about.

 @since LibreOffice 4.0

*/

#if defined LIBO_INTERNAL_ONLY && (defined __GNUC__ || defined __clang__)
#define SAL_WARN_UNUSED __attribute__((warn_unused))
#else
#define SAL_WARN_UNUSED
#endif

/// @cond INTERNAL

#if defined __GNUC__ || defined __clang__
// Macro to try to catch and warn on assignments inside expr.
#    define SAL_DETAIL_BOOLEAN_EXPR(expr)   \
        __extension__ ({                    \
            int sal_boolean_var_;           \
            if (expr)                       \
               sal_boolean_var_ = 1;        \
            else                            \
               sal_boolean_var_ = 0;        \
            sal_boolean_var_;               \
        })

/** An optimization annotation: denotes that expression is likely to be true.

    Use it to annotate paths that we think are likely eg.
    if (SAL_LIKELY(ptr != nullptr))
       // this path is the one that is ~always taken.

    @since LibreOffice 5.2

    Returns: the boolean value of expr (expressed as either int 1 or 0)
 */
#    define SAL_LIKELY(expr) __builtin_expect(SAL_DETAIL_BOOLEAN_EXPR((expr)), 1)

/** An optimization annotation: denotes that expression is unlikely to be true.

    Use it to annotate paths that we think are likely eg.
    if (SAL_UNLIKELY(ptr != nullptr))
       // this path is the one that is ~never taken.

    @since LibreOffice 5.2

    Returns: the boolean value of expr (expressed as either int 1 or 0)
 */
#    define SAL_UNLIKELY(expr) __builtin_expect(SAL_DETAIL_BOOLEAN_EXPR((expr)), 0)

/** An optimization annotation: tells the compiler to work harder at this code

    If the SAL_HOT annotation is present on a function or a label then
    subsequent code statements may have more aggressive compiler
    optimization and in-lining work performed on them.

    In addition this code can end up in a special section, to be
    grouped with other frequently used code.

    @since LibreOffice 5.2
 */
#    define SAL_HOT __attribute__((hot))

/** An optimization annotation: tells the compiler to work less on this code

    If the SAL_COLD annotation is present on a function or a label then
    subsequent code statements are unlikely to be performed except in
    exceptional circumstances, and optimizing for code-size rather
    than performance is preferable.

    In addition this code can end up in a special section, to be grouped
    with (and away from) other more frequently used code, to improve
    locality of reference.

    @since LibreOffice 5.2
 */
#    define SAL_COLD __attribute__((cold))
#else
#    define SAL_LIKELY(expr) (expr)
#    define SAL_UNLIKELY(expr) (expr)
#    define SAL_HOT
#    define SAL_COLD
#endif

/// @endcond

/// @cond INTERNAL
/** Annotate pointer-returning functions to indicate that such a pointer
    is never nullptr.

    Note that MSVC supports this feature via it's SAL _Ret_notnull_
    annotation, but since it's in a completely different place on
    the function declaration, it's a little hard to support both.

    @since LibreOffice 5.5
*/
#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#if defined LIBO_INTERNAL_ONLY && ((defined __GNUC__ && !defined __clang__) || (defined __clang__ && __has_attribute(returns_nonnull)))
#define SAL_RETURNS_NONNULL  __attribute__((returns_nonnull))
#else
#define SAL_RETURNS_NONNULL
#endif
/// @endcond

#if defined LIBO_INTERNAL_ONLY
// An annotation mechanism used by some loplugins.  The id argument must be an ordinary string
// literal.  For Clang, this expands to a clang::annotate attribute with an annotation consisting of
// the concatenation of a "loplugin:" prefix and the given id suffix.  For non-Clang, this expands
// to nothing to avoid e.g. -Wattributes from GCC's -Wall.
#if defined __clang__
#define SAL_LOPLUGIN_ANNOTATE(id) [[clang::annotate("loplugin:" id)]]
#else
#define SAL_LOPLUGIN_ANNOTATE(id)
#endif
#endif

#endif // INCLUDED_SAL_TYPES_H

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
