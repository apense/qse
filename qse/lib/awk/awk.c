/*
 * $Id: awk.c 501 2008-12-17 08:39:15Z baconevi $ 
 *
   Copyright 2006-2008 Chung, Hyung-Hwan.

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

#if defined(__BORLANDC__)
#pragma hdrstop
#define Library
#endif

#include "awk.h"

#define SETERR(awk,code) qse_awk_seterrnum(awk,code)

#define SETERRARG(awk,code,line,arg,leng) \
	do { \
		qse_cstr_t errarg; \
		errarg.len = (leng); \
		errarg.ptr = (arg); \
		qse_awk_seterror ((awk), (code), (line), &errarg, 1); \
	} while (0)

static void free_afn (qse_map_t* map, void* vptr, qse_size_t vlen)
{
	qse_awk_t* awk = *(qse_awk_t**)QSE_MAP_XTN(map);
	qse_awk_afn_t* f = (qse_awk_afn_t*)vptr;

	/* f->name doesn't have to be freed */
	/*QSE_AWK_FREE (awk, f->name);*/

	qse_awk_clrpt (awk, f->body);
	QSE_AWK_FREE (awk, f);
}

static void free_bfn (qse_map_t* map, void* vptr, qse_size_t vlen)
{
	qse_awk_t* awk = *(qse_awk_t**)QSE_MAP_XTN(map);
	qse_awk_bfn_t* f = (qse_awk_bfn_t*)vptr;

	QSE_AWK_FREE (awk, f);
}

qse_awk_t* qse_awk_open (qse_mmgr_t* mmgr, qse_size_t ext)
{
	qse_awk_t* awk;

	if (mmgr == QSE_NULL) 
	{
		mmgr = QSE_MMGR_GETDFL();

		QSE_ASSERTX (mmgr != QSE_NULL,
			"Set the memory manager with QSE_MMGR_SETDFL()");

		if (mmgr == QSE_NULL) return QSE_NULL;
	}

	awk = QSE_MMGR_ALLOC (mmgr, QSE_SIZEOF(qse_awk_t) + ext);
	if (awk == QSE_NULL) return QSE_NULL;

	QSE_MEMSET (awk, 0, QSE_SIZEOF(qse_awk_t) + ext);
	awk->mmgr = mmgr;

	awk->token.name = qse_str_open (mmgr, 0, 128);
	if (awk->token.name == QSE_NULL) goto oops;

	awk->wtab = qse_map_open (mmgr, QSE_SIZEOF(awk), 512, 70);
	if (awk->wtab == QSE_NULL) goto oops;
	*(qse_awk_t**)QSE_MAP_XTN(awk->wtab) = awk;
	qse_map_setcopier (awk->wtab, QSE_MAP_KEY, QSE_MAP_COPIER_INLINE);
	qse_map_setcopier (awk->wtab, QSE_MAP_VAL, QSE_MAP_COPIER_INLINE);
	qse_map_setscale (awk->wtab, QSE_MAP_KEY, QSE_SIZEOF(qse_char_t));
	qse_map_setscale (awk->wtab, QSE_MAP_VAL, QSE_SIZEOF(qse_char_t));

	awk->rwtab = qse_map_open (mmgr, QSE_SIZEOF(awk), 512, 70);
	if (awk->rwtab == QSE_NULL) goto oops;
	*(qse_awk_t**)QSE_MAP_XTN(awk->rwtab) = awk;
	qse_map_setcopier (awk->rwtab, QSE_MAP_KEY, QSE_MAP_COPIER_INLINE);
	qse_map_setcopier (awk->rwtab, QSE_MAP_VAL, QSE_MAP_COPIER_INLINE);
	qse_map_setscale (awk->rwtab, QSE_MAP_KEY, QSE_SIZEOF(qse_char_t));
	qse_map_setscale (awk->rwtab, QSE_MAP_VAL, QSE_SIZEOF(qse_char_t));

	/* TODO: initial map size?? */
	awk->tree.afns = qse_map_open (mmgr, QSE_SIZEOF(awk), 512, 70);
	if (awk->tree.afns == QSE_NULL) goto oops;
	*(qse_awk_t**)QSE_MAP_XTN(awk->tree.afns) = awk;
	qse_map_setcopier (awk->tree.afns, QSE_MAP_KEY, QSE_MAP_COPIER_INLINE);
	qse_map_setfreeer (awk->tree.afns, QSE_MAP_VAL, free_afn);
	qse_map_setscale (awk->tree.afns, QSE_MAP_KEY, QSE_SIZEOF(qse_char_t));

	awk->parse.afns = qse_map_open (mmgr, QSE_SIZEOF(awk), 256, 70);
	if (awk->parse.afns == QSE_NULL) goto oops;
	*(qse_awk_t**)QSE_MAP_XTN(awk->parse.afns) = awk;
	qse_map_setcopier (awk->parse.afns, QSE_MAP_KEY, QSE_MAP_COPIER_INLINE);
	qse_map_setcopier (awk->parse.afns, QSE_MAP_VAL, QSE_MAP_COPIER_INLINE);
	qse_map_setscale (awk->parse.afns, QSE_MAP_KEY, QSE_SIZEOF(qse_char_t));

	awk->parse.named = qse_map_open (mmgr, QSE_SIZEOF(awk), 256, 70);
	if (awk->parse.named == QSE_NULL) goto oops;
	*(qse_awk_t**)QSE_MAP_XTN(awk->parse.named) = awk;
	qse_map_setcopier (awk->parse.named, QSE_MAP_KEY, QSE_MAP_COPIER_INLINE);
	qse_map_setcopier (awk->parse.named, QSE_MAP_VAL, QSE_MAP_COPIER_INLINE);
	qse_map_setscale (awk->parse.named, QSE_MAP_KEY, QSE_SIZEOF(qse_char_t));

	awk->parse.globals = qse_lda_open (mmgr, QSE_SIZEOF(awk), 128);
	awk->parse.locals = qse_lda_open (mmgr, QSE_SIZEOF(awk), 64);
	awk->parse.params = qse_lda_open (mmgr, QSE_SIZEOF(awk), 32);

	if (awk->parse.globals == QSE_NULL ||
	    awk->parse.locals == QSE_NULL ||
	    awk->parse.params == QSE_NULL) goto oops;

	*(qse_awk_t**)QSE_LDA_XTN(awk->parse.globals) = awk;
	qse_lda_setcopier (awk->parse.globals, QSE_LDA_COPIER_INLINE);
	qse_lda_setscale (awk->parse.globals, QSE_SIZEOF(qse_char_t));

	*(qse_awk_t**)QSE_LDA_XTN(awk->parse.locals) = awk;
	qse_lda_setcopier (awk->parse.locals, QSE_LDA_COPIER_INLINE);
	qse_lda_setscale (awk->parse.locals, QSE_SIZEOF(qse_char_t));

	*(qse_awk_t**)QSE_LDA_XTN(awk->parse.params) = awk;
	qse_lda_setcopier (awk->parse.params, QSE_LDA_COPIER_INLINE);
	qse_lda_setscale (awk->parse.params, QSE_SIZEOF(qse_char_t));

	awk->option = 0;
	awk->errnum = QSE_AWK_ENOERR;
	awk->errlin = 0;
	awk->stopall = QSE_FALSE;

	awk->parse.nlocals_max = 0;

	awk->tree.nglobals = 0;
	awk->tree.nbglobals = 0;
	awk->tree.begin = QSE_NULL;
	awk->tree.begin_tail = QSE_NULL;
	awk->tree.end = QSE_NULL;
	awk->tree.end_tail = QSE_NULL;
	awk->tree.chain = QSE_NULL;
	awk->tree.chain_tail = QSE_NULL;
	awk->tree.chain_size = 0;

	awk->token.prev.type = 0;
	awk->token.prev.line = 0;
	awk->token.prev.column = 0;
	awk->token.type = 0;
	awk->token.line = 0;
	awk->token.column = 0;

	awk->src.lex.curc = QSE_CHAR_EOF;
	awk->src.lex.ungotc_count = 0;
	awk->src.lex.line = 1;
	awk->src.lex.column = 1;
	awk->src.shared.buf_pos = 0;
	awk->src.shared.buf_len = 0;

	awk->bfn.sys = QSE_NULL;
	awk->bfn.user = qse_map_open (mmgr, QSE_SIZEOF(awk), 512, 70);
	if (awk->bfn.user == QSE_NULL) goto oops;
	*(qse_awk_t**)QSE_MAP_XTN(awk->bfn.user) = awk;
	qse_map_setcopier (awk->bfn.user, QSE_MAP_KEY, QSE_MAP_COPIER_INLINE);
	qse_map_setfreeer (awk->bfn.user, QSE_MAP_VAL, free_bfn); 
	qse_map_setscale (awk->bfn.user, QSE_MAP_KEY, QSE_SIZEOF(qse_char_t));

	awk->parse.depth.cur.block = 0;
	awk->parse.depth.cur.loop = 0;
	awk->parse.depth.cur.expr = 0;

	qse_awk_setmaxdepth (awk, QSE_AWK_DEPTH_BLOCK_PARSE, 0);
	qse_awk_setmaxdepth (awk, QSE_AWK_DEPTH_BLOCK_RUN, 0);
	qse_awk_setmaxdepth (awk, QSE_AWK_DEPTH_EXPR_PARSE, 0);
	qse_awk_setmaxdepth (awk, QSE_AWK_DEPTH_EXPR_RUN, 0);
	qse_awk_setmaxdepth (awk, QSE_AWK_DEPTH_REX_BUILD, 0);
	qse_awk_setmaxdepth (awk, QSE_AWK_DEPTH_REX_MATCH, 0);

	awk->assoc_data = QSE_NULL;

	if (qse_awk_initglobals (awk) == -1) goto oops;

	return awk;


oops:
	if (awk->bfn.user) qse_map_close (awk->bfn.user);
	if (awk->parse.params) qse_lda_close (awk->parse.params);
	if (awk->parse.locals) qse_lda_close (awk->parse.locals);
	if (awk->parse.globals) qse_lda_close (awk->parse.globals);
	if (awk->parse.named) qse_map_close (awk->parse.named);
	if (awk->parse.afns) qse_map_close (awk->parse.afns);
	if (awk->tree.afns) qse_map_close (awk->tree.afns);
	if (awk->rwtab) qse_map_close (awk->rwtab);
	if (awk->wtab) qse_map_close (awk->wtab);
	if (awk->token.name) qse_str_close (awk->token.name);
	QSE_AWK_FREE (awk, awk);

	return QSE_NULL;
}


int qse_awk_close (qse_awk_t* awk)
{
	qse_size_t i;

	if (qse_awk_clear (awk) == -1) return -1;
	/*qse_awk_clrbfn (awk);*/
	qse_map_close (awk->bfn.user);

	qse_lda_close (awk->parse.params);
	qse_lda_close (awk->parse.locals);
	qse_lda_close (awk->parse.globals);
	qse_map_close (awk->parse.named);
	qse_map_close (awk->parse.afns);

	qse_map_close (awk->tree.afns);
	qse_map_close (awk->rwtab);
	qse_map_close (awk->wtab);

	qse_str_close (awk->token.name);

	for (i = 0; i < QSE_COUNTOF(awk->errstr); i++)
	{
		if (awk->errstr[i] != QSE_NULL)
		{
			QSE_AWK_FREE (awk, awk->errstr[i]);
			awk->errstr[i] = QSE_NULL;
		}
	}

	/* QSE_AWK_ALLOC, QSE_AWK_FREE, etc can not be used 
	 * from the next line onwards */
	QSE_AWK_FREE (awk, awk);
	return 0;
}

int qse_awk_clear (qse_awk_t* awk)
{
	awk->stopall = QSE_FALSE;

	QSE_MEMSET (&awk->src.ios, 0, QSE_SIZEOF(awk->src.ios));
	awk->src.lex.curc = QSE_CHAR_EOF;
	awk->src.lex.ungotc_count = 0;
	awk->src.lex.line = 1;
	awk->src.lex.column = 1;
	awk->src.shared.buf_pos = 0;
	awk->src.shared.buf_len = 0;

	QSE_ASSERT (QSE_LDA_SIZE(awk->parse.globals) == awk->tree.nglobals);
	/* delete all non-builtin global variables */
	qse_lda_delete (
		awk->parse.globals, awk->tree.nbglobals, 
		QSE_LDA_SIZE(awk->parse.globals) - awk->tree.nbglobals);

	qse_lda_clear (awk->parse.locals);
	qse_lda_clear (awk->parse.params);
	qse_map_clear (awk->parse.named);
	qse_map_clear (awk->parse.afns);

	awk->parse.nlocals_max = 0; 
	awk->parse.depth.cur.block = 0;
	awk->parse.depth.cur.loop = 0;
	awk->parse.depth.cur.expr = 0;

	/* clear parse trees */	
	awk->tree.ok = 0;
	/*awk->tree.nbglobals = 0;
	awk->tree.nglobals = 0;	 */
	awk->tree.nglobals = awk->tree.nbglobals;

	awk->tree.cur_afn.ptr = QSE_NULL;
	awk->tree.cur_afn.len = 0;
	qse_map_clear (awk->tree.afns);

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

	return 0;
}

void* qse_awk_getxtn (qse_awk_t* awk)
{
	return (void*)(awk + 1);
}

qse_mmgr_t* qse_awk_getmmgr (qse_awk_t* awk)
{
	return awk->mmgr;
}

void qse_awk_setmmgr (qse_awk_t* awk, qse_mmgr_t* mmgr)
{
	awk->mmgr = mmgr;
}

qse_ccls_t* qse_awk_getccls (qse_awk_t* awk)
{
	return awk->ccls;
}

void qse_awk_setccls (qse_awk_t* awk, qse_ccls_t* ccls)
{
	QSE_ASSERT (ccls->is != QSE_NULL);
	QSE_ASSERT (ccls->to != QSE_NULL);
	awk->ccls = ccls;
}

qse_awk_prmfns_t* qse_awk_getprmfns (qse_awk_t* awk)
{
	return awk->prmfns;
}

void qse_awk_setprmfns (qse_awk_t* awk, qse_awk_prmfns_t* prmfns)
{
	QSE_ASSERT (prmfns->pow     != QSE_NULL);
	QSE_ASSERT (prmfns->sprintf != QSE_NULL);

	awk->prmfns = prmfns;
}

int qse_awk_getoption (qse_awk_t* awk)
{
	return awk->option;
}

void qse_awk_setoption (qse_awk_t* awk, int opt)
{
	awk->option = opt;
}


void qse_awk_rtx_stopall (qse_awk_t* awk)
{
	awk->stopall = QSE_TRUE;
}

int qse_awk_getword (qse_awk_t* awk, 
	const qse_char_t* okw, qse_size_t olen,
	const qse_char_t** nkw, qse_size_t* nlen)
{
	qse_map_pair_t* p;

	p = qse_map_search (awk->wtab, okw, olen);
	if (p == QSE_NULL) return -1;

	*nkw = ((qse_cstr_t*)p->vptr)->ptr;
	*nlen = ((qse_cstr_t*)p->vptr)->len;

	return 0;
}

int qse_awk_unsetword (qse_awk_t* awk, const qse_char_t* kw, qse_size_t len)
{
	qse_map_pair_t* p;

	p = qse_map_search (awk->wtab, kw, len);
	if (p == QSE_NULL)
	{
		SETERRARG (awk, QSE_AWK_ENOENT, 0, kw, len);
		return -1;
	}

	qse_map_delete (awk->rwtab, QSE_MAP_VPTR(p), QSE_MAP_VLEN(p));
	qse_map_delete (awk->wtab, kw, len);
	return 0;
}

void qse_awk_unsetallwords (qse_awk_t* awk)
{
	qse_map_clear (awk->wtab);
	qse_map_clear (awk->rwtab);
}

int qse_awk_setword (qse_awk_t* awk, 
	const qse_char_t* okw, qse_size_t olen,
	const qse_char_t* nkw, qse_size_t nlen)
{
	if (nkw == QSE_NULL || nlen == 0)
	{
		if (okw == QSE_NULL || olen == 0)
		{
			/* clear the entire table */
			qse_awk_unsetallwords (awk);
			return 0;
		}

		return qse_awk_unsetword (awk, okw, olen);
	}
	else if (okw == QSE_NULL || olen == 0)
	{
		SETERR (awk, QSE_AWK_EINVAL);
		return -1;
	}

	/* set the word */
	if (qse_map_upsert (awk->wtab, 
		(qse_char_t*)okw, olen, (qse_char_t*)nkw, nlen) == QSE_NULL)
	{
		SETERR (awk, QSE_AWK_ENOMEM);
		return -1;
	}

	if (qse_map_upsert (awk->rwtab, 
		(qse_char_t*)nkw, nlen, (qse_char_t*)okw, olen) == QSE_NULL)
	{
		qse_map_delete (awk->wtab, okw, olen);
		SETERR (awk, QSE_AWK_ENOMEM);
		return -1;
	}
 
	return 0;
}

/* TODO: XXXX */
int qse_awk_setrexfns (qse_awk_t* awk, qse_awk_rexfns_t* rexfns)
{
	if (rexfns->build == QSE_NULL ||
	    rexfns->match == QSE_NULL ||
	    rexfns->free == QSE_NULL ||
	    rexfns->isempty == QSE_NULL)
	{
		SETERR (awk, QSE_AWK_EINVAL);
		return -1;
	}

	awk->rexfns = rexfns;
	return 0;
}
