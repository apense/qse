/*
 * $Id: misc.c,v 1.10 2006-09-01 06:42:52 bacon Exp $
 */

#include <xp/awk/awk_i.h>

#ifndef XP_AWK_STAND_ALONE
#include <xp/bas/ctype.h>
#include <xp/bas/assert.h>
#endif

static int __vprintf (
	xp_awk_t* awk, const xp_char_t* fmt, xp_va_list ap);
static int __vsprintf (
	xp_awk_t* awk, xp_char_t* buf, xp_size_t size, 
	const xp_char_t* fmt, xp_va_list ap);

static xp_char_t* __adjust_format (xp_awk_t* awk, const xp_char_t* format);

xp_long_t xp_awk_strtolong (
	xp_awk_t* awk, const xp_char_t* str, 
	int base, const xp_char_t** endptr)
{
	xp_long_t n = 0;
	const xp_char_t* p;
	int digit, negative = 0;

	xp_assert (base < 37); 

	p = str; while (XP_AWK_ISSPACE(awk,*p)) p++;

	while (*p != XP_T('\0')) 
	{
		if (*p == XP_T('-')) 
		{
			negative = ~negative;
			p++;
		}
		else if (*p == XP_T('+')) p++;
		else break;
	}

	if (base == 0) 
	{
		if (*p == XP_T('0')) 
		{
			p++;
			if (*p == XP_T('x') || *p == XP_T('X'))
			{
				p++; base = 16;
			} 
			else if (*p == XP_T('b') || *p == XP_T('B'))
			{
				p++; base = 2;
			}
			else base = 8;
		}
		else base = 10;
	} 
	else if (base == 16) 
	{
		if (*p == XP_T('0') && 
		    (*(p+1) == XP_T('x') || *(p+1) == XP_T('X'))) p += 2; 
	}
	else if (base == 2)
	{
		if (*p == XP_T('0') && 
		    (*(p+1) == XP_T('b') || *(p+1) == XP_T('B'))) p += 2; 
	}

	while (*p != XP_T('\0'))
	{
		if (*p >= XP_T('0') && *p <= XP_T('9'))
			digit = *p - XP_T('0');
		else if (*p >= XP_T('A') && *p <= XP_T('Z'))
			digit = *p - XP_T('A') + 10;
		else if (*p >= XP_T('a') && *p <= XP_T('z'))
			digit = *p - XP_T('a') + 10;
		else break;

		if (digit >= base) break;
		n = n * base + digit;

		p++;
	}

	if (endptr != XP_NULL) *endptr = p;
	return (negative)? -n: n;
}


/*
 * xp_awk_strtoreal is almost a replica of strtod.
 *
 * strtod.c --
 *
 *      Source code for the "strtod" library procedure.
 *
 * Copyright (c) 1988-1993 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#define MAX_EXPONENT 511

xp_real_t xp_awk_strtoreal (xp_awk_t* awk, const xp_char_t* str)
{
	/* 
	 * Table giving binary powers of 10. Entry is 10^2^i.  
	 * Used to convert decimal exponents into floating-point numbers.
	 */ 
	static xp_real_t powersOf10[] = {
		10.,    100.,   1.0e4,   1.0e8,   1.0e16,
		1.0e32, 1.0e64, 1.0e128, 1.0e256
	};

	xp_real_t fraction, dblExp, * d;
	const xp_char_t* p;
	xp_cint_t c;
	int exp = 0;		/* Exponent read from "EX" field */

	/* 
	 * Exponent that derives from the fractional part.  Under normal 
	 * circumstatnces, it is the negative of the number of digits in F.
	 * However, if I is very long, the last digits of I get dropped 
	 * (otherwise a long I with a large negative exponent could cause an
	 * unnecessary overflow on I alone).  In this case, frac_exp is 
	 * incremented one for each dropped digit. 
	 */

	int frac_exp;
	int mantSize; /* Number of digits in mantissa. */
	int decPt;    /* Number of mantissa digits BEFORE decimal point */
	const xp_char_t *pExp;  /* Temporarily holds location of exponent in string */
	int sign = 0, expSign = 0;

	p = str;

	/* Strip off leading blanks and check for a sign */
	while (XP_AWK_ISSPACE(awk,*p)) p++;

	while (*p != XP_T('\0')) 
	{
		if (*p == XP_T('-')) 
		{
			sign = ~sign;
			p++;
		}
		else if (*p == XP_T('+')) p++;
		else break;
	}

	/* Count the number of digits in the mantissa (including the decimal
	 * point), and also locate the decimal point. */
	decPt = -1;
	for (mantSize = 0; ; mantSize++) {
		c = *p;
		if (!XP_AWK_ISDIGIT (awk, c)) {
			if ((c != XP_T('.')) || (decPt >= 0)) break;
			decPt = mantSize;
		}
		p++;
	}

	/*
	 * Now suck up the digits in the mantissa.  Use two integers to
	 * collect 9 digits each (this is faster than using floating-point).
	 * If the mantissa has more than 18 digits, ignore the extras, since
	 * they can't affect the value anyway.
	 */
	pExp  = p;
	p -= mantSize;
	if (decPt < 0) 
	{
		decPt = mantSize;
	} 
	else 
	{
		mantSize -= 1;	/* One of the digits was the point */
	}

	if (mantSize > 18) 
	{
		frac_exp = decPt - 18;
		mantSize = 18;
	} 
	else 
	{
		frac_exp = decPt - mantSize;
	}

	if (mantSize == 0) 
	{
		fraction = 0.0;
		/*p = str;*/
		goto done;
	} 
	else 
	{
		int frac1, frac2;
		frac1 = 0;
		for ( ; mantSize > 9; mantSize -= 1) 
		{
			c = *p;
			p++;
			if (c == XP_T('.')) 
			{
				c = *p;
				p++;
			}
			frac1 = 10 * frac1 + (c - XP_T('0'));
		}
		frac2 = 0;
		for (; mantSize > 0; mantSize -= 1) {
			c = *p;
			p++;
			if (c == XP_T('.')) 
			{
				c = *p;
				p++;
			}
			frac2 = 10*frac2 + (c - XP_T('0'));
		}
		fraction = (1.0e9 * frac1) + frac2;
	}

	/* Skim off the exponent */
	p = pExp;
	if ((*p == XP_T('E')) || (*p == XP_T('e'))) 
	{
		p++;
		if (*p == XP_T('-')) 
		{
			expSign = 1;
			p++;
		} 
		else 
		{
			if (*p == XP_T('+')) p++;
			expSign = 0;
		}
		if (!XP_AWK_ISDIGIT (awk, *p)) 
		{
			/* p = pExp; */
			goto done;
		}
		while (XP_AWK_ISDIGIT (awk, *p)) 
		{
			exp = exp * 10 + (*p - XP_T('0'));
			p++;
		}
	}

	if (expSign) exp = frac_exp - exp;
	else exp = frac_exp + exp;

	/*
	 * Generate a floating-point number that represents the exponent.
	 * Do this by processing the exponent one bit at a time to combine
	 * many powers of 2 of 10. Then combine the exponent with the
	 * fraction.
	 */
	if (exp < 0) 
	{
		expSign = 1;
		exp = -exp;
	} 
	else expSign = 0;

	if (exp > MAX_EXPONENT) exp = MAX_EXPONENT;

	dblExp = 1.0;

	for (d = powersOf10; exp != 0; exp >>= 1, d++) 
	{
		if (exp & 01) dblExp *= *d;
	}

	if (expSign) fraction /= dblExp;
	else fraction *= dblExp;

done:
	return (sign)? -fraction: fraction;
}

xp_char_t* xp_awk_strdup (xp_awk_t* awk, const xp_char_t* str)
{
	xp_char_t* tmp;

	tmp = (xp_char_t*) XP_AWK_MALLOC (
		awk, (xp_awk_strlen(str) + 1) * xp_sizeof(xp_char_t));
	if (tmp == XP_NULL) return XP_NULL;

	xp_awk_strcpy (tmp, str);
	return tmp;
}

xp_char_t* xp_awk_strxdup (xp_awk_t* awk, const xp_char_t* str, xp_size_t len)
{
	xp_char_t* tmp;

	tmp = (xp_char_t*) XP_AWK_MALLOC (
		awk, (len + 1) * xp_sizeof(xp_char_t));
	if (tmp == XP_NULL) return XP_NULL;

	xp_awk_strncpy (tmp, str, len);
	return tmp;
}

xp_char_t* xp_awk_strxdup2 (
	xp_awk_t* awk,
	const xp_char_t* str1, xp_size_t len1,
	const xp_char_t* str2, xp_size_t len2)
{
	xp_char_t* tmp;

	tmp = (xp_char_t*) XP_AWK_MALLOC (
		awk, (len1 + len2 + 1) * xp_sizeof(xp_char_t));
	if (tmp == XP_NULL) return XP_NULL;

	xp_awk_strncpy (tmp, str1, len1);
	xp_awk_strncpy (tmp + len1, str2, len2);
	return tmp;
}

xp_size_t xp_awk_strlen (const xp_char_t* str)
{
	const xp_char_t* p = str;
	while (*p != XP_T('\0')) p++;
	return p - str;
}

xp_size_t xp_awk_strcpy (xp_char_t* buf, const xp_char_t* str)
{
	xp_char_t* org = buf;
	while ((*buf++ = *str++) != XP_T('\0'));
	return buf - org - 1;
}

xp_size_t xp_awk_strncpy (xp_char_t* buf, const xp_char_t* str, xp_size_t len)
{
	const xp_char_t* end = str + len;
	while (str < end) *buf++ = *str++;
	*buf = XP_T('\0');
	return len;
}

int xp_awk_strcmp (const xp_char_t* s1, const xp_char_t* s2)
{
	while (*s1 == *s2) 
	{
		if (*s1 == XP_C('\0')) return 0;
		s1++, s2++;
	}

	return (*s1 > *s2)? 1: -1;
}

int xp_awk_strxncmp (
	const xp_char_t* s1, xp_size_t len1, 
	const xp_char_t* s2, xp_size_t len2)
{
	const xp_char_t* end1 = s1 + len1;
	const xp_char_t* end2 = s2 + len2;

	while (s1 < end1 && s2 < end2 && *s1 == *s2) s1++, s2++;
	if (s1 == end1 && s2 == end2) return 0;
	if (*s1 == *s2) return (s1 < end1)? 1: -1;
	return (*s1 > *s2)? 1: -1;
}

xp_char_t* xp_awk_strxnstr (
	const xp_char_t* str, xp_size_t strsz, 
	const xp_char_t* sub, xp_size_t subsz)
{
	const xp_char_t* end, * subp;

	if (subsz == 0) return (xp_char_t*)str;
	if (strsz < subsz) return XP_NULL;
	
	end = str + strsz - subsz;
	subp = sub + subsz;

	while (str <= end) {
		const xp_char_t* x = str;
		const xp_char_t* y = sub;

		while (xp_true) {
			if (y >= subp) return (xp_char_t*)str;
			if (*x != *y) break;
			x++; y++;
		}	

		str++;
	}
		
	return XP_NULL;
}

xp_char_t* xp_awk_strtok (
	xp_awk_t* awk, const xp_char_t* s, 
	const xp_char_t* delim, xp_char_t** tok, xp_size_t* tok_len)
{
	return xp_awk_strxntok (
		awk, s, xp_awk_strlen(s), 
		delim, xp_awk_strlen(delim), tok, tok_len);
}

xp_char_t* xp_awk_strxtok (
	xp_awk_t* awk, const xp_char_t* s, xp_size_t len,
	const xp_char_t* delim, xp_char_t** tok, xp_size_t* tok_len)
{
	return xp_awk_strxntok (
		awk, s, len, delim, xp_awk_strlen(delim), tok, tok_len);
}

xp_char_t* xp_awk_strxntok (
	xp_awk_t* awk, const xp_char_t* s, xp_size_t len,
	const xp_char_t* delim, xp_size_t delim_len, 
	xp_char_t** tok, xp_size_t* tok_len)
{
	const xp_char_t* p = s, *d;
	const xp_char_t* end = s + len;	
	const xp_char_t* sp = XP_NULL, * ep = XP_NULL;
	const xp_char_t* delim_end = delim + delim_len;
	xp_char_t c; 
	int delim_mode;

	/* skip preceding space xp_char_tacters */
	while (p < end && XP_AWK_ISSPACE(awk,*p)) p++;

	if (delim == XP_NULL) delim_mode = 0;
	else 
	{
		delim_mode = 1;
		for (d = delim; d < delim_end; d++) 
			if (!XP_AWK_ISSPACE(awk,*d)) delim_mode = 2;
	}

	if (delim_mode == 0) 
	{ 
		/* when XP_NULL is given as "delim", it has an effect of cutting
		   preceding and trailing space xp_char_tacters off "s". */
		while (p < end) 
		{
			c = *p;

			if (!XP_AWK_ISSPACE(awk,c)) 
			{
				if (sp == XP_NULL) sp = p;
				ep = p;
			}
			p++;
		}
	}
	else if (delim_mode == 1) 
	{
		while (p < end) 
		{
			c = *p;
			if (XP_AWK_ISSPACE(awk,c)) break;
			if (sp == XP_NULL) sp = p;
			ep = p++;
		}
	}
	else /* if (delim_mode == 2) */ 
	{
		while (p < end) 
		{
			c = *p;
			if (XP_AWK_ISSPACE(awk,c)) 
			{
				p++;
				continue;
			}
			for (d = delim; d < delim_end; d++) 
			{
				if (c == *d) goto exit_loop;
			}
			if (sp == XP_NULL) sp = p;
			ep = p++;
		}
	}

exit_loop:
	if (sp == XP_NULL) 
	{
		*tok = XP_NULL;
		*tok_len = (xp_size_t)0;
	}
	else 
	{
		*tok = (xp_char_t*)sp;
		*tok_len = ep - sp + 1;
	}

	return (p >= end)? XP_NULL: ((xp_char_t*)++p);
}

int xp_awk_printf (xp_awk_t* awk, const xp_char_t* fmt, ...)
{
	int n;
	xp_va_list ap;

	xp_va_start (ap, fmt);
	n = __vprintf (awk, fmt, ap);
	xp_va_end (ap);
	return n;
}

static int __vprintf (xp_awk_t* awk, const xp_char_t* fmt, xp_va_list ap)
{
	int n;
	xp_char_t* nf = __adjust_format (awk, fmt);
	if (nf == XP_NULL) return -1;

#ifdef XP_CHAR_IS_MCHAR
	n = vprintf (nf, ap);
#else
	n =  vwprintf (nf, ap);
#endif

	XP_AWK_FREE (awk, nf);
	return n;
}

int xp_awk_sprintf (
	xp_awk_t* awk, xp_char_t* buf, xp_size_t size, 
	const xp_char_t* fmt, ...)
{
	int n;
	xp_va_list ap;

	xp_va_start (ap, fmt);
	n = __vsprintf (awk, buf, size, fmt, ap);
	xp_va_end (ap);
	return n;
}

static int __vsprintf (
	xp_awk_t* awk, xp_char_t* buf, xp_size_t size, 
	const xp_char_t* fmt, xp_va_list ap)
{
	int n;
	xp_char_t* nf = __adjust_format (awk, fmt);
	if (nf == XP_NULL) return -1;

#if defined(dos) || defined(__dos)
	n = vsprintf (buf, nf, ap); /* TODO: write your own vsnprintf */
#elif defined(XP_CHAR_IS_MCHAR)
	n = vsnprintf (buf, size, nf, ap);
#elif defined(_WIN32)
	n = _vsnwprintf (buf, size, nf, ap);
#else
	n = vswprintf (buf, size, nf, ap);
#endif
	XP_AWK_FREE (awk, nf);
	return n;
}

#define MOD_SHORT       1
#define MOD_LONG        2
#define MOD_LONGLONG    3

#define ADDC(str,c) \
	do { \
		if (xp_awk_str_ccat(&str, c) == (xp_size_t)-1) { \
			xp_awk_str_close (&str); \
			return XP_NULL; \
		} \
	} while (0)

static xp_char_t* __adjust_format (xp_awk_t* awk, const xp_char_t* format)
{
	const xp_char_t* fp = format;
	xp_char_t* tmp;
	xp_awk_str_t str;
	xp_char_t ch;
	int modifier;

	if (xp_awk_str_open (&str, 256, awk) == XP_NULL) return XP_NULL;

	while (*fp != XP_T('\0')) 
	{
		while (*fp != XP_T('\0') && *fp != XP_T('%')) 
		{
			ADDC (str, *fp++);
		}

		if (*fp == XP_T('\0')) break;
		xp_assert (*fp == XP_T('%'));

		ch = *fp++;	
		ADDC (str, ch); /* add % */

		ch = *fp++;

		/* flags */
		for (;;) 
		{
			if (ch == XP_T(' ') || ch == XP_T('+') ||
			    ch == XP_T('-') || ch == XP_T('#')) 
			{
				ADDC (str, ch);
			}
			else if (ch == XP_T('0')) 
			{
				ADDC (str, ch);
				ch = *fp++; 
				break;
			}
			else break;

			ch = *fp++;
		}

		/* check the width */
		if (ch == XP_T('*')) ADDC (str, ch);
		else 
		{
			while (XP_AWK_ISDIGIT (awk, ch)) 
			{
				ADDC (str, ch);
				ch = *fp++;
			}
		}

		/* precision */
		if (ch == XP_T('.')) 
		{
			ADDC (str, ch);
			ch = *fp++;

			if (ch == XP_T('*')) ADDC (str, ch);
			else 
			{
				while (XP_AWK_ISDIGIT (awk, ch)) 
				{
					ADDC (str, ch);
					ch = *fp++;
				}
			}
		}

		/* modifier */
		for (modifier = 0;;) 
		{
			if (ch == XP_T('h')) modifier = MOD_SHORT;
			else if (ch == XP_T('l')) 
			{
				modifier = (modifier == MOD_LONG)? MOD_LONGLONG: MOD_LONG;
			}
			else break;
			ch = *fp++;
		}		


		/* type */
		if (ch == XP_T('%')) ADDC (str, ch);
		else if (ch == XP_T('c') || ch == XP_T('s')) 
		{
#if !defined(XP_CHAR_IS_MCHAR) && !defined(_WIN32)
			ADDC (str, 'l');
#endif
			ADDC (str, ch);
		}
		else if (ch == XP_T('C') || ch == XP_T('S')) 
		{
#ifdef _WIN32
			ADDC (str, ch);
#else
	#ifdef XP_CHAR_IS_MCHAR
			ADDC (str, 'l');
	#endif
			ADDC (str, XP_AWK_TOLOWER(awk,ch));
#endif
		}
		else if (ch == XP_T('d') || ch == XP_T('i') || 
		         ch == XP_T('o') || ch == XP_T('u') || 
		         ch == XP_T('x') || ch == XP_T('X')) 
		{
			if (modifier == MOD_SHORT) 
			{
				ADDC (str, 'h');
			}
			else if (modifier == MOD_LONG) 
			{
				ADDC (str, 'l');
			}
			else if (modifier == MOD_LONGLONG) 
			{
#if defined(_WIN32) && !defined(__LCC__)
				ADDC (str, 'I');
				ADDC (str, '6');
				ADDC (str, '4');
#else
				ADDC (str, 'l');
				ADDC (str, 'l');
#endif
			}
			ADDC (str, ch);
		}
		else if (ch == XP_T('\0')) break;
		else ADDC (str, ch);
	}

	tmp = XP_AWK_STR_BUF(&str);
	xp_awk_str_forfeit (&str);
	return tmp;
}

