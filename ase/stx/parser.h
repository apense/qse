/*
 * $Id: parser.h,v 1.9 2005-06-05 05:26:24 bacon Exp $
 */

#ifndef _XP_STX_PARSER_H_
#define _XP_STX_PARSER_H_

#include <xp/stx/stx.h>
#include <xp/stx/token.h>

enum
{
	XP_STX_PARSER_ERROR_NONE = 0,
	XP_STX_PARSER_ERROR_INPUT,
	XP_STX_PARSER_ERROR_INVALID,
	XP_STX_PARSER_ERROR_CHAR
};

typedef struct xp_stx_parser_t xp_stx_parser_t;

struct xp_stx_parser_t
{
	int error_code;
	xp_stx_token_t token;

	xp_stx_cint_t curc;
	xp_stx_cint_t ungotc[5];
	xp_size_t ungotc_count;

	void* input;
	int (*input_reset) (xp_stx_parser_t*);
	int (*input_consume) (xp_stx_parser_t*, xp_stx_cint_t*);

	xp_bool_t __malloced;
};

#ifdef __cplusplus
extern "C" {
#endif

xp_stx_parser_t* xp_stx_parser_open (xp_stx_parser_t* parser);
void xp_stx_parser_close (xp_stx_parser_t* parser);

int xp_stx_parse_method (xp_stx_parser_t* parser, 
	xp_stx_word_t method_class, xp_stx_char_t* method_text);

#ifdef __cplusplus
}
#endif

#endif
