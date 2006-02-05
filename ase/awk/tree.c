/*
 * $Id: tree.c,v 1.17 2006-02-05 06:10:43 bacon Exp $
 */

#include <xp/awk/awk.h>

#ifndef __STAND_ALONE
#include <xp/bas/memory.h>
#include <xp/bas/assert.h>
#include <xp/bas/stdio.h>
#endif

static xp_char_t __binop_char[] =
{
	XP_CHAR('+'),
	XP_CHAR('-'),
	XP_CHAR('*'),
	XP_CHAR('/'),
	XP_CHAR('%')
};

static void __print_tabs (int depth);
static int __print_expr_node (xp_awk_node_t* node);
static int __print_expr_node_list (xp_awk_node_t* tree);
static void __print_statements (xp_awk_node_t* tree, int depth);

static void __print_tabs (int depth)
{
	int i;
	for (i = 0; i < depth; i++) xp_printf (XP_TEXT("\t"));
}

static int __print_expr_node (xp_awk_node_t* node)
{
	switch (node->type) {
	case XP_AWK_NODE_ASSIGN:
		if (__print_expr_node (((xp_awk_node_assign_t*)node)->left) == -1) return -1;
		xp_printf (XP_TEXT(" = "));
		if (__print_expr_node (((xp_awk_node_assign_t*)node)->right) == -1) return -1;
		xp_assert ((((xp_awk_node_assign_t*)node)->right)->next == XP_NULL);
		break;

	case XP_AWK_NODE_BINARY:
		xp_printf (XP_TEXT("("));
		if (__print_expr_node (((xp_awk_node_expr_t*)node)->left) == -1) return -1;
		xp_assert ((((xp_awk_node_expr_t*)node)->left)->next == XP_NULL);
		xp_printf (XP_TEXT(" %c "), __binop_char[((xp_awk_node_expr_t*)node)->opcode]);
		if (((xp_awk_node_expr_t*)node)->right->type == XP_AWK_NODE_ASSIGN) xp_printf (XP_TEXT("("));
		if (__print_expr_node (((xp_awk_node_expr_t*)node)->right) == -1) return -1;
		if (((xp_awk_node_expr_t*)node)->right->type == XP_AWK_NODE_ASSIGN) xp_printf (XP_TEXT(")"));
		xp_assert ((((xp_awk_node_expr_t*)node)->right)->next == XP_NULL);
		xp_printf (XP_TEXT(")"));
		break;

	case XP_AWK_NODE_UNARY:
// TODO:
		xp_printf (XP_TEXT("unary basic expression\n"));
		break;

	case XP_AWK_NODE_STR:
		xp_printf (XP_TEXT("\"%s\""), ((xp_awk_node_term_t*)node)->value);
		break;

	case XP_AWK_NODE_NUM:
		xp_printf (XP_TEXT("%s"), ((xp_awk_node_term_t*)node)->value);
		break;

	case XP_AWK_NODE_ARG:
		xp_printf (XP_TEXT("__arg%u"), 
			(unsigned int)((xp_awk_node_var_t*)node)->id.idxa);
		break;

	case XP_AWK_NODE_ARGIDX:
		xp_printf (XP_TEXT("__arg%u["), 
			(unsigned int)((xp_awk_node_idx_t*)node)->id.idxa);
		__print_expr_node (((xp_awk_node_idx_t*)node)->idx);
		xp_printf (XP_TEXT("]"));
		break;

	case XP_AWK_NODE_VAR:
		xp_printf (XP_TEXT("%s"), ((xp_awk_node_var_t*)node)->id.name);
		break;

	case XP_AWK_NODE_VARIDX:
		xp_printf (XP_TEXT("%s["), ((xp_awk_node_idx_t*)node)->id.name);
		__print_expr_node (((xp_awk_node_idx_t*)node)->idx);
		xp_printf (XP_TEXT("]"));
		break;

	case XP_AWK_NODE_POS:
		xp_printf (XP_TEXT("$"));
		__print_expr_node (((xp_awk_node_sgv_t*)node)->value);
		break;

	case XP_AWK_NODE_CALL:
		xp_printf (XP_TEXT("%s ("), ((xp_awk_node_call_t*)node)->name);
		if (__print_expr_node_list (((xp_awk_node_call_t*)node)->args) == -1) return -1;
		xp_printf (XP_TEXT(")"));
		break;

	default:
		return -1;
	}

	return 0;
}

static int __print_expr_node_list (xp_awk_node_t* tree)
{
	xp_awk_node_t* p = tree;

	while (p != XP_NULL) {
		if (__print_expr_node (p) == -1) return -1;
		p = p->next;
		if (p != XP_NULL) xp_printf (XP_TEXT(","));
	}

	return 0;
}

static void __print_statements (xp_awk_node_t* tree, int depth)
{
	xp_awk_node_t* p = tree;
	xp_size_t i;

	while (p != XP_NULL) {

		switch (p->type) {
		case XP_AWK_NODE_NULL:
			__print_tabs (depth);
			xp_printf (XP_TEXT(";\n"));
			break;

		case XP_AWK_NODE_BLOCK:
			__print_tabs (depth);
			xp_printf (XP_TEXT("{\n"));

			if (((xp_awk_node_block_t*)p)->nlocals > 0) {
				__print_tabs (depth + 1);
				xp_printf (XP_TEXT("local "));

				for (i = 0; i < ((xp_awk_node_block_t*)p)->nlocals - 1; i++) {
					xp_printf (XP_TEXT("__local%u, "), (unsigned int)i);
				}
				xp_printf (XP_TEXT("__local%u;\n"), (unsigned int)i);
			}

			__print_statements (((xp_awk_node_block_t*)p)->body, depth + 1);	
			__print_tabs (depth);
			xp_printf (XP_TEXT("}\n"));
			break;

		case XP_AWK_NODE_IF: 
			__print_tabs (depth);
			xp_printf (XP_TEXT("if ("));	
			__print_expr_node (((xp_awk_node_if_t*)p)->test);
			xp_printf (XP_TEXT(")\n"));

			xp_assert (((xp_awk_node_if_t*)p)->then_part != XP_NULL);
			if (((xp_awk_node_if_t*)p)->then_part->type == XP_AWK_NODE_BLOCK)
				__print_statements (((xp_awk_node_if_t*)p)->then_part, depth);
			else
				__print_statements (((xp_awk_node_if_t*)p)->then_part, depth + 1);

			if (((xp_awk_node_if_t*)p)->else_part != XP_NULL) {
				__print_tabs (depth);
				xp_printf (XP_TEXT("else\n"));	
				if (((xp_awk_node_if_t*)p)->else_part->type == XP_AWK_NODE_BLOCK)
					__print_statements (((xp_awk_node_if_t*)p)->else_part, depth);
				else
					__print_statements (((xp_awk_node_if_t*)p)->else_part, depth + 1);
			}
			break;
		case XP_AWK_NODE_WHILE: 
			__print_tabs (depth);
			xp_printf (XP_TEXT("while ("));	
			__print_expr_node (((xp_awk_node_while_t*)p)->test);
			xp_printf (XP_TEXT(")\n"));
			if (((xp_awk_node_while_t*)p)->body->type == XP_AWK_NODE_BLOCK) {
				__print_statements (((xp_awk_node_while_t*)p)->body, depth);
			}
			else {
				__print_statements (((xp_awk_node_while_t*)p)->body, depth + 1);
			}
			break;

		case XP_AWK_NODE_DOWHILE: 
			__print_tabs (depth);
			xp_printf (XP_TEXT("do\n"));	
			if (((xp_awk_node_while_t*)p)->body->type == XP_AWK_NODE_BLOCK) {
				__print_statements (((xp_awk_node_while_t*)p)->body, depth);
			}
			else {
				__print_statements (((xp_awk_node_while_t*)p)->body, depth + 1);
			}

			__print_tabs (depth);
			xp_printf (XP_TEXT("while ("));	
			__print_expr_node (((xp_awk_node_while_t*)p)->test);
			xp_printf (XP_TEXT(");\n"));	
			break;

		case XP_AWK_NODE_FOR:
			__print_tabs (depth);
			xp_printf (XP_TEXT("for ("));
			if (((xp_awk_node_for_t*)p)->init != XP_NULL) {
				__print_expr_node (((xp_awk_node_for_t*)p)->init);
			}
			xp_printf (XP_TEXT("; "));
			if (((xp_awk_node_for_t*)p)->test != XP_NULL) {
				__print_expr_node (((xp_awk_node_for_t*)p)->test);
			}
			xp_printf (XP_TEXT("; "));
			if (((xp_awk_node_for_t*)p)->incr != XP_NULL) {
				__print_expr_node (((xp_awk_node_for_t*)p)->incr);
			}
			xp_printf (XP_TEXT(")\n"));

			if (((xp_awk_node_for_t*)p)->body->type == XP_AWK_NODE_BLOCK) {
				__print_statements (((xp_awk_node_for_t*)p)->body, depth);
			}
			else {
				__print_statements (((xp_awk_node_for_t*)p)->body, depth + 1);
			}

		case XP_AWK_NODE_BREAK:
			__print_tabs (depth);
			xp_printf (XP_TEXT("break;\n"));
			break;

		case XP_AWK_NODE_CONTINUE:
			__print_tabs (depth);
			xp_printf (XP_TEXT("continue;\n"));
			break;

		case XP_AWK_NODE_RETURN:
			__print_tabs (depth);
			if (((xp_awk_node_sgv_t*)p)->value == XP_NULL) {
				xp_printf (XP_TEXT("return;\n"));
			}
			else {
				xp_printf (XP_TEXT("return "));
				xp_assert (((xp_awk_node_sgv_t*)p)->value->next == XP_NULL);
				if (__print_expr_node(((xp_awk_node_sgv_t*)p)->value) == 0) {
					xp_printf (XP_TEXT(";\n"));
				}
				else {
					xp_awk_node_sgv_t* x = (xp_awk_node_sgv_t*)p;
					xp_printf (XP_TEXT("***INTERNAL ERROR: unknown node type - %d\n"), x->type);
				}
			}
			break;

		case XP_AWK_NODE_EXIT:
			__print_tabs (depth);

			if (((xp_awk_node_sgv_t*)p)->value == XP_NULL) {
				xp_printf (XP_TEXT("exit;\n"));
			}
			else {
				xp_printf (XP_TEXT("exit "));
				xp_assert (((xp_awk_node_sgv_t*)p)->value->next == XP_NULL);
				if (__print_expr_node(((xp_awk_node_sgv_t*)p)->value) == 0) {
					xp_printf (XP_TEXT(";\n"));
				}
				else {
					xp_awk_node_sgv_t* x = (xp_awk_node_sgv_t*)p;
					xp_printf (XP_TEXT("***INTERNAL ERROR: unknown node type - %d\n"), x->type);
				}
			}
			break;

		case XP_AWK_NODE_NEXT:
			__print_tabs (depth);
			xp_printf (XP_TEXT("next;\n"));
			break;

		case XP_AWK_NODE_NEXTFILE:
			__print_tabs (depth);
			xp_printf (XP_TEXT("nextfile;\n"));
			break;

		default:
			__print_tabs (depth);
			if (__print_expr_node(p) == 0) {
				xp_printf (XP_TEXT(";\n"));
			}
			else {
				xp_printf (XP_TEXT("***INTERNAL ERROR: unknown type - %d\n"), p->type);
			}
		}

		p = p->next;
	}
}

void xp_awk_prnpt (xp_awk_node_t* tree)
{
	__print_statements (tree, 0);
}

void xp_awk_clrpt (xp_awk_node_t* tree)
{
	xp_awk_node_t* p = tree;
	xp_awk_node_t* next;

	while (p != XP_NULL) {
		next = p->next;

		switch (p->type) {
		case XP_AWK_NODE_NULL:
			xp_free (p);
			break;

		case XP_AWK_NODE_BLOCK:
			xp_awk_clrpt (((xp_awk_node_block_t*)p)->body);
			xp_free (p);
			break;

		case XP_AWK_NODE_IF:
			xp_awk_clrpt (((xp_awk_node_if_t*)p)->test);
			xp_awk_clrpt (((xp_awk_node_if_t*)p)->then_part);

			if (((xp_awk_node_if_t*)p)->else_part != XP_NULL)
				xp_awk_clrpt (((xp_awk_node_if_t*)p)->else_part);
			xp_free (p);
			break;

		case XP_AWK_NODE_WHILE:
		case XP_AWK_NODE_DOWHILE:
			xp_awk_clrpt (((xp_awk_node_while_t*)p)->test);
			xp_awk_clrpt (((xp_awk_node_while_t*)p)->body);
			xp_free (p);
			break;

		case XP_AWK_NODE_FOR:
			if (((xp_awk_node_for_t*)p)->init != XP_NULL)
				xp_awk_clrpt (((xp_awk_node_for_t*)p)->init);
			if (((xp_awk_node_for_t*)p)->test != XP_NULL)
				xp_awk_clrpt (((xp_awk_node_for_t*)p)->test);
			if (((xp_awk_node_for_t*)p)->incr != XP_NULL)
				xp_awk_clrpt (((xp_awk_node_for_t*)p)->incr);
			xp_awk_clrpt (((xp_awk_node_for_t*)p)->body);
			xp_free (p);
			break;

		case XP_AWK_NODE_BREAK:
		case XP_AWK_NODE_CONTINUE:
		case XP_AWK_NODE_NEXT:
		case XP_AWK_NODE_NEXTFILE:
			xp_free (p);
			break;
		
		case XP_AWK_NODE_RETURN:
		case XP_AWK_NODE_EXIT:
			if (((xp_awk_node_sgv_t*)p)->value != XP_NULL) 
				xp_awk_clrpt (((xp_awk_node_sgv_t*)p)->value);
			xp_free (p);
			break;

		case XP_AWK_NODE_ASSIGN:
			xp_awk_clrpt (((xp_awk_node_assign_t*)p)->left);
			xp_awk_clrpt (((xp_awk_node_assign_t*)p)->right);
			xp_free (p);
			break;

		case XP_AWK_NODE_BINARY:
			xp_assert ((((xp_awk_node_expr_t*)p)->left)->next == XP_NULL);
			xp_assert ((((xp_awk_node_expr_t*)p)->right)->next == XP_NULL);

			xp_awk_clrpt (((xp_awk_node_expr_t*)p)->left);
			xp_awk_clrpt (((xp_awk_node_expr_t*)p)->right);
			xp_free (p);
			break;

		case XP_AWK_NODE_UNARY:
// TODO: clear unary expression...
			xp_free (p);
			break;

		case XP_AWK_NODE_STR:
		case XP_AWK_NODE_NUM:
			xp_free (((xp_awk_node_term_t*)p)->value);
			xp_free (p);
			break;

		case XP_AWK_NODE_ARG:
			if (((xp_awk_node_var_t*)p)->id.name != XP_NULL)
				xp_free (((xp_awk_node_var_t*)p)->id.name);
			xp_free (p);
			break;

		case XP_AWK_NODE_ARGIDX:
			xp_awk_clrpt (((xp_awk_node_idx_t*)p)->idx);
			if (((xp_awk_node_idx_t*)p)->id.name != XP_NULL)
				xp_free (((xp_awk_node_idx_t*)p)->id.name);
			xp_free (p);
			break;

		case XP_AWK_NODE_VAR:
			if (((xp_awk_node_var_t*)p)->id.name != XP_NULL)
				xp_free (((xp_awk_node_var_t*)p)->id.name);
			xp_free (p);
			break;

		case XP_AWK_NODE_VARIDX:
			xp_awk_clrpt (((xp_awk_node_idx_t*)p)->idx);
			if (((xp_awk_node_idx_t*)p)->id.name != XP_NULL)
				xp_free (((xp_awk_node_idx_t*)p)->id.name);
			xp_free (p);
			break;

		case XP_AWK_NODE_POS:
			xp_assert (((xp_awk_node_sgv_t*)p)->value != XP_NULL);
			xp_awk_clrpt (((xp_awk_node_sgv_t*)p)->value);
			xp_free (p);
			break;

		case XP_AWK_NODE_CALL:
			xp_free (((xp_awk_node_call_t*)p)->name);
			xp_awk_clrpt (((xp_awk_node_call_t*)p)->args);
			xp_free (p);
			break;

		default:
			xp_assert (XP_TEXT("shoud not happen") == XP_TEXT(" here"));
		}

		p = next;
	}
}
