/*
 * $Id: parse.c,v 1.47 2006-02-05 14:21:18 bacon Exp $
 */

#include <xp/awk/awk.h>

#ifndef __STAND_ALONE
#include <xp/bas/memory.h>
#include <xp/bas/ctype.h>
#include <xp/bas/string.h>
#include <xp/bas/assert.h>
#endif

enum
{
	TOKEN_EOF,

	TOKEN_ASSIGN,
	TOKEN_EQ,
	TOKEN_NE,
	TOKEN_NOT,
	TOKEN_PLUS,
	TOKEN_PLUS_PLUS,
	TOKEN_PLUS_ASSIGN,
	TOKEN_MINUS,
	TOKEN_MINUS_MINUS,
	TOKEN_MINUS_ASSIGN,
	TOKEN_MUL,
	TOKEN_DIV,
	TOKEN_MOD,

	TOKEN_LPAREN,
	TOKEN_RPAREN,
	TOKEN_LBRACE,
	TOKEN_RBRACE,
	TOKEN_LBRACK,
	TOKEN_RBRACK,

	TOKEN_DOLLAR,
	TOKEN_COMMA,
	TOKEN_SEMICOLON,

	TOKEN_INTEGER,
	TOKEN_STRING,
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

	__TOKEN_COUNT__
};

enum {
	BINOP_PLUS,
	BINOP_MINUS,
	BINOP_MUL,
	BINOP_DIV,
	BINOP_MOD
};

#if defined(__BORLANDC__) || defined(_MSC_VER)
	#define INLINE 
#else
	#define INLINE inline
#endif


static xp_awk_node_t* __parse_progunit (xp_awk_t* awk);
static xp_awk_node_t* __parse_function (xp_awk_t* awk);
static xp_awk_node_t* __parse_begin (xp_awk_t* awk);
static xp_awk_node_t* __parse_end (xp_awk_t* awk);
static xp_awk_node_t* __parse_action (xp_awk_t* awk);
static xp_awk_node_t* __parse_block (xp_awk_t* awk, xp_bool_t is_top);
static xp_awk_t*      __collect_locals (xp_awk_t* awk, xp_size_t nlocals);
static xp_awk_node_t* __parse_statement (xp_awk_t* awk);
static xp_awk_node_t* __parse_statement_nb (xp_awk_t* awk);
static xp_awk_node_t* __parse_expression (xp_awk_t* awk);
static xp_awk_node_t* __parse_basic_expr (xp_awk_t* awk);
static xp_awk_node_t* __parse_additive (xp_awk_t* awk);
static xp_awk_node_t* __parse_multiplicative (xp_awk_t* awk);
static xp_awk_node_t* __parse_unary (xp_awk_t* awk);
static xp_awk_node_t* __parse_primary (xp_awk_t* awk);
static xp_awk_node_t* __parse_hashidx (xp_awk_t* awk, xp_char_t* name);
static xp_awk_node_t* __parse_funcall (xp_awk_t* awk, xp_char_t* name);
static xp_awk_node_t* __parse_if (xp_awk_t* awk);
static xp_awk_node_t* __parse_while (xp_awk_t* awk);
static xp_awk_node_t* __parse_for (xp_awk_t* awk);
static xp_awk_node_t* __parse_dowhile (xp_awk_t* awk);
static xp_awk_node_t* __parse_break (xp_awk_t* awk);
static xp_awk_node_t* __parse_continue (xp_awk_t* awk);
static xp_awk_node_t* __parse_return (xp_awk_t* awk);
static xp_awk_node_t* __parse_exit (xp_awk_t* awk);
static xp_awk_node_t* __parse_next (xp_awk_t* awk);
static xp_awk_node_t* __parse_nextfile (xp_awk_t* awk);

static int __get_token (xp_awk_t* awk);
static int __get_char (xp_awk_t* awk);
static int __unget_char (xp_awk_t* awk, xp_cint_t c);
static int __skip_spaces (xp_awk_t* awk);
static int __skip_comment (xp_awk_t* awk);
static int __classify_ident (xp_awk_t* awk, const xp_char_t* ident);

static xp_long_t __str_to_long (const xp_char_t* name);

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

	{ XP_NULL,             0,              0 }
};

#define GET_CHAR(awk) \
	do { if (__get_char(awk) == -1) return -1; } while(0)

#define GET_CHAR_TO(awk,c) do { \
	if (__get_char(awk) == -1) return -1; \
	c = (awk)->lex.curc; \
} while(0)

#define SET_TOKEN_TYPE(awk,code) ((awk)->token.type = code)

#define ADD_TOKEN_CHAR(awk,c) do { \
	if (xp_str_ccat(&(awk)->token.name,(c)) == (xp_size_t)-1) { \
		(awk)->errnum = XP_AWK_ENOMEM; return -1; \
	} \
} while (0)

#define ADD_TOKEN_STR(awk,str) do { \
	if (xp_str_cat(&(awk)->token.name,(str)) == (xp_size_t)-1) { \
		(awk)->errnum = XP_AWK_ENOMEM; return -1; \
	} \
} while (0)

#define GET_TOKEN(awk) \
	do { if (__get_token(awk) == -1) return -1; } while(0)

#define MATCH(awk,token_type) ((awk)->token.type == (token_type))
#define CONSUME(awk) \
	do { if (__get_token(awk) == -1) return XP_NULL; } while(0)

#define PANIC(awk,code) do { (awk)->errnum = (code);  return XP_NULL; } while (0);

// TODO: remove stdio.h
#ifndef __STAND_ALONE
#include <xp/bas/stdio.h>
#endif
static int __dump_func (xp_awk_pair_t* pair)
{
	xp_awk_func_t* func = (xp_awk_func_t*)pair->value;
	xp_size_t i;

	xp_assert (xp_strcmp(pair->key, func->name) == 0);
	xp_printf (XP_TEXT("function %s ("), func->name);
	for (i = 0; i < func->nargs; ) {
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
	xp_awk_hash_walk (&awk->tree.funcs, __dump_func);

	if (awk->tree.begin != NULL) {
		xp_printf (XP_TEXT("BEGIN "));
		xp_awk_prnpt (awk->tree.begin);
		xp_printf (XP_TEXT("\n"));
	}

	if (awk->tree.end != NULL) {
		xp_printf (XP_TEXT("END "));
		xp_awk_prnpt (awk->tree.end);
	}

// TODO: dump unmaed top-level blocks...
}

int xp_awk_parse (xp_awk_t* awk)
{
	/* if you want to parse anew, call xp_awk_clear first.
	 * otherwise, the result is appened to the accumulated result */

	GET_CHAR (awk);
	GET_TOKEN (awk);

	while (1) {
		if (MATCH(awk,TOKEN_EOF)) break;

		if (__parse_progunit(awk) == XP_NULL) {
// TODO: cleanup the parse tree created so far....
//       function tables also etc...
			return -1;
		}
	}

xp_printf (XP_TEXT("-----------------------------\n"));
xp_printf (XP_TEXT("sucessful end - %d\n"), awk->errnum);
__dump (awk);

	return 0;
}

static xp_awk_node_t* __parse_progunit (xp_awk_t* awk)
{
	/*
	pattern { action }
	function name (parameter-list) { statement }
	*/
	xp_awk_node_t* node;

	if (MATCH(awk,TOKEN_FUNCTION)) {
		node = __parse_function(awk);
		if (node == XP_NULL) return XP_NULL;
	}
	else if (MATCH(awk,TOKEN_BEGIN)) {
		node = __parse_begin (awk);
		if (node == XP_NULL) return XP_NULL;
	}
	else if (MATCH(awk, TOKEN_END)) {
		node = __parse_end (awk);
		if (node == XP_NULL) return XP_NULL;
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
	else {
		/* pattern-less actions */
		node = __parse_action (awk);
		if (node == XP_NULL) return XP_NULL;

		// TODO: weave the action block into awk->tree.actions...
	}

	return node;
}

static xp_awk_node_t* __parse_function (xp_awk_t* awk)
{
	xp_char_t* name;
	xp_char_t* name_dup;
	xp_awk_node_t* body;
	xp_awk_func_t* func;
	xp_size_t nargs;

	/* eat up the keyword 'function' and get the next token */
	if (__get_token(awk) == -1) return XP_NULL;  

	/* match a function name */
	if (!MATCH(awk,TOKEN_IDENT)) {
		/* cannot find a valid identifier for a function name */
		PANIC (awk, XP_AWK_EIDENT);
	}

	name = XP_STR_BUF(&awk->token.name);
	if (xp_awk_hash_get(&awk->tree.funcs, name) != XP_NULL) {
		/* the function is defined previously */
		PANIC (awk, XP_AWK_EDUPFUNC);
	}

	if (awk->opt.parse & XP_AWK_UNIQUE) {
		/* check if it coincides to be a global variable name */
		if (xp_awk_tab_find(&awk->parse.globals, name, 0) != (xp_size_t)-1) {
			PANIC (awk, XP_AWK_EDUPNAME);
		}
	}

	/* clone the function name before it is overwritten */
	name_dup = xp_strdup (name);
	if (name_dup == XP_NULL) PANIC (awk, XP_AWK_ENOMEM);

	/* get the next token */
	if (__get_token(awk) == -1) {
		xp_free (name_dup);
		return XP_NULL;  
	}

	/* match a left parenthesis */
	if (!MATCH(awk,TOKEN_LPAREN)) {
		/* a function name is not followed by a left parenthesis */
		xp_free (name_dup);
		PANIC (awk, XP_AWK_ELPAREN);
	}	

	/* get the next token */
	if (__get_token(awk) == -1) {
		xp_free (name_dup);
		return XP_NULL;
	}

	/* make sure that parameter table is empty */
	xp_assert (xp_awk_tab_getsize(&awk->parse.params) == 0);

	/* read parameter list */
	if (MATCH(awk,TOKEN_RPAREN)) {
		/* no function parameter found. get the next token */
		if (__get_token(awk) == -1) {
			xp_free (name_dup);
			return XP_NULL;
		}
	}
	else {
		while (1) {
			xp_char_t* param;

			if (!MATCH(awk,TOKEN_IDENT)) {
				xp_free (name_dup);
				xp_awk_tab_clear (&awk->parse.params);
				PANIC (awk, XP_AWK_EIDENT);
			}

			param = XP_STR_BUF(&awk->token.name);

			if (awk->opt.parse & XP_AWK_UNIQUE) {
				/* check if a parameter conflicts with a function */
				if (xp_strcmp(name_dup, param) == 0 ||
				    xp_awk_hash_get(&awk->tree.funcs, param) != XP_NULL) {
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
			if (xp_awk_tab_find(&awk->parse.params, param, 0) != (xp_size_t)-1) {
				xp_free (name_dup);
				xp_awk_tab_clear (&awk->parse.params);
				PANIC (awk, XP_AWK_EDUPPARAM);
			}

			/* push the parameter to the parameter list */
			if (xp_awk_tab_adddatum(&awk->parse.params, param) == (xp_size_t)-1) {
				xp_free (name_dup);
				xp_awk_tab_clear (&awk->parse.params);
				PANIC (awk, XP_AWK_ENOMEM);
			}	

			if (__get_token(awk) == -1) {
				xp_free (name_dup);
				xp_awk_tab_clear (&awk->parse.params);
				return XP_NULL;
			}	

			if (MATCH(awk,TOKEN_RPAREN)) break;

			if (!MATCH(awk,TOKEN_COMMA)) {
				xp_free (name_dup);
				xp_awk_tab_clear (&awk->parse.params);
				PANIC (awk, XP_AWK_ECOMMA);
			}

			if (__get_token(awk) == -1) {
				xp_free (name_dup);
				xp_awk_tab_clear (&awk->parse.params);
				return XP_NULL;
			}
		}

		if (__get_token(awk) == -1) {
			xp_free (name_dup);
			xp_awk_tab_clear (&awk->parse.params);
			return XP_NULL;
		}
	}

	/* check if the function body starts with a left brace */
	if (!MATCH(awk,TOKEN_LBRACE)) {
		xp_free (name_dup);
		xp_awk_tab_clear (&awk->parse.params);
		PANIC (awk, XP_AWK_ELBRACE);
	}
	if (__get_token(awk) == -1) {
		xp_free (name_dup);
		xp_awk_tab_clear (&awk->parse.params);
		return XP_NULL; 
	}

	/* actual function body */
	body = __parse_block (awk, xp_true);
	if (body == XP_NULL) {
		xp_free (name_dup);
		xp_awk_tab_clear (&awk->parse.params);
		return XP_NULL;
	}

// TODO: consider if the parameters should be saved for some reasons..
	nargs = xp_awk_tab_getsize(&awk->parse.params);
	/* parameter names are not required anymore. clear them */
	xp_awk_tab_clear (&awk->parse.params);

	func = (xp_awk_func_t*) xp_malloc (xp_sizeof(xp_awk_func_t));
	if (func == XP_NULL) {
		xp_free (name_dup);
		xp_awk_clrpt (body);
		return XP_NULL;
	}

	func->name  = name_dup;
	func->nargs = nargs;
	func->body  = body;

	xp_assert (xp_awk_hash_get(&awk->tree.funcs, name_dup) == XP_NULL);
	if (xp_awk_hash_put(&awk->tree.funcs, name_dup, func) == XP_NULL) {
		xp_free (name_dup);
		xp_awk_clrpt (body);
		xp_free (func);
		PANIC (awk, XP_AWK_ENOMEM);
	}

	return body;
}

static xp_awk_node_t* __parse_begin (xp_awk_t* awk)
{
	xp_awk_node_t* node;

	if (awk->tree.begin != XP_NULL) PANIC (awk, XP_AWK_EDUPBEGIN);
	if (__get_token(awk) == -1) return XP_NULL; 

	node = __parse_action (awk);
	if (node == XP_NULL) return XP_NULL;

	awk->tree.begin = node;
	return node;
}

static xp_awk_node_t* __parse_end (xp_awk_t* awk)
{
	xp_awk_node_t* node;

	if (awk->tree.end != XP_NULL) PANIC (awk, XP_AWK_EDUPEND);
	if (__get_token(awk) == -1) return XP_NULL; 

	node = __parse_action (awk);
	if (node == XP_NULL) return XP_NULL;

	awk->tree.end = node;
	return node;
}

static xp_awk_node_t* __parse_action (xp_awk_t* awk)
{
	if (!MATCH(awk,TOKEN_LBRACE)) PANIC (awk, XP_AWK_ELBRACE);
	if (__get_token(awk) == -1) return XP_NULL; 
	return __parse_block(awk, xp_true);
}

static xp_awk_node_t* __parse_block (xp_awk_t* awk, xp_bool_t is_top) 
{
	xp_awk_node_t* head, * curr, * node;
	xp_awk_node_block_t* block;
	xp_size_t nlocals, nlocals_max, tmp;

	nlocals = xp_awk_tab_getsize(&awk->parse.locals);
	nlocals_max = awk->parse.nlocals_max;

	/* local variable declarations */
	if (awk->opt.parse & XP_AWK_EXPLICIT) {
		while (1) {
			if (!MATCH(awk,TOKEN_LOCAL)) break;

			if (__get_token(awk) == -1) {
				xp_awk_tab_remrange (
					&awk->parse.locals, nlocals, 
					xp_awk_tab_getsize(&awk->parse.locals) - nlocals);
				return XP_NULL;
			}

			if (__collect_locals(awk, nlocals) == XP_NULL) {
				xp_awk_tab_remrange (
					&awk->parse.locals, nlocals, 
					xp_awk_tab_getsize(&awk->parse.locals) - nlocals);
				return XP_NULL;
			}
		}
	}

	/* block body */
	head = XP_NULL; curr = XP_NULL;

	while (1) {
		if (MATCH(awk,TOKEN_EOF)) {
			xp_awk_tab_remrange (
				&awk->parse.locals, nlocals, 
				xp_awk_tab_getsize(&awk->parse.locals) - nlocals);
			if (head != XP_NULL) xp_awk_clrpt (head);
			PANIC (awk, XP_AWK_EENDSRC);
		}

		if (MATCH(awk,TOKEN_RBRACE)) {
			if (__get_token(awk) == -1) {
				xp_awk_tab_remrange (
					&awk->parse.locals, nlocals, 
					xp_awk_tab_getsize(&awk->parse.locals) - nlocals);
				if (head != XP_NULL) xp_awk_clrpt (head);
				return XP_NULL; 
			}
			break;
		}

		node = __parse_statement (awk);
		if (node == XP_NULL) {
			xp_awk_tab_remrange (
				&awk->parse.locals, nlocals, 
				xp_awk_tab_getsize(&awk->parse.locals) - nlocals);
			if (head != XP_NULL) xp_awk_clrpt (head);
			return XP_NULL;
		}

		/* remove unnecessary statements */
		if (node->type == XP_AWK_NODE_NULL ||
		    (node->type == XP_AWK_NODE_BLOCK && 
		     ((xp_awk_node_block_t*)node)->body == XP_NULL)) continue;
			
		if (curr == XP_NULL) head = node;
		else curr->next = node;	
		curr = node;
	}

	block = (xp_awk_node_block_t*) xp_malloc (xp_sizeof(xp_awk_node_block_t));
	if (block == XP_NULL) {
		xp_awk_tab_remrange (
			&awk->parse.locals, nlocals, 
			xp_awk_tab_getsize(&awk->parse.locals) - nlocals);
		xp_awk_clrpt (head);
		PANIC (awk, XP_AWK_ENOMEM);
	}

	tmp = xp_awk_tab_getsize(&awk->parse.locals);
	if (tmp > awk->parse.nlocals_max) awk->parse.nlocals_max = tmp;

	xp_awk_tab_remrange (
		&awk->parse.locals, nlocals, tmp - nlocals);

	/* adjust number of locals for a block without any statements */
	if (head == NULL) tmp = 0;

	block->type = XP_AWK_NODE_BLOCK;
	block->next = XP_NULL;
	block->body = head;

	/* migrate all block-local variables to a top-level block */
	if (is_top) {
		block->nlocals = awk->parse.nlocals_max - nlocals;
		awk->parse.nlocals_max = nlocals_max;
	}
	else {
		/*block->nlocals = tmp - nlocals;*/
		block->nlocals = 0;
	}

	return (xp_awk_node_t*)block;
}

static xp_awk_t* __collect_locals (xp_awk_t* awk, xp_size_t nlocals)
{
	xp_char_t* local;

	while (1) {
		if (!MATCH(awk,TOKEN_IDENT)) {
			PANIC (awk, XP_AWK_EIDENT);
		}

		local = XP_STR_BUF(&awk->token.name);

		/* NOTE: it is not checked againt globals names */

		if (awk->opt.parse & XP_AWK_UNIQUE) {
			/* check if it conflict with a function name */
			if (xp_awk_hash_get(&awk->tree.funcs, local) != XP_NULL) {
				PANIC (awk, XP_AWK_EDUPNAME);
			}
		}

		/* check if it conflicts with a paremeter name */
		if (xp_awk_tab_find(&awk->parse.params, local, 0) != (xp_size_t)-1) {
			PANIC (awk, XP_AWK_EDUPNAME);
		}

		/* check if it conflicts with other local variable names */
		if (xp_awk_tab_find(&awk->parse.locals, local, 
			((awk->opt.parse & XP_AWK_SHADING)? nlocals: 0)) != (xp_size_t)-1)  {
			PANIC (awk, XP_AWK_EDUPVAR);	
		}

		if (xp_awk_tab_adddatum(&awk->parse.locals, local) == (xp_size_t)-1) {
			PANIC (awk, XP_AWK_ENOMEM);
		}

		if (__get_token(awk) == -1) return XP_NULL;

		if (MATCH(awk,TOKEN_SEMICOLON)) break;

		if (!MATCH(awk,TOKEN_COMMA)) {
			PANIC (awk, XP_AWK_ECOMMA);
		}

		if (__get_token(awk) == -1) return XP_NULL;
	}

	/* skip a semicolon */
	if (__get_token(awk) == -1) return XP_NULL;

	return awk;
}

static xp_awk_node_t* __parse_statement (xp_awk_t* awk)
{
	xp_awk_node_t* node;

	if (MATCH(awk,TOKEN_SEMICOLON)) {
		/* null statement */	
		node = (xp_awk_node_t*) xp_malloc (xp_sizeof(xp_awk_node_t));
		if (node == XP_NULL) PANIC (awk, XP_AWK_ENOMEM);

		node->type = XP_AWK_NODE_NULL;
		node->next = XP_NULL;

		if (__get_token(awk) == -1) {
			xp_free (node);
			return XP_NULL;
		}
	}
	else if (MATCH(awk,TOKEN_LBRACE)) {
		if (__get_token(awk) == -1) return XP_NULL; 
		node = __parse_block (awk, xp_false);
	}
	else node = __parse_statement_nb (awk);

	return node;
}

static xp_awk_node_t* __parse_statement_nb (xp_awk_t* awk)
{
	xp_awk_node_t* node;

	/* 
	 * keywords that don't require any terminating semicolon 
	 */
	if (MATCH(awk,TOKEN_IF)) {
		if (__get_token(awk) == -1) return XP_NULL;
		return __parse_if (awk);
	}
	else if (MATCH(awk,TOKEN_WHILE)) {
		if (__get_token(awk) == -1) return XP_NULL;
		return __parse_while (awk);
	}
	else if (MATCH(awk,TOKEN_FOR)) {
		if (__get_token(awk) == -1) return XP_NULL;
		return __parse_for (awk);
	}

	/* 
	 * keywords that require a terminating semicolon 
	 */
	if (MATCH(awk,TOKEN_DO)) {
		if (__get_token(awk) == -1) return XP_NULL;
		node = __parse_dowhile (awk);
	}
	else if (MATCH(awk,TOKEN_BREAK)) {
		if (__get_token(awk) == -1) return XP_NULL;
		node = __parse_break(awk);
	}
	else if (MATCH(awk,TOKEN_CONTINUE)) {
		if (__get_token(awk) == -1) return XP_NULL;
		node = __parse_continue(awk);
	}
	else if (MATCH(awk,TOKEN_RETURN)) {
		if (__get_token(awk) == -1) return XP_NULL;
		node = __parse_return(awk);
	}
	else if (MATCH(awk,TOKEN_EXIT)) {
		if (__get_token(awk) == -1) return XP_NULL;
		node = __parse_exit(awk);
	}

/* TODO:
	else if (MATCH(awk,TOKEN_DELETE)) {
		if (__get_token(awk) == -1) return XP_NULL;
		node = __parse_delete(awk);
	}
*/
	else if (MATCH(awk,TOKEN_NEXT)) {
		if (__get_token(awk) == -1) return XP_NULL;
		node = __parse_next(awk);
	}
	else if (MATCH(awk,TOKEN_NEXTFILE)) {
		if (__get_token(awk) == -1) return XP_NULL;
		node = __parse_nextfile(awk);
	}
	else {
		node = __parse_expression(awk);
	}

	if (node == XP_NULL) return XP_NULL;

	/* check if a statement ends with a semicolon */
	if (!MATCH(awk,TOKEN_SEMICOLON)) {
		if (node != XP_NULL) xp_awk_clrpt (node);
		PANIC (awk, XP_AWK_ESEMICOLON);
	}

	/* eat up the semicolon and read in the next token */
	if (__get_token(awk) == -1) {
		if (node != XP_NULL) xp_awk_clrpt (node);
		return XP_NULL;
	}

	return node;
}

static xp_awk_node_t* __parse_expression (xp_awk_t* awk)
{
	/*
	 * <expression> ::= <assignment> | <basic expression>
	 * <assignment> ::= <assignment target> assignmentOperator <basic expression>
	 * assignmentOperator ::= '='
	 * <basic expression> ::= 
	 */

	xp_awk_node_t* x, * y;
	xp_awk_node_assign_t* node;

	x = __parse_basic_expr (awk);
	if (x == XP_NULL) return XP_NULL;
	if (!MATCH(awk,TOKEN_ASSIGN)) return x;

	xp_assert (x->next == XP_NULL);
	if (x->type != XP_AWK_NODE_ARG &&
	    x->type != XP_AWK_NODE_ARGIDX &&
	    x->type != XP_AWK_NODE_VAR && 
	    x->type != XP_AWK_NODE_VARIDX &&
	    x->type != XP_AWK_NODE_POS) {
		xp_awk_clrpt (x);
		PANIC (awk, XP_AWK_EASSIGN);
	}

	if (__get_token(awk) == -1) {
		xp_awk_clrpt (x);
		return XP_NULL;
	}

	y = __parse_basic_expr (awk);
	if (y == XP_NULL) {
		xp_awk_clrpt (x);
		return XP_NULL;
	}

	node = (xp_awk_node_assign_t*)xp_malloc (xp_sizeof(xp_awk_node_assign_t));
	if (node == XP_NULL) {
		xp_awk_clrpt (x);
		xp_awk_clrpt (y);
		PANIC (awk, XP_AWK_ENOMEM);
	}

	node->type = XP_AWK_NODE_ASSIGN;
	node->next = XP_NULL;
	node->left = x;
	node->right = y;

	return (xp_awk_node_t*)node;
}

static xp_awk_node_t* __parse_basic_expr (xp_awk_t* awk)
{
	/* <basic expression> ::= <additive> 
	 * <additive> ::= <multiplicative> [additiveOp <multiplicative>]*
	 * <multiplicative> ::= <unary> [multiplicativeOp <unary>]*
	 * <unary> ::= [unaryOp]* <term>
	 * <term> ::= <function call> | variable | literals
	 * <function call> ::= <identifier> <lparen> <basic expression list>* <rparen>
	 * <basic expression list> ::= <basic expression> [comma <basic expression>]*
	 */
	
	return __parse_additive (awk);
}


static xp_awk_node_t* __parse_additive (xp_awk_t* awk)
{
	xp_awk_node_expr_t* node;
	xp_awk_node_t* left, * right;
	int opcode;

	left = __parse_multiplicative (awk);
	if (left == XP_NULL) return XP_NULL;
	
	while (1) {
		if (MATCH(awk,TOKEN_PLUS)) opcode = BINOP_PLUS;
		else if (MATCH(awk,TOKEN_MINUS)) opcode = BINOP_MINUS;
		else break;

		if (__get_token(awk) == -1) {
			xp_awk_clrpt (left);
			return XP_NULL; 
		}

		right = __parse_multiplicative (awk);
		if (right == XP_NULL) {
			xp_awk_clrpt (left);
			return XP_NULL;
		}

		// TODO: constant folding -> in other parts of the program also...

		node = (xp_awk_node_expr_t*)xp_malloc(xp_sizeof(xp_awk_node_expr_t));
		if (node == XP_NULL) {
			xp_awk_clrpt (right);
			xp_awk_clrpt (left);
			PANIC (awk, XP_AWK_ENOMEM);
		}

		node->type = XP_AWK_NODE_BINARY;
		node->next = XP_NULL;
		node->opcode = opcode; 
		node->left = left;
		node->right = right;

		left = (xp_awk_node_t*)node;
	}

	return left;
}

static xp_awk_node_t* __parse_multiplicative (xp_awk_t* awk)
{
	xp_awk_node_expr_t* node;
	xp_awk_node_t* left, * right;
	int opcode;

	left = __parse_unary (awk);
	if (left == XP_NULL) return XP_NULL;
	
	while (1) {
		if (MATCH(awk,TOKEN_MUL)) opcode = BINOP_MUL;
		else if (MATCH(awk,TOKEN_DIV)) opcode = BINOP_DIV;
		else if (MATCH(awk,TOKEN_MOD)) opcode = BINOP_MOD;
		else break;

		if (__get_token(awk) == -1) {
			xp_awk_clrpt (left);
			return XP_NULL; 
		}

		right = __parse_unary (awk);
		if (right == XP_NULL) {
			xp_awk_clrpt (left);	
			return XP_NULL;
		}

		/* TODO: enhance constant folding. do it in a better way */
		/* TODO: differentiate different types of numbers ... */
		if (left->type == XP_AWK_NODE_NUM && 
		    right->type == XP_AWK_NODE_NUM) {
			xp_long_t l, r;
			xp_awk_node_term_t* tmp;
			xp_char_t buf[256];

			l = __str_to_long (((xp_awk_node_term_t*)left)->value); 
			r = __str_to_long (((xp_awk_node_term_t*)right)->value); 

			xp_awk_clrpt (left);
			xp_awk_clrpt (right);
			
			if (opcode == BINOP_MUL) l *= r;
			else if (opcode == BINOP_DIV) l /= r;
			else if (opcode == BINOP_MOD) l %= r;
			
			xp_sprintf (buf, xp_countof(buf), XP_TEXT("%lld"), (long long)l);

			tmp = (xp_awk_node_term_t*) xp_malloc (xp_sizeof(xp_awk_node_term_t));
			if (tmp == XP_NULL) PANIC (awk, XP_AWK_ENOMEM);

			tmp->type = XP_AWK_NODE_NUM;
			tmp->next = XP_NULL;
			tmp->value = xp_strdup (buf);

			if (tmp->value == XP_NULL) {
				xp_free (tmp);
				PANIC (awk, XP_AWK_ENOMEM);
			}

			left = (xp_awk_node_t*) tmp;
			continue;
		} 

		node = (xp_awk_node_expr_t*)xp_malloc(xp_sizeof(xp_awk_node_expr_t));
		if (node == XP_NULL) {
			xp_awk_clrpt (right);	
			xp_awk_clrpt (left);	
			PANIC (awk, XP_AWK_ENOMEM);
		}

		node->type = XP_AWK_NODE_BINARY;
		node->next = XP_NULL;
		node->opcode = opcode;
		node->left = left;
		node->right = right;

		left = (xp_awk_node_t*)node;
	}

	return left;
}

static xp_awk_node_t* __parse_unary (xp_awk_t* awk)
{
	return __parse_primary (awk);
}

static xp_awk_node_t* __parse_primary (xp_awk_t* awk)
{
	if (MATCH(awk,TOKEN_IDENT))  {
		xp_char_t* name_dup;

		name_dup = (xp_char_t*)xp_strdup(XP_STR_BUF(&awk->token.name));
		if (name_dup == XP_NULL) PANIC (awk, XP_AWK_ENOMEM);

		if (__get_token(awk) == -1) {
			xp_free (name_dup);	
			return XP_NULL;			
		}

		if (MATCH(awk,TOKEN_LBRACK)) {
			xp_awk_node_t* node;
			node = __parse_hashidx (awk, name_dup);
			if (node == XP_NULL) xp_free (name_dup);
			return (xp_awk_node_t*)node;
		}
		else if (MATCH(awk,TOKEN_LPAREN)) {
			/* function call */
			xp_awk_node_t* node;
			node = __parse_funcall (awk, name_dup);
			if (node == XP_NULL) xp_free (name_dup);
			return (xp_awk_node_t*)node;
		}	
		else {
			/* normal variable */
			xp_awk_node_var_t* node;
			xp_size_t idxa;
	
			node = (xp_awk_node_var_t*)xp_malloc(xp_sizeof(xp_awk_node_var_t));
			if (node == XP_NULL) {
				xp_free (name_dup);
				PANIC (awk, XP_AWK_ENOMEM);
			}

			/* search the parameter name list */
			idxa = xp_awk_tab_find(&awk->parse.params, name_dup, 0);
			if (idxa != (xp_size_t)-1) {
				node->type = XP_AWK_NODE_ARG;
				node->next = XP_NULL;
				//node->id.name = XP_NULL;
				node->id.name = name_dup;
				node->id.idxa = idxa;

				return (xp_awk_node_t*)node;
			}

			/* search the local variable list */
			idxa = xp_awk_tab_rrfind(&awk->parse.locals, name_dup, 0);
			if (idxa != (xp_size_t)-1) {
				node->type = XP_AWK_NODE_VAR;
				node->next = XP_NULL;
				//node->id.name = XP_NULL;
				node->id.name = name_dup;
				node->id.idxa = idxa;

				return (xp_awk_node_t*)node;
			}

			/* TODO: search the global variable list... */
			/* search the global variable list */

			if (awk->opt.parse & XP_AWK_IMPLICIT) {
				node->type = XP_AWK_NODE_VAR;
				node->next = XP_NULL;
				node->id.name = name_dup;
				node->id.idxa = (xp_size_t)-1;
				return (xp_awk_node_t*)node;
			}

			/* undefined variable */
			xp_free (name_dup);
			xp_free (node);
			PANIC (awk, XP_AWK_EUNDEF);
		}
	}
	else if (MATCH(awk,TOKEN_INTEGER)) {
		xp_awk_node_term_t* node;

		node = (xp_awk_node_term_t*)xp_malloc(xp_sizeof(xp_awk_node_term_t));
		if (node == XP_NULL) PANIC (awk, XP_AWK_ENOMEM);

		node->type = XP_AWK_NODE_NUM;
		node->next = XP_NULL;
		node->value = xp_strdup(XP_STR_BUF(&awk->token.name));
		if (node->value == XP_NULL) {
			xp_free (node);
			PANIC (awk, XP_AWK_ENOMEM);
		}

		if (__get_token(awk) == -1) {
			xp_free (node->value);
			xp_free (node);
			return XP_NULL;			
		}

		return (xp_awk_node_t*)node;
	}
	else if (MATCH(awk,TOKEN_STRING))  {
		xp_awk_node_term_t* node;

		node = (xp_awk_node_term_t*)xp_malloc(xp_sizeof(xp_awk_node_term_t));
		if (node == XP_NULL) PANIC (awk, XP_AWK_ENOMEM);

		node->type = XP_AWK_NODE_STR;
		node->next = XP_NULL;
		node->value = xp_strdup(XP_STR_BUF(&awk->token.name));
		if (node->value == XP_NULL) {
			xp_free (node);
			PANIC (awk, XP_AWK_ENOMEM);
		}

		if (__get_token(awk) == -1) {
			xp_free (node->value);
			xp_free (node);
			return XP_NULL;			
		}

		return (xp_awk_node_t*)node;
	}
	else if (MATCH(awk,TOKEN_DOLLAR)) {
		xp_awk_node_sgv_t* node;
		xp_awk_node_t* prim;

		if (__get_token(awk)) return XP_NULL;
		
		prim = __parse_primary (awk);
		if (prim == XP_NULL) return XP_NULL;

		node = (xp_awk_node_sgv_t*) xp_malloc (xp_sizeof(xp_awk_node_sgv_t));
		if (node == XP_NULL) {
			xp_awk_clrpt (prim);
			PANIC (awk, XP_AWK_ENOMEM);
		}

		node->type = XP_AWK_NODE_POS;
		node->next = XP_NULL;
		node->value = prim;

		return (xp_awk_node_t*)node;
	}
	else if (MATCH(awk,TOKEN_LPAREN)) {
		xp_awk_node_t* node;

		/* eat up the left parenthesis */
		if (__get_token(awk) == -1) return XP_NULL;

		/* parse the sub-expression inside the parentheses */
		node = __parse_expression (awk);
		if (node == XP_NULL) return XP_NULL;

		/* check for the closing parenthesis */
		if (!MATCH(awk,TOKEN_RPAREN)) {
			xp_awk_clrpt (node);
			PANIC (awk, XP_AWK_ERPAREN);
		}

		if (__get_token(awk) == -1) {
			xp_awk_clrpt (node);
			return XP_NULL;
		}

		return node;
	}

	/* valid expression introducer is expected */
	PANIC (awk, XP_AWK_EEXPR);
}

static xp_awk_node_t* __parse_hashidx (xp_awk_t* awk, xp_char_t* name)
{
	xp_awk_node_t* idx;
	xp_awk_node_idx_t* node;
	xp_size_t idxa;

	if (__get_token(awk) == -1) return XP_NULL;

	idx = __parse_expression (awk);
	if (idx == XP_NULL) return XP_NULL;

	if (!MATCH(awk,TOKEN_RBRACK)) {
		xp_awk_clrpt (idx);
		PANIC (awk, XP_AWK_ERBRACK);
	}

	if (__get_token(awk) == -1) {
		xp_awk_clrpt (idx);
		return XP_NULL;
	}

	node = (xp_awk_node_idx_t*) xp_malloc (xp_sizeof(xp_awk_node_idx_t));
	if (node == XP_NULL) {
		xp_awk_clrpt (idx);
		PANIC (awk, XP_AWK_ENOMEM);
	}

	/* search the parameter name list */
	idxa = xp_awk_tab_find(&awk->parse.params, name, 0);
	if (idxa != (xp_size_t)-1) {
		node->type = XP_AWK_NODE_ARGIDX;
		node->next = XP_NULL;
		//node->id.name = XP_NULL;
		node->id.name = name;
		node->id.idxa = idxa;
		node->idx = idx;

		return (xp_awk_node_t*)node;
	}

	/* search the local variable list */
	idxa = xp_awk_tab_rrfind(&awk->parse.locals, name, 0);
	if (idxa != (xp_size_t)-1) {
		node->type = XP_AWK_NODE_VARIDX;
		node->next = XP_NULL;
		//node->id.name = XP_NULL;
		node->id.name = name;
		node->id.idxa = idxa;
		node->idx = idx;

		return (xp_awk_node_t*)node;
	}

	/* TODO: search the global variable list... */
	/* search the global variable list */

	if (awk->opt.parse & XP_AWK_IMPLICIT) {
		node->type = XP_AWK_NODE_VAR;
		node->next = XP_NULL;
		node->id.name = name;
		node->id.idxa = (xp_size_t)-1;
		return (xp_awk_node_t*)node;
	}

	/* undefined variable */
	xp_awk_clrpt (idx);
	xp_free (node);
	PANIC (awk, XP_AWK_EUNDEF);
}

static xp_awk_node_t* __parse_funcall (xp_awk_t* awk, xp_char_t* name)
{
	xp_awk_node_t* head, * curr, * node;
	xp_awk_node_call_t* call;

	if (__get_token(awk) == -1) return XP_NULL;
	
	head = curr = XP_NULL;

	if (MATCH(awk,TOKEN_RPAREN)) {
		/* no parameters to the function call */
		if (__get_token(awk) == -1) return XP_NULL;
	}
	else {
		while (1) {
			node = __parse_expression (awk);
			if (node == XP_NULL) {
				if (head != XP_NULL) xp_awk_clrpt (head);
				return XP_NULL;
			}
	
			if (head == XP_NULL) head = node;
			else curr->next = node;
			curr = node;

			if (MATCH(awk,TOKEN_RPAREN)) {
				if (__get_token(awk) == -1) {
					if (head != XP_NULL) xp_awk_clrpt (head);
					return XP_NULL;
				}
				break;
			}

			if (!MATCH(awk,TOKEN_COMMA)) {
				if (head != XP_NULL) xp_awk_clrpt (head);
				PANIC (awk, XP_AWK_ECOMMA);	
			}

			if (__get_token(awk) == -1) {
				if (head != XP_NULL) xp_awk_clrpt (head);
				return XP_NULL;
			}
		}

	}

	call = (xp_awk_node_call_t*)xp_malloc (xp_sizeof(xp_awk_node_call_t));
	if (call == XP_NULL) {
		if (head != XP_NULL) xp_awk_clrpt (head);
		PANIC (awk, XP_AWK_ENOMEM);
	}

	call->type = XP_AWK_NODE_CALL;
	call->next = XP_NULL;
	call->name = name;
	call->args = head;

	return (xp_awk_node_t*)call;
}

static xp_awk_node_t* __parse_if (xp_awk_t* awk)
{
	xp_awk_node_t* test;
	xp_awk_node_t* then_part;
	xp_awk_node_t* else_part;
	xp_awk_node_if_t* node;

	if (!MATCH(awk,TOKEN_LPAREN)) PANIC (awk, XP_AWK_ELPAREN);
	if (__get_token(awk) == -1) return XP_NULL;

	test = __parse_expression (awk);
	if (test == XP_NULL) return XP_NULL;

	if (!MATCH(awk,TOKEN_RPAREN)) {
		xp_awk_clrpt (test);
		PANIC (awk, XP_AWK_ERPAREN);
	}

	if (__get_token(awk) == -1) {
		xp_awk_clrpt (test);
		return XP_NULL;
	}

	then_part = __parse_statement (awk);
	if (then_part == XP_NULL) {
		xp_awk_clrpt (test);
		return XP_NULL;
	}

	if (MATCH(awk,TOKEN_ELSE)) {
		if (__get_token(awk) == -1) {
			xp_awk_clrpt (then_part);
			xp_awk_clrpt (test);
			return XP_NULL;
		}

		else_part = __parse_statement (awk);
		if (else_part == XP_NULL) {
			xp_awk_clrpt (then_part);
			xp_awk_clrpt (test);
			return XP_NULL;
		}
	}
	else else_part = XP_NULL;

	node = (xp_awk_node_if_t*) xp_malloc (xp_sizeof(xp_awk_node_if_t));
	if (node == XP_NULL) {
		xp_awk_clrpt (else_part);
		xp_awk_clrpt (then_part);
		xp_awk_clrpt (test);
		PANIC (awk, XP_AWK_ENOMEM);
	}

	node->type = XP_AWK_NODE_IF;
	node->next = XP_NULL;
	node->test = test;
	node->then_part = then_part;
	node->else_part = else_part;

	return (xp_awk_node_t*)node;
}

static xp_awk_node_t* __parse_while (xp_awk_t* awk)
{
	xp_awk_node_t* test, * body;
	xp_awk_node_while_t* node;

	if (!MATCH(awk,TOKEN_LPAREN)) PANIC (awk, XP_AWK_ELPAREN);
	if (__get_token(awk) == -1) return XP_NULL;

	test = __parse_expression (awk);
	if (test == XP_NULL) return XP_NULL;

	if (!MATCH(awk,TOKEN_RPAREN)) {
		xp_awk_clrpt (test);
		PANIC (awk, XP_AWK_ERPAREN);
	}

	if (__get_token(awk) == -1) {
		xp_awk_clrpt (test);
		return XP_NULL;
	}

	body = __parse_statement (awk);
	if (body == XP_NULL) {
		xp_awk_clrpt (test);
		return XP_NULL;
	}

	node = (xp_awk_node_while_t*) xp_malloc (xp_sizeof(xp_awk_node_while_t));
	if (node == XP_NULL) {
		xp_awk_clrpt (body);
		xp_awk_clrpt (test);
		PANIC (awk, XP_AWK_ENOMEM);
	}

	node->type = XP_AWK_NODE_WHILE;
	node->next = XP_NULL;
	node->test = test;
	node->body = body;

	return (xp_awk_node_t*)node;
}

static xp_awk_node_t* __parse_for (xp_awk_t* awk)
{
	xp_awk_node_t* init, * test, * incr, * body;
	xp_awk_node_for_t* node;

	// TODO: parse for (x in list) ...

	if (!MATCH(awk,TOKEN_LPAREN)) PANIC (awk, XP_AWK_ELPAREN);
	if (__get_token(awk) == -1) return XP_NULL;
		
	if (MATCH(awk,TOKEN_SEMICOLON)) init = XP_NULL;
	else {
		init = __parse_expression (awk);
		if (init == XP_NULL) return XP_NULL;

		if (!MATCH(awk,TOKEN_SEMICOLON)) {
			xp_awk_clrpt (init);
			PANIC (awk, XP_AWK_ESEMICOLON);
		}
	}

	if (__get_token(awk) == -1) {
		xp_awk_clrpt (init);
		return XP_NULL;
	}

	if (MATCH(awk,TOKEN_SEMICOLON)) test = XP_NULL;
	else {
		test = __parse_expression (awk);
		if (test == XP_NULL) {
			xp_awk_clrpt (init);
			return XP_NULL;
		}

		if (!MATCH(awk,TOKEN_SEMICOLON)) {
			xp_awk_clrpt (init);
			xp_awk_clrpt (test);
			PANIC (awk, XP_AWK_ESEMICOLON);
		}
	}

	if (__get_token(awk) == -1) {
		xp_awk_clrpt (init);
		xp_awk_clrpt (test);
		return XP_NULL;
	}
	
	if (MATCH(awk,TOKEN_RPAREN)) incr = XP_NULL;
	else {
		incr = __parse_expression (awk);
		if (incr == XP_NULL) {
			xp_awk_clrpt (init);
			xp_awk_clrpt (test);
			return XP_NULL;
		}

		if (!MATCH(awk,TOKEN_RPAREN)) {
			xp_awk_clrpt (init);
			xp_awk_clrpt (test);
			xp_awk_clrpt (incr);
			PANIC (awk, XP_AWK_ERPAREN);
		}
	}

	if (__get_token(awk) == -1) {
		xp_awk_clrpt (init);
		xp_awk_clrpt (test);
		xp_awk_clrpt (incr);
		return XP_NULL;
	}

	body = __parse_statement (awk);
	if (body == XP_NULL) {
		xp_awk_clrpt (init);
		xp_awk_clrpt (test);
		xp_awk_clrpt (incr);
		return XP_NULL;
	}

	node = (xp_awk_node_for_t*) xp_malloc (xp_sizeof(xp_awk_node_for_t));
	if (node == XP_NULL) {
		xp_awk_clrpt (init);
		xp_awk_clrpt (test);
		xp_awk_clrpt (incr);
		xp_awk_clrpt (body);
		PANIC (awk, XP_AWK_ENOMEM);
	}

	node->type = XP_AWK_NODE_FOR;
	node->next = XP_NULL;
	node->init = init;
	node->test = test;
	node->incr = incr;
	node->body = body;

	return (xp_awk_node_t*)node;
}

static xp_awk_node_t* __parse_dowhile (xp_awk_t* awk)
{
	xp_awk_node_t* test, * body;
	xp_awk_node_while_t* node;

	body = __parse_statement (awk);
	if (body == XP_NULL) return XP_NULL;

	if (!MATCH(awk,TOKEN_WHILE)) {
		xp_awk_clrpt (body);
		PANIC (awk, XP_AWK_EWHILE);
	}

	if (__get_token(awk) == -1) {
		xp_awk_clrpt (body);
		return XP_NULL;
	}

	if (!MATCH(awk,TOKEN_LPAREN)) {
		xp_awk_clrpt (body);
		PANIC (awk, XP_AWK_ELPAREN);
	}

	if (__get_token(awk) == -1) {
		xp_awk_clrpt (body);
		return XP_NULL;
	}

	test = __parse_expression (awk);
	if (test == XP_NULL) {
		xp_awk_clrpt (body);
		return XP_NULL;
	}

	if (!MATCH(awk,TOKEN_RPAREN)) {
		xp_awk_clrpt (body);
		xp_awk_clrpt (test);
		PANIC (awk, XP_AWK_ERPAREN);
	}

	if (__get_token(awk) == -1) {
		xp_awk_clrpt (body);
		xp_awk_clrpt (test);
		return XP_NULL;
	}
	
	node = (xp_awk_node_while_t*) xp_malloc (xp_sizeof(xp_awk_node_while_t));
	if (node == XP_NULL) {
		xp_awk_clrpt (body);
		xp_awk_clrpt (test);
		PANIC (awk, XP_AWK_ENOMEM);
	}

	node->type = XP_AWK_NODE_DOWHILE;
	node->next = XP_NULL;
	node->test = test;
	node->body = body;

	return (xp_awk_node_t*)node;
}

static xp_awk_node_t* __parse_break (xp_awk_t* awk)
{
	xp_awk_node_t* node;

	node = (xp_awk_node_t*) xp_malloc (xp_sizeof(xp_awk_node_t));
	if (node == XP_NULL) PANIC (awk, XP_AWK_ENOMEM);
	node->type = XP_AWK_NODE_BREAK;
	node->next = XP_NULL;
	
	return node;
}

static xp_awk_node_t* __parse_continue (xp_awk_t* awk)
{
	xp_awk_node_t* node;

	node = (xp_awk_node_t*) xp_malloc (xp_sizeof(xp_awk_node_t));
	if (node == XP_NULL) PANIC (awk, XP_AWK_ENOMEM);
	node->type = XP_AWK_NODE_CONTINUE;
	node->next = XP_NULL;
	
	return node;
}

static xp_awk_node_t* __parse_return (xp_awk_t* awk)
{
	xp_awk_node_sgv_t* node;
	xp_awk_node_t* val;

	node = (xp_awk_node_sgv_t*) xp_malloc (xp_sizeof(xp_awk_node_sgv_t));
	if (node == XP_NULL) PANIC (awk, XP_AWK_ENOMEM);
	node->type = XP_AWK_NODE_RETURN;
	node->next = XP_NULL;

	if (MATCH(awk,TOKEN_SEMICOLON)) {
		/* no return value */
		val = XP_NULL;
	}
	else {

		val = __parse_expression (awk);
		if (val == XP_NULL) {
			xp_free (node);
			return XP_NULL;
		}
	}

	node->value = val;
	return (xp_awk_node_t*)node;
}

static xp_awk_node_t* __parse_exit (xp_awk_t* awk)
{
	xp_awk_node_sgv_t* node;
	xp_awk_node_t* val;

	node = (xp_awk_node_sgv_t*) xp_malloc (xp_sizeof(xp_awk_node_sgv_t));
	if (node == XP_NULL) PANIC (awk, XP_AWK_ENOMEM);
	node->type = XP_AWK_NODE_EXIT;
	node->next = XP_NULL;

	if (MATCH(awk,TOKEN_SEMICOLON)) {
		/* no exit code */
		val = XP_NULL;
	}
	else {
		val = __parse_expression (awk);
		if (val == XP_NULL) {
			xp_free (node);
			return XP_NULL;
		}
	}

	node->value = val;
	return (xp_awk_node_t*)node;
}

static xp_awk_node_t* __parse_next (xp_awk_t* awk)
{
	xp_awk_node_t* node;

	node = (xp_awk_node_t*) xp_malloc (xp_sizeof(xp_awk_node_t));
	if (node == XP_NULL) PANIC (awk, XP_AWK_ENOMEM);
	node->type = XP_AWK_NODE_NEXT;
	node->next = XP_NULL;
	
	return node;
}

static xp_awk_node_t* __parse_nextfile (xp_awk_t* awk)
{
	xp_awk_node_t* node;

	node = (xp_awk_node_t*) xp_malloc (xp_sizeof(xp_awk_node_t));
	if (node == XP_NULL) PANIC (awk, XP_AWK_ENOMEM);
	node->type = XP_AWK_NODE_NEXTFILE;
	node->next = XP_NULL;
	
	return node;
}

static int __get_token (xp_awk_t* awk)
{
	xp_cint_t c;
	int n;

	do {
		if (__skip_spaces(awk) == -1) return -1;
		if ((n = __skip_comment(awk)) == -1) return -1;
	} while (n == 1);

	xp_str_clear (&awk->token.name);
	c = awk->lex.curc;

	if (c == XP_CHAR_EOF) {
		SET_TOKEN_TYPE (awk, TOKEN_EOF);
	}	
	else if (xp_isdigit(c)) {
		/* number */
		do {
			ADD_TOKEN_CHAR (awk, c);
			GET_CHAR_TO (awk, c);
		} while (xp_isdigit(c));

		SET_TOKEN_TYPE (awk, TOKEN_INTEGER);
// TODO: enhance nubmer handling
	}
	else if (xp_isalpha(c) || c == XP_CHAR('_')) {
		/* identifier */
		do {
			ADD_TOKEN_CHAR (awk, c);
			GET_CHAR_TO (awk, c);
		} while (xp_isalpha(c) || c == XP_CHAR('_') || xp_isdigit(c));

		SET_TOKEN_TYPE (awk, __classify_ident(awk, XP_STR_BUF(&awk->token.name)));
	}
	else if (c == XP_CHAR('\"')) {
		/* string */
		GET_CHAR_TO (awk, c);
		do {
			ADD_TOKEN_CHAR (awk, c);
			GET_CHAR_TO (awk, c);
		} while (c != XP_CHAR('\"'));

		SET_TOKEN_TYPE (awk, TOKEN_STRING);
		GET_CHAR_TO (awk, c); 
// TODO: enhance string handling including escaping
	}
	else if (c == XP_CHAR('=')) {
		GET_CHAR_TO (awk, c);
		if (c == XP_CHAR('=')) {
			SET_TOKEN_TYPE (awk, TOKEN_EQ);
			ADD_TOKEN_STR (awk, XP_TEXT("=="));
			GET_CHAR_TO (awk, c);
		}
		else {
			SET_TOKEN_TYPE (awk, TOKEN_ASSIGN);
			ADD_TOKEN_STR (awk, XP_TEXT("="));
		}
	}
	else if (c == XP_CHAR('!')) {
		GET_CHAR_TO (awk, c);
		if (c == XP_CHAR('=')) {
			SET_TOKEN_TYPE (awk, TOKEN_NE);
			ADD_TOKEN_STR (awk, XP_TEXT("!="));
			GET_CHAR_TO (awk, c);
		}
		else {
			SET_TOKEN_TYPE (awk, TOKEN_NOT);
			ADD_TOKEN_STR (awk, XP_TEXT("!"));
		}
	}
	else if (c == XP_CHAR('+')) {
		GET_CHAR_TO (awk, c);
		if (c == XP_CHAR('+')) {
			SET_TOKEN_TYPE (awk, TOKEN_PLUS_PLUS);
			ADD_TOKEN_STR (awk, XP_TEXT("++"));
			GET_CHAR_TO (awk, c);
		}
		else if (c == XP_CHAR('=')) {
			SET_TOKEN_TYPE (awk, TOKEN_PLUS_ASSIGN);
			ADD_TOKEN_STR (awk, XP_TEXT("+="));
			GET_CHAR_TO (awk, c);
		}
		else if (xp_isdigit(c)) {
		//	read_number (XP_CHAR('+'));
		}
		else {
			SET_TOKEN_TYPE (awk, TOKEN_PLUS);
			ADD_TOKEN_STR (awk, XP_TEXT("+"));
		}
	}
	else if (c == XP_CHAR('-')) {
		GET_CHAR_TO (awk, c);
		if (c == XP_CHAR('-')) {
			SET_TOKEN_TYPE (awk, TOKEN_MINUS_MINUS);
			ADD_TOKEN_STR (awk, XP_TEXT("--"));
			GET_CHAR_TO (awk, c);
		}
		else if (c == XP_CHAR('=')) {
			SET_TOKEN_TYPE (awk, TOKEN_MINUS_ASSIGN);
			ADD_TOKEN_STR (awk, XP_TEXT("-="));
			GET_CHAR_TO (awk, c);
		}
		else if (xp_isdigit(c)) {
		//	read_number (XP_CHAR('-'));
		}
		else {
			SET_TOKEN_TYPE (awk, TOKEN_MINUS);
			ADD_TOKEN_STR (awk, XP_TEXT("-"));
		}
	}
	else if (c == XP_CHAR('*')) {
		SET_TOKEN_TYPE (awk, TOKEN_MUL);
		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);
	}
	else if (c == XP_CHAR('/')) {
// TODO: handle regular expression here... /^pattern$/
		SET_TOKEN_TYPE (awk, TOKEN_DIV);
		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);
	}
	else if (c == XP_CHAR('%')) {
		SET_TOKEN_TYPE (awk, TOKEN_MOD);
		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);
	}
	else if (c == XP_CHAR('(')) {
		SET_TOKEN_TYPE (awk, TOKEN_LPAREN);
		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);
	}
	else if (c == XP_CHAR(')')) {
		SET_TOKEN_TYPE (awk, TOKEN_RPAREN);
		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);
	}
	else if (c == XP_CHAR('{')) {
		SET_TOKEN_TYPE (awk, TOKEN_LBRACE);
		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);
	}
	else if (c == XP_CHAR('}')) {
		SET_TOKEN_TYPE (awk, TOKEN_RBRACE);
		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);
	}
	else if (c == XP_CHAR('[')) {
		SET_TOKEN_TYPE (awk, TOKEN_LBRACK);
		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);
	}
	else if (c == XP_CHAR(']')) {
		SET_TOKEN_TYPE (awk, TOKEN_RBRACK);
		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);
	}
	else if (c == XP_CHAR('$')) {
		SET_TOKEN_TYPE (awk, TOKEN_DOLLAR);
		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);
	}
	else if (c == XP_CHAR(',')) {
		SET_TOKEN_TYPE (awk, TOKEN_COMMA);
		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);
	}
	else if (c == XP_CHAR(';')) {
		SET_TOKEN_TYPE (awk, TOKEN_SEMICOLON);
		ADD_TOKEN_CHAR (awk, c);
		GET_CHAR_TO (awk, c);
	}
	else {
		awk->errnum = XP_AWK_ELXCHR;
		return -1;
	}

	return 0;
}

static int __get_char (xp_awk_t* awk)
{
	xp_ssize_t n;
	xp_char_t c;

	if (awk->lex.ungotc_count > 0) {
		awk->lex.curc = awk->lex.ungotc[--awk->lex.ungotc_count];
		return 0;
	}

	n = awk->src_func(XP_AWK_IO_DATA, awk->src_arg, &c, 1);
	if (n == -1) {
		awk->errnum = XP_AWK_ESRCDT;
		return -1;
	}
	awk->lex.curc = (n == 0)? XP_CHAR_EOF: c;
	
	return 0;
}

static int __unget_char (xp_awk_t* awk, xp_cint_t c)
{
	if (awk->lex.ungotc_count >= xp_countof(awk->lex.ungotc)) {
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

	if (c != XP_CHAR('/')) return 0;
	GET_CHAR_TO (awk, c);

	if (c == XP_CHAR('/')) {
		do { 
			GET_CHAR_TO (awk, c);
		} while (c != '\n' && c != XP_CHAR_EOF);
		GET_CHAR (awk);
		return 1;
	}
	else if (c == XP_CHAR('*')) {
		do {
			GET_CHAR_TO (awk, c);
			if (c == XP_CHAR('*')) {
				GET_CHAR_TO (awk, c);
				if (c == XP_CHAR('/')) {
					GET_CHAR_TO (awk, c);
					break;
				}
			}
		} while (0);
		return 1;
	}

	if (__unget_char(awk, c) == -1) return -1;
	awk->lex.curc = XP_CHAR('/');

	return 0;
}

static int __classify_ident (xp_awk_t* awk, const xp_char_t* ident)
{
	struct __kwent* p = __kwtab;

	for (p = __kwtab; p->name != XP_NULL; p++) {
		if (p->valid != 0 && (awk->opt.parse & p->valid) == 0) continue;
		if (xp_strcmp(p->name, ident) == 0) return p->type;
	}

	return TOKEN_IDENT;
}

static xp_long_t __str_to_long (const xp_char_t* name)
{
	xp_long_t n = 0;

	while (xp_isdigit(*name)) {
		n = n * 10 + (*name - XP_CHAR('0'));
		name++;
	}

	return n;
}

