/*
 * $Id: rex.h,v 1.6 2006-07-24 11:58:54 bacon Exp $
 **/

#ifndef _XP_AWK_REX_H_
#define _XP_AWK_REX_H_

#ifndef _XP_AWK_AWK_H_
#error Never include this file directly. Include <xp/awk/awk.h> instead
#endif

/*
 * Regular Expression Syntax
 *   A regular expression is zero or more branches, separated by '|'.
 *   ......
 *   ......
 *
 * Compiled form of a regular expression:
 *
 *   | expression                                                                      |
 *   | header  | branch                          | branch              | branch        |
 *   | nb | el | na | bl | cmd | arg | cmd | arg | na | bl | cmd | arg | na | bl | cmd |
 *
 *   nb: the number of branches
 *   el: the length of a expression excluding the length of nb and el
 *   na: the number of atoms
 *   bl: the length of a branch excluding the length of bl
 *   cmd: The command and repetition info encoded together. 
 *      Some commands require an argument to follow them but some other don't.
 *      It is encoded as follows:
 *
 *   Subexpressions can be nested by having the command "GROUP" 
 *   and a subexpression as its argument.
 *
 * Examples:
 *   a.c -> |1|6|5|ORD_CHAR(no bound)|a|ANY_CHAR(no bound)|ORD_CHAR(no bound)|c|
 *   ab|xy -> |2|10|4|ORD_CHAR(no bound)|a|ORD_CHAR(no bound)|b|4|ORD_CHAR(no bound)|x|ORD_CHAR(no bound)|y|
 */

struct xp_awk_rex_t
{
	struct
	{
		const xp_char_t* ptr;
		const xp_char_t* end;
		const xp_char_t* curp;
		struct
		{
			int type;
			xp_char_t value;
		} curc;
	} ptn;

	struct
	{
		xp_byte_t* buf;
		xp_size_t  size;
		xp_size_t  capa;
	} code;

	struct
	{
		struct
		{
			const xp_char_t* ptr;
			const xp_char_t* end;
		} str;
	} match;

	xp_bool_t __dynamic;
};

#ifdef __cplusplus
extern "C" {
#endif

xp_awk_rex_t* xp_awk_rex_open (xp_awk_rex_t* rex);
void xp_awk_rex_close (xp_awk_rex_t* rex);

int xp_awk_rex_compile (xp_awk_rex_t* rex, const xp_char_t* ptn, xp_size_t len);

void xp_awk_rex_print (xp_awk_rex_t* rex);

int xp_awk_rex_match (xp_awk_rex_t* rex, 
	const xp_char_t* str, xp_size_t len, 
	const xp_char_t** match_ptr, xp_size_t* match_len);

#ifdef __cplusplus
}
#endif

#endif
