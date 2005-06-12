/*
 * $Id: token.h,v 1.11 2005-06-12 12:33:31 bacon Exp $
 */

#ifndef _XP_STX_TOKEN_H_
#define _XP_STX_TOKEN_H_

#include <xp/stx/stx.h>

enum
{
	XP_STX_TOKEN_END,
	XP_STX_TOKEN_CHARLIT,
	XP_STX_TOKEN_STRLIT,
	XP_STX_TOKEN_NUMLIT,
	XP_STX_TOKEN_IDENT,
	XP_STX_TOKEN_BINARY,
	XP_STX_TOKEN_KEYWORD,
	XP_STX_TOKEN_PRIMITIVE,
	XP_STX_TOKEN_ASSIGN,
	XP_STX_TOKEN_COLON,
	XP_STX_TOKEN_RETURN,
	XP_STX_TOKEN_LBRACKET,
	XP_STX_TOKEN_RBRACKET,
	XP_STX_TOKEN_LPAREN,
	XP_STX_TOKEN_RPAREN,
	XP_STX_TOKEN_PERIOD
};

struct xp_stx_token_t 
{
	int type;

	/*
	xp_stx_int_t   ivalue;
	xp_stx_real_t  fvalue;
	*/

	xp_word_t   capacity;
	xp_word_t   size;
	xp_char_t*  buffer;

	xp_bool_t __malloced;
};

typedef struct xp_stx_token_t xp_stx_token_t;

#ifdef __cplusplus
extern "C" {
#endif

xp_stx_token_t* xp_stx_token_open (
	xp_stx_token_t* token, xp_word_t capacity);
void xp_stx_token_close (xp_stx_token_t* token);

int xp_stx_token_addc (xp_stx_token_t* token, xp_cint_t c);
void xp_stx_token_clear (xp_stx_token_t* token);
xp_char_t* xp_stx_token_yield (xp_stx_token_t* token, xp_word_t capacity);
int xp_stx_token_compare  (xp_stx_token_t* token, const xp_char_t* str);

#ifdef __cplusplus
}
#endif

#endif
