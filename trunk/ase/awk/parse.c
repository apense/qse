/*
 * $Id: parse.c,v 1.81 2006-04-14 16:26:00 bacon Exp $
 */

#include <xp/awk/awk_i.h>

#ifndef __STAND_ALONE
#include <xp/bas/memory.h>
#include <xp/bas/ctype.h>
#include <xp/bas/string.h>
#include <xp/bas/stdlib.h>
#include <xp/bas/assert.h>
#endif

enum
{
	TOKEN_EOF,

	TOKEN_ASSIGN,
	TOKEN_PLUS_ASSIGN,
	TOKEN_MINUS_ASSIGN,
	TOKEN_MUL_ASSIGN,
	TOKEN_DIV_ASSIGN,
	TOKEN_MOD_ASSIGN,
	TOKEN_EXP_ASSIGN,

	TOKEN_EQ,
	TOKEN_NE,
	TOKEN_LE,
	TOKEN_LT,
	TOKEN_GE,
	TOKEN_GT,
	TOKEN_NM,   /* not match */
	TOKEN_NOT,
	TOKEN_PLUS,
	TOKEN_PLUSPLUS,
	TOKEN_MINUS,
	TOKEN_MINUSMINUS,
	TOKEN_MUL,
	TOKEN_DIV,
	TOKEN_MOD,
	TOKEN_LOR,
	TOKEN_LAND,
	TOKEN_BOR,
	TOKEN_BXOR,
	TOKEN_BAND,
	TOKEN_TILDE, /* used for unary bitwise-not and regex match */
	TOKEN_RSHIFT,
	TOKEN_LSHIFT,
	TOKEN_EXP,

	TOKEN_LPAREN,
	TOKEN_RPAREN,
	TOKEN_LBRACE,
	TOKEN_RBRACE,
	TOKEN_LBRACK,
	TOKEN_RBRACK,

	TOKEN_DOLLAR,
	TOKEN_COMMA,
	TOKEN_SEMICOLON,
	TOKEN_COLON,
	TOKEN_QUEST,

	TOKEN_INT,
	TOKEN_REAL,
	TOKEN_STR,
	TOKEN_REGEX,

	TOKEN_IDENT,
	TOKEN_BEGIN,
	TOKEN_END,
	TOKEN_FUNCTION,
	TOKEN_IF,
	TOKEN_ELSE,
	TOKEN_WHILE,
	TOKEN_FOR,
	TOKEN_DO,
	TOKEN_BREAK,
	TOKEN_CONTINUE,
	TOKEN_RETURN,
	TOKEN_EXIT,
	TOKEN_DELETE,
	TOKEN_NEXT,
	TOKEN_NEXTFILE,

	TOKEN_LOCAL,
	TOKEN_GLOBAL,

	TOKEN_IN,

	__TOKEN_COUNT__
};

typedef struct __binmap_t __binmap_t;

struct __binmap_t
{
	int token;
	int binop;
};

static xp_awk_t* __parse_progunit (xp_awk_t* awk);
static xp_awk_t* __collect_globals (xp_awk_t* awk);
static xp_awk_t* __collect_locals (xp_awk_t* awk, xp_size_t nlocals);

static xp_awk_nde_t* __parse_function (xp_awk_t* awk);
static xp_awk_nde_t* __parse_begin (xp_awk_t* awk);
static xp_awk_nde_t* __parse_end (xp_awk_t* awk);
static xp_awk_nde_t* __parse_patternless (xp_awk_t* awk);

static xp_awk_nde_t* __parse_action (xp_awk_t* awk);
static xp_awk_nde_t* __parse_block (xp_awk_t* awk, xp_bool_t is_top);
static xp_awk_nde_t* __parse_statement (xp_awk_t* awk);
static xp_awk_nde_t* __parse_statement_nb (xp_awk_t* awk);
static xp_awk_nde_t* __parse_expression (xp_awk_t* awk);

static xp_awk_nde_t* __parse_basic_expr (xp_awk_t* awk);

static xp_awk_nde_t* __parse_binary_expr (
	xp_awk_t* awk, const __binmap_t* binmap,
	xp_awk_nde_t*(*next_level_func)(xp_awk_t*));

static xp_awk_nde_t* __parse_logical_or (xp_awk_t* awk);
static xp_awk_nde_t* __parse_logical_and (xp_awk_t* awk);
static xp_awk_nde_t* __parse_regex_match (xp_awk_t* awk);
static xp_awk_nde_t* __parse_bitwise_or (xp_awk_t* awk);
static xp_awk_nde_t* __parse_bitwise_xor (xp_awk_t* awk);
static xp_awk_nde_t* __parse_bitwise_and (xp_awk_t* awk);
static xp_awk_nde_t* __parse_equality (xp_awk_t* awk);
static xp_awk_nde_t* __parse_relational (xp_awk_t* awk);
static xp_awk_nde_t* __parse_shift (xp_awk_t* awk);
static xp_awk_nde_t* __parse_additive (xp_awk_t* awk);
static xp_awk_nde_t* __parse_multiplicative (xp_awk_t* awk);

static xp_awk_nde_t* __parse_unary (xp_awk_t* awk);
static xp_awk_nde_t* __parse_increment (xp_awk_t* awk);
static xp_awk_nde_t* __parse_primary (xp_awk_t* awk);

static xp_awk_nde_t* __parse_hashidx (xp_awk_t* awk, xp_char_t* name);
static xp_awk_nde_t* __parse_funcall (xp_awk_t* awk, xp_char_t* name);
static xp_awk_nde_t* __parse_if (xp_awk_t* awk);
static xp_awk_nde_t* __parse_while (xp_awk_t* awk);
static xp_awk_nde_t* __parse_for (xp_awk_t* awk);
static xp_awk_nde_t* __parse_dowhile (xp_awk_t* awk);
static xp_awk_nde_t* __parse_break (xp_awk_t* awk);
static xp_awk_nde_t* __parse_continue (xp_awk_t* awk);
static xp_awk_nde_t* __parse_return (xp_awk_t* awk);
static xp_awk_nde_t* __parse_exit (xp_awk_t* awk);
static xp_awk_nde_t* __parse_delete (xp_awk_t* awk);
static xp_awk_nde_t* __parse_next (xp_awk_t* awk);
static xp_awk_nde_t* __parse_nextfile (xp_awk_t* awk);

static int __get_token (xp_awk_t* awk);
static int __get_number (xp_awk_t* awk);
static int __get_string (xp_awk_t* awk);
static int __get_char (xp_awk_t* awk);
static int __unget_char (xp_awk_t* awk, xp_cint_t c);
static int __skip_spaces (xp_awk_t* awk);
static int __skip_comment (xp_awk_t* awk);
static int __classify_ident (xp_awk_t* awk, const xp_char_t* ident);
static int __assign_to_opcode (xp_awk_t* awk);

struct __kwent 
{ 
	const xp_char_t* name; 
	int type; 
	int valid; /* the entry is valid when this option is set */
};

static struct __kwent __kwtab[] = 
{
	{ XP_TEXT("BEGIN"),    TOKEN_BEGIN,    0 },
	{ XP_TEXT("END"),      TOKEN_END,      0 },

	{ XP_TEXT("function"), TOKEN_FUNCTION, 0 },
	{ XP_TEXT("func"),     TOKEN_FUNCTION, 0 },
	{ XP_TEXT("if"),       TOKEN_IF,       0 },
	{ XP_TEXT("else"),     TOKEN_ELSE,     0 },
	{ XP_TEXT("while"),    TOKEN_WHILE,    0 },
	{ XP_TEXT("for"),      TOKEN_FOR,      0 },
	{ XP_TEXT("do"),       TOKEN_DO,       0 },
	{ XP_TEXT("break"),    TOKEN_BREAK,    0 },
	{ XP_TEXT("continue"), TOKEN_CONTINUE, 0 },
	{ XP_TEXT("return"),   TOKEN_RETURN,   0 },
	{ XP_TEXT("exit"),     TOKEN_EXIT,     0 },
	{ XP_TEXT("delete"),   TOKEN_DELETE,   0 },
	{ XP_TEXT("next"),     TOKEN_NEXT,     0 },
	{ XP_TEXT("nextfile"), TOKEN_NEXTFILE, 0 },

	{ XP_TEXT("local"),    TOKEN_LOCAL,    XP_AWK_EXPLICIT },
	{ XP_TEXT("global"),   TOKEN_GLOBAL,   XP_AWK_EXPLICIT },

	{ XP_TEXT("in"),       TOKEN_IN,       0 },

	{ XP_NULL,             0,              0 }
};

#define GET_CHAR(awk) \
	do { if (__get_char(awk) == -1) return -1; } while(0)

#define GET_CHAR_TO(awk,c) \
do { \
	if (__get_char(awk) == -1) return -1; \
	c = (awk)->lex.curc; \
} while(0)

#define SET_TOKEN_TYPE(awk,code) ((awk)->token.type = code)

#define ADD_TOKEN_CHAR(awk,c) \
do { \
	if (xp_str_ccat(&(awk)->token.name,(c)) == (xp_size_t)-1) { \
		(awk)->errnum = XP_AWK_ENOMEM; return -1; \
	} \
} while (0)

#define ADD_TOKEN_STR(awk,str) \
do { \
	if (xp_str_cat(&(awk)->token.name,(str)) == (xp_size_t)-1) { \
		(awk)->errnum = XP_AWK_ENOMEM; return -1; \
	} \
} while (0)

#define GET_TOKEN(awk) \
	do { if (__get_token(awk) == -1) return -1; } while (0)

#define MATCH(awk,token_type) ((awk)->token.type == (token_type))

#define CONSUME(awk) \
	do { if (__get_token(awk) == -1) return XP_NULL; } while (0)

#define PANIC(awk,code) \
	do { (awk)->errnum = (code); return XP_NULL; } while (0)

/* TODO: remove stdio.h */
#ifndef __STAND_ALONE
#include <xp/bas/stdio.h>
#endif
static int __dump_func (xp_awk_pair_t* pair)
{
	xp_awk_func_t* func = (xp_awk_func_t*)pair->val;
	xp_size_t i;

	xp_assert (xp_strcmp(pair->key, func->name) == 0);
	xp_printf (XP_TEXT("function %s ("), func->name);
	for (i = 0; i < func->nargs; ) 
	{
		xp_printf (XP_TEXT("__arg%lu"), (unsigned long)i++);
		if (i >= func->nargs) break;
		xp_printf (XP_TEXT(", "));
	}
	xp_printf (XP_TEXT(")\n"));
	xp_awk_prnpt (func->body);
	xp_printf (XP_TEXT("\n"));

	return 0;
}

static void __dump (xp_awk_t* awk)
{
	xp_awk_chain_t* chain;

	if (awk->tree.nglobals > 0) 
	{
		xp_size_t i;

		xp_printf (XP_TEXT("global "));
		for (i = 0; i < awk->tree.nglobals - 1; i++) 
		{
			xp_printf (XP_TEXT("__global%lu, "), (unsigned long)i);
		}
		xp_printf (XP_TEXT("__global%lu;\n\n"), (unsigned long)i);
	}

	xp_awk_map_walk (&awk->tree.funcs, __dump_func);

	if (awk->tree.begin != XP_NULL) 
	{
		xp_printf (XP_TEXT("BEGIN "));
		xp_awk_prnpt (awk->tree.begin);
		xp_printf (XP_TEXT("\n"));
	}

	chain = awk->tree.chain;
	while (chain != XP_NULL) 
	{
		if (chain->pattern != XP_NULL) xp_awk_prnpt (chain->pattern);
		if (chain->action != XP_NULL) xp_awk_prnpt (chain->action);	
		xp_printf (XP_TEXT("\n"));
		chain = chain->next;	
	}

	if (awk->tree.end != XP_NULL) 
	{
		xp_printf (XP_TEXT("END "));
		xp_awk_prnpt (awk->tree.end);
	}
}

int xp_awk_parse (xp_awk_t* awk)
{
	/* if you want to parse anew, call xp_awk_clear first.
	 * otherwise, the result is appened to the accumulated result */

	GET_CHAR (awk);
	GET_TOKEN (awk);

	while (1) 
	{
		if (MATCH(awk,TOKEN_EOF)) break;

		if (__parse_progunit(awk) == XP_NULL) 
		{
/* TODO: cleanup the parse tree created so far....
         function tables also etc... */
			return -1;
		}
	}

	awk->tree.nglobals = xp_awk_tab_getsize(&awk->parse.globals);
xp_printf (XP_TEXT("-----------------------------\n"));
__dump (awk);

	return 0;
}

static xp_awk_t* __parse_progunit (xp_awk_t* awk)
{
	/*
	pattern { action }
	function name (parameter-list) { statement }
	*/

	if ((awk->opt.parse & XP_AWK_EXPLICIT) && MATCH(awk,TOKEN_GLOBAL)) 
	{
		xp_size_t nglobals;

		if (__get_token(awk) == -1) return XP_NULL;

		nglobals = xp_awk_tab_getsize(&awk->parse.globals);
		if (__collect_globals(awk) == XP_NULL) 
		{
			xp_awk_tab_remove (
				&awk->parse.globals, nglobals, 
				xp_awk_tab_getsize(&awk->parse.globals) - nglobals);
			return XP_NULL;
		}
	}
	else if (MATCH(awk,TOKEN_FUNCTION)) 
	{
		if (__parse_function(awk) == XP_NULL) return XP_NULL;
	}
	else if (MATCH(awk,TOKEN_BEGIN)) 
	{
		if (__parse_begin(awk) == XP_NULL) return XP_NULL;
	}
	else if (MATCH(awk, TOKEN_END)) 
	{
		if (__parse_end(awk) == XP_NULL) return XP_NULL;
	}
	/* TODO: process patterns and expressions */
	/* 
	expressions 
	/regular expression/
	pattern && pattern
	pattern || pattern
	!pattern
	(pattern)
	pattern, pattern
	*/
	else 
	{
		if (__parse_patternless(awk) == XP_NULL) return XP_NULL;
	}

	return awk;
}

static xp_awk_nde_t* __parse_function (xp_awk_t* awk)
{
	xp_char_t* name;
	xp_char_t* name_dup;
	xp_awk_nde_t* body;
	xp_awk_func_t* func;
	xp_size_t nargs;

	/* eat up the keyword 'function' and get the next token */
	if (__get_token(awk) == -1) return XP_NULL;  

	/* match a function name */
	if (!MATCH(awk,TOKEN_IDENT)) 
	{
		/* cannot find a valid identifier for a function name */
		PANIC (awk, XP_AWK_EIDENT);
	}

	name = XP_STR_BUF(&awk->token.name);
	if (xp_awk_map_get(&awk->tree.funcs, name) != XP_NULL) 
	{
		/* the function is defined previously */
		PANIC (awk, XP_AWK_EDUPFUNC);
	}

	if (awk->opt.parse & XP_AWK_UNIQUE) 
	{
		/* check if it coincides to be a global variable name */
		if (xp_awk_tab_find(&awk->parse.globals, name, 0) != (xp_size_t)-1) 
		{
			PANIC (awk, XP_AWK_EDUPNAME);
		}
	}

	/* clone the function name before it is overwritten */
	name_dup = xp_strdup (name);
	if (name_dup == XP_NULL) PANIC (awk, XP_AWK_ENOMEM);

	/* get the next token */
	if (__get_token(awk) == -1) 
	{
		xp_free (name_dup);
		return XP_NULL;  
	}

	/* match a left parenthesis */
	if (!MATCH(awk,TOKEN_LPAREN)) 
	{
		/* a function name is not followed by a left parenthesis */
		xp_free (name_dup);
		PANIC (awk, XP_AWK_ELPAREN);
	}	

	/* get the next token */
	if (__get_token(awk) == -1) 
	{
		xp_free (name_dup);
		return XP_NULL;
	}

	/* make sure that parameter table is empty */
	xp_assert (xp_awk_tab_getsize(&awk->parse.params) == 0);

	/* read parameter list */
	if (MATCH(awk,TOKEN_RPAREN)) 
	{
		/* no function parameter found. get the next token */
		if (__get_token(awk) == -1) 
		{
			xp_free (name_dup);
			return XP_NULL;
		}
	}
	else 
	{
		while (1) 
		{
			xp_char_t* param;

			if (!MATCH(awk,TOKEN_IDENT)) 
			{
				xp_free (name_dup);
				xp_awk_tab_clear (&awk->parse.params);
				PANIC (awk, XP_AWK_EIDENT);
			}

			param = XP_STR_BUF(&awk->token.name);

			if (awk->opt.parse & XP_AWK_UNIQUE) 
			{
				/* check if a parameter conflicts with a function */
				if (xp_strcmp(name_dup, param) == 0 ||
				    xp_awk_map_get(&awk->tree.funcs, param) != XP_NULL) 
				{
					xp_free (name_dup);
					xp_awk_tab_clear (&awk->parse.params);
					PANIC (awk, XP_AWK_EDUPNAME);
				}

				/* NOTE: the following is not a conflict
				 *  global x; 
				 *  function f (x) { print x; } 
				 *  x in print x is a parameter
				 */
			}

			/* check if a parameter conflicts with other parameters */
			if (xp_awk_tab_find(&awk->parse.params, param, 0) != (xp_size_t)-1) 
			{
				xp_free (name_dup);
				xp_awk_tab_clear (&awk->parse.params);
				PANIC (awk, XP_AWK_EDUPPARAM);
			}

			/* push the parameter to the parameter list */
			if (xp_awk_tab_add(&awk->parse.params, param) == (xp_size_t)-1) 
			{
				xp_free (name_dup);
				xp_awk_tab_clear (&awk->parse.params);
				PANIC (awk, XP_AWK_ENOMEM);
			}	

			if (__get_token(awk) == -1) 
			{
				xp_free (name_dup);
				xp_awk_tab_clear (&awk->parse.params);
				return XP_NULL;
			}	

			if (MATCH(awk,TOKEN_RPAREN)) break;

			if (!MATCH(awk,TOKEN_COMMA)) 
			{
				xp_free (name_dup);
				xp_awk_tab_clear (&awk->parse.params);
				PANIC (awk, XP_AWK_ECOMMA);
			}

			if (__get_token(awk) == -1) 
			{
				xp_free (name_dup);
				xp_awk_tab_clear (&awk->parse.params);
				return XP_NULL;
			}
		}

		if (__get_token(awk) == -1) 
		{
			xp_free (name_dup);
			xp_awk_tab_clear (&awk->parse.params);
			return XP_NULL;
		}
	}

	/* check if the function body starts with a left brace */
	if (!MATCH(awk,TOKEN_LBRACE)) 
	{
		xp_free (name_dup);
		xp_awk_tab_clear (&awk->parse.params);
		PANIC (awk, XP_AWK_ELBRACE);
	}
	if (__get_token(awk) == -1) 
	{
		xp_free (name_dup);
		xp_awk_tab_clear (&awk->parse.params);
		return XP_NULL; 
	}

	/* actual function body */
	body = __parse_block (awk, xp_true);
	if (body == XP_NULL) 
	{
		xp_free (name_dup);
		xp_awk_tab_clear (&awk->parse.params);
		return XP_NULL;
	}

/* TODO: consider if the parameters should be saved for some reasons.. */
	nargs = xp_awk_tab_getsize(&awk->parse.params);
	/* parameter names are not required anymore. clear them */
	xp_awk_tab_clear (&awk->parse.params);

	func = (xp_awk_func_t*)xp_malloc(xp_sizeof(xp_awk_func_t));
	if (func == XP_NULL) 
	{
		xp_free (name_dup);
		xp_awk_clrpt (body);
		return XP_NULL;
	}

	func->name  = name_dup;
	func->nargs = nargs;
	func->body  = body;

	xp_assert (xp_awk_map_get(&awk->tree.funcs, name_dup) == XP_NULL);
	if (xp_awk_map_put(&awk->tree.funcs, name_dup, func) == XP_NULL) 
	{
		xp_free (name_dup);
		xp_awk_clrpt (body);
		xp_free (func);
		PANIC (awk, XP_AWK_ENOMEM);
	}

	return body;
}

static xp_awk_nde_t* __parse_begin (xp_awk_t* awk)
{
	xp_awk_nde_t* nde;

	if (awk->tree.begin != XP_NULL) PANIC (awk, XP_AWK_EDUPBEGIN);
	if (__get_token(awk) == -1) return XP_NULL; 

	nde = __parse_action (awk);
	if (nde == XP_NULL) return XP_NULL;

	awk->tree.begin = nde;
	return nde;
}

static xp_awk_nde_t* __parse_end (xp_awk_t* awk)
{
	xp_awk_nde_t* nde;

	if (awk->tree.end != XP_NULL) PANIC (awk, XP_AWK_EDUPEND);
	if (__get_token(awk) == -1) return XP_NULL; 

	nde = __parse_action (awk);
	if (nde == XP_NULL) return XP_NULL;

	awk->tree.end = nde;
	return nde;
}

static xp_awk_nde_t* __parse_patternless (xp_awk_t* awk)
{
	xp_awk_nde_t* nde;
	xp_awk_chain_t* chain;

	nde = __parse_action (awk);
	if (nde == XP_NULL) return XP_NULL;

	chain = (xp_awk_chain_t*)xp_malloc(xp_sizeof(xp_awk_chain_t));
	if (chain == XP_NULL) 
	{
		xp_awk_clrpt (nde);
		PANIC (awk, XP_AWK_ENOMEM);
	}

	chain->pattern = XP_NULL;
	chain->action = nde;
	chain->next = XP_NULL;

	if (awk->tree.chain == XP_NULL) 
	{
		awk->tree.chain = chain;
		awk->tree.chain_tail = chain;
	}
	else 
	{
		awk->tree.chain_tail->next = chain;
		awk->tree.chain_tail = chain;
	}

	return nde;
}

static xp_awk_nde_t* __parse_action (xp_awk_t* awk)
{
	if (!MATCH(awk,TOKEN_LBRACE)) PANIC (awk, XP_AWK_ELBRACE);
	if (__get_token(awk) == -1) return XP_NULL; 
	return __parse_block(awk, xp_true);
}

static xp_awk_nde_t* __parse_block (xp_awk_t* awk, xp_bool_t is_top) 
{
	xp_awk_nde_t* head, * curr, * nde;
	xp_awk_nde_blk_t* block;
	xp_size_t nlocals, nlocals_max, tmp;

	nlocals = xp_awk_tab_getsize(&awk->parse.locals);
	nlocals_max = awk->parse.nlocals_max;

	/* local variable declarations */
	if (awk->opt.parse & XP_AWK_EXPLICIT) 
	{
		while (1) 
		{
			if (!MATCH(awk,TOKEN_LOCAL)) break;

			if (__get_token(awk) == -1) 
			{
				xp_awk_tab_remove (
					&awk->parse.locals, nlocals, 
					xp_awk_tab_getsize(&awk->parse.locals) - nlocals);
				return XP_NULL;
			}

			if (__collect_locals(awk, nlocals) == XP_NULL)
			{
				xp_awk_tab_remove (
					&awk->parse.locals, nlocals, 
					xp_awk_tab_getsize(&awk->parse.locals) - nlocals);
				return XP_NULL;
			}
		}
	}

	/* block body */
	head = XP_NULL; curr = XP_NULL;

	while (1) 
	{
		if (MATCH(awk,TOKEN_EOF)) 
		{
			xp_awk_tab_remove (
				&awk->parse.locals, nlocals, 
				xp_awk_tab_getsize(&awk->parse.locals) - nlocals);
			if (head != XP_NULL) xp_awk_clrpt (head);
			PANIC (awk, XP_AWK_EENDSRC);
		}

		if (MATCH(awk,TOKEN_RBRACE)) 
		{
			if (__get_token(awk) == -1) 
			{
				xp_awk_tab_remove (
					&awk->parse.locals, nlocals, 
					xp_awk_tab_getsize(&awk->parse.locals) - nlocals);
				if (head != XP_NULL) xp_awk_clrpt (head);
				return XP_NULL; 
			}
			break;
		}

		nde = __parse_statement (awk);
		if (nde == XP_NULL) 
		{
			xp_awk_tab_remove (
				&awk->parse.locals, nlocals, 
				xp_awk_tab_getsize(&awk->parse.locals) - nlocals);
			if (head != XP_NULL) xp_awk_clrpt (head);
			return XP_NULL;
		}

		/* remove unnecessary statements */
		if (nde->type == XP_AWK_NDE_NULL ||
		    (nde->type == XP_AWK_NDE_BLK && 
		     ((xp_awk_nde_blk_t*)nde)->body == XP_NULL)) continue;
			
		if (curr == XP_NULL) head = nde;
		else curr->next = nde;	
		curr = nde;
	}

	block = (xp_awk_nde_blk_t*)xp_malloc(xp_sizeof(xp_awk_nde_blk_t));
	if (block == XP_NULL) 
	{
		xp_awk_tab_remove (
			&awk->parse.locals, nlocals, 
			xp_awk_tab_getsize(&awk->parse.locals) - nlocals);
		xp_awk_clrpt (head);
		PANIC (awk, XP_AWK_ENOMEM);
	}

	tmp = xp_awk_tab_getsize(&awk->parse.locals);
	if (tmp > awk->parse.nlocals_max) awk->parse.nlocals_max = tmp;

	xp_awk_tab_remove (
		&awk->parse.locals, nlocals, tmp - nlocals);

	/* adjust the number of locals for a block without any statements */
	/* if (head == XP_NULL) tmp = 0; */

	block->type = XP_AWK_NDE_BLK;
	block->next = XP_NULL;
	block->body = head;

/* TODO: not only local variables but also nested blocks, 
unless it is part of other constructs such as if, can be promoted 
and merged to top-level block */

	/* migrate all block-local variables to a top-level block */
	if (is_top) 
	{
		block->nlocals = awk->parse.nlocals_max - nlocals;
		awk->parse.nlocals_max = nlocals_max;
	}
	else 
	{
		/*block->nlocals = tmp - nlocals;*/
		block->nlocals = 0;
	}

	return (xp_awk_nde_t*)block;
}

static xp_awk_t* __collect_globals (xp_awk_t* awk)
{
	xp_char_t* global;

	while (1) 
	{
		if (!MATCH(awk,TOKEN_IDENT)) 
		{
			PANIC (awk, XP_AWK_EIDENT);
		}

		global = XP_STR_BUF(&awk->token.name);

		if (awk->opt.parse & XP_AWK_UNIQUE) 
		{
			/* check if it conflict with a function name */
			if (xp_awk_map_get(&awk->tree.funcs, global) != XP_NULL) 
			{
				PANIC (awk, XP_AWK_EDUPNAME);
			}
		}

		/* check if it conflicts with other global variable names */
		if (xp_awk_tab_find(&awk->parse.globals, global, 0) != (xp_size_t)-1) 
		{ 
			PANIC (awk, XP_AWK_EDUPVAR);	
		}

		if (xp_awk_tab_add(&awk->parse.globals, global) == (xp_size_t)-1) 
		{
			PANIC (awk, XP_AWK_ENOMEM);
		}

		if (__get_token(awk) == -1) return XP_NULL;

		if (MATCH(awk,TOKEN_SEMICOLON)) break;

		if (!MATCH(awk,TOKEN_COMMA)) 
		{
			PANIC (awk, XP_AWK_ECOMMA);
		}

		if (__get_token(awk) == -1) return XP_NULL;
	}

	/* skip a semicolon */
	if (__get_token(awk) == -1) return XP_NULL;

	return awk;
}

static xp_awk_t* __collect_locals (xp_awk_t* awk, xp_size_t nlocals)
{
	xp_char_t* local;

	while (1) 
	{
		if (!MATCH(awk,TOKEN_IDENT)) 
		{
			PANIC (awk, XP_AWK_EIDENT);
		}

		local = XP_STR_BUF(&awk->token.name);

		/* NOTE: it is not checked againt globals names */

		if (awk->opt.parse & XP_AWK_UNIQUE) 
		{
			/* check if it conflict with a function name */
			if (xp_awk_map_get(&awk->tree.funcs, local) != XP_NULL) 
			{
				PANIC (awk, XP_AWK_EDUPNAME);
			}
		}

		/* check if it conflicts with a paremeter name */
		if (xp_awk_tab_find(&awk->parse.params, local, 0) != (xp_size_t)-1) 
		{
			PANIC (awk, XP_AWK_EDUPNAME);
		}

		/* check if it conflicts with other local variable names */
		if (xp_awk_tab_find(&awk->parse.locals, local, 
			((awk->opt.parse & XP_AWK_SHADING)? nlocals: 0)) != (xp_size_t)-1)
		{
			PANIC (awk, XP_AWK_EDUPVAR);	
		}

		if (xp_awk_tab_add(&awk->parse.locals, local) == (xp_size_t)-1) 
		{
			PANIC (awk, XP_AWK_ENOMEM);
		}

		if (__get_token(awk) == -1) return XP_NULL;

		if (MATCH(awk,TOKEN_SEMICOLON)) break;

		if (!MATCH(awk,TOKEN_COMMA)) 
		{
			PANIC (awk, XP_AWK_ECOMMA);
		}

		if (__get_token(awk) == -1) return XP_NULL;
	}

	/* skip a semicolon */
	if (__get_token(awk) == -1) return XP_NULL;

	return awk;
}

static xp_awk_nde_t* __parse_statement (xp_awk_t* awk)
{
	xp_awk_nde_t* nde;

	if (MATCH(awk,TOKEN_SEMICOLON)) 
	{
		/* null statement */	
		nde = (xp_awk_nde_t*)xp_malloc(xp_sizeof(xp_awk_nde_t));
		if (nde == XP_NULL) PANIC (awk, XP_AWK_ENOMEM);

		nde->type = XP_AWK_NDE_NULL;
		nde->next = XP_NULL;

		if (__get_token(awk) == -1) 
		{
			xp_free (nde);
			return XP_NULL;
		}
	}
	else if (MATCH(awk,TOKEN_LBRACE)) 
	{
		if (__get_token(awk) == -1) return XP_NULL; 
		nde = __parse_block (awk, xp_false);
	}
	else nde = __parse_statement_nb (awk);

	return nde;
}

static xp_awk_nde_t* __parse_statement_nb (xp_awk_t* awk)
{
	xp_awk_nde_t* nde;

	/* 
	 * keywords that don't require any terminating semicolon 
	 */
	if (MATCH(awk,TOKEN_IF)) 
	{
		if (__get_token(awk) == -1) return XP_NULL;
		return __parse_if (awk);
	}
	else if (MATCH(awk,TOKEN_WHILE)) 
	{
		if (__get_token(awk) == -1) return XP_NULL;
		return __parse_while (awk);
	}
	else if (MATCH(awk,TOKEN_FOR)) 
	{
		if (__get_token(awk) == -1) return XP_NULL;
		return __parse_for (awk);
	}

	/* 
	 * keywords that require a terminating semicolon 
	 */
	if (MATCH(awk,TOKEN_DO)) 
	{
		if (__get_token(awk) == -1) return XP_NULL;
		nde = __parse_dowhile (awk);
	}
	else if (MATCH(awk,TOKEN_BREAK)) 
	{
		if (__get_token(awk) == -1) return XP_NULL;
		nde = __parse_break(awk);
	}
	else if (MATCH(awk,TOKEN_CONTINUE)) 
	{
		if (__get_token(awk) == -1) return XP_NULL;
		nde = __parse_continue(awk);
	}
	else if (MATCH(awk,TOKEN_RETURN)) 
	{
		if (__get_token(awk) == -1) return XP_NULL;
		nde = __parse_return(awk);
	}
	else if (MATCH(awk,TOKEN_EXIT)) 
	{
		if (__get_token(awk) == -1) return XP_NULL;
		nde = __parse_exit(awk);
	}
	else if (MATCH(awk,TOKEN_DELETE)) 
	{
		if (__get_token(awk) == -1) return XP_NULL;
		nde = __parse_delete(awk);
	}
	else if (MATCH(awk,TOKEN_NEXT)) 
	{
		if (__get_token(awk) == -1) return XP_NULL;
		nde = __parse_next(awk);
	}
	else if (MATCH(awk,TOKEN_NEXTFILE)) 
	{
		if (__get_token(awk) == -1) return XP_NULL;
		nde = __parse_nextfile(awk);
	}
	else 
	{
		nde = __parse_expression(awk);
	}

	if (nde == XP_NULL) return XP_NULL;

	/* check if a statement ends with a semicolon */
	if (!MATCH(awk,TOKEN_SEMICOLON)) 
	{
		if (nde != XP_NULL) xp_awk_clrpt (nde);
		PANIC (awk, XP_AWK_ESEMICOLON);
	}

	/* eat up the semicolon and read in the next token */
	if (__get_token(awk) == -1) 
	{
		if (nde != XP_NULL) xp_awk_clrpt (nde);
		return XP_NULL;
	}

	return nde;
}

static xp_awk_nde_t* __parse_expression (xp_awk_t* awk)
{
	/*
	 * <expression> ::= <assignment> | <basic expression>
	 * <assignment> ::= <assignment target> assignmentOperator <basic expression>
	 * assignmentOperator ::= '='
	 * <basic expression> ::= 
	 */

	xp_awk_nde_t* x, * y;
	xp_awk_nde_ass_t* nde;
	int opcode;

	x = __parse_basic_expr (awk);
	if (x == XP_NULL) return XP_NULL;

	opcode = __assign_to_opcode (awk);
	if (opcode == -1) return x;

	xp_assert (x->next == XP_NULL);
	if (x->type != XP_AWK_NDE_ARG &&
	    x->type != XP_AWK_NDE_ARGIDX &&
	    x->type != XP_AWK_NDE_NAMED && 
	    x->type != XP_AWK_NDE_NAMEDIDX &&
	    x->type != XP_AWK_NDE_GLOBAL && 
	    x->type != XP_AWK_NDE_GLOBALIDX &&
	    x->type != XP_AWK_NDE_LOCAL && 
	    x->type != XP_AWK_NDE_LOCALIDX &&
	    x->type != XP_AWK_NDE_POS) 
	{
		xp_awk_clrpt (x);
		PANIC (awk, XP_AWK_EASSIGNMENT);
	}

	if (__get_token(awk) == -1) 
	{
		xp_awk_clrpt (x);
		return XP_NULL;
	}

	y = __parse_basic_expr (awk);
	if (y == XP_NULL) 
	{
		xp_awk_clrpt (x);
		return XP_NULL;
	}

	nde = (xp_awk_nde_ass_t*)xp_malloc(xp_sizeof(xp_awk_nde_ass_t));
	if (nde == XP_NULL) 
	{
		xp_awk_clrpt (x);
		xp_awk_clrpt (y);
		PANIC (awk, XP_AWK_ENOMEM);
	}

	nde->type = XP_AWK_NDE_ASS;
	nde->next = XP_NULL;
	nde->opcode = opcode;
	nde->left = x;
	nde->right = y;

	return (xp_awk_nde_t*)nde;
}

static xp_awk_nde_t* __parse_basic_expr (xp_awk_t* awk)
{
	xp_awk_nde_t* nde, * n1, * n2;
	
	nde = __parse_logical_or (awk);
	if (nde == XP_NULL) return XP_NULL;

	if (MATCH(awk,TOKEN_QUEST))
	{ 
		xp_awk_nde_cnd_t* tmp;

		if (__get_token(awk) == -1) return XP_NULL;

		n1 = __parse_basic_expr (awk);
		if (n1 == XP_NULL) 
		{
			xp_awk_clrpt (nde);
			return XP_NULL;
		}

		if (!MATCH(awk,TOKEN_COLON)) PANIC (awk, XP_AWK_ECOLON);
		if (__get_token(awk) == -1) return XP_NULL;

		n2 = __parse_basic_expr (awk);
		if (n2 == XP_NULL)
		{
			xp_awk_clrpt (nde);
			xp_awk_clrpt (n1);
			return XP_NULL;
		}

		tmp = (xp_awk_nde_cnd_t*)xp_malloc(xp_sizeof(xp_awk_nde_cnd_t));
		if (tmp == XP_NULL)
		{
			xp_awk_clrpt (nde);
			xp_awk_clrpt (n1);
			xp_awk_clrpt (n2);
			return XP_NULL;
		}

		tmp->type = XP_AWK_NDE_CND;
		tmp->next = XP_NULL;
		tmp->test = nde;
		tmp->left = n1;
		tmp->right = n2;

		nde = (xp_awk_nde_t*)tmp;
	}

	return nde;
}

static xp_awk_nde_t* __parse_binary_expr (
	xp_awk_t* awk, const __binmap_t* binmap,
	xp_awk_nde_t*(*next_level_func)(xp_awk_t*))
{
	xp_awk_nde_exp_t* nde;
	xp_awk_nde_t* left, * right;
	int opcode;

	left = next_level_func (awk);
	if (left == XP_NULL) return XP_NULL;
	
	while (1) 
	{
		const __binmap_t* p = binmap;
		xp_bool_t matched = xp_false;

		while (p->token != TOKEN_EOF)
		{
			if (MATCH(awk,p->token)) 
			{
				opcode = p->binop;
				matched = xp_true;
				break;
			}
			p++;
		}
		if (!matched) break;

		if (__get_token(awk) == -1) 
		{
			xp_awk_clrpt (left);
			return XP_NULL; 
		}

		right = next_level_func (awk);
		if (right == XP_NULL) 
		{
			xp_awk_clrpt (left);
			return XP_NULL;
		}

		/* TODO: enhance constant folding. do it in a better way */
		/* TODO: differentiate different types of numbers ... */
		if (left->type == XP_AWK_NDE_INT && 
		    right->type == XP_AWK_NDE_INT) 
		{
			xp_long_t l, r;

			l = ((xp_awk_nde_int_t*)left)->val; 
			r = ((xp_awk_nde_int_t*)right)->val; 

			/* TODO: more operators */
			if (opcode == XP_AWK_BINOP_PLUS) l += r;
			else if (opcode == XP_AWK_BINOP_MINUS) l -= r;
			else if (opcode == XP_AWK_BINOP_MUL) l *= r;
			else if (opcode == XP_AWK_BINOP_DIV && r != 0) l /= r;
			else if (opcode == XP_AWK_BINOP_MOD && r != 0) l %= r;
			else goto skip_constant_folding;

			xp_awk_clrpt (right);
			((xp_awk_nde_int_t*)left)->val = l;
			continue;
		} 
		else if (left->type == XP_AWK_NDE_REAL && 
		         right->type == XP_AWK_NDE_REAL) 
		{
			xp_real_t l, r;

			l = ((xp_awk_nde_real_t*)left)->val; 
			r = ((xp_awk_nde_real_t*)right)->val; 

			/* TODO: more operators */
			if (opcode == XP_AWK_BINOP_PLUS) l += r;
			else if (opcode == XP_AWK_BINOP_MINUS) l -= r;
			else if (opcode == XP_AWK_BINOP_MUL) l *= r;
			else if (opcode == XP_AWK_BINOP_DIV) l /= r;
			else goto skip_constant_folding;

			xp_awk_clrpt (right);
			((xp_awk_nde_real_t*)left)->val = l;
			continue;
		}
		/* TODO: enhance constant folding more... */

	skip_constant_folding:
		nde = (xp_awk_nde_exp_t*)
			xp_malloc (xp_sizeof(xp_awk_nde_exp_t));
		if (nde == XP_NULL) 
		{
			xp_awk_clrpt (right);
			xp_awk_clrpt (left);
			PANIC (awk, XP_AWK_ENOMEM);
		}

		nde->type = XP_AWK_NDE_EXP_BIN;
		nde->next = XP_NULL;
		nde->opcode = opcode; 
		nde->left = left;
		nde->right = right;

		left = (xp_awk_nde_t*)nde;
	}

	return left;
}

static xp_awk_nde_t* __parse_logical_or (xp_awk_t* awk)
{
	__binmap_t map[] = 
	{
		{ TOKEN_LOR, XP_AWK_BINOP_LOR },
		{ TOKEN_EOF, 0 }
	};

	return __parse_binary_expr (awk, map, __parse_logical_and);
}

static xp_awk_nde_t* __parse_logical_and (xp_awk_t* awk)
{
	__binmap_t map[] = 
	{
		{ TOKEN_LAND, XP_AWK_BINOP_LAND },
		{ TOKEN_EOF,  0 }
	};

	return __parse_binary_expr (awk, map, __parse_regex_match);
}

static xp_awk_nde_t* __parse_regex_match (xp_awk_t* awk)
{
	__binmap_t map[] =
	{
		{ TOKEN_TILDE, XP_AWK_BINOP_MA },
		{ TOKEN_NM,    XP_AWK_BINOP_NM },
		{ TOKEN_EOF,   0 },
	};

	return __parse_binary_expr (awk, map, __parse_bitwise_or);
}

static xp_awk_nde_t* __parse_bitwise_or (xp_awk_t* awk)
{
	__binmap_t map[] = 
	{
		{ TOKEN_BOR, XP_AWK_BINOP_BOR },
		{ TOKEN_EOF, 0 }
	};

	return __parse_binary_expr (awk, map, __parse_bitwise_xor);
}

static xp_awk_nde_t* __parse_bitwise_xor (xp_awk_t* awk)
{
	__binmap_t map[] = 
	{
		{ TOKEN_BXOR, XP_AWK_BINOP_BXOR },
		{ TOKEN_EOF,  0 }
	};

	return __parse_binary_expr (awk, map, __parse_bitwise_and);
}

static xp_awk_nde_t* __parse_bitwise_and (xp_awk_t* awk)
{
	__binmap_t map[] = 
	{
		{ TOKEN_BAND, XP_AWK_BINOP_BAND },
		{ TOKEN_EOF,  0 }
	};

	return __parse_binary_expr (awk, map, __parse_equality);
}

static xp_awk_nde_t* __parse_equality (xp_awk_t* awk)
{
	__binmap_t map[] = 
	{
		{ TOKEN_EQ, XP_AWK_BINOP_EQ },
		{ TOKEN_NE, XP_AWK_BINOP_NE },
		{ TOKEN_EOF, 0 }
	};

	return __parse_binary_expr (awk, map, __parse_relational);
}

static xp_awk_nde_t* __parse_relational (xp_awk_t* awk)
{
	__binmap_t map[] = 
	{
		{ TOKEN_GT, XP_AWK_BINOP_GT },
		{ TOKEN_GE, XP_AWK_BINOP_GE },
		{ TOKEN_LT, XP_AWK_BINOP_LT },
		{ TOKEN_LE, XP_AWK_BINOP_LE },
		{ TOKEN_EOF, 0 }
	};

	return __parse_binary_expr (awk, map, __parse_shift);
}

static xp_awk_nde_t* __parse_shift (xp_awk_t* awk)
{
	__binmap_t map[] = 
	{
		{ TOKEN_LSHIFT, XP_AWK_BINOP_LSHIFT },
		{ TOKEN_RSHIFT, XP_AWK_BINOP_RSHIFT },
		{ TOKEN_EOF, 0 }
	};

	return __parse_binary_expr (awk, map, __parse_additive);
}

static xp_awk_nde_t* __parse_additive (xp_awk_t* awk)
{
	__binmap_t map[] = 
	{
		{ TOKEN_PLUS, XP_AWK_BINOP_PLUS },
		{ TOKEN_MINUS, XP_AWK_BINOP_MINUS },
		{ TOKEN_EOF, 0 }
	};

	return __parse_binary_expr (awk, map, __parse_multiplicative);
}

static xp_awk_nde_t* __parse_multiplicative (xp_awk_t* awk)
{
	__binmap_t map[] = 
	{
		{ TOKEN_MUL, XP_AWK_BINOP_MUL },
		{ TOKEN_DIV, XP_AWK_BINOP_DIV },
		{ TOKEN_MOD, XP_AWK_BINOP_MOD },
		{ TOKEN_EXP, XP_AWK_BINOP_EXP },
		{ TOKEN_EOF, 0 }
	};

	return __parse_binary_expr (awk, map, __parse_unary);
}

static xp_awk_nde_t* __parse_unary (xp_awk_t* awk)
{
	xp_awk_nde_exp_t* nde; 
	xp_awk_nde_t* left;
	int opcode;

	opcode = (MATCH(awk,TOKEN_PLUS))?  XP_AWK_UNROP_PLUS:
	         (MATCH(awk,TOKEN_MINUS))? XP_AWK_UNROP_MINUS:
	         (MATCH(awk,TOKEN_NOT))?   XP_AWK_UNROP_NOT:
	         (MATCH(awk,TOKEN_TILDE))? XP_AWK_UNROP_BNOT: -1;

	if (opcode == -1) return __parse_increment (awk);

	if (__get_token(awk) == -1) return XP_NULL;

	left = __parse_unary (awk);
	if (left == XP_NULL) return XP_NULL;

	nde = (xp_awk_nde_exp_t*)
		xp_malloc (xp_sizeof(xp_awk_nde_exp_t));
	if (nde == XP_NULL)
	{
		xp_awk_clrpt (left);
		PANIC (awk, XP_AWK_ENOMEM);
	}

	nde->type = XP_AWK_NDE_EXP_UNR;
	nde->next = XP_NULL;
	nde->opcode = opcode;
	nde->left = left;
	nde->right = XP_NULL;

	return (xp_awk_nde_t*)nde;
}

static xp_awk_nde_t* __parse_increment (xp_awk_t* awk)
{
	xp_awk_nde_exp_t* nde;
	xp_awk_nde_t* left;
	int type, opcode, opcode1, opcode2;

	opcode1 = MATCH(awk,TOKEN_PLUSPLUS)? XP_AWK_INCOP_PLUS:
	          MATCH(awk,TOKEN_MINUSMINUS)? XP_AWK_INCOP_MINUS: -1;

	if (opcode1 != -1)
	{
		if (__get_token(awk) == -1) return XP_NULL;
	}

	left = __parse_primary (awk);
	if (left == XP_NULL) return XP_NULL;

	opcode2 = MATCH(awk,TOKEN_PLUSPLUS)? XP_AWK_INCOP_PLUS:
	          MATCH(awk,TOKEN_MINUSMINUS)? XP_AWK_INCOP_MINUS: -1;

	if (opcode1 != -1 && opcode2 != -1)
	{
		xp_awk_clrpt (left);
		PANIC (awk, XP_AWK_ELVALUE);
	}
	else if (opcode1 == -1 && opcode2 == -1)
	{
		return left;
	}
	else if (opcode1 != -1) 
	{
		type = XP_AWK_NDE_EXP_INCPRE;
		opcode = opcode1;
	}
	else if (opcode2 != -1) 
	{
		type = XP_AWK_NDE_EXP_INCPST;
		opcode = opcode2;

		if (__get_token(awk) == -1) return XP_NULL;
	}

	nde = (xp_awk_nde_exp_t*)
		xp_malloc (xp_sizeof(xp_awk_nde_exp_t));
	if (nde == XP_NULL)
	{
		xp_awk_clrpt (left);
		PANIC (awk, XP_AWK_ENOMEM);
	}

	nde->type = type;
	nde->next = XP_NULL;
	nde->opcode = opcode;
	nde->left = left;
	nde->right = XP_NULL;

	return (xp_awk_nde_t*)nde;
}

static xp_awk_nde_t* __parse_primary (xp_awk_t* awk)
{
	if (MATCH(awk,TOKEN_IDENT))  
	{
		xp_char_t* name_dup;

		name_dup = (xp_char_t*)xp_strdup(XP_STR_BUF(&awk->token.name));
		if (name_dup == XP_NULL) PANIC (awk, XP_AWK_ENOMEM);

		if (__get_token(awk) == -1) 
		{
			xp_free (name_dup);	
			return XP_NULL;			
		}

		if (MATCH(awk,TOKEN_LBRACK)) 
		{
			xp_awk_nde_t* nde;
			nde = __parse_hashidx (awk, name_dup);
			if (nde == XP_NULL) xp_free (name_dup);
			return (xp_awk_nde_t*)nde;
		}
		else if (MATCH(awk,TOKEN_LPAREN)) 
		{
			/* function call */
			xp_awk_nde_t* nde;
			nde = __parse_funcall (awk, name_dup);
			if (nde == XP_NULL) xp_free (name_dup);
			return (xp_awk_nde_t*)nde;
		}	
		else 
		{
			/* normal variable */
			xp_awk_nde_var_t* nde;
			xp_size_t idxa;
	
			nde = (xp_awk_nde_var_t*)xp_malloc(xp_sizeof(xp_awk_nde_var_t));
			if (nde == XP_NULL) 
			{
				xp_free (name_dup);
				PANIC (awk, XP_AWK_ENOMEM);
			}

			/* search the parameter name list */
			idxa = xp_awk_tab_find(&awk->parse.params, name_dup, 0);
			if (idxa != (xp_size_t)-1) 
			{
				nde->type = XP_AWK_NDE_ARG;
				nde->next = XP_NULL;
				/*nde->id.name = XP_NULL;*/
				nde->id.name = name_dup;
				nde->id.idxa = idxa;

				return (xp_awk_nde_t*)nde;
			}

			/* search the local variable list */
			idxa = xp_awk_tab_rrfind(&awk->parse.locals, name_dup, 0);
			if (idxa != (xp_size_t)-1) 
			{
				nde->type = XP_AWK_NDE_LOCAL;
				nde->next = XP_NULL;
				/*nde->id.name = XP_NULL;*/
				nde->id.name = name_dup;
				nde->id.idxa = idxa;

				return (xp_awk_nde_t*)nde;
			}

			/* search the global variable list */
			idxa = xp_awk_tab_rrfind(&awk->parse.globals, name_dup, 0);
			if (idxa != (xp_size_t)-1) 
			{
				nde->type = XP_AWK_NDE_GLOBAL;
				nde->next = XP_NULL;
				/*nde->id.name = XP_NULL;*/
				nde->id.name = name_dup;
				nde->id.idxa = idxa;

				return (xp_awk_nde_t*)nde;
			}

			if (awk->opt.parse & XP_AWK_IMPLICIT) 
			{
				nde->type = XP_AWK_NDE_NAMED;
				nde->next = XP_NULL;
				nde->id.name = name_dup;
				nde->id.idxa = (xp_size_t)-1;

				return (xp_awk_nde_t*)nde;
			}

			/* undefined variable */
			xp_free (name_dup);
			xp_free (nde);
			PANIC (awk, XP_AWK_EUNDEF);
		}
	}
	else if (MATCH(awk,TOKEN_INT)) 
	{
		xp_awk_nde_int_t* nde;

		nde = (xp_awk_nde_int_t*)xp_malloc(xp_sizeof(xp_awk_nde_int_t));
		if (nde == XP_NULL) PANIC (awk, XP_AWK_ENOMEM);

		nde->type = XP_AWK_NDE_INT;
		nde->next = XP_NULL;
		nde->val = xp_awk_strtolong (XP_STR_BUF(&awk->token.name), 0);

		xp_assert (
			XP_STR_LEN(&awk->token.name) ==
			xp_strlen(XP_STR_BUF(&awk->token.name)));

		if (__get_token(awk) == -1) 
		{
			xp_free (nde);
			return XP_NULL;			
		}

		return (xp_awk_nde_t*)nde;
	}
	else if (MATCH(awk,TOKEN_REAL)) 
	{
		xp_awk_nde_real_t* nde;

		nde = (xp_awk_nde_real_t*) 
			xp_malloc (xp_sizeof(xp_awk_nde_real_t));
		if (nde == XP_NULL) PANIC (awk, XP_AWK_ENOMEM);

		nde->type = XP_AWK_NDE_REAL;
		nde->next = XP_NULL;
		nde->val = xp_awk_strtoreal (XP_STR_BUF(&awk->token.name));

		xp_assert (
			XP_STR_LEN(&awk->token.name) ==
			xp_strlen(XP_STR_BUF(&awk->token.name)));

		if (__get_token(awk) == -1) 
		{
			xp_free (nde);
			return XP_NULL;			
		}

		return (xp_awk_nde_t*)nde;
	}
	else if (MATCH(awk,TOKEN_STR))  
	{
		xp_awk_nde_str_t* nde;

		nde = (xp_awk_nde_str_t*)xp_malloc(xp_sizeof(xp_awk_nde_str_t));
		if (nde == XP_NULL) PANIC (awk, XP_AWK_ENOMEM);

		nde->type = XP_AWK_NDE_STR;
		nde->next = XP_NULL;
		nde->len = XP_STR_LEN(&awk->token.name);
		nde->buf = xp_strxdup(XP_STR_BUF(&awk->token.name), nde->len);
		if (nde->buf == XP_NULL) 
		{
			xp_free (nde);
			PANIC (awk, XP_AWK_ENOMEM);
		}

		if (__get_token(awk) == -1) 
		{
			xp_free (nde->buf);
			xp_free (nde);
			return XP_NULL;			
		}

		return (xp_awk_nde_t*)nde;
	}
	else if (MATCH(awk,TOKEN_REGEX))
	{
		/* TODO: .... */
	}
	else if (MATCH(awk,TOKEN_DOLLAR)) 
	{
		xp_awk_nde_pos_t* nde;
		xp_awk_nde_t* prim;

		if (__get_token(awk)) return XP_NULL;
		
		prim = __parse_primary (awk);
		if (prim == XP_NULL) return XP_NULL;

		nde = (xp_awk_nde_pos_t*)xp_malloc(xp_sizeof(xp_awk_nde_pos_t));
		if (nde == XP_NULL) 
		{
			xp_awk_clrpt (prim);
			PANIC (awk, XP_AWK_ENOMEM);
		}

		nde->type = XP_AWK_NDE_POS;
		nde->next = XP_NULL;
		nde->val = prim;

		return (xp_awk_nde_t*)nde;
	}
	else if (MATCH(awk,TOKEN_LPAREN)) 
	{
		xp_awk_nde_t* nde;

		/* eat up the left parenthesis */
		if (__get_token(awk) == -1) return XP_NULL;

		/* parse the sub-expression inside the parentheses */
		nde = __parse_expression (awk);
		if (nde == XP_NULL) return XP_NULL;

		/* check for the closing parenthesis */
		if (!MATCH(awk,TOKEN_RPAREN)) 
		{
			xp_awk_clrpt (nde);
			PANIC (awk, XP_AWK_ERPAREN);
		}

		if (__get_token(awk) == -1) 
		{
			xp_awk_clrpt (nde);
			return XP_NULL;
		}

		return nde;
	}

	/* valid expression introducer is expected */
	PANIC (awk, XP_AWK_EEXPRESSION);
}

static xp_awk_nde_t* __parse_hashidx (xp_awk_t* awk, xp_char_t* name)
{
	xp_awk_nde_t* idx;
	xp_awk_nde_idx_t* nde;
	xp_size_t idxa;

	if (__get_token(awk) == -1) return XP_NULL;

	idx = __parse_expression (awk);
	if (idx == XP_NULL) return XP_NULL;

	if (!MATCH(awk,TOKEN_RBRACK)) 
	{
		xp_awk_clrpt (idx);
		PANIC (awk, XP_AWK_ERBRACK);
	}

	if (__get_token(awk) == -1) 
	{
		xp_awk_clrpt (idx);
		return XP_NULL;
	}

	nde = (xp_awk_nde_idx_t*)xp_malloc(xp_sizeof(xp_awk_nde_idx_t));
	if (nde == XP_NULL) 
	{
		xp_awk_clrpt (idx);
		PANIC (awk, XP_AWK_ENOMEM);
	}

	/* search the parameter name list */
	idxa = xp_awk_tab_find(&awk->parse.params, name, 0);
	if (idxa != (xp_size_t)-1) 
	{
		nde->type = XP_AWK_NDE_ARGIDX;
		nde->next = XP_NULL;
		/*nde->id.name = XP_NULL; */
		nde->id.name = name;
		nde->id.idxa = idxa;
		nde->idx = idx;

		return (xp_awk_nde_t*)nde;
	}

	/* search the local variable list */
	idxa = xp_awk_tab_rrfind(&awk->parse.locals, name, 0);
	if (idxa != (xp_size_t)-1) 
	{
		nde->type = XP_AWK_NDE_LOCALIDX;
		nde->next = XP_NULL;
		/*nde->id.name = XP_NULL; */
		nde->id.name = name;
		nde->id.idxa = idxa;
		nde->idx = idx;

		return (xp_awk_nde_t*)nde;
	}

	/* search the global variable list */
	idxa = xp_awk_tab_rrfind(&awk->parse.globals, name, 0);
	if (idxa != (xp_size_t)-1) 
	{
		nde->type = XP_AWK_NDE_GLOBALIDX;
		nde->next = XP_NULL;
		/*nde->id.name = XP_NULL;*/
		nde->id.name = name;
		nde->id.idxa = idxa;

		return (xp_awk_nde_t*)nde;
	}

	if (awk->opt.parse & XP_AWK_IMPLICIT) 
	{
		nde->type = XP_AWK_NDE_NAMEDIDX;
		nde->next = XP_NULL;
		nde->id.name = name;
		nde->id.idxa = (xp_size_t)-1;
		nde->idx = idx;

		return (xp_awk_nde_t*)nde;
	}

	/* undefined variable */
	xp_awk_clrpt (idx);
	xp_free (nde);
	PANIC (awk, XP_AWK_EUNDEF);
}

static xp_awk_nde_t* __parse_funcall (xp_awk_t* awk, xp_char_t* name)
{
	xp_awk_nde_t* head, * curr, * nde;
	xp_awk_nde_call_t* call;

	if (__get_token(awk) == -1) return XP_NULL;
	
	head = curr = XP_NULL;

	if (MATCH(awk,TOKEN_RPAREN)) 
	{
		/* no parameters to the function call */
		if (__get_token(awk) == -1) return XP_NULL;
	}
	else 
	{
		while (1) 
		{
			nde = __parse_expression (awk);
			if (nde == XP_NULL) 
			{
				if (head != XP_NULL) xp_awk_clrpt (head);
				return XP_NULL;
			}
	
			if (head == XP_NULL) head = nde;
			else curr->next = nde;
			curr = nde;

			if (MATCH(awk,TOKEN_RPAREN)) 
			{
				if (__get_token(awk) == -1) 
				{
					if (head != XP_NULL) xp_awk_clrpt (head);
					return XP_NULL;
				}
				break;
			}

			if (!MATCH(awk,TOKEN_COMMA)) 
			{
				if (head != XP_NULL) xp_awk_clrpt (head);
				PANIC (awk, XP_AWK_ECOMMA);	
			}

			if (__get_token(awk) == -1) 
			{
				if (head != XP_NULL) xp_awk_clrpt (head);
				return XP_NULL;
			}
		}

	}

	call = (xp_awk_nde_call_t*)xp_malloc(xp_sizeof(xp_awk_nde_call_t));
	if (call == XP_NULL) 
	{
		if (head != XP_NULL) xp_awk_clrpt (head);
		PANIC (awk, XP_AWK_ENOMEM);
	}

	call->type = XP_AWK_NDE_CALL;
	call->next = XP_NULL;
	call->name = name;
	call->args = head;

	return (xp_awk_nde_t*)call;
}

static xp_awk_nde_t* __parse_if (xp_awk_t* awk)
{
	xp_awk_nde_t* test;
	xp_awk_nde_t* then_part;
	xp_awk_nde_t* else_part;
	xp_awk_nde_if_t* nde;

	if (!MATCH(awk,TOKEN_LPAREN)) PANIC (awk, XP_AWK_ELPAREN);
	if (__get_token(awk) == -1) return XP_NULL;

	test = __parse_expression (awk);
	if (test == XP_NULL) return XP_NULL;

	if (!MATCH(awk,TOKEN_RPAREN)) 
	{
		xp_awk_clrpt (test);
		PANIC (awk, XP_AWK_ERPAREN);
	}

	if (__get_token(awk) == -1) 
	{
		xp_awk_clrpt (test);
		return XP_NULL;
	}

	then_part = __parse_statement (awk);
	if (then_part == XP_NULL) 
	{
		xp_awk_clrpt (test);
		return XP_NULL;
	}

	if (MATCH(awk,TOKEN_ELSE)) 
	{
		if (__get_token(awk) == -1) 
		{
			xp_awk_clrpt (then_part);
			xp_awk_clrpt (test);
			return XP_NULL;
		}

		else_part = __parse_statement (awk);
		if (else_part == XP_NULL) 
		{
			xp_awk_clrpt (then_part);
			xp_awk_clrpt (test);
			return XP_NULL;
		}
	}
	else else_part = XP_NULL;

	nde = (xp_awk_nde_if_t*)xp_malloc(xp_sizeof(xp_awk_nde_if_t));
	if (nde == XP_NULL) 
	{
		xp_awk_clrpt (else_part);
		xp_awk_clrpt (then_part);
		xp_awk_clrpt (test);
		PANIC (awk, XP_AWK_ENOMEM);
	}

	nde->type = XP_AWK_NDE_IF;
	nde->next = XP_NULL;
	nde->test = test;
	nde->then_part = then_part;
	nde->else_part = else_part;

	return (xp_awk_nde_t*)nde;
}

static xp_awk_nde_t* __parse_while (xp_awk_t* awk)
{
	xp_awk_nde_t* test, * body;
	xp_awk_nde_while_t* nde;

	if (!MATCH(awk,TOKEN_LPAREN)) PANIC (awk, XP_AWK_ELPAREN);
	if (__get_token(awk) == -1) return XP_NULL;

	test = __parse_expression (awk);
	if (test == XP_NULL) return XP_NULL;

	if (!MATCH(awk,TOKEN_RPAREN)) 
	{
		xp_awk_clrpt (test);
		PANIC (awk, XP_AWK_ERPAREN);
	}

	if (__get_token(awk) == -1) 
	{
		xp_awk_clrpt (test);
		return XP_NULL;
	}

	body = __parse_statement (awk);
	if (body == XP_NULL) 
	{
		xp_awk_clrpt (test);
		return XP_NULL;
	}

	nde = (xp_awk_nde_while_t*)xp_malloc(xp_sizeof(xp_awk_nde_while_t));
	if (nde == XP_NULL) 
	{
		xp_awk_clrpt (body);
		xp_awk_clrpt (test);
		PANIC (awk, XP_AWK_ENOMEM);
	}

	nde->type = XP_AWK_NDE_WHILE;
	nde->next = XP_NULL;
	nde->test = test;
	nde->body = body;

	return (xp_awk_nde_t*)nde;
}

static xp_awk_nde_t* __parse_for (xp_awk_t* awk)
{
	xp_awk_nde_t* init, * test, * incr, * body;
	xp_awk_nde_for_t* nde;

	/* TODO: parse for (x in list) ... */

	if (!MATCH(awk,TOKEN_LPAREN)) PANIC (awk, XP_AWK_ELPAREN);
	if (__get_token(awk) == -1) return XP_NULL;
		
	if (MATCH(awk,TOKEN_SEMICOLON)) init = XP_NULL;
	else 
	{
		init = __parse_expression (awk);
		if (init == XP_NULL) return XP_NULL;

		if (!MATCH(awk,TOKEN_SEMICOLON)) 
		{
			xp_awk_clrpt (init);
			PANIC (awk, XP_AWK_ESEMICOLON);
		}
	}

	if (__get_token(awk) == -1) 
	{
		xp_awk_clrpt (init);
		return XP_NULL;
	}

	if (MATCH(awk,TOKEN_SEMICOLON)) test = XP_NULL;
	else 
	{
		test = __parse_expression (awk);
		if (test == XP_NULL) 
		{
			xp_awk_clrpt (init);
			return XP_NULL;
		}

		if (!MATCH(awk,TOKEN_SEMICOLON)) 
		{
			xp_awk_clrpt (init);
			xp_awk_clrpt (test);
			PANIC (awk, XP_AWK_ESEMICOLON);
		}
	}

	if (__get_token(awk) == -1) 
	{
		xp_awk_clrpt (init);
		xp_awk_clrpt (test);
		return XP_NULL;
	}
	
	if (MATCH(awk,TOKEN_RPAREN)) incr = XP_NULL;
	else 
	{
		incr = __parse_expression (awk);
		if (incr == XP_NULL) 
		{
			xp_awk_clrpt (init);
			xp_awk_clrpt (test);
			return XP_NULL;
		}

		if (!MATCH(awk,TOKEN_RPAREN)) 
		{
			xp_awk_clrpt (init);
			xp_awk_clrpt (test);
			xp_awk_clrpt (incr);
			PANIC (awk, XP_AWK_ERPAREN);
		}
	}

	if (__get_token(awk) == -1) 
	{
		xp_awk_clrpt (init);
		xp_awk_clrpt (test);
		xp_awk_clrpt (incr);
		return XP_NULL;
	}

	body = __parse_statement (awk);
	if (body == XP_NULL) 
	{
		xp_awk_clrpt (init);
		xp_awk_clrpt (test);
		xp_awk_clrpt (incr);
		return XP_NULL;
	}

	nde = (xp_awk_nde_for_t*)xp_malloc(xp_sizeof(xp_awk_nde_for_t));
	if (nde == XP_NULL) 
	{
		xp_awk_clrpt (init);
		xp_awk_clrpt (test);
		xp_awk_clrpt (incr);
		xp_awk_clrpt (body);
		PANIC (awk, XP_AWK_ENOMEM);
	}

	nde->type = XP_AWK_NDE_FOR;
	nde->next = XP_NULL;
	nde->init = init;
	nde->test = test;
	nde->incr = incr;
	nde->body = body;

	return (xp_awk_nde_t*)nde;
}

static xp_awk_nde_t* __parse_dowhile (xp_awk_t* awk)
{
	xp_awk_nde_t* test, * body;
	xp_awk_nde_while_t* nde;

	body = __parse_statement (awk);
	if (body == XP_NULL) return XP_NULL;

	if (!MATCH(awk,TOKEN_WHILE)) 
	{
		xp_awk_clrpt (body);
		PANIC (awk, XP_AWK_EWHILE);
	}

	if (__get_token(awk) == -1) 
	{
		xp_awk_clrpt (body);
		return XP_NULL;
	}

	if (!MATCH(awk,TOKEN_LPAREN)) 
	{
		xp_awk_clrpt (body);
		PANIC (awk, XP_AWK_ELPAREN);
	}

	if (__get_token(awk) == -1) 
	{
		xp_awk_clrpt (body);
		return XP_NULL;
	}

	test = __parse_expression (awk);
	if (test == XP_NULL) 
	{
		xp_awk_clrpt (body);
		return XP_NULL;
	}

	if (!MATCH(awk,TOKEN_RPAREN)) 
	{
		xp_awk_clrpt (body);
		xp_awk_clrpt (test);
		PANIC (awk, XP_AWK_ERPAREN);
	}

	if (__get_token(awk) == -1) 
	{
		xp_awk_clrpt (body);
		xp_awk_clrpt (test);
		return XP_NULL;
	}
	
	nde = (xp_awk_nde_while_t*)xp_malloc(xp_sizeof(xp_awk_nde_while_t));
	if (nde == XP_NULL) 
	{
		xp_awk_clrpt (body);
		xp_awk_clrpt (test);
		PANIC (awk, XP_AWK_ENOMEM);
	}

	nde->type = XP_AWK_NDE_DOWHILE;
	nde->next = XP_NULL;
	nde->test = test;
	nde->body = body;

	return (xp_awk_nde_t*)nde;
}

static xp_awk_nde_t* __parse_break (xp_awk_t* awk)
{
	xp_awk_nde_break_t* nde;

	nde = (xp_awk_nde_break_t*)xp_malloc(xp_sizeof(xp_awk_nde_break_t));
	if (nde == XP_NULL) PANIC (awk, XP_AWK_ENOMEM);
	nde->type = XP_AWK_NDE_BREAK;
	nde->next = XP_NULL;
	
	return (xp_awk_nde_t*)nde;
}

static xp_awk_nde_t* __parse_continue (xp_awk_t* awk)
{
	xp_awk_nde_continue_t* nde;

	nde = (xp_awk_nde_continue_t*)xp_malloc(xp_sizeof(xp_awk_nde_continue_t));
	if (nde == XP_NULL) PANIC (awk, XP_AWK_ENOMEM);
	nde->type = XP_AWK_NDE_CONTINUE;
	nde->next = XP_NULL;
	
	return (xp_awk_nde_t*)nde;
}

static xp_awk_nde_t* __parse_return (xp_awk_t* awk)
{
	xp_awk_nde_return_t* nde;
	xp_awk_nde_t* val;

	nde = (xp_awk_nde_return_t*)xp_malloc(xp_sizeof(xp_awk_nde_return_t));
	if (nde == XP_NULL) PANIC (awk, XP_AWK_ENOMEM);
	nde->type = XP_AWK_NDE_RETURN;
	nde->next = XP_NULL;

	if (MATCH(awk,TOKEN_SEMICOLON)) 
	{
		/* no return value */
		val = XP_NULL;
	}
	else 
	{
		val = __parse_expression (awk);
		if (val == XP_NULL) 
		{
			xp_free (nde);
			return XP_NULL;
		}
	}

	nde->val = val;
	return (xp_awk_nde_t*)nde;
}

static xp_awk_nde_t* __parse_exit (xp_awk_t* awk)
{
	xp_awk_nde_exit_t* nde;
	xp_awk_nde_t* val;

	nde = (xp_awk_nde_exit_t*)xp_malloc(xp_sizeof(xp_awk_nde_exit_t));
	if (nde == XP_NULL) PANIC (awk, XP_AWK_ENOMEM);
	nde->type = XP_AWK_NDE_EXIT;
	nde->next = XP_NULL;

	if (MATCH(awk,TOKEN_SEMICOLON)) 
	{
		/* no exit code */
		val = XP_NULL;
	}
	else 
	{
		val = __parse_expression (awk);
		if (val == XP_NULL) 
		{
			xp_free (nde);
			return XP_NULL;
		}
	}

	nde->val = val;
	return (xp_awk_nde_t*)nde;
}

static xp_awk_nde_t* __parse_delete (xp_awk_t* awk)
{
/* TODO: implement this... */
	return XP_NULL;
}

static xp_awk_nde_t* __parse_next (xp_awk_t* awk)
{
	xp_awk_nde_t* nde;

	nde = (xp_awk_nde_t*)xp_malloc(xp_sizeof(xp_awk_nde_t));
	if (nde == XP_NULL) PANIC (awk, XP_AWK_ENOMEM);
	nde->type = XP_AWK_NDE_NEXT;
	nde->next = XP_NULL;
	
	return nde;
}

static xp_awk_nde_t* __parse_nextfile (xp_awk_t* awk)
{
	xp_awk_nde_t* nde;

	nde = (xp_awk_nde_t*)xp_malloc(xp_sizeof(xp_awk_nde_t));
	if (nde == XP_NULL) PANIC (awk, XP_AWK_ENOMEM);
	nde->type = XP_AWK_NDE_NEXTFILE;
	nde->next = XP_NULL;
	
	return nde;
}

static int __get_token (xp_awk_t* awk)
{
	xp_cint_t c;
	int n;

	do 
	{
		if (__skip_spaces(awk) == -1) return -1;
		if ((n = __skip_comment(awk)) == -1) return -1;
	} 
	while (n == 1);

	xp_str_clear (&awk->token.name);
	c = awk->lex.curc;

	if (c == XP_CHAR_EOF) 
	{
		SET_TOKEN_TYPE (awk, TOKEN_EOF);
	}	
	else if (xp_isdigit(c)) 
	{
		if (__get_number(awk) == -1) return -1;
	}
	else if (xp_isalpha(c) || c == XP_CHAR('_')) 
	{
		/* identifier */
		do 
		{
			ADD_TOKEN_CHAR (awk, c);
			GET_CHAR_TO (awk, c);
		} 
		while (xp_isalpha(c) || c == XP_CHAR('_') || xp_isdigit(c));

		SET_TOKEN_TYPE (awk, __classify_ident(awk,XP_STR_BUF(&awk->token.name)));
	}
	else if (c == XP_CHAR('\"')) 
	{
		SET_TOKEN_TYPE (awk, TOKEN_STR);

		if (__get_string(awk) == -1) return -1;
		while (1)
		{
			do 
			{
				if (__skip_spaces(awk) == -1) return -1;
				if ((n = __skip_comment(awk)) == -1) return -1;
			} 
			while (n == 1);

			c = awk->lex.curc;
			if (c != XP_CHAR('\"')) break;

			if (__get_string(awk) == -1) return -1;
		}
	}
	else if (c == XP_CHAR('=')) 
	{
		GET_CHAR_TO (awk, c);
		if (c == XP_CHAR('=')) 
		{
			SET_TOKEN_TYPE (awk, TOKEN_EQ);
			ADD_TOKEN_STR (awk, XP_TEXT("=="));
			GET_CHAR_TO (awk, c);
		}
		else 
		{
			SET_TOKEN_TYPE (awk, TOKEN_ASSIGN);
			ADD_TOKEN_STR (awk, XP_TEXT("="));
		}
	}
	else if (c == XP_CHAR('!')) 
	{
		GET_CHAR_TO (awk, c);
		if (c == XP_CHAR('=')) 
		{
			SET_TOKEN_TYPE (awk, TOKEN_NE);
			ADD_TOKEN_STR (awk, XP_TEXT("!="));
			GET_CHAR_TO (awk, c);
		}
		else if (c == XP_CHAR('~'))
		{
			SET_TOKEN_TYPE (awk, TOKEN_NM);
			ADD_TOKEN_STR (awk, XP_TEXT("!~"));
			GET_CHAR_TO (awk, c);
		}
		else 
		{
			SET_TOKEN_TYPE (awk, TOKEN_NOT);
			ADD_TOKEN_STR (awk, XP_TEXT("!"));
		}
	}
	else if (c == XP_CHAR('>')) 
	{
		GET_CHAR_TO (awk, c);
		if ((awk->opt.parse & XP_AWK_SHIFT) && c == XP_CHAR('>')) 
		{
			SET_TOKEN_TYPE (awk, TOKEN_RSHIFT);
			ADD_TOKEN_STR (awk, XP_TEXT(">>"));
			GET_CHAR_TO (awk, c);
		}
		else if (c == XP_CHAR('=')) 
		{
			SET_TOKEN_TYPE (awk, TOKEN_GE);
			ADD_TOKEN_STR (awk, XP_TEXT(">="));
			GET_CHAR_TO (awk, c);
		}
		else 
		{
			SET_TOKEN_TYPE (awk, TOKEN_GT);
			ADD_TOKEN_STR (awk, XP_TEXT(">"));
		}
	}
	else if (c == XP_CHAR('<')) 
	{
		GET_CHAR_TO (awk, c);
		if ((awk->opt.parse & XP_AWK_SHIFT) && c == XP_CHAR('<')) 
		{
			SET_TOKEN_TYPE (awk, TOKEN_LSHIFT);
			ADD_TOKEN_STR (awk, XP_TEXT("<<"));
			GET_CHAR_TO (awk, c);
		}
		else if (c == XP_CHAR('=')) 
		{
			SET_TOKEN_TYPE (awk, TOKEN_LE);
			ADD_TOKEN_STR (awk, XP_TEXT("<="));
			GET_CHAR_TO (awk, c);
		}
		else 
		{
			SET_TOKEN_TYPE (awk, TOKEN_LT);
			ADD_TOKEN_STR (awk, XP_TEXT("<"));
		}
	}
	else if (c == XP_CHAR('|'))
	{
		GET_CHAR_TO (awk, c);
		if (c == XP_CHAR('|'))
		{
			SET_TOKEN_TYPE (awk, TOKEN_LOR);
			ADD_TOKEN_STR (awk, XP_TEXT("||"));
			GET_CHAR_TO (awk, c);
		}
		else
		{
			SET_TOKEN_TYPE (awk, TOKEN_BOR);
			ADD_TOKEN_STR (awk, XP_TEXT("|"));
		}
	}
	else if (c == XP_CHAR('&'))
	{
		GET_CHAR_TO (awk, c);
		if (c == XP_CHAR('&'))
		{
			SET_TOKEN_TYPE (awk, TOKEN_LAND);
			ADD_TOKEN_STR (awk, XP_TEXT("&&"));
			GET_CHAR_TO (awk, c);
		}
		else
		{
			SET_TOKEN_TYPE (awk, TOKEN_BAND);
			ADD_TOKEN_STR (awk, XP_TEXT("&"));
		}
	}
	else if (c == XP_CHAR('~'))
	{
		SET_TOKEN_TYPE (awk, TOKEN_TILDE);
		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);
	}
	else if (c == XP_CHAR('^'))
	{
		SET_TOKEN_TYPE (awk, TOKEN_BXOR);
		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);
	}
	else if (c == XP_CHAR('+')) 
	{
		GET_CHAR_TO (awk, c);
		if (c == XP_CHAR('+')) 
		{
			SET_TOKEN_TYPE (awk, TOKEN_PLUSPLUS);
			ADD_TOKEN_STR (awk, XP_TEXT("++"));
			GET_CHAR_TO (awk, c);
		}
		else if (c == XP_CHAR('=')) 
		{
			SET_TOKEN_TYPE (awk, TOKEN_PLUS_ASSIGN);
			ADD_TOKEN_STR (awk, XP_TEXT("+="));
			GET_CHAR_TO (awk, c);
		}
		else 
		{
			SET_TOKEN_TYPE (awk, TOKEN_PLUS);
			ADD_TOKEN_STR (awk, XP_TEXT("+"));
		}
	}
	else if (c == XP_CHAR('-')) 
	{
		GET_CHAR_TO (awk, c);
		if (c == XP_CHAR('-')) 
		{
			SET_TOKEN_TYPE (awk, TOKEN_MINUSMINUS);
			ADD_TOKEN_STR (awk, XP_TEXT("--"));
			GET_CHAR_TO (awk, c);
		}
		else if (c == XP_CHAR('=')) 
		{
			SET_TOKEN_TYPE (awk, TOKEN_MINUS_ASSIGN);
			ADD_TOKEN_STR (awk, XP_TEXT("-="));
			GET_CHAR_TO (awk, c);
		}
		else 
		{
			SET_TOKEN_TYPE (awk, TOKEN_MINUS);
			ADD_TOKEN_STR (awk, XP_TEXT("-"));
		}
	}
	else if (c == XP_CHAR('*')) 
	{
		GET_CHAR_TO (awk, c);

		if (c == XP_CHAR('='))
		{
			SET_TOKEN_TYPE (awk, TOKEN_MUL_ASSIGN);
			ADD_TOKEN_CHAR (awk, c);
			GET_CHAR_TO (awk, c);
		}
		else if (c == XP_CHAR('*'))
		{
			GET_CHAR_TO (awk, c);
			if (c == XP_CHAR('='))
			{
				SET_TOKEN_TYPE (awk, TOKEN_EXP_ASSIGN);
				ADD_TOKEN_CHAR (awk, c);
				GET_CHAR_TO (awk, c);
			}
			else 
			{
				SET_TOKEN_TYPE (awk, TOKEN_EXP);
				ADD_TOKEN_CHAR (awk, c);
			}
		}
		else
		{
			SET_TOKEN_TYPE (awk, TOKEN_MUL);
			ADD_TOKEN_CHAR (awk, c);
		}
	}
	else if (c == XP_CHAR('/')) 
	{
/* TODO: handle regular expression here... /^pattern$/ */
		GET_CHAR_TO (awk, c);

		if (c == XP_CHAR('='))
		{
			SET_TOKEN_TYPE (awk, TOKEN_DIV_ASSIGN);
			ADD_TOKEN_CHAR (awk, c);
			GET_CHAR_TO (awk, c);
		}
		else
		{
			SET_TOKEN_TYPE (awk, TOKEN_DIV);
			ADD_TOKEN_CHAR (awk, c);
		}
	}
	else if (c == XP_CHAR('%')) 
	{
		GET_CHAR_TO (awk, c);

		if (c == XP_CHAR('='))
		{
			SET_TOKEN_TYPE (awk, TOKEN_MOD_ASSIGN);
			ADD_TOKEN_CHAR (awk, c);
			GET_CHAR_TO (awk, c);
		}
		else
		{
			SET_TOKEN_TYPE (awk, TOKEN_MOD);
			ADD_TOKEN_CHAR (awk, c);
		}
	}
	else if (c == XP_CHAR('(')) 
	{
		SET_TOKEN_TYPE (awk, TOKEN_LPAREN);
		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);
	}
	else if (c == XP_CHAR(')')) 
	{
		SET_TOKEN_TYPE (awk, TOKEN_RPAREN);
		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);
	}
	else if (c == XP_CHAR('{')) 
	{
		SET_TOKEN_TYPE (awk, TOKEN_LBRACE);
		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);
	}
	else if (c == XP_CHAR('}')) 
	{
		SET_TOKEN_TYPE (awk, TOKEN_RBRACE);
		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);
	}
	else if (c == XP_CHAR('[')) 
	{
		SET_TOKEN_TYPE (awk, TOKEN_LBRACK);
		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);
	}
	else if (c == XP_CHAR(']')) 
	{
		SET_TOKEN_TYPE (awk, TOKEN_RBRACK);
		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);
	}
	else if (c == XP_CHAR('$')) 
	{
		SET_TOKEN_TYPE (awk, TOKEN_DOLLAR);
		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);
	}
	else if (c == XP_CHAR(',')) 
	{
		SET_TOKEN_TYPE (awk, TOKEN_COMMA);
		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);
	}
	else if (c == XP_CHAR(';')) 
	{
		SET_TOKEN_TYPE (awk, TOKEN_SEMICOLON);
		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);
	}
	else if (c == XP_CHAR(':'))
	{
		SET_TOKEN_TYPE (awk, TOKEN_COLON);
		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);
	}
	else if (c == XP_CHAR('?'))
	{
		SET_TOKEN_TYPE (awk, TOKEN_QUEST);
		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);
	}
	else 
	{
		awk->errnum = XP_AWK_ELXCHR;
		return -1;
	}

	return 0;
}

static int __get_number (xp_awk_t* awk)
{
	xp_cint_t c;

	xp_assert (XP_STR_LEN(&awk->token.name) == 0);
	SET_TOKEN_TYPE (awk, TOKEN_INT);

	c = awk->lex.curc;

	if (c == XP_CHAR('0'))
	{
		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);

		if (c == XP_CHAR('x') || c == XP_CHAR('X'))
		{
			/* hexadecimal number */
			do 
			{
				ADD_TOKEN_CHAR (awk, c);
				GET_CHAR_TO (awk, c);
			} 
			while (xp_isxdigit(c));

			return 0;
		}
		else if (c == XP_CHAR('b') || c == XP_CHAR('B'))
		{
			/* binary number */
			do
			{
				ADD_TOKEN_CHAR (awk, c);
				GET_CHAR_TO (awk, c);
			} 
			while (c == XP_CHAR('0') || c == XP_CHAR('1'));

			return 0;
		}
		else if (c != '.')
		{
			/* octal number */
			while (c >= XP_CHAR('0') && c <= XP_CHAR('7'))
			{
				ADD_TOKEN_CHAR (awk, c);
				GET_CHAR_TO (awk, c);
			}

			return 0;
		}
	}

	while (xp_isdigit(c)) 
	{
		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);
	} 

	if (c == XP_CHAR('.'))
	{
		/* floating-point number */
		SET_TOKEN_TYPE (awk, TOKEN_REAL);

		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);

		while (xp_isdigit(c))
		{
			ADD_TOKEN_CHAR (awk, c);
			GET_CHAR_TO (awk, c);
		}
	}

	if (c == XP_CHAR('E') || c == XP_CHAR('e'))
	{
		SET_TOKEN_TYPE (awk, TOKEN_REAL);

		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);

		if (c == XP_CHAR('+') || c == XP_CHAR('-'))
		{
			ADD_TOKEN_CHAR (awk, c);
			GET_CHAR_TO (awk, c);
		}

		while (xp_isdigit(c))
		{
			ADD_TOKEN_CHAR (awk, c);
			GET_CHAR_TO (awk, c);
		}
	}

	return 0;
}

static int __get_string (xp_awk_t* awk)
{
	xp_cint_t c;
	xp_bool_t escaped = xp_false;

	GET_CHAR_TO (awk, c);
	while (1)
	{
		if (c == XP_CHAR_EOF)
		{
			awk->errnum = XP_AWK_EENDSTR;
			return -1;
		}

		if (escaped == xp_false && c == XP_CHAR('\"'))
		{
			GET_CHAR_TO (awk, c);
			break;
		}

		if (escaped == xp_false && c == XP_CHAR('\\'))
		{
			GET_CHAR_TO (awk, c);
			escaped = xp_true;
			continue;
		}

		if (escaped == xp_true)
		{
			if (c == XP_CHAR('n')) c = XP_CHAR('\n');
			else if (c == XP_CHAR('r')) c = XP_CHAR('\r');
			else if (c == XP_CHAR('t')) c = XP_CHAR('\t');
			/* TODO: more escape characters */
			escaped = xp_false;
		}

		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);
	}
	
	return 0;
}

static int __get_char (xp_awk_t* awk)
{
	xp_ssize_t n;
	xp_char_t c;

	if (awk->lex.ungotc_count > 0) 
	{
		awk->lex.curc = awk->lex.ungotc[--awk->lex.ungotc_count];
		return 0;
	}

	n = awk->src_func(XP_AWK_IO_DATA, awk->src_arg, &c, 1);
	if (n == -1) 
	{
		awk->errnum = XP_AWK_ESRCDT;
		return -1;
	}
	awk->lex.curc = (n == 0)? XP_CHAR_EOF: c;
	
	return 0;
}

static int __unget_char (xp_awk_t* awk, xp_cint_t c)
{
	if (awk->lex.ungotc_count >= xp_countof(awk->lex.ungotc)) 
	{
		awk->errnum = XP_AWK_ELXUNG;
		return -1;
	}

	awk->lex.ungotc[awk->lex.ungotc_count++] = c;
	return 0;
}

static int __skip_spaces (xp_awk_t* awk)
{
	xp_cint_t c = awk->lex.curc;
	while (xp_isspace(c)) GET_CHAR_TO (awk, c);
	return 0;
}

static int __skip_comment (xp_awk_t* awk)
{
	xp_cint_t c = awk->lex.curc;

	if ((awk->opt.parse & XP_AWK_HASHSIGN) && c == XP_CHAR('#'))
	{
		do 
		{ 
			GET_CHAR_TO (awk, c);
		} 
		while (c != '\n' && c != XP_CHAR_EOF);

		GET_CHAR (awk);
		return 1; /* comment by # */
	}

	if (c != XP_CHAR('/')) return 0; /* not a comment */
	GET_CHAR_TO (awk, c);

	if (c == XP_CHAR('/')) 
	{
		do 
		{ 
			GET_CHAR_TO (awk, c);
		} 
		while (c != '\n' && c != XP_CHAR_EOF);

		GET_CHAR (awk);
		return 1; /* comment by // */
	}
	else if (c == XP_CHAR('*')) 
	{
		do 
		{
			GET_CHAR_TO (awk, c);
			if (c == XP_CHAR('*')) 
			{
				GET_CHAR_TO (awk, c);
				if (c == XP_CHAR('/')) 
				{
					GET_CHAR_TO (awk, c);
					break;
				}
			}
		} 
		while (1);

		return 1; /* c-style comment */
	}

	if (__unget_char(awk,c) == -1) return -1; /* error */
	awk->lex.curc = XP_CHAR('/');

	return 0;
}

static int __classify_ident (xp_awk_t* awk, const xp_char_t* ident)
{
	struct __kwent* p;

	for (p = __kwtab; p->name != XP_NULL; p++) 
	{
		if (p->valid != 0 && (awk->opt.parse & p->valid) == 0) continue;
		if (xp_strcmp(p->name, ident) == 0) return p->type;
	}

	return TOKEN_IDENT;
}

static int __assign_to_opcode (xp_awk_t* awk)
{
	if (MATCH(awk,TOKEN_ASSIGN)) return XP_AWK_ASSOP_NONE;
	if (MATCH(awk,TOKEN_PLUS_ASSIGN)) return XP_AWK_ASSOP_PLUS;
	if (MATCH(awk,TOKEN_MINUS_ASSIGN)) return XP_AWK_ASSOP_MINUS;
	if (MATCH(awk,TOKEN_MUL_ASSIGN)) return XP_AWK_ASSOP_MUL;
	if (MATCH(awk,TOKEN_DIV_ASSIGN)) return XP_AWK_ASSOP_DIV;
	if (MATCH(awk,TOKEN_MOD_ASSIGN)) return XP_AWK_ASSOP_MOD;
	if (MATCH(awk,TOKEN_EXP_ASSIGN)) return XP_AWK_ASSOP_EXP;

	return -1;
}
