/*
 * $Id: awk.c 474 2011-05-23 16:52:37Z hyunghwan.chung $ 
 *
    Copyright 2006-2011 Chung, Hyung-Hwan.
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

#include "awk.h"

QSE_IMPLEMENT_COMMON_FUNCTIONS (awk)

static void free_fun (qse_htb_t* map, void* vptr, qse_size_t vlen)
{
	qse_awk_t* awk = *(qse_awk_t**)QSE_XTN(map);
	qse_awk_fun_t* f = (qse_awk_fun_t*)vptr;

	/* f->name doesn't have to be freed */
	/*QSE_AWK_FREE (awk, f->name);*/

	qse_awk_clrpt (awk, f->body);
	QSE_AWK_FREE (awk, f);
}

static void free_fnc (qse_htb_t* map, void* vptr, qse_size_t vlen)
{
	qse_awk_t* awk = *(qse_awk_t**)QSE_XTN(map);
	qse_awk_fnc_t* f = (qse_awk_fnc_t*)vptr;
	QSE_AWK_FREE (awk, f);
}

static int init_token (qse_mmgr_t* mmgr, qse_awk_tok_t* tok)
{
	tok->name = qse_str_open (mmgr, 0, 128);
	if (tok->name == QSE_NULL) return -1;
	
	tok->type = 0;
	tok->loc.file = QSE_NULL;
	tok->loc.line = 0;
	tok->loc.colm = 0;

	return 0;
}

static void fini_token (qse_awk_tok_t* tok)
{
	if (tok->name != QSE_NULL)
	{
		qse_str_close (tok->name);
		tok->name = QSE_NULL;
	}
}

static void clear_token (qse_awk_tok_t* tok)
{
	if (tok->name != QSE_NULL) qse_str_clear (tok->name);
	tok->type = 0;
	tok->loc.file = QSE_NULL;
	tok->loc.line = 0;
	tok->loc.colm = 0;
}

qse_awk_t* qse_awk_open (qse_mmgr_t* mmgr, qse_size_t xtn, qse_awk_prm_t* prm)
{
	qse_awk_t* awk;

	static qse_htb_mancbs_t treefuncbs =
	{
		{
			QSE_HTB_COPIER_INLINE,
			QSE_HTB_COPIER_DEFAULT 
		},
		{
			QSE_HTB_FREEER_DEFAULT,
			free_fun
		},
		QSE_HTB_COMPER_DEFAULT,
		QSE_HTB_KEEPER_DEFAULT,
		QSE_HTB_SIZER_DEFAULT,
		QSE_HTB_HASHER_DEFAULT
	};

	static qse_htb_mancbs_t fncusercbs =
	{
		{
			QSE_HTB_COPIER_INLINE,
			QSE_HTB_COPIER_DEFAULT 
		},
		{
			QSE_HTB_FREEER_DEFAULT,
			free_fnc
		},
		QSE_HTB_COMPER_DEFAULT,
		QSE_HTB_KEEPER_DEFAULT,
		QSE_HTB_SIZER_DEFAULT,
		QSE_HTB_HASHER_DEFAULT
	};

	/* allocate the object */
	awk = QSE_MMGR_ALLOC (mmgr, QSE_SIZEOF(qse_awk_t) + xtn);
	if (awk == QSE_NULL) return QSE_NULL;

	/* zero out the object */
	QSE_MEMSET (awk, 0, QSE_SIZEOF(qse_awk_t) + xtn);

	/* remember the memory manager */
	awk->mmgr = mmgr;

	/* progagate the primitive functions */
	QSE_ASSERT (prm             != QSE_NULL);
	QSE_ASSERT (prm->sprintf    != QSE_NULL);
	QSE_ASSERT (prm->math.pow   != QSE_NULL);
	QSE_ASSERT (prm->math.sin   != QSE_NULL);
	QSE_ASSERT (prm->math.cos   != QSE_NULL);
	QSE_ASSERT (prm->math.tan   != QSE_NULL);
	QSE_ASSERT (prm->math.atan  != QSE_NULL);
	QSE_ASSERT (prm->math.atan2 != QSE_NULL);
	QSE_ASSERT (prm->math.log   != QSE_NULL);
	QSE_ASSERT (prm->math.log10 != QSE_NULL);
	QSE_ASSERT (prm->math.exp   != QSE_NULL);
	QSE_ASSERT (prm->math.sqrt  != QSE_NULL);
	if (prm             == QSE_NULL || 
	    prm->sprintf    == QSE_NULL ||
	    prm->math.pow   == QSE_NULL ||
	    prm->math.sin   == QSE_NULL ||
	    prm->math.cos   == QSE_NULL ||
	    prm->math.tan   == QSE_NULL ||
	    prm->math.atan  == QSE_NULL ||
	    prm->math.atan2 == QSE_NULL ||
	    prm->math.log   == QSE_NULL ||
	    prm->math.log10 == QSE_NULL ||
	    prm->math.exp   == QSE_NULL ||
	    prm->math.sqrt  == QSE_NULL)
	{
		QSE_AWK_FREE (awk, awk);
		return QSE_NULL;
	}
	awk->prm = *prm;

	if (init_token (mmgr, &awk->ptok) == -1) goto oops;
	if (init_token (mmgr, &awk->tok) == -1) goto oops;
	if (init_token (mmgr, &awk->ntok) == -1) goto oops;

	awk->sio.names = qse_htb_open (
		mmgr, QSE_SIZEOF(awk), 128, 70, QSE_SIZEOF(qse_char_t), 1
	);
	if (awk->sio.names == QSE_NULL) goto oops;
	*(qse_awk_t**)QSE_XTN(awk->sio.names) = awk;
	qse_htb_setmancbs (awk->sio.names, 
		qse_gethtbmancbs(QSE_HTB_MANCBS_INLINE_KEY_COPIER)
	);
	awk->sio.inp = &awk->sio.arg;

	/* TODO: initial map size?? */
	awk->tree.funs = qse_htb_open (
		mmgr, QSE_SIZEOF(awk), 512, 70, QSE_SIZEOF(qse_char_t), 1
	);
	if (awk->tree.funs == QSE_NULL) goto oops;
	*(qse_awk_t**)QSE_XTN(awk->tree.funs) = awk;
	qse_htb_setmancbs (awk->tree.funs, &treefuncbs);

	awk->parse.funs = qse_htb_open (
		mmgr, QSE_SIZEOF(awk), 256, 70, QSE_SIZEOF(qse_char_t), 1
	);
	if (awk->parse.funs == QSE_NULL) goto oops;
	*(qse_awk_t**)QSE_XTN(awk->parse.funs) = awk;
	qse_htb_setmancbs (awk->parse.funs,
		qse_gethtbmancbs(QSE_HTB_MANCBS_INLINE_KEY_COPIER)
	);

	awk->parse.named = qse_htb_open (
		mmgr, QSE_SIZEOF(awk), 256, 70, QSE_SIZEOF(qse_char_t), 1
	);
	if (awk->parse.named == QSE_NULL) goto oops;
	*(qse_awk_t**)QSE_XTN(awk->parse.named) = awk;
	qse_htb_setmancbs (awk->parse.named,
		qse_gethtbmancbs(QSE_HTB_MANCBS_INLINE_KEY_COPIER)
	);

	awk->parse.gbls = qse_lda_open (mmgr, QSE_SIZEOF(awk), 128);
	awk->parse.lcls = qse_lda_open (mmgr, QSE_SIZEOF(awk), 64);
	awk->parse.params = qse_lda_open (mmgr, QSE_SIZEOF(awk), 32);

	if (awk->parse.gbls == QSE_NULL ||
	    awk->parse.lcls == QSE_NULL ||
	    awk->parse.params == QSE_NULL) goto oops;

	*(qse_awk_t**)QSE_XTN(awk->parse.gbls) = awk;
	qse_lda_setscale (awk->parse.gbls, QSE_SIZEOF(qse_char_t));
	qse_lda_setcopier (awk->parse.gbls, QSE_LDA_COPIER_INLINE);

	*(qse_awk_t**)QSE_XTN(awk->parse.lcls) = awk;
	qse_lda_setscale (awk->parse.lcls, QSE_SIZEOF(qse_char_t));
	qse_lda_setcopier (awk->parse.lcls, QSE_LDA_COPIER_INLINE);

	*(qse_awk_t**)QSE_XTN(awk->parse.params) = awk;
	qse_lda_setscale (awk->parse.params, QSE_SIZEOF(qse_char_t));
	qse_lda_setcopier (awk->parse.params, QSE_LDA_COPIER_INLINE);

	awk->option = QSE_AWK_CLASSIC;
#if defined(__OS2__) || defined(_WIN32) || defined(__DOS__)
	awk->option |= QSE_AWK_CRLF;
#endif

	awk->errinf.num = QSE_AWK_ENOERR;
	awk->errinf.loc.line = 0;
	awk->errinf.loc.colm = 0;
	awk->errinf.loc.file = QSE_NULL;
	awk->errstr = qse_awk_dflerrstr;
	awk->stopall = QSE_FALSE;

	awk->tree.ngbls = 0;
	awk->tree.ngbls_base = 0;
	awk->tree.begin = QSE_NULL;
	awk->tree.begin_tail = QSE_NULL;
	awk->tree.end = QSE_NULL;
	awk->tree.end_tail = QSE_NULL;
	awk->tree.chain = QSE_NULL;
	awk->tree.chain_tail = QSE_NULL;
	awk->tree.chain_size = 0;

	awk->fnc.sys = QSE_NULL;
	awk->fnc.user = qse_htb_open (
		mmgr, QSE_SIZEOF(awk), 512, 70, QSE_SIZEOF(qse_char_t), 1
	);
	if (awk->fnc.user == QSE_NULL) goto oops;
	*(qse_awk_t**)QSE_XTN(awk->fnc.user) = awk;
	qse_htb_setmancbs (awk->fnc.user, &fncusercbs);

	if (qse_awk_initgbls (awk) <= -1) goto oops;

	return awk;

oops:
	if (awk->fnc.user) qse_htb_close (awk->fnc.user);
	if (awk->parse.params) qse_lda_close (awk->parse.params);
	if (awk->parse.lcls) qse_lda_close (awk->parse.lcls);
	if (awk->parse.gbls) qse_lda_close (awk->parse.gbls);
	if (awk->parse.named) qse_htb_close (awk->parse.named);
	if (awk->parse.funs) qse_htb_close (awk->parse.funs);
	if (awk->tree.funs) qse_htb_close (awk->tree.funs);
	if (awk->sio.names) qse_htb_close (awk->sio.names);
	fini_token (&awk->ntok);
	fini_token (&awk->tok);
	fini_token (&awk->ptok);
	QSE_AWK_FREE (awk, awk);

	return QSE_NULL;
}

int qse_awk_close (qse_awk_t* awk)
{
	if (qse_awk_clear (awk) <= -1) return -1;
	/*qse_awk_clrfnc (awk);*/
	qse_htb_close (awk->fnc.user);

	qse_lda_close (awk->parse.params);
	qse_lda_close (awk->parse.lcls);
	qse_lda_close (awk->parse.gbls);
	qse_htb_close (awk->parse.named);
	qse_htb_close (awk->parse.funs);

	qse_htb_close (awk->tree.funs);
	qse_htb_close (awk->sio.names);

	fini_token (&awk->ntok);
	fini_token (&awk->tok);
	fini_token (&awk->ptok);

	/* QSE_AWK_ALLOC, QSE_AWK_FREE, etc can not be used 
	 * from the next line onwards */
	QSE_AWK_FREE (awk, awk);
	return 0;
}

int qse_awk_clear (qse_awk_t* awk)
{
	awk->stopall = QSE_FALSE;

	clear_token (&awk->tok);
	clear_token (&awk->ntok);
	clear_token (&awk->ptok);

	QSE_ASSERT (QSE_LDA_SIZE(awk->parse.gbls) == awk->tree.ngbls);
	/* delete all non-builtin global variables */
	qse_lda_delete (
		awk->parse.gbls, awk->tree.ngbls_base, 
		QSE_LDA_SIZE(awk->parse.gbls) - awk->tree.ngbls_base);

	qse_lda_clear (awk->parse.lcls);
	qse_lda_clear (awk->parse.params);
	qse_htb_clear (awk->parse.named);
	qse_htb_clear (awk->parse.funs);

	awk->parse.nlcls_max = 0; 
	awk->parse.depth.cur.block = 0;
	awk->parse.depth.cur.loop = 0;
	awk->parse.depth.cur.expr = 0;
	awk->parse.depth.cur.incl = 0;

	/* clear parse trees */	
	/*awk->tree.ngbls_base = 0;
	awk->tree.ngbls = 0;	 */
	awk->tree.ngbls = awk->tree.ngbls_base;

	awk->tree.cur_fun.ptr = QSE_NULL;
	awk->tree.cur_fun.len = 0;
	qse_htb_clear (awk->tree.funs);

	if (awk->tree.begin != QSE_NULL) 
	{
		/*QSE_ASSERT (awk->tree.begin->next == QSE_NULL);*/
		qse_awk_clrpt (awk, awk->tree.begin);
		awk->tree.begin = QSE_NULL;
		awk->tree.begin_tail = QSE_NULL;	
	}

	if (awk->tree.end != QSE_NULL) 
	{
		/*QSE_ASSERT (awk->tree.end->next == QSE_NULL);*/
		qse_awk_clrpt (awk, awk->tree.end);
		awk->tree.end = QSE_NULL;
		awk->tree.end_tail = QSE_NULL;	
	}

	while (awk->tree.chain != QSE_NULL) 
	{
		qse_awk_chain_t* next = awk->tree.chain->next;

		if (awk->tree.chain->pattern != QSE_NULL)
			qse_awk_clrpt (awk, awk->tree.chain->pattern);
		if (awk->tree.chain->action != QSE_NULL)
			qse_awk_clrpt (awk, awk->tree.chain->action);
		QSE_AWK_FREE (awk, awk->tree.chain);
		awk->tree.chain = next;
	}

	awk->tree.chain_tail = QSE_NULL;	
	awk->tree.chain_size = 0;

	QSE_ASSERT (awk->sio.inp == &awk->sio.arg);
	/* this table must not be cleared here as there can be a reference
	 * to an entry of this table from errinf.fil when qse_awk_parse() 
	 * failed. this table is cleared in qse_awk_parse().
	 * qse_htb_clear (awk->sio.names);
	 */

	awk->sio.last.c = QSE_CHAR_EOF;
	awk->sio.last.line = 0;
	awk->sio.last.colm = 0;
	awk->sio.last.file = QSE_NULL;
	awk->sio.nungots = 0;

	awk->sio.arg.line = 1;
	awk->sio.arg.colm = 1;
	awk->sio.arg.b.pos = 0;
	awk->sio.arg.b.len = 0;

	return 0;
}

qse_awk_prm_t* qse_awk_getprm (qse_awk_t* awk)
{
	return &awk->prm;
}

int qse_awk_getoption (const qse_awk_t* awk)
{
	return awk->option;
}

void qse_awk_setoption (qse_awk_t* awk, int opt)
{
	awk->option = opt;
}

void qse_awk_stopall (qse_awk_t* awk)
{
	awk->stopall = QSE_TRUE;
}

qse_size_t qse_awk_getmaxdepth (const qse_awk_t* awk, qse_awk_depth_t type)
{
	return (type == QSE_AWK_DEPTH_BLOCK_PARSE)? awk->parse.depth.max.block:
	       (type == QSE_AWK_DEPTH_BLOCK_RUN)? awk->run.depth.max.block:
	       (type == QSE_AWK_DEPTH_EXPR_PARSE)? awk->parse.depth.max.expr:
	       (type == QSE_AWK_DEPTH_EXPR_RUN)? awk->run.depth.max.expr:
	       (type == QSE_AWK_DEPTH_REX_BUILD)? awk->rex.depth.max.build:
	       (type == QSE_AWK_DEPTH_REX_MATCH)? awk->rex.depth.max.match: 
	       (type == QSE_AWK_DEPTH_INCLUDE)? awk->parse.depth.max.incl: 0;
}

void qse_awk_setmaxdepth (qse_awk_t* awk, int types, qse_size_t depth)
{
	if (types & QSE_AWK_DEPTH_BLOCK_PARSE)
	{
		awk->parse.depth.max.block = depth;
	}

	if (types & QSE_AWK_DEPTH_EXPR_PARSE)
	{
		awk->parse.depth.max.expr = depth;
	}

	if (types & QSE_AWK_DEPTH_BLOCK_RUN)
	{
		awk->run.depth.max.block = depth;
	}

	if (types & QSE_AWK_DEPTH_EXPR_RUN)
	{
		awk->run.depth.max.expr = depth;
	}

	if (types & QSE_AWK_DEPTH_REX_BUILD)
	{
		awk->rex.depth.max.build = depth;
	}

	if (types & QSE_AWK_DEPTH_REX_MATCH)
	{
		awk->rex.depth.max.match = depth;
	}

	if (types & QSE_AWK_DEPTH_INCLUDE)
	{
		awk->parse.depth.max.incl = depth;
	}
}