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

#pragma once

#include <sal/config.h>

// tdf#126879 - drop obsolete DOCTYPE HTML 4.0 and use the HTML5 DOCTYPE declaration
#define OOO_STRING_SVTOOLS_HTML_doctype5 "html"
#define OOO_STRING_SVTOOLS_XHTML_doctype11                                                         \
    "html PUBLIC \"-//W3C//DTD XHTML 1.1 plus MathML 2.0//EN\" "                                   \
    "\"http://www.w3.org/Math/DTD/mathml2/xhtml-math11-f.dtd\""

// these are only switched on
#define OOO_STRING_SVTOOLS_HTML_area "area"
#define OOO_STRING_SVTOOLS_HTML_base "base"
#define OOO_STRING_SVTOOLS_HTML_comment "!--"
#define OOO_STRING_SVTOOLS_HTML_doctype "!DOCTYPE"
#define OOO_STRING_SVTOOLS_HTML_cdata "![cdata["
#define OOO_STRING_SVTOOLS_HTML_embed "embed"
#define OOO_STRING_SVTOOLS_HTML_horzrule "hr"
#define OOO_STRING_SVTOOLS_HTML_image "img"
#define OOO_STRING_SVTOOLS_HTML_input "input"
#define OOO_STRING_SVTOOLS_HTML_linebreak "br"
#define OOO_STRING_SVTOOLS_HTML_li "li"
#define OOO_STRING_SVTOOLS_HTML_link "link"
#define OOO_STRING_SVTOOLS_HTML_meta "meta"
#define OOO_STRING_SVTOOLS_HTML_nobr "nobr"
#define OOO_STRING_SVTOOLS_HTML_option "option"
#define OOO_STRING_SVTOOLS_HTML_param "param"
#define OOO_STRING_SVTOOLS_HTML_spacer "spacer"

// these are switched off again
#define OOO_STRING_SVTOOLS_HTML_abbreviation "abbrev"
#define OOO_STRING_SVTOOLS_HTML_acronym "acronym"
#define OOO_STRING_SVTOOLS_HTML_address "address"
#define OOO_STRING_SVTOOLS_HTML_anchor "a"
#define OOO_STRING_SVTOOLS_HTML_applet "applet"
#define OOO_STRING_SVTOOLS_HTML_author "au"
#define OOO_STRING_SVTOOLS_HTML_banner "banner"
#define OOO_STRING_SVTOOLS_HTML_basefont "basefont"
#define OOO_STRING_SVTOOLS_HTML_bigprint "big"
#define OOO_STRING_SVTOOLS_HTML_blink "blink"
#define OOO_STRING_SVTOOLS_HTML_blockquote "blockquote"
#define OOO_STRING_SVTOOLS_HTML_blockquote30 "bq"
#define OOO_STRING_SVTOOLS_HTML_body "body"
#define OOO_STRING_SVTOOLS_HTML_bold "b"
#define OOO_STRING_SVTOOLS_HTML_caption "caption"
#define OOO_STRING_SVTOOLS_HTML_center "center"
#define OOO_STRING_SVTOOLS_HTML_citation "cite"
#define OOO_STRING_SVTOOLS_HTML_code "code"
#define OOO_STRING_SVTOOLS_HTML_col "col"
#define OOO_STRING_SVTOOLS_HTML_colgroup "colgroup"
#define OOO_STRING_SVTOOLS_HTML_credit "credit"
#define OOO_STRING_SVTOOLS_HTML_dd "dd"
#define OOO_STRING_SVTOOLS_HTML_deflist "dl"
#define OOO_STRING_SVTOOLS_HTML_deletedtext "del"
#define OOO_STRING_SVTOOLS_HTML_dirlist "dir"
#define OOO_STRING_SVTOOLS_HTML_division "div"
#define OOO_STRING_SVTOOLS_HTML_dt "dt"
#define OOO_STRING_SVTOOLS_HTML_emphasis "em"
#define OOO_STRING_SVTOOLS_HTML_figure "fig"
#define OOO_STRING_SVTOOLS_HTML_font "font"
#define OOO_STRING_SVTOOLS_HTML_footnote "fn"
#define OOO_STRING_SVTOOLS_HTML_form "form"
#define OOO_STRING_SVTOOLS_HTML_frame "frame"
#define OOO_STRING_SVTOOLS_HTML_frameset "frameset"
#define OOO_STRING_SVTOOLS_HTML_head1 "h1"
#define OOO_STRING_SVTOOLS_HTML_head2 "h2"
#define OOO_STRING_SVTOOLS_HTML_head3 "h3"
#define OOO_STRING_SVTOOLS_HTML_head4 "h4"
#define OOO_STRING_SVTOOLS_HTML_head5 "h5"
#define OOO_STRING_SVTOOLS_HTML_head6 "h6"
#define OOO_STRING_SVTOOLS_HTML_head "head"
#define OOO_STRING_SVTOOLS_HTML_html "html"
#define OOO_STRING_SVTOOLS_HTML_iframe "iframe"
#define OOO_STRING_SVTOOLS_HTML_insertedtext "ins"
#define OOO_STRING_SVTOOLS_HTML_italic "i"
#define OOO_STRING_SVTOOLS_HTML_keyboard "kbd"
#define OOO_STRING_SVTOOLS_HTML_language "lang"
#define OOO_STRING_SVTOOLS_HTML_listheader "lh"
#define OOO_STRING_SVTOOLS_HTML_map "map"
#define OOO_STRING_SVTOOLS_HTML_menulist "menu"
#define OOO_STRING_SVTOOLS_HTML_multicol "multicol"
#define OOO_STRING_SVTOOLS_HTML_noembed "noembed"
#define OOO_STRING_SVTOOLS_HTML_noframe "noframe"
#define OOO_STRING_SVTOOLS_HTML_noframes "noframes"
#define OOO_STRING_SVTOOLS_HTML_noscript "noscript"
#define OOO_STRING_SVTOOLS_HTML_note "note"
#define OOO_STRING_SVTOOLS_HTML_object "object"
#define OOO_STRING_SVTOOLS_HTML_orderlist "ol"
#define OOO_STRING_SVTOOLS_HTML_parabreak "p"
#define OOO_STRING_SVTOOLS_HTML_person "person"
#define OOO_STRING_SVTOOLS_HTML_plaintext "t"
#define OOO_STRING_SVTOOLS_HTML_preformtxt "pre"
#define OOO_STRING_SVTOOLS_HTML_sample "samp"
#define OOO_STRING_SVTOOLS_HTML_script "script"
#define OOO_STRING_SVTOOLS_HTML_select "select"
#define OOO_STRING_SVTOOLS_HTML_shortquote "q"
#define OOO_STRING_SVTOOLS_HTML_smallprint "small"
#define OOO_STRING_SVTOOLS_HTML_span "span"
#define OOO_STRING_SVTOOLS_HTML_strikethrough "s"
#define OOO_STRING_SVTOOLS_HTML_strong "strong"
#define OOO_STRING_SVTOOLS_HTML_style "style"
#define OOO_STRING_SVTOOLS_HTML_subscript "sub"
#define OOO_STRING_SVTOOLS_HTML_superscript "sup"
#define OOO_STRING_SVTOOLS_HTML_table "table"
#define OOO_STRING_SVTOOLS_HTML_tablerow "tr"
#define OOO_STRING_SVTOOLS_HTML_tabledata "td"
#define OOO_STRING_SVTOOLS_HTML_tableheader "th"
#define OOO_STRING_SVTOOLS_HTML_tbody "tbody"
#define OOO_STRING_SVTOOLS_HTML_teletype "tt"
#define OOO_STRING_SVTOOLS_HTML_textarea "textarea"
#define OOO_STRING_SVTOOLS_HTML_tfoot "tfoot"
#define OOO_STRING_SVTOOLS_HTML_thead "thead"
#define OOO_STRING_SVTOOLS_HTML_title "title"
#define OOO_STRING_SVTOOLS_HTML_underline "u"
#define OOO_STRING_SVTOOLS_HTML_unorderlist "ul"
#define OOO_STRING_SVTOOLS_HTML_variable "var"

// obsolete features
#define OOO_STRING_SVTOOLS_HTML_xmp "xmp"
#define OOO_STRING_SVTOOLS_HTML_listing "listing"

// proposed features
#define OOO_STRING_SVTOOLS_HTML_definstance "dfn"
#define OOO_STRING_SVTOOLS_HTML_strike "strike"
#define OOO_STRING_SVTOOLS_HTML_comment2 "comment"
#define OOO_STRING_SVTOOLS_HTML_marquee "marquee"
#define OOO_STRING_SVTOOLS_HTML_plaintext2 "plaintext"
#define OOO_STRING_SVTOOLS_HTML_sdfield "sdfield"

// names for all characters
// _C_ for HTML < 4
// _S_ for HTML >= 4
#define OOO_STRING_SVTOOLS_HTML_C_AElig "AElig"
#define OOO_STRING_SVTOOLS_HTML_C_Aacute "Aacute"
#define OOO_STRING_SVTOOLS_HTML_C_Acirc "Acirc"
#define OOO_STRING_SVTOOLS_HTML_C_Agrave "Agrave"
#define OOO_STRING_SVTOOLS_HTML_S_Alpha "Alpha"
#define OOO_STRING_SVTOOLS_HTML_C_Aring "Aring"
#define OOO_STRING_SVTOOLS_HTML_C_Atilde "Atilde"
#define OOO_STRING_SVTOOLS_HTML_C_Auml "Auml"
#define OOO_STRING_SVTOOLS_HTML_S_Beta "Beta"
#define OOO_STRING_SVTOOLS_HTML_C_Ccedil "Ccedil"
#define OOO_STRING_SVTOOLS_HTML_S_Chi "Chi"
#define OOO_STRING_SVTOOLS_HTML_S_Dagger "Dagger"
#define OOO_STRING_SVTOOLS_HTML_S_Delta "Delta"
#define OOO_STRING_SVTOOLS_HTML_C_ETH "ETH"
#define OOO_STRING_SVTOOLS_HTML_C_Eacute "Eacute"
#define OOO_STRING_SVTOOLS_HTML_C_Ecirc "Ecirc"
#define OOO_STRING_SVTOOLS_HTML_C_Egrave "Egrave"
#define OOO_STRING_SVTOOLS_HTML_S_Epsilon "Epsilon"
#define OOO_STRING_SVTOOLS_HTML_S_Eta "Eta"
#define OOO_STRING_SVTOOLS_HTML_C_Euml "Euml"
#define OOO_STRING_SVTOOLS_HTML_S_Gamma "Gamma"
#define OOO_STRING_SVTOOLS_HTML_C_Iacute "Iacute"
#define OOO_STRING_SVTOOLS_HTML_C_Icirc "Icirc"
#define OOO_STRING_SVTOOLS_HTML_C_Igrave "Igrave"
#define OOO_STRING_SVTOOLS_HTML_S_Iota "Iota"
#define OOO_STRING_SVTOOLS_HTML_C_Iuml "Iuml"
#define OOO_STRING_SVTOOLS_HTML_S_Kappa "Kappa"
#define OOO_STRING_SVTOOLS_HTML_S_Lambda "Lambda"
#define OOO_STRING_SVTOOLS_HTML_S_Mu "Mu"
#define OOO_STRING_SVTOOLS_HTML_C_Ntilde "Ntilde"
#define OOO_STRING_SVTOOLS_HTML_S_Nu "Nu"
#define OOO_STRING_SVTOOLS_HTML_S_OElig "OElig"
#define OOO_STRING_SVTOOLS_HTML_C_Oacute "Oacute"
#define OOO_STRING_SVTOOLS_HTML_C_Ocirc "Ocirc"
#define OOO_STRING_SVTOOLS_HTML_C_Ograve "Ograve"
#define OOO_STRING_SVTOOLS_HTML_S_Omega "Omega"
#define OOO_STRING_SVTOOLS_HTML_S_Omicron "Omicron"
#define OOO_STRING_SVTOOLS_HTML_C_Oslash "Oslash"
#define OOO_STRING_SVTOOLS_HTML_C_Otilde "Otilde"
#define OOO_STRING_SVTOOLS_HTML_C_Ouml "Ouml"
#define OOO_STRING_SVTOOLS_HTML_S_Phi "Phi"
#define OOO_STRING_SVTOOLS_HTML_S_Pi "Pi"
#define OOO_STRING_SVTOOLS_HTML_S_Prime "Prime"
#define OOO_STRING_SVTOOLS_HTML_S_Psi "Psi"
#define OOO_STRING_SVTOOLS_HTML_S_Rho "Rho"
#define OOO_STRING_SVTOOLS_HTML_S_Scaron "Scaron"
#define OOO_STRING_SVTOOLS_HTML_S_Sigma "Sigma"
#define OOO_STRING_SVTOOLS_HTML_C_THORN "THORN"
#define OOO_STRING_SVTOOLS_HTML_S_Tau "Tau"
#define OOO_STRING_SVTOOLS_HTML_S_Theta "Theta"
#define OOO_STRING_SVTOOLS_HTML_C_Uacute "Uacute"
#define OOO_STRING_SVTOOLS_HTML_C_Ucirc "Ucirc"
#define OOO_STRING_SVTOOLS_HTML_C_Ugrave "Ugrave"
#define OOO_STRING_SVTOOLS_HTML_S_Upsilon "Upsilon"
#define OOO_STRING_SVTOOLS_HTML_C_Uuml "Uuml"
#define OOO_STRING_SVTOOLS_HTML_S_Xi "Xi"
#define OOO_STRING_SVTOOLS_HTML_C_Yacute "Yacute"
#define OOO_STRING_SVTOOLS_HTML_S_Yuml "Yuml"
#define OOO_STRING_SVTOOLS_HTML_S_Zeta "Zeta"
#define OOO_STRING_SVTOOLS_HTML_S_aacute "aacute"
#define OOO_STRING_SVTOOLS_HTML_S_acirc "acirc"
#define OOO_STRING_SVTOOLS_HTML_S_acute "acute"
#define OOO_STRING_SVTOOLS_HTML_S_aelig "aelig"
#define OOO_STRING_SVTOOLS_HTML_S_agrave "agrave"
#define OOO_STRING_SVTOOLS_HTML_S_alefsym "alefsym"
#define OOO_STRING_SVTOOLS_HTML_S_alpha "alpha"
#define OOO_STRING_SVTOOLS_HTML_C_amp "amp"
#define OOO_STRING_SVTOOLS_HTML_S_and "and"
#define OOO_STRING_SVTOOLS_HTML_S_ang "ang"
#define OOO_STRING_SVTOOLS_HTML_C_apos "apos"
#define OOO_STRING_SVTOOLS_HTML_S_aring "aring"
#define OOO_STRING_SVTOOLS_HTML_S_asymp "asymp"
#define OOO_STRING_SVTOOLS_HTML_S_atilde "atilde"
#define OOO_STRING_SVTOOLS_HTML_S_auml "auml"
#define OOO_STRING_SVTOOLS_HTML_S_bdquo "bdquo"
#define OOO_STRING_SVTOOLS_HTML_S_beta "beta"
#define OOO_STRING_SVTOOLS_HTML_S_brvbar "brvbar"
#define OOO_STRING_SVTOOLS_HTML_S_bull "bull"
#define OOO_STRING_SVTOOLS_HTML_S_cap "cap"
#define OOO_STRING_SVTOOLS_HTML_S_ccedil "ccedil"
#define OOO_STRING_SVTOOLS_HTML_S_cedil "cedil"
#define OOO_STRING_SVTOOLS_HTML_S_cent "cent"
#define OOO_STRING_SVTOOLS_HTML_S_chi "chi"
#define OOO_STRING_SVTOOLS_HTML_S_circ "circ"
#define OOO_STRING_SVTOOLS_HTML_S_clubs "clubs"
#define OOO_STRING_SVTOOLS_HTML_S_cong "cong"
#define OOO_STRING_SVTOOLS_HTML_S_copy "copy"
#define OOO_STRING_SVTOOLS_HTML_S_crarr "crarr"
#define OOO_STRING_SVTOOLS_HTML_S_cup "cup"
#define OOO_STRING_SVTOOLS_HTML_S_curren "curren"
#define OOO_STRING_SVTOOLS_HTML_S_dArr "dArr"
#define OOO_STRING_SVTOOLS_HTML_S_dagger "dagger"
#define OOO_STRING_SVTOOLS_HTML_S_darr "darr"
#define OOO_STRING_SVTOOLS_HTML_S_deg "deg"
#define OOO_STRING_SVTOOLS_HTML_S_delta "delta"
#define OOO_STRING_SVTOOLS_HTML_S_diams "diams"
#define OOO_STRING_SVTOOLS_HTML_S_divide "divide"
#define OOO_STRING_SVTOOLS_HTML_S_eacute "eacute"
#define OOO_STRING_SVTOOLS_HTML_S_ecirc "ecirc"
#define OOO_STRING_SVTOOLS_HTML_S_egrave "egrave"
#define OOO_STRING_SVTOOLS_HTML_S_empty "empty"
#define OOO_STRING_SVTOOLS_HTML_S_emsp "emsp"
#define OOO_STRING_SVTOOLS_HTML_S_ensp "ensp"
#define OOO_STRING_SVTOOLS_HTML_S_epsilon "epsilon"
#define OOO_STRING_SVTOOLS_HTML_S_equiv "equiv"
#define OOO_STRING_SVTOOLS_HTML_S_eta "eta"
#define OOO_STRING_SVTOOLS_HTML_S_eth "eth"
#define OOO_STRING_SVTOOLS_HTML_S_euml "euml"
#define OOO_STRING_SVTOOLS_HTML_S_euro "euro"
#define OOO_STRING_SVTOOLS_HTML_S_exist "exist"
#define OOO_STRING_SVTOOLS_HTML_S_fnof "fnof"
#define OOO_STRING_SVTOOLS_HTML_S_forall "forall"
#define OOO_STRING_SVTOOLS_HTML_S_frac12 "frac12"
#define OOO_STRING_SVTOOLS_HTML_S_frac14 "frac14"
#define OOO_STRING_SVTOOLS_HTML_S_frac34 "frac34"
#define OOO_STRING_SVTOOLS_HTML_S_frasl "frasl"
#define OOO_STRING_SVTOOLS_HTML_S_gamma "gamma"
#define OOO_STRING_SVTOOLS_HTML_S_ge "ge"
#define OOO_STRING_SVTOOLS_HTML_C_gt "gt"
#define OOO_STRING_SVTOOLS_HTML_S_hArr "hArr"
#define OOO_STRING_SVTOOLS_HTML_S_harr "harr"
#define OOO_STRING_SVTOOLS_HTML_S_hearts "hearts"
#define OOO_STRING_SVTOOLS_HTML_S_hellip "hellip"
#define OOO_STRING_SVTOOLS_HTML_S_iacute "iacute"
#define OOO_STRING_SVTOOLS_HTML_S_icirc "icirc"
#define OOO_STRING_SVTOOLS_HTML_S_iexcl "iexcl"
#define OOO_STRING_SVTOOLS_HTML_S_igrave "igrave"
#define OOO_STRING_SVTOOLS_HTML_S_image "image"
#define OOO_STRING_SVTOOLS_HTML_S_infin "infin"
#define OOO_STRING_SVTOOLS_HTML_S_int "int"
#define OOO_STRING_SVTOOLS_HTML_S_iota "iota"
#define OOO_STRING_SVTOOLS_HTML_S_iquest "iquest"
#define OOO_STRING_SVTOOLS_HTML_S_isin "isin"
#define OOO_STRING_SVTOOLS_HTML_S_iuml "iuml"
#define OOO_STRING_SVTOOLS_HTML_S_kappa "kappa"
#define OOO_STRING_SVTOOLS_HTML_S_lArr "lArr"
#define OOO_STRING_SVTOOLS_HTML_S_lambda "lambda"
#define OOO_STRING_SVTOOLS_HTML_S_lang "lang"
#define OOO_STRING_SVTOOLS_HTML_S_laquo "laquo"
#define OOO_STRING_SVTOOLS_HTML_S_larr "larr"
#define OOO_STRING_SVTOOLS_HTML_S_lceil "lceil"
#define OOO_STRING_SVTOOLS_HTML_S_ldquo "ldquo"
#define OOO_STRING_SVTOOLS_HTML_S_le "le"
#define OOO_STRING_SVTOOLS_HTML_S_lfloor "lfloor"
#define OOO_STRING_SVTOOLS_HTML_S_lowast "lowast"
#define OOO_STRING_SVTOOLS_HTML_S_loz "loz"
#define OOO_STRING_SVTOOLS_HTML_S_lrm "lrm"
#define OOO_STRING_SVTOOLS_HTML_S_lsaquo "lsaquo"
#define OOO_STRING_SVTOOLS_HTML_S_lsquo "lsquo"
#define OOO_STRING_SVTOOLS_HTML_C_lt "lt"
#define OOO_STRING_SVTOOLS_HTML_S_macr "macr"
#define OOO_STRING_SVTOOLS_HTML_S_mdash "mdash"
#define OOO_STRING_SVTOOLS_HTML_S_micro "micro"
#define OOO_STRING_SVTOOLS_HTML_S_middot "middot"
#define OOO_STRING_SVTOOLS_HTML_S_minus "minus"
#define OOO_STRING_SVTOOLS_HTML_S_mu "mu"
#define OOO_STRING_SVTOOLS_HTML_S_nabla "nabla"
#define OOO_STRING_SVTOOLS_HTML_S_nbsp "nbsp"
#define OOO_STRING_SVTOOLS_HTML_S_ndash "ndash"
#define OOO_STRING_SVTOOLS_HTML_S_ne "ne"
#define OOO_STRING_SVTOOLS_HTML_S_ni "ni"
#define OOO_STRING_SVTOOLS_HTML_S_not "not"
#define OOO_STRING_SVTOOLS_HTML_S_notin "notin"
#define OOO_STRING_SVTOOLS_HTML_S_nsub "nsub"
#define OOO_STRING_SVTOOLS_HTML_S_ntilde "ntilde"
#define OOO_STRING_SVTOOLS_HTML_S_nu "nu"
#define OOO_STRING_SVTOOLS_HTML_S_oacute "oacute"
#define OOO_STRING_SVTOOLS_HTML_S_ocirc "ocirc"
#define OOO_STRING_SVTOOLS_HTML_S_oelig "oelig"
#define OOO_STRING_SVTOOLS_HTML_S_ograve "ograve"
#define OOO_STRING_SVTOOLS_HTML_S_oline "oline"
#define OOO_STRING_SVTOOLS_HTML_S_omega "omega"
#define OOO_STRING_SVTOOLS_HTML_S_omicron "omicron"
#define OOO_STRING_SVTOOLS_HTML_S_oplus "oplus"
#define OOO_STRING_SVTOOLS_HTML_S_or "or"
#define OOO_STRING_SVTOOLS_HTML_S_ordf "ordf"
#define OOO_STRING_SVTOOLS_HTML_S_ordm "ordm"
#define OOO_STRING_SVTOOLS_HTML_S_oslash "oslash"
#define OOO_STRING_SVTOOLS_HTML_S_otilde "otilde"
#define OOO_STRING_SVTOOLS_HTML_S_otimes "otimes"
#define OOO_STRING_SVTOOLS_HTML_S_ouml "ouml"
#define OOO_STRING_SVTOOLS_HTML_S_para "para"
#define OOO_STRING_SVTOOLS_HTML_S_part "part"
#define OOO_STRING_SVTOOLS_HTML_S_permil "permil"
#define OOO_STRING_SVTOOLS_HTML_S_perp "perp"
#define OOO_STRING_SVTOOLS_HTML_S_phi "phi"
#define OOO_STRING_SVTOOLS_HTML_S_pi "pi"
#define OOO_STRING_SVTOOLS_HTML_S_piv "piv"
#define OOO_STRING_SVTOOLS_HTML_S_plusmn "plusmn"
#define OOO_STRING_SVTOOLS_HTML_S_pound "pound"
#define OOO_STRING_SVTOOLS_HTML_S_prime "prime"
#define OOO_STRING_SVTOOLS_HTML_S_prod "prod"
#define OOO_STRING_SVTOOLS_HTML_S_prop "prop"
#define OOO_STRING_SVTOOLS_HTML_S_psi "psi"
#define OOO_STRING_SVTOOLS_HTML_C_quot "quot"
#define OOO_STRING_SVTOOLS_HTML_S_rArr "rArr"
#define OOO_STRING_SVTOOLS_HTML_S_radic "radic"
#define OOO_STRING_SVTOOLS_HTML_S_rang "rang"
#define OOO_STRING_SVTOOLS_HTML_S_raquo "raquo"
#define OOO_STRING_SVTOOLS_HTML_S_rarr "rarr"
#define OOO_STRING_SVTOOLS_HTML_S_rceil "rceil"
#define OOO_STRING_SVTOOLS_HTML_S_rdquo "rdquo"
#define OOO_STRING_SVTOOLS_HTML_S_real "real"
#define OOO_STRING_SVTOOLS_HTML_S_reg "reg"
#define OOO_STRING_SVTOOLS_HTML_S_rfloor "rfloor"
#define OOO_STRING_SVTOOLS_HTML_S_rho "rho"
#define OOO_STRING_SVTOOLS_HTML_S_rlm "rlm"
#define OOO_STRING_SVTOOLS_HTML_S_rsaquo "rsaquo"
#define OOO_STRING_SVTOOLS_HTML_S_rsquo "rsquo"
#define OOO_STRING_SVTOOLS_HTML_S_sbquo "sbquo"
#define OOO_STRING_SVTOOLS_HTML_S_scaron "scaron"
#define OOO_STRING_SVTOOLS_HTML_S_sdot "sdot"
#define OOO_STRING_SVTOOLS_HTML_S_sect "sect"
#define OOO_STRING_SVTOOLS_HTML_S_shy "shy"
#define OOO_STRING_SVTOOLS_HTML_S_sigma "sigma"
#define OOO_STRING_SVTOOLS_HTML_S_sigmaf "sigmaf"
#define OOO_STRING_SVTOOLS_HTML_S_sim "sim"
#define OOO_STRING_SVTOOLS_HTML_S_spades "spades"
#define OOO_STRING_SVTOOLS_HTML_S_sub "sub"
#define OOO_STRING_SVTOOLS_HTML_S_sube "sube"
#define OOO_STRING_SVTOOLS_HTML_S_sum "sum"
#define OOO_STRING_SVTOOLS_HTML_S_sup "sup"
#define OOO_STRING_SVTOOLS_HTML_S_sup1 "sup1"
#define OOO_STRING_SVTOOLS_HTML_S_sup2 "sup2"
#define OOO_STRING_SVTOOLS_HTML_S_sup3 "sup3"
#define OOO_STRING_SVTOOLS_HTML_S_supe "supe"
#define OOO_STRING_SVTOOLS_HTML_C_szlig "szlig"
#define OOO_STRING_SVTOOLS_HTML_S_tau "tau"
#define OOO_STRING_SVTOOLS_HTML_S_there4 "there4"
#define OOO_STRING_SVTOOLS_HTML_S_theta "theta"
#define OOO_STRING_SVTOOLS_HTML_S_thetasym "thetasym"
#define OOO_STRING_SVTOOLS_HTML_S_thinsp "thinsp"
#define OOO_STRING_SVTOOLS_HTML_S_thorn "thorn"
#define OOO_STRING_SVTOOLS_HTML_S_tilde "tilde"
#define OOO_STRING_SVTOOLS_HTML_S_times "times"
#define OOO_STRING_SVTOOLS_HTML_S_trade "trade"
#define OOO_STRING_SVTOOLS_HTML_S_uArr "uArr"
#define OOO_STRING_SVTOOLS_HTML_S_uacute "uacute"
#define OOO_STRING_SVTOOLS_HTML_S_uarr "uarr"
#define OOO_STRING_SVTOOLS_HTML_S_ucirc "ucirc"
#define OOO_STRING_SVTOOLS_HTML_S_ugrave "ugrave"
#define OOO_STRING_SVTOOLS_HTML_S_uml "uml"
#define OOO_STRING_SVTOOLS_HTML_S_upsih "upsih"
#define OOO_STRING_SVTOOLS_HTML_S_upsilon "upsilon"
#define OOO_STRING_SVTOOLS_HTML_S_uuml "uuml"
#define OOO_STRING_SVTOOLS_HTML_S_weierp "weierp"
#define OOO_STRING_SVTOOLS_HTML_S_xi "xi"
#define OOO_STRING_SVTOOLS_HTML_S_yacute "yacute"
#define OOO_STRING_SVTOOLS_HTML_S_yen "yen"
#define OOO_STRING_SVTOOLS_HTML_S_yuml "yuml"
#define OOO_STRING_SVTOOLS_HTML_S_zeta "zeta"
#define OOO_STRING_SVTOOLS_HTML_S_zwj "zwj"
#define OOO_STRING_SVTOOLS_HTML_S_zwnj "zwnj"

// HTML attribute tokens (=options)

#define OOO_STRING_SVTOOLS_HTML_O_accept "accept" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_accesskey "accesskey" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_action "action" // attribute with a URI as value
#define OOO_STRING_SVTOOLS_HTML_O_align "align" // attribute with context-dependent values
#define OOO_STRING_SVTOOLS_HTML_O_alink "alink" // attribute with a colour as value (Netscape)
#define OOO_STRING_SVTOOLS_HTML_O_alt "alt" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_archive "archive" // attribute with a URI as value
#define OOO_STRING_SVTOOLS_HTML_O_axis "axis" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_background "background" // attribute with a URI as value
#define OOO_STRING_SVTOOLS_HTML_O_behavior "behavior" // attribute with enum values
#define OOO_STRING_SVTOOLS_HTML_O_bgcolor "bgcolor" // attribute with a colour as value (Netscape)
#define OOO_STRING_SVTOOLS_HTML_O_border "border" // attribute with a numerical value
#define OOO_STRING_SVTOOLS_HTML_O_bordercolor                                                      \
    "bordercolor" // attribute with a colour as value (Netscape)
#define OOO_STRING_SVTOOLS_HTML_O_bordercolordark                                                  \
    "bordercolordark" // attribute with a colour as value (Netscape)
#define OOO_STRING_SVTOOLS_HTML_O_bordercolorlight                                                 \
    "bordercolorlight" // attribute with a colour as value (Netscape)
#define OOO_STRING_SVTOOLS_HTML_O_cellpadding "cellpadding" // attribute with a numerical value
#define OOO_STRING_SVTOOLS_HTML_O_cellspacing "cellspacing" // attribute with a numerical value
#define OOO_STRING_SVTOOLS_HTML_O_char "char" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_charoff "charoff" // attribute with a numerical value
#define OOO_STRING_SVTOOLS_HTML_O_charset "charset" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_checked "checked" // attribute without value
#define OOO_STRING_SVTOOLS_HTML_O_class "class" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_classid "classid" // attribute with a URI as value
#define OOO_STRING_SVTOOLS_HTML_O_clear "clear" // attribute with enum values
#define OOO_STRING_SVTOOLS_HTML_O_code "code" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_codebase "codebase" // attribute with a URI as value
#define OOO_STRING_SVTOOLS_HTML_O_codetype "codetype" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_color "color" // attribute with a colour as value (Netscape)
#define OOO_STRING_SVTOOLS_HTML_O_cols "cols" // attribute with context-dependent values
#define OOO_STRING_SVTOOLS_HTML_O_colspan "colspan" // attribute with a numerical value
#define OOO_STRING_SVTOOLS_HTML_O_compact "compact" // attribute without value
#define OOO_STRING_SVTOOLS_HTML_O_content "content" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_coords "coords" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_data "data" // attribute with a URI as value
#define OOO_STRING_SVTOOLS_HTML_O_DSformula                                                        \
    "data-sheets-formula" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_DSnum                                                            \
    "data-sheets-numberformat" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_DSval "data-sheets-value" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_declare "declare" // attribute without value
#define OOO_STRING_SVTOOLS_HTML_O_dir "dir" // attribute with enum values
#define OOO_STRING_SVTOOLS_HTML_O_direction "direction" // attribute with enum values
#define OOO_STRING_SVTOOLS_HTML_O_disabled "disabled" // attribute without value
#define OOO_STRING_SVTOOLS_HTML_O_enctype "enctype" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_face "face" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_format "format" // attribute with enum values
#define OOO_STRING_SVTOOLS_HTML_O_frame "frame" // attribute with enum values
#define OOO_STRING_SVTOOLS_HTML_O_frameborder "frameborder" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_framespacing "framespacing" // attribute with a numerical value
#define OOO_STRING_SVTOOLS_HTML_O_gutter "gutter" // attribute with a numerical value
#define OOO_STRING_SVTOOLS_HTML_O_height "height" // attribute with a numerical value
#define OOO_STRING_SVTOOLS_HTML_O_href "href" // attribute with a URI as value
#define OOO_STRING_SVTOOLS_HTML_O_hspace "hspace" // attribute with a numerical value
#define OOO_STRING_SVTOOLS_HTML_O_httpequiv "http-equiv" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_id "id" // attribute with an SGML identifier as value
#define OOO_STRING_SVTOOLS_HTML_O_ismap "ismap" // attribute without value
#define OOO_STRING_SVTOOLS_HTML_O_lang "lang" // attribute with enum values
#define OOO_STRING_SVTOOLS_HTML_O_language "language" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_left "left" // attribute with a numerical value
#define OOO_STRING_SVTOOLS_HTML_O_link "link" // attribute with a colour as value (Netscape)
#define OOO_STRING_SVTOOLS_HTML_O_loop "loop" // attribute with a numerical value
#define OOO_STRING_SVTOOLS_HTML_O_marginheight "marginheight" // attribute with a numerical value
#define OOO_STRING_SVTOOLS_HTML_O_marginwidth "marginwidth" // attribute with a numerical value
#define OOO_STRING_SVTOOLS_HTML_O_maxlength "maxlength" // attribute with a numerical value
#define OOO_STRING_SVTOOLS_HTML_O_mayscript "mayscript" // attribute without value
#define OOO_STRING_SVTOOLS_HTML_O_method "method" // attribute with enum values
#define OOO_STRING_SVTOOLS_HTML_O_multiple "multiple" // attribute without value
#define OOO_STRING_SVTOOLS_HTML_O_name "name" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_nohref "nohref" // attribute without value
#define OOO_STRING_SVTOOLS_HTML_O_noresize "noresize" // attribute without value
#define OOO_STRING_SVTOOLS_HTML_O_noshade "noshade" // attribute without value
#define OOO_STRING_SVTOOLS_HTML_O_nowrap "nowrap" // attribute without value
#define OOO_STRING_SVTOOLS_HTML_O_onabort "onabort" // attribute with script code as value
#define OOO_STRING_SVTOOLS_HTML_O_onblur "onblur" // attribute with script code as value
#define OOO_STRING_SVTOOLS_HTML_O_onchange "onchange" // attribute with script code as value
#define OOO_STRING_SVTOOLS_HTML_O_onclick "onclick" // attribute with script code as value
#define OOO_STRING_SVTOOLS_HTML_O_onerror "onerror" // attribute with script code as value
#define OOO_STRING_SVTOOLS_HTML_O_onfocus "onfocus" // attribute with script code as value
#define OOO_STRING_SVTOOLS_HTML_O_onload "onload" // attribute with script code as value
#define OOO_STRING_SVTOOLS_HTML_O_onmouseout "onmouseout" // attribute with script code as value
#define OOO_STRING_SVTOOLS_HTML_O_onmouseover "onmouseover" // attribute with script code as value
#define OOO_STRING_SVTOOLS_HTML_O_onreset "onreset" // attribute with script code as value
#define OOO_STRING_SVTOOLS_HTML_O_onselect "onselect" // attribute with script code as value
#define OOO_STRING_SVTOOLS_HTML_O_onsubmit "onsubmit" // attribute with script code as value
#define OOO_STRING_SVTOOLS_HTML_O_onunload "onunload" // attribute with script code as value
#define OOO_STRING_SVTOOLS_HTML_O_prompt "prompt" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_sdreadonly "readonly" // attribute with enum values
#define OOO_STRING_SVTOOLS_HTML_O_rel "rel" // attribute with enum values
#define OOO_STRING_SVTOOLS_HTML_O_rev "rev" // attribute with enum values
#define OOO_STRING_SVTOOLS_HTML_O_rows "rows" // attribute with context-dependent values
#define OOO_STRING_SVTOOLS_HTML_O_rowspan "rowspan" // attribute with a numerical value
#define OOO_STRING_SVTOOLS_HTML_O_rules "rules" // attribute with enum values
#define OOO_STRING_SVTOOLS_HTML_O_script "script" // attribute with a URI as value
#define OOO_STRING_SVTOOLS_HTML_O_scrollamount "scrollamount" // attribute with a numerical value
#define OOO_STRING_SVTOOLS_HTML_O_scrolldelay "scrolldelay" // attribute with a numerical value
#define OOO_STRING_SVTOOLS_HTML_O_scrolling "scrolling" // attribute with enum values
#define OOO_STRING_SVTOOLS_HTML_O_sdaddparam "sdaddparam-" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_sdevent "sdevent-" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_sdfixed "sdfixed" // attribute without value
#define OOO_STRING_SVTOOLS_HTML_O_sdlibrary "sdlibrary" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_sdmodule "sdmodule" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_SDnum "sdnum" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_SDonabort "sdonabort" // attribute with script code as value
#define OOO_STRING_SVTOOLS_HTML_O_SDonblur "sdonblur" // attribute with script code as value
#define OOO_STRING_SVTOOLS_HTML_O_SDonchange "sdonchange" // attribute with script code as value
#define OOO_STRING_SVTOOLS_HTML_O_SDonclick "sdonclick" // attribute with script code as value
#define OOO_STRING_SVTOOLS_HTML_O_SDonerror "sdonerror" // attribute with script code as value
#define OOO_STRING_SVTOOLS_HTML_O_SDonfocus "sdonfocus" // attribute with script code as value
#define OOO_STRING_SVTOOLS_HTML_O_SDonload "sdonload" // attribute with script code as value
#define OOO_STRING_SVTOOLS_HTML_O_SDonmouseout "sdonmouseout" // attribute with script code as value
#define OOO_STRING_SVTOOLS_HTML_O_SDonmouseover                                                    \
    "sdonmouseover" // attribute with script code as value
#define OOO_STRING_SVTOOLS_HTML_O_SDonreset "sdonreset" // attribute with script code as value
#define OOO_STRING_SVTOOLS_HTML_O_SDonselect "sdonselect" // attribute with script code as value
#define OOO_STRING_SVTOOLS_HTML_O_SDonsubmit "sdonsubmit" // attribute with script code as value
#define OOO_STRING_SVTOOLS_HTML_O_SDonunload "sdonunload" // attribute with script code as value
#define OOO_STRING_SVTOOLS_HTML_O_SDval "sdval" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_selected "selected" // attribute without value
#define OOO_STRING_SVTOOLS_HTML_O_shape "shape" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_size "size" // attribute with context-dependent values
#define OOO_STRING_SVTOOLS_HTML_O_span "span" // attribute with a numerical value
#define OOO_STRING_SVTOOLS_HTML_O_src "src" // attribute with a URI as value
#define OOO_STRING_SVTOOLS_HTML_O_standby "standby" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_start "start" // attribute with context-dependent values
#define OOO_STRING_SVTOOLS_HTML_O_style "style" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_subtype "subtype" // attribute with enum values
#define OOO_STRING_SVTOOLS_HTML_O_tabindex "tabindex" // attribute with a numerical value
#define OOO_STRING_SVTOOLS_HTML_O_target "target" // attribute with an SGML identifier as value
#define OOO_STRING_SVTOOLS_HTML_O_text "text" // attribute with a colour as value (Netscape)
#define OOO_STRING_SVTOOLS_HTML_O_title "title" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_to "to" // attribute with an SGML identifier as value
#define OOO_STRING_SVTOOLS_HTML_O_type "type" // attribute with enum values
#define OOO_STRING_SVTOOLS_HTML_O_usemap "usemap" // attribute with a URI as value
#define OOO_STRING_SVTOOLS_HTML_O_valign "valign" // attribute with enum values
#define OOO_STRING_SVTOOLS_HTML_O_value "value" // attribute with a string as value
#define OOO_STRING_SVTOOLS_HTML_O_valuetype "valuetype" // attribute with enum values
#define OOO_STRING_SVTOOLS_HTML_O_vlink "vlink" // attribute with a colour as value (Netscape)
#define OOO_STRING_SVTOOLS_HTML_O_vspace "vspace" // attribute with a numerical value
#define OOO_STRING_SVTOOLS_HTML_O_width "width" // attribute with a numerical value
#define OOO_STRING_SVTOOLS_HTML_O_wrap "wrap" // attribute with enum values
#define OOO_STRING_SVTOOLS_XHTML_O_lang "xml:lang" // attribute with enum values
#define OOO_STRING_SVTOOLS_XHTML_O_xml_space "xml:space" // attribute with enum values
#define OOO_STRING_SVTOOLS_HTML_O_zindex "z-index" // attribute with a numerical value

// values of <INPUT TYPE=...>
#define OOO_STRING_SVTOOLS_HTML_IT_text "text"
#define OOO_STRING_SVTOOLS_HTML_IT_password "password"
#define OOO_STRING_SVTOOLS_HTML_IT_checkbox "checkbox"
#define OOO_STRING_SVTOOLS_HTML_IT_radio "radio"
#define OOO_STRING_SVTOOLS_HTML_IT_range "range"
#define OOO_STRING_SVTOOLS_HTML_IT_scribble "scribble"
#define OOO_STRING_SVTOOLS_HTML_IT_file "file"
#define OOO_STRING_SVTOOLS_HTML_IT_hidden "hidden"
#define OOO_STRING_SVTOOLS_HTML_IT_submit "submit"
#define OOO_STRING_SVTOOLS_HTML_IT_image "image"
#define OOO_STRING_SVTOOLS_HTML_IT_reset "reset"
#define OOO_STRING_SVTOOLS_HTML_IT_button "button"

// values of <TABLE FRAME=...>
#define OOO_STRING_SVTOOLS_HTML_TF_void "void"
#define OOO_STRING_SVTOOLS_HTML_TF_above "above"
#define OOO_STRING_SVTOOLS_HTML_TF_below "below"
#define OOO_STRING_SVTOOLS_HTML_TF_hsides "hsides"
#define OOO_STRING_SVTOOLS_HTML_TF_lhs "lhs"
#define OOO_STRING_SVTOOLS_HTML_TF_rhs "rhs"
#define OOO_STRING_SVTOOLS_HTML_TF_vsides "vsides"
#define OOO_STRING_SVTOOLS_HTML_TF_box "box"
#define OOO_STRING_SVTOOLS_HTML_TF_border "border"

// values of <TABLE RULES=...>
#define OOO_STRING_SVTOOLS_HTML_TR_none "none"
#define OOO_STRING_SVTOOLS_HTML_TR_groups "groups"
#define OOO_STRING_SVTOOLS_HTML_TR_rows "rows"
#define OOO_STRING_SVTOOLS_HTML_TR_cols "cols"
#define OOO_STRING_SVTOOLS_HTML_TR_all "all"

// values of <P, H?, TR, TH, TD ALIGN=...>
#define OOO_STRING_SVTOOLS_HTML_AL_left "left"
#define OOO_STRING_SVTOOLS_HTML_AL_center "center"
#define OOO_STRING_SVTOOLS_HTML_AL_middle "middle"
#define OOO_STRING_SVTOOLS_HTML_AL_right "right"
#define OOO_STRING_SVTOOLS_HTML_AL_justify "justify"
#define OOO_STRING_SVTOOLS_HTML_AL_char "char"
#define OOO_STRING_SVTOOLS_HTML_AL_all "all"

// values of <TR VALIGN=...>, <IMG ALIGN=...>
#define OOO_STRING_SVTOOLS_HTML_VA_top "top"
#define OOO_STRING_SVTOOLS_HTML_VA_middle "middle"
#define OOO_STRING_SVTOOLS_HTML_VA_bottom "bottom"
#define OOO_STRING_SVTOOLS_HTML_VA_baseline "baseline"
#define OOO_STRING_SVTOOLS_HTML_VA_texttop "texttop"
#define OOO_STRING_SVTOOLS_HTML_VA_absmiddle "absmiddle"
#define OOO_STRING_SVTOOLS_HTML_VA_absbottom "absbottom"

// values of <AREA SHAPE=...>
#define OOO_STRING_SVTOOLS_HTML_SH_rect "rect"
#define OOO_STRING_SVTOOLS_HTML_SH_rectangle "rectangle"
#define OOO_STRING_SVTOOLS_HTML_SH_circ "circ"
#define OOO_STRING_SVTOOLS_HTML_SH_circle "circle"
#define OOO_STRING_SVTOOLS_HTML_SH_poly "poly"
#define OOO_STRING_SVTOOLS_HTML_SH_polygon "polygon"

#define OOO_STRING_SVTOOLS_HTML_LG_starbasic "starbasic"
#define OOO_STRING_SVTOOLS_HTML_LG_javascript "javascript"
#define OOO_STRING_SVTOOLS_HTML_LG_javascript11 "javascript1.1"
#define OOO_STRING_SVTOOLS_HTML_LG_livescript "livescript"

// a few values for StarBASIC-Support
#define OOO_STRING_SVTOOLS_HTML_SB_library "$LIBRARY:"
#define OOO_STRING_SVTOOLS_HTML_SB_module "$MODULE:"

// values of <FORM METHOD=...>
#define OOO_STRING_SVTOOLS_HTML_METHOD_get "get"
#define OOO_STRING_SVTOOLS_HTML_METHOD_post "post"

// values of <META CONTENT/HTTP-EQUIV=...>
#define OOO_STRING_SVTOOLS_HTML_META_refresh "refresh"
#define OOO_STRING_SVTOOLS_HTML_META_generator "generator"
#define OOO_STRING_SVTOOLS_HTML_META_author "author"
#define OOO_STRING_SVTOOLS_HTML_META_classification "classification"
#define OOO_STRING_SVTOOLS_HTML_META_description "description"
#define OOO_STRING_SVTOOLS_HTML_META_keywords "keywords"
#define OOO_STRING_SVTOOLS_HTML_META_changed "changed"
#define OOO_STRING_SVTOOLS_HTML_META_changedby "changedby"
#define OOO_STRING_SVTOOLS_HTML_META_created "created"
#define OOO_STRING_SVTOOLS_HTML_META_content_type "content-type"
#define OOO_STRING_SVTOOLS_HTML_META_content_script_type "content-script-type"
#define OOO_STRING_SVTOOLS_HTML_META_sdendnote "sdendnote"
#define OOO_STRING_SVTOOLS_HTML_META_sdfootnote "sdfootnote"

// values of <UL TYPE=...>
#define OOO_STRING_SVTOOLS_HTML_ULTYPE_disc "disc"
#define OOO_STRING_SVTOOLS_HTML_ULTYPE_square "square"
#define OOO_STRING_SVTOOLS_HTML_ULTYPE_circle "circle"

// values of <MARQUEE BEHAVIOUR=...>
#define OOO_STRING_SVTOOLS_HTML_BEHAV_scroll "scroll"
#define OOO_STRING_SVTOOLS_HTML_BEHAV_slide "slide"
#define OOO_STRING_SVTOOLS_HTML_BEHAV_alternate "alternate"

// values of <MARQUEE LOOP=...>
#define OOO_STRING_SVTOOLS_HTML_LOOP_infinite "infinite"
#define OOO_STRING_SVTOOLS_HTML_SPTYPE_block "block"
#define OOO_STRING_SVTOOLS_HTML_SPTYPE_horizontal "horizontal"
#define OOO_STRING_SVTOOLS_HTML_SPTYPE_vertical "vertical"

// internal graphics names
#define OOO_STRING_SVTOOLS_HTML_private_image "private:image/"
#define OOO_STRING_SVTOOLS_HTML_internal_icon "internal-icon-"
#define OOO_STRING_SVTOOLS_HTML_INT_ICON_baddata "baddata"
#define OOO_STRING_SVTOOLS_HTML_INT_ICON_delayed "delayed"
#define OOO_STRING_SVTOOLS_HTML_INT_ICON_embed "embed"
#define OOO_STRING_SVTOOLS_HTML_INT_ICON_insecure "insecure"
#define OOO_STRING_SVTOOLS_HTML_INT_ICON_notfound "notfound"
#define OOO_STRING_SVTOOLS_HTML_sdendnote "sdendnote"
#define OOO_STRING_SVTOOLS_HTML_sdendnote_anc "sdendnoteanc"
#define OOO_STRING_SVTOOLS_HTML_sdendnote_sym "sdendnotesym"
#define OOO_STRING_SVTOOLS_HTML_sdfootnote "sdfootnote"
#define OOO_STRING_SVTOOLS_HTML_sdfootnote_anc "sdfootnoteanc"
#define OOO_STRING_SVTOOLS_HTML_sdfootnote_sym "sdfootnotesym"
#define OOO_STRING_SVTOOLS_HTML_FTN_anchor "anc"
#define OOO_STRING_SVTOOLS_HTML_FTN_symbol "sym"
#define OOO_STRING_SVTOOLS_HTML_WW_off "off"
#define OOO_STRING_SVTOOLS_HTML_WW_hard "hard"
#define OOO_STRING_SVTOOLS_HTML_WW_soft "soft"
#define OOO_STRING_SVTOOLS_HTML_WW_virtual "virtual"
#define OOO_STRING_SVTOOLS_HTML_WW_physical "physical"
#define OOO_STRING_SVTOOLS_HTML_on "on"
#define OOO_STRING_SVTOOLS_HTML_ET_url "application/x-www-form-urlencoded"
#define OOO_STRING_SVTOOLS_HTML_ET_multipart "multipart/form-data"
#define OOO_STRING_SVTOOLS_HTML_ET_text "text/plain"

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
