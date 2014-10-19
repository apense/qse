/*
 * $Id$
 *
    Copyright 2006-2014 Chung, Hyung-Hwan.
    This file is part of QSE.

    QSE is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as 
    published by the Free Software Foundation, either version 3 of 
    the License, or (at your option) any later version.

    QSE is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public 
    License along with QSE. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _QSE_LIB_AWK_FNC_H_
#define _QSE_LIB_AWK_FNC_H_

struct qse_awk_fnc_t
{
	struct
	{
		qse_char_t* ptr;
		qse_size_t  len;
	} name;

	int dfl0; /* if set, ($0) is assumed if () is missing. 
	           * this ia mainly for the weird length() function */

	qse_awk_fnc_spec_t spec;
	const qse_char_t* owner; /* set this to a module name if a built-in function is located in a module */

	qse_awk_mod_t* mod; /* set by the engine to a valid pointer if it's associated to a module */
};

#ifdef __cplusplus
extern "C" {
#endif

qse_awk_fnc_t* qse_awk_findfnc (qse_awk_t* awk, const qse_cstr_t* name);

/* EXPORT is required for linking on windows as they are referenced by mod-str.c */
QSE_EXPORT int qse_awk_fnc_index   (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi);
QSE_EXPORT int qse_awk_fnc_rindex  (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi);
QSE_EXPORT int qse_awk_fnc_length  (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi);
QSE_EXPORT int qse_awk_fnc_substr  (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi);
QSE_EXPORT int qse_awk_fnc_split   (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi);
QSE_EXPORT int qse_awk_fnc_match   (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi);
QSE_EXPORT int qse_awk_fnc_gsub    (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi);
QSE_EXPORT int qse_awk_fnc_sub     (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi);
QSE_EXPORT int qse_awk_fnc_tolower (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi);
QSE_EXPORT int qse_awk_fnc_toupper (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi);
QSE_EXPORT int qse_awk_fnc_sprintf (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi);

#ifdef __cplusplus
}
#endif

#endif
