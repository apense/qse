/*
 * $Id: str.h 352 2008-08-31 10:55:59Z baconevi $
 *
 * {License}
 */

#ifndef _ASE_CMN_STR_H_
#define _ASE_CMN_STR_H_

#include <ase/types.h>
#include <ase/macros.h>

#define ASE_STR_LEN(x)      ((x)->size)
#define ASE_STR_SIZE(x)     ((x)->size + 1)
#define ASE_STR_CAPA(x)     ((x)->capa)
#define ASE_STR_BUF(x)      ((x)->buf)
#define ASE_STR_CHAR(x,idx) ((x)->buf[idx])

typedef struct ase_str_t ase_str_t;

struct ase_str_t
{
	ase_mmgr_t* mmgr;
	ase_char_t* buf;
	ase_size_t  size;
	ase_size_t  capa;
};

/* int ase_chartonum (ase_char_t c, int base) */
#define ASE_CHAR_TO_NUM(c,base) \
	((c>=ASE_T('0') && c<=ASE_T('9'))? ((c-ASE_T('0')<base)? (c-ASE_T('0')): base): \
	 (c>=ASE_T('A') && c<=ASE_T('Z'))? ((c-ASE_T('A')+10<base)? (c-ASE_T('A')+10): base): \
	 (c>=ASE_T('a') && c<=ASE_T('z'))? ((c-ASE_T('a')+10<base)? (c-ASE_T('a')+10): base): base)

/* ase_strtonum (const ase_char_t* nptr, ase_char_t** endptr, int base) */
#define ASE_STR_TO_NUM(value,nptr,endptr,base) \
{ \
	int __ston_f = 0, __ston_v; \
	const ase_char_t* __ston_ptr = nptr; \
	for (;;) { \
		ase_char_t __ston_c = *__ston_ptr; \
		if (__ston_c == ASE_T(' ') || \
		    __ston_c == ASE_T('\t')) { __ston_ptr++; continue; } \
		if (__ston_c == ASE_T('-')) { __ston_f++; __ston_ptr++; } \
		if (__ston_c == ASE_T('+')) { __ston_ptr++; } \
		break; \
	} \
	for (value = 0; (__ston_v = ASE_CHAR_TO_NUM(*__ston_ptr, base)) < base; __ston_ptr++) { \
		value = value * base + __ston_v; \
	} \
	if (endptr != ASE_NULL) *((const ase_char_t**)endptr) = __ston_ptr; \
	if (__ston_f > 0) value *= -1; \
}

/* ase_strxtonum (const ase_char_t* nptr, ase_size_t len, ase_char_char** endptr, int base) */
#define ASE_STRX_TO_NUM(value,nptr,len,endptr,base) \
{ \
	int __ston_f = 0, __ston_v; \
	const ase_char_t* __ston_ptr = nptr; \
	const ase_char_t* __ston_end = __ston_ptr + len; \
	value = 0; \
	while (__ston_ptr < __ston_end) { \
		ase_char_t __ston_c = *__ston_ptr; \
		if (__ston_c == ASE_T(' ') || __ston_c == ASE_T('\t')) { \
			__ston_ptr++; continue; \
		} \
		if (__ston_c == ASE_T('-')) { __ston_f++; __ston_ptr++; } \
		if (__ston_c == ASE_T('+')) { __ston_ptr++; } \
		break; \
	} \
	for (value = 0; __ston_ptr < __ston_end && \
	               (__ston_v = ASE_CHAR_TO_NUM(*__ston_ptr, base)) != base; __ston_ptr++) { \
		value = value * base + __ston_v; \
	} \
	if (endptr != ASE_NULL) *((const ase_char_t**)endptr) = __ston_ptr; \
	if (__ston_f > 0) value *= -1; \
}

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * basic string functions
 */
ase_size_t ase_strlen (const ase_char_t* str);

ase_size_t ase_strcpy (
	ase_char_t* buf, const ase_char_t* str);
ase_size_t ase_strxcpy (
	ase_char_t* buf, ase_size_t bsz, const ase_char_t* str);
ase_size_t ase_strncpy (
	ase_char_t* buf, const ase_char_t* str, ase_size_t len);
ase_size_t ase_strxncpy (
    ase_char_t* buf, ase_size_t bsz, const ase_char_t* str, ase_size_t len);

ase_size_t ase_strxcat (
    ase_char_t* buf, ase_size_t bsz, const ase_char_t* str);
ase_size_t ase_strxncat (
    ase_char_t* buf, ase_size_t bsz, const ase_char_t* str, ase_size_t len);

int ase_strcmp (const ase_char_t* s1, const ase_char_t* s2);
int ase_strxcmp (const ase_char_t* s1, ase_size_t len1, const ase_char_t* s2);
int ase_strxncmp (
	const ase_char_t* s1, ase_size_t len1, 
	const ase_char_t* s2, ase_size_t len2);

int ase_strcasecmp (
	const ase_char_t* s1, const ase_char_t* s2, ase_ccls_t* ccls);
int ase_strxncasecmp (
	const ase_char_t* s1, ase_size_t len1, 
	const ase_char_t* s2, ase_size_t len2, ase_ccls_t* ccls);

ase_char_t* ase_strdup (const ase_char_t* str, ase_mmgr_t* mmgr);
ase_char_t* ase_strxdup (
	const ase_char_t* str, ase_size_t len, ase_mmgr_t* mmgr);
ase_char_t* ase_strxdup2 (
	const ase_char_t* str1, ase_size_t len1,
	const ase_char_t* str2, ase_size_t len2, ase_mmgr_t* mmgr);

ase_char_t* ase_strstr (const ase_char_t* str, const ase_char_t* sub);
ase_char_t* ase_strxstr (
	const ase_char_t* str, ase_size_t size, const ase_char_t* sub);
ase_char_t* ase_strxnstr (
	const ase_char_t* str, ase_size_t strsz, 
	const ase_char_t* sub, ase_size_t subsz);

ase_char_t* ase_strchr (const ase_char_t* str, ase_cint_t c);
ase_char_t* ase_strxchr (const ase_char_t* str, ase_size_t len, ase_cint_t c);
ase_char_t* ase_strrchr (const ase_char_t* str, ase_cint_t c);
ase_char_t* ase_strxrchr (const ase_char_t* str, ase_size_t len, ase_cint_t c);

/* Checks if a string begins with a substring */
ase_char_t* ase_strbeg (const ase_char_t* str, const ase_char_t* sub);
ase_char_t* ase_strxbeg (
	const ase_char_t* str, ase_size_t len, const ase_char_t* sub);
ase_char_t* ase_strnbeg (
	const ase_char_t* str, const ase_char_t* sub, ase_size_t len);
ase_char_t* ase_strxnbeg (
	const ase_char_t* str, ase_size_t len1, 
	const ase_char_t* sub, ase_size_t len2);

/* Checks if a string ends with a substring */
ase_char_t* ase_strend (const ase_char_t* str, const ase_char_t* sub);
ase_char_t* ase_strxend (
	const ase_char_t* str, ase_size_t len, const ase_char_t* sub);
ase_char_t* ase_strnend (
	const ase_char_t* str, const ase_char_t* sub, ase_size_t len);
ase_char_t* ase_strxnend (
	const ase_char_t* str, ase_size_t len1, 
	const ase_char_t* sub, ase_size_t len2);

/* 
 * string conversion
 */
int ase_strtoi (const ase_char_t* str);
long ase_strtol (const ase_char_t* str);
unsigned int ase_strtoui (const ase_char_t* str);
unsigned long ase_strtoul (const ase_char_t* str);

int ase_strxtoi (const ase_char_t* str, ase_size_t len);
long ase_strxtol (const ase_char_t* str, ase_size_t len);
unsigned int ase_strxtoui (const ase_char_t* str, ase_size_t len);
unsigned long ase_strxtoul (const ase_char_t* str, ase_size_t len);

ase_int_t ase_strtoint (const ase_char_t* str);
ase_long_t ase_strtolong (const ase_char_t* str);
ase_uint_t ase_strtouint (const ase_char_t* str);
ase_ulong_t ase_strtoulong (const ase_char_t* str);

ase_int_t ase_strxtoint (const ase_char_t* str, ase_size_t len);
ase_long_t ase_strxtolong (const ase_char_t* str, ase_size_t len);
ase_uint_t ase_strxtouint (const ase_char_t* str, ase_size_t len);
ase_ulong_t ase_strxtoulong (const ase_char_t* str, ase_size_t len);

/* 
 * dynamic string 
 */

ase_str_t* ase_str_open (
	ase_mmgr_t* mmgr,
	ase_size_t ext,
	ase_size_t capa
);

void ase_str_close (
	ase_str_t* str
);

/* 
 * If capa is 0, it doesn't allocate the buffer in advance. 
 */
ase_str_t* ase_str_init (
	ase_str_t* str,
	ase_mmgr_t* mmgr,
	ase_size_t capa
);

void ase_str_fini (
	ase_str_t* str
);

/*
 * NAME: yield the buffer 
 * 
 * DESCRIPTION:
 *  The ase_str_yield() function assigns the buffer to an variable of the
 *  ase_cstr_t type and recreate a new buffer of the new_capa capacity.
 *  The function fails if it fails to allocate a new buffer.
 *
 * RETURNS: 0 on success, -1 on failure.
 */
int ase_str_yield (
	ase_str_t* str  /* a dynamic string */,
	ase_cstr_t* buf /* the pointer to a ase_cstr_t variable */,
	int new_capa    /* new capacity in number of characters */
);

void ase_str_clear (ase_str_t* str);
void ase_str_swap (ase_str_t* str, ase_str_t* str2);

ase_size_t ase_str_cpy (ase_str_t* str, const ase_char_t* s);
ase_size_t ase_str_ncpy (ase_str_t* str, const ase_char_t* s, ase_size_t len);

ase_size_t ase_str_cat (ase_str_t* str, const ase_char_t* s);
ase_size_t ase_str_ncat (ase_str_t* str, const ase_char_t* s, ase_size_t len);
ase_size_t ase_str_ccat (ase_str_t* str, ase_char_t c);
ase_size_t ase_str_nccat (ase_str_t* str, ase_char_t c, ase_size_t len);

#ifdef __cplusplus
}
#endif

#endif