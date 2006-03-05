/*
 * $Id: run.c,v 1.7 2006-03-05 17:07:33 bacon Exp $
 */

#include <xp/awk/awk.h>

#ifndef __STAND_ALONE
#include <xp/bas/assert.h>
#endif

static int __run_block (xp_awk_t* awk, xp_awk_nde_blk_t* nde);
static int __run_statement (xp_awk_t* awk, xp_awk_nde_t* nde);

static xp_awk_val_t* __eval_expression (xp_awk_t* awk, xp_awk_nde_t* nde);
static xp_awk_val_t* __eval_assignment (xp_awk_t* awk, xp_awk_nde_ass_t* nde);

int __printval (xp_awk_pair_t* pair)
{
	xp_printf (XP_TEXT("%s = "), (const xp_char_t*)pair->key);
	xp_awk_printval ((xp_awk_val_t*)pair->val);
	xp_printf (XP_TEXT("\n"));
	return 0;
}

int xp_awk_run (xp_awk_t* awk)
{
	if (awk->tree.begin != XP_NULL) 
	{
		xp_assert (awk->tree.begin->type == XP_AWK_NDE_BLK);
		if (__run_block(awk, (xp_awk_nde_blk_t*)awk->tree.begin) == -1) return -1;
	}

	if (awk->tree.end != XP_NULL) 
	{
		xp_assert (awk->tree.end->type == XP_AWK_NDE_BLK);
		if (__run_block(awk, (xp_awk_nde_blk_t*)awk->tree.end) == -1) return -1;
	}

xp_awk_map_walk (&awk->run.named, __printval);
	return 0;
}

static int __run_block (xp_awk_t* awk, xp_awk_nde_blk_t* nde)
{
	xp_awk_nde_t* p;

	xp_assert (nde->type == XP_AWK_NDE_BLK);

	p = nde->body;

	while (p != XP_NULL) 
	{
		if (__run_statement(awk, p) == -1) return -1;
		p = p->next;
	}
	
	return 0;
}

static int __run_statement (xp_awk_t* awk, xp_awk_nde_t* nde)
{
	switch (nde->type) 
	{
	case XP_AWK_NDE_NULL:
		/* do nothing */
		break;

	case XP_AWK_NDE_BLK:
		if (__run_block(awk, (xp_awk_nde_blk_t*)nde) == -1) return -1;
		break;

	case XP_AWK_NDE_IF:
		break;
	case XP_AWK_NDE_WHILE:
		break;
	case XP_AWK_NDE_DOWHILE:
		break;
	case XP_AWK_NDE_FOR:
		break;

	case XP_AWK_NDE_BREAK:
		break;
	case XP_AWK_NDE_CONTINUE:
		break;

	case XP_AWK_NDE_RETURN:
		break;

	case XP_AWK_NDE_EXIT:
		break;

	case XP_AWK_NDE_NEXT:
		break;

	case XP_AWK_NDE_NEXTFILE:
		break;

	default:
		if (__eval_expression(awk,nde) == XP_NULL) return -1;
		break;
	}

	return 0;
}

static xp_awk_val_t* __eval_expression (xp_awk_t* awk, xp_awk_nde_t* nde)
{
	xp_awk_val_t* val;

	switch (nde->type) 
	{
	case XP_AWK_NDE_ASS:
		val = __eval_assignment(awk,(xp_awk_nde_ass_t*)nde);
		break;

	case XP_AWK_NDE_EXP_BIN:

	case XP_AWK_NDE_EXP_UNR:

	case XP_AWK_NDE_STR:
		val = xp_awk_makestrval(
			((xp_awk_nde_str_t*)nde)->buf,
			((xp_awk_nde_str_t*)nde)->len);
		break;

	case XP_AWK_NDE_INT:
		val = xp_awk_makeintval(((xp_awk_nde_int_t*)nde)->val);
		break;

	/* TODO:
	case XP_AWK_NDE_REAL:
		val = xp_awk_makerealval(((xp_awk_nde_real_t*)nde)->val);
		break;
	*/

	case XP_AWK_NDE_ARG:

	case XP_AWK_NDE_ARGIDX:

	case XP_AWK_NDE_NAMED:

	case XP_AWK_NDE_NAMEDIDX:

	case XP_AWK_NDE_GLOBAL:

	case XP_AWK_NDE_GLOBALIDX:

	case XP_AWK_NDE_LOCAL:

	case XP_AWK_NDE_LOCALIDX:

	case XP_AWK_NDE_POS:

	case XP_AWK_NDE_CALL:
		break;

	default:
		/* somthing wrong */
		return XP_NULL;
	}

	return val;
}

static xp_awk_val_t* __eval_assignment (xp_awk_t* awk, xp_awk_nde_ass_t* nde)
{
	xp_awk_val_t* v;
	xp_awk_nde_var_t* tgt;

	tgt = (xp_awk_nde_var_t*)nde->left;

	if (tgt->type == XP_AWK_NDE_NAMED) 
	{
		xp_awk_val_t* old, * new;
		xp_char_t* name;

		new = __eval_expression(awk, nde->right);

		xp_assert (tgt != XP_NULL);
		if (new == XP_NULL) return XP_NULL;

		name = tgt->id.name;
		old = (xp_awk_val_t*)xp_awk_map_getval(&awk->run.named, name);
		if (old == XP_NULL) {
			name = xp_strdup (tgt->id.name);
			if (name == XP_NULL) {
				xp_awk_freeval(new);
				awk->errnum = XP_AWK_ENOMEM;
				return XP_NULL;
			}
		}

		if (xp_awk_map_put(&awk->run.named, name, new) == XP_NULL) 
		{
			xp_free (name);
			xp_awk_freeval (new);
			awk->errnum = XP_AWK_ENOMEM;
			return XP_NULL;
		}

		v = new;
	}
	else if (tgt->type == XP_AWK_NDE_GLOBAL) 
	{
	}
	else if (tgt->type == XP_AWK_NDE_LOCAL) 
	{
	}
	else if (tgt->type == XP_AWK_NDE_ARG) 
	{
	}

	else if (tgt->type == XP_AWK_NDE_NAMEDIDX) 
	{
	}
	else if (tgt->type == XP_AWK_NDE_GLOBALIDX) 
	{
	}
	else if (tgt->type == XP_AWK_NDE_LOCALIDX) 
	{
	}
	else if (tgt->type == XP_AWK_NDE_ARGIDX) 
	{
	}
	else
	{
		/* this should never be reached. something wrong */
		// TODO: set errnum ....
		return XP_NULL;
	}

	return v;
}
