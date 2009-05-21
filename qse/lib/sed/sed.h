/*
 * $Id$
 *
   Copyright 2006-2009 Chung, Hyung-Hwan.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 */

#ifndef _QSE_LIB_SED_SED_H_
#define _QSE_LIB_SED_SED_H_

#include <qse/sed/sed.h>
#include <qse/cmn/str.h>

typedef qse_int_t qse_sed_line_t;
typedef struct qse_sed_adr_t qse_sed_adr_t; 
typedef struct qse_sed_cmd_t qse_sed_cmd_t;

/** 
 * The qse_sed_t type defines a stream editor 
 */
struct qse_sed_t
{
	QSE_DEFINE_COMMON_FIELDS (sed)

	qse_sed_errnum_t errnum; /**< stores an error number */
	int option;              /**< stores options */

	/** source text pointers */
	struct
	{
		const qse_char_t* ptr; /**< beginning of the source text */
		const qse_char_t* end; /**< end of the source text */
		const qse_char_t* cur; /**< current source text pointer */
	} src;

	/** temporary regular expression buffer */
	qse_str_t rexbuf;

	/** compiled commands */
	struct
	{
		qse_sed_cmd_t* buf; /**< buffer holding compiled commands */
		qse_sed_cmd_t* end; /**< end of the buffer */
		qse_sed_cmd_t* cur; /**< points next to the last command */
	} cmd;

	/** a table storing labels seen */
	qse_map_t labs; 

	/** data structure to compile command groups */
	struct
	{
		/** current level of command group nesting */
		int level;
		/** keeps track of the begining of nested command groups */
		qse_sed_cmd_t* cmd[128];
	} grp;

	/** data for execution */
	struct
	{
		/** data needed for output streams and files */
		struct
		{
			qse_sed_io_fun_t fun; /**< an output handler */
			qse_sed_io_arg_t arg; /**< output handling data */

			qse_char_t buf[2048];
			qse_size_t len;
			int        eof;

			/*****************************************************/
			/* the following two fields are very tightly-coupled.
			 * don't make any partial changes */
			qse_map_t  files;
			qse_sed_t* files_ext;
			/*****************************************************/
		} out;

		/** data needed for input streams */
		struct
		{
			qse_sed_io_fun_t fun; /**< an input handler */
			qse_sed_io_arg_t arg; /**< input handling data */

			qse_char_t xbuf[1]; /**< a read-ahead buffer */
			int xbuf_len; /**< data length in the buffer */

			qse_char_t buf[2048]; /**< input buffer */
			qse_size_t len; /**< data length in the buffer */
			qse_size_t pos; /**< current position in the buffer */
			int        eof; /**< EOF indicator */

			qse_str_t line; /**< pattern space */
			qse_size_t num; /**< current line number */
		} in;

		/** text buffers */
		struct
		{
			qse_lda_t appended;
			qse_str_t read;
			qse_str_t held;
			qse_str_t subst;
		} txt;

		/** indicates if a successful substitution has been made 
		 *  since the last read on the input stream. */
		int subst_done;
	} e;
};

struct qse_sed_adr_t
{
	enum
	{
		QSE_SED_ADR_NONE,  /* no address */
		QSE_SED_ADR_DOL,   /* $ - last line */
		QSE_SED_ADR_LINE,  /* specified line */
		QSE_SED_ADR_REX,   /* lines matching regular expression */
		QSE_SED_ADR_STEP   /* line steps - only in the second address */
	} type;

	union 
	{
		qse_sed_line_t line;
		void*  rex;
	} u;
};

struct qse_sed_cmd_t
{
	enum
	{
		QSE_SED_CMD_QUIT           = QSE_T('q'),
		QSE_SED_CMD_QUIT_QUIET     = QSE_T('Q'),

		QSE_SED_CMD_APPEND         = QSE_T('a'),
		QSE_SED_CMD_INSERT         = QSE_T('i'),
		QSE_SED_CMD_CHANGE         = QSE_T('c'),

		QSE_SED_CMD_DELETE         = QSE_T('d'),
		QSE_SED_CMD_DELETE_FIRSTLN = QSE_T('D'),

		QSE_SED_CMD_PRINT_LNNUM    = QSE_T('='),
		QSE_SED_CMD_PRINT          = QSE_T('p'),
		QSE_SED_CMD_PRINT_FIRSTLN  = QSE_T('P'),
		QSE_SED_CMD_PRINT_CLEARLY  = QSE_T('l'),

		QSE_SED_CMD_HOLD           = QSE_T('h'),
		QSE_SED_CMD_HOLD_APPEND    = QSE_T('H'),
		QSE_SED_CMD_RELEASE        = QSE_T('g'),
		QSE_SED_CMD_RELEASE_APPEND = QSE_T('G'),
		QSE_SED_CMD_EXCHANGE       = QSE_T('x'), 

		QSE_SED_CMD_NEXT           = QSE_T('n'),
		QSE_SED_CMD_NEXT_APPEND    = QSE_T('N'),

		QSE_SED_CMD_READ_FILE      = QSE_T('r'),
		QSE_SED_CMD_READ_FILELN    = QSE_T('R'),
		QSE_SED_CMD_WRITE_FILE     = QSE_T('w'),
		QSE_SED_CMD_WRITE_FILELN   = QSE_T('W'),

		QSE_SED_CMD_BRANCH         = QSE_T('b'), 
		QSE_SED_CMD_BRANCH_COND    = QSE_T('t'),

		QSE_SED_CMD_SUBSTITUTE     = QSE_T('s'),
		QSE_SED_CMD_TRANSLATE      = QSE_T('y')

	} type;

	int negated;

	qse_sed_adr_t a1; /* optional start address */
	qse_sed_adr_t a2; /* optional end address */

	union
	{
		/* text for the a, i, c commands */
		qse_xstr_t text;  

		/* file name for r, w, R, W */
		qse_xstr_t file;

		/* data for the s command */
		struct
		{
			void* rex; /* regular expression */
			qse_xstr_t rpl;  /* replacement */

			/* flags */
			qse_xstr_t file; /* file name for w */
			unsigned short occ;
			unsigned short g: 1; /* global */
			unsigned short p: 1; /* print */
			unsigned short i: 1; /* case insensitive */
		} subst;

		/* translation set for the y command */
		qse_xstr_t transet;

		/* branch target for b and t */
		struct
		{
			qse_xstr_t label;
			qse_sed_cmd_t* target;
		} branch;
	} u;	

	struct
	{
		int a1_matched;
		int c_ready;
	} state;
};

#endif