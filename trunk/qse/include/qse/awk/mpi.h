/*
 * $Id$
 *
    Copyright 2006-2012 Chung, Hyung-Hwan.
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

#ifndef _QSE_AWK_MPI_H_
#define _QSE_AWK_MPI_H_

#include <qse/awk/std.h>

/** @file
 * This file defines functions and data types for parallel processing.
 */

enum qse_awk_parsempi_type_t
{
	QSE_AWK_PARSEMPI_NULL = QSE_AWK_PARSESTD_NULL,
	QSE_AWK_PARSEMPI_FILE = QSE_AWK_PARSESTD_FILE,
	QSE_AWK_PARSEMPI_STR  = QSE_AWK_PARSESTD_STR
};

typedef enum qse_awk_parsempi_type_t qse_awk_parsempi_type_t;

typedef qse_awk_parsestd_t qse_awk_parsempi_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The qse_awk_openmpi() function creates an awk object using the default 
 * memory manager and primitive functions. Besides, it adds a set of
 * standard intrinsic functions like atan, system, etc. Use this function
 * over qse_awk_open() if you don't need finer-grained customization.
 */
qse_awk_t* qse_awk_openmpi (
	qse_size_t xtnsize  /**< extension size in bytes */
);

/**
 * The qse_awk_openmpiwithmmgr() function creates an awk object with a 
 * user-defined memory manager. It is equivalent to qse_awk_openmpi(), 
 * except that you can specify your own memory manager.
 */
qse_awk_t* qse_awk_openmpiwithmmgr (
	qse_mmgr_t* mmgr,    /**< memory manager */
	qse_size_t  xtnsize  /**< extension size in bytes */
);

/**
 * The qse_awk_getxtnmpi() gets the pointer to extension space. 
 * Note that you must not call qse_awk_getxtn() for an awk object
 * created with qse_awk_openmpi() and qse_awk_openmpiwithmmgr().
 */
void* qse_awk_getxtnmpi (
	qse_awk_t* awk
);

/**
 * The qse_awk_parsempi() functions parses source script.
 * The code below shows how to parse a literal string 'BEGIN { print 10; }' 
 * and deparses it out to a buffer 'buf'.
 * @code
 * int n;
 * qse_awk_parsempi_t in;
 * qse_awk_parsempi_t out;
 *
 * in.type = QSE_AWK_PARSESTD_STR;
 * in.u.str.ptr = QSE_T("BEGIN { print 10; }");
 * in.u.str.len = qse_strlen(in.u.str.ptr);
 * out.type = QSE_AWK_PARSESTD_STR;
 * n = qse_awk_parsempi (awk, &in, &out);
 * if (n >= 0) 
 * {
 *   qse_printf (QSE_T("%s\n"), out.u.str.ptr);
 *   QSE_MMGR_FREE (out.u.str.ptr);
 * }
 * @endcode
 */
int qse_awk_parsempi (
	qse_awk_t*          awk,
	qse_awk_parsempi_t* in,
	qse_awk_parsempi_t* out
);

/**
 * The qse_awk_rtx_openmpi() function creates a standard runtime context.
 * The caller should keep the contents of @a icf and @a ocf valid throughout
 * the lifetime of the runtime context created. The @a cmgr is set to the
 * streams created with @a icf and @a ocf if it is not #QSE_NULL.
 */
qse_awk_rtx_t* qse_awk_rtx_openmpi (
	qse_awk_t*             awk,
	qse_size_t             xtn,
	const qse_char_t*      id,
	const qse_char_t*const icf[],
	const qse_char_t*const ocf[],
	qse_cmgr_t*            cmgr
);

/**
 * The qse_awk_rtx_getxtnmpi() function gets the pointer to extension space.
 */
void* qse_awk_rtx_getxtnmpi (
	qse_awk_rtx_t* rtx
);


/**
 * The qse_awk_rtx_getcmgrmpi() function gets the current character 
 * manager associated with a particular I/O target indicated by the name 
 * @a ioname if #QSE_CHAR_IS_WCHAR is defined. It always returns #QSE_NULL
 * if #QSE_CHAR_IS_MCHAR is defined.
 */
qse_cmgr_t* qse_awk_rtx_getcmgrmpi (
	qse_awk_rtx_t*    rtx,
	const qse_char_t* ioname
);

#ifdef __cplusplus
}
#endif


#endif