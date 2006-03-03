/*
 * $Id: sa.h,v 1.12 2006-03-03 11:51:48 bacon Exp $
 */

#ifndef _XP_AWK_SA_H_
#define _XP_AWK_SA_H_

#ifndef _XP_AWK_AWK_H_
#error Never include this file directly. Include <xp/awk/awk.h> instead
#endif

#ifdef __STAND_ALONE 

#include <stdio.h>
#include <wchar.h>
#include <wctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#define xp_malloc malloc
#define xp_realloc realloc
#define xp_free free
#define xp_assert assert

#define xp_isdigit iswdigit
#define xp_isalpha iswalpha
#define xp_isalnum iswalnum
#define xp_isspace iswspace
#define xp_toupper toupper
#define xp_tolower tolower

#define xp_strcpy wcscpy
#define xp_strcmp wcscmp
#define xp_strlen wcslen

#define xp_memcpy memcpy
#define xp_memcmp memcmp

#define xp_va_start(pvar,param) va_start(pvar,param)
#define xp_va_list va_list
#define xp_va_end(pvar)      va_end(pvar)
#define xp_va_arg(pvar,type) va_arg(pvar,type)

#define xp_main  main

#define XP_CHAR(c) L##c
#define XP_TEXT(c) L##c
#define XP_CHAR_EOF WEOF

#define XP_NULL NULL

#define xp_sizeof(n)   (sizeof(n))
#define xp_countof(n)  (sizeof(n) / sizeof(n[0]))

#define xp_true (0 == 0)
#define xp_false (0 != 0)

typedef unsigned char xp_byte_t;
typedef int	      xp_bool_t;
typedef wchar_t       xp_char_t;
typedef wint_t        xp_cint_t;
typedef size_t        xp_size_t;

#if defined(_WIN32) || defined(vms) || defined(__vms)
typedef long	xp_ssize_t;
#else
typedef ssize_t xp_ssize_t;
#endif

#if defined(_WIN32)
typedef __int64 xp_long_t;
typedef long double xp_real_t;
#elif defined(vax) || defined(__vax)
typedef long xp_long_t;
typedef long double xp_real_t;
#else
typedef long long xp_long_t;
typedef long double xp_real_t;
#endif

#define XP_STR_LEN(x)  ((x)->size)
#define XP_STR_SIZE(x) ((x)->size + 1)
#define XP_STR_CAPA(x) ((x)->capa)
#define XP_STR_BUF(x)  ((x)->buf)

typedef struct xp_str_t xp_str_t;

struct xp_str_t
{
	xp_char_t* buf;
	xp_size_t size;
	xp_size_t capa;
	xp_bool_t __dynamic;
};

#ifdef __cplusplus
extern "C" {
#endif

xp_char_t* xp_strdup (const xp_char_t* str);

int xp_printf (const xp_char_t* fmt, ...);
int xp_vprintf (const xp_char_t* fmt, xp_va_list ap);
int xp_sprintf (xp_char_t* buf, xp_size_t size, const xp_char_t* fmt, ...);
int xp_vsprintf (
	xp_char_t* buf, xp_size_t size, const xp_char_t* fmt, xp_va_list ap);

xp_str_t* xp_str_open (xp_str_t* str, xp_size_t capa);
void xp_str_close (xp_str_t* str);
xp_size_t xp_str_cat (xp_str_t* str, const xp_char_t* s);
xp_size_t xp_str_ncat (xp_str_t* str, const xp_char_t* s, xp_size_t len);
xp_size_t xp_str_ccat (xp_str_t* str, xp_char_t c);
void xp_str_clear (xp_str_t* str);

#ifdef __cplusplus
}
#endif

#endif

#endif
