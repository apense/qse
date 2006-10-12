/*
 * $Id: misc.c,v 1.28 2006-10-12 14:36:25 bacon Exp $
 */

#include <xp/awk/awk_i.h>

void* xp_awk_memcpy  (void* dst, const void* src, xp_size_t n)
{
	void* p = dst;
	void* e = (xp_byte_t*)dst + n;

	while (dst < e) 
	{
		*(xp_byte_t*)dst = *(xp_byte_t*)src;
		dst = (xp_byte_t*)dst + 1;
		src = (xp_byte_t*)src + 1;
	}

	return p;
}

void* xp_awk_memset (void* dst, int val, xp_size_t n)
{
	void* p = dst;
	void* e = (xp_byte_t*)p + n;

	while (p < e) 
	{
		*(xp_byte_t*)p = (xp_byte_t)val;
		p = (xp_byte_t*)p + 1;
	}

	return dst;
}

xp_long_t xp_awk_strxtolong (
	xp_awk_t* awk, const xp_char_t* str, xp_size_t len,
	int base, const xp_char_t** endptr)
{
	xp_long_t n = 0;
	const xp_char_t* p;
	const xp_char_t* end;
	xp_size_t rem;
	int digit, negative = 0;

	xp_awk_assert (awk, base < 37); 

	p = str; 
	end = str + len;
	
	/* strip off leading spaces */
	/*while (XP_AWK_ISSPACE(awk,*p)) p++;*/

	/* check for a sign */
	/*while (*p != XP_T('\0')) */
	while (p < end)
	{
		if (*p == XP_T('-')) 
		{
			negative = ~negative;
			p++;
		}
		else if (*p == XP_T('+')) p++;
		else break;
	}

	/* check for a binary/octal/hexadecimal notation */
	rem = end - p;
	if (base == 0) 
	{
		if (rem >= 1 && *p == XP_T('0')) 
		{
			p++;

			if (rem == 1) base = 8;
			else if (*p == XP_T('x') || *p == XP_T('X'))
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
	else if (rem >= 2 && base == 16)
	{
		if (*p == XP_T('0') && 
		    (*(p+1) == XP_T('x') || *(p+1) == XP_T('X'))) p += 2; 
	}
	else if (rem >= 2 && base == 2)
	{
		if (*p == XP_T('0') && 
		    (*(p+1) == XP_T('b') || *(p+1) == XP_T('B'))) p += 2; 
	}

	/* process the digits */
	/*while (*p != XP_T('\0'))*/
	while (p < end)
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
	static xp_real_t powers_of_10[] = 
	{
		10.,    100.,   1.0e4,   1.0e8,   1.0e16,
		1.0e32, 1.0e64, 1.0e128, 1.0e256
	};

	xp_real_t fraction, dbl_exp, * d;
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
	int mant_size; /* Number of digits in mantissa. */
	int dec_pt;    /* Number of mantissa digits BEFORE decimal point */
	const xp_char_t *pexp;  /* Temporarily holds location of exponent in string */
	int negative = 0, exp_negative = 0;

	p = str;

	/* strip off leading blanks */ 
	/*while (XP_AWK_ISSPACE(awk,*p)) p++;*/

	/* check for a sign */
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

	/* Count the number of digits in the mantissa (including the decimal
	 * point), and also locate the decimal point. */
	dec_pt = -1;
	for (mant_size = 0; ; mant_size++) 
	{
		c = *p;
		if (!XP_AWK_ISDIGIT (awk, c)) 
		{
			if ((c != XP_T('.')) || (dec_pt >= 0)) break;
			dec_pt = mant_size;
		}
		p++;
	}

	/*
	 * Now suck up the digits in the mantissa.  Use two integers to
	 * collect 9 digits each (this is faster than using floating-point).
	 * If the mantissa has more than 18 digits, ignore the extras, since
	 * they can't affect the value anyway.
	 */
	pexp = p;
	p -= mant_size;
	if (dec_pt < 0) 
	{
		dec_pt = mant_size;
	} 
	else 
	{
		mant_size--;	/* One of the digits was the point */
	}

	if (mant_size > 18) 
	{
		frac_exp = dec_pt - 18;
		mant_size = 18;
	} 
	else 
	{
		frac_exp = dec_pt - mant_size;
	}

	if (mant_size == 0) 
	{
		fraction = 0.0;
		/*p = str;*/
		p = pexp;
		goto done;
	} 
	else 
	{
		int frac1, frac2;
		frac1 = 0;
		for ( ; mant_size > 9; mant_size--) 
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
		for (; mant_size > 0; mant_size--) {
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
	p = pexp;
	if ((*p == XP_T('E')) || (*p == XP_T('e'))) 
	{
		p++;
		if (*p == XP_T('-')) 
		{
			exp_negative = 1;
			p++;
		} 
		else 
		{
			if (*p == XP_T('+')) p++;
			exp_negative = 0;
		}
		if (!XP_AWK_ISDIGIT (awk, *p)) 
		{
			/* p = pexp; */
			/* goto done; */
			goto no_exp;
		}
		while (XP_AWK_ISDIGIT (awk, *p)) 
		{
			exp = exp * 10 + (*p - XP_T('0'));
			p++;
		}
	}

no_exp:
	if (exp_negative) exp = frac_exp - exp;
	else exp = frac_exp + exp;

	/*
	 * Generate a floating-point number that represents the exponent.
	 * Do this by processing the exponent one bit at a time to combine
	 * many powers of 2 of 10. Then combine the exponent with the
	 * fraction.
	 */
	if (exp < 0) 
	{
		exp_negative = 1;
		exp = -exp;
	} 
	else exp_negative = 0;

	if (exp > MAX_EXPONENT) exp = MAX_EXPONENT;

	dbl_exp = 1.0;

	for (d = powers_of_10; exp != 0; exp >>= 1, d++) 
	{
		if (exp & 01) dbl_exp *= *d;
	}

	if (exp_negative) fraction /= dbl_exp;
	else fraction *= dbl_exp;

done:
	return (negative)? -fraction: fraction;
}

xp_real_t xp_awk_strxtoreal (
	xp_awk_t* awk, const xp_char_t* str, xp_size_t len, 
	const xp_char_t** endptr)
{
	/* 
	 * Table giving binary powers of 10. Entry is 10^2^i.  
	 * Used to convert decimal exponents into floating-point numbers.
	 */ 
	static xp_real_t powers_of_10[] = 
	{
		10.,    100.,   1.0e4,   1.0e8,   1.0e16,
		1.0e32, 1.0e64, 1.0e128, 1.0e256
	};

	xp_real_t fraction, dbl_exp, * d;
	const xp_char_t* p, * end;
	xp_cint_t c;
	int exp = 0; /* Exponent read from "EX" field */

	/* 
	 * Exponent that derives from the fractional part.  Under normal 
	 * circumstatnces, it is the negative of the number of digits in F.
	 * However, if I is very long, the last digits of I get dropped 
	 * (otherwise a long I with a large negative exponent could cause an
	 * unnecessary overflow on I alone).  In this case, frac_exp is 
	 * incremented one for each dropped digit. 
	 */

	int frac_exp;
	int mant_size; /* Number of digits in mantissa. */
	int dec_pt;    /* Number of mantissa digits BEFORE decimal point */
	const xp_char_t *pexp;  /* Temporarily holds location of exponent in string */
	int negative = 0, exp_negative = 0;

	p = str;
	end = str + len;

	/* Strip off leading blanks and check for a sign */
	/*while (XP_AWK_ISSPACE(awk,*p)) p++;*/

	/*while (*p != XP_T('\0')) */
	while (p < end)
	{
		if (*p == XP_T('-')) 
		{
			negative = ~negative;
			p++;
		}
		else if (*p == XP_T('+')) p++;
		else break;
	}

	/* Count the number of digits in the mantissa (including the decimal
	 * point), and also locate the decimal point. */
	dec_pt = -1;
	/*for (mant_size = 0; ; mant_size++) */
	for (mant_size = 0; p < end; mant_size++) 
	{
		c = *p;
		if (!XP_AWK_ISDIGIT (awk, c)) 
		{
			if (c != XP_T('.') || dec_pt >= 0) break;
			dec_pt = mant_size;
		}
		p++;
	}

	/*
	 * Now suck up the digits in the mantissa.  Use two integers to
	 * collect 9 digits each (this is faster than using floating-point).
	 * If the mantissa has more than 18 digits, ignore the extras, since
	 * they can't affect the value anyway.
	 */
	pexp = p;
	p -= mant_size;
	if (dec_pt < 0) 
	{
		dec_pt = mant_size;
	} 
	else 
	{
		mant_size--;	/* One of the digits was the point */
	}

	if (mant_size > 18)  /* TODO: is 18 correct for xp_real_t??? */
	{
		frac_exp = dec_pt - 18;
		mant_size = 18;
	} 
	else 
	{
		frac_exp = dec_pt - mant_size;
	}

	if (mant_size == 0) 
	{
		fraction = 0.0;
		/*p = str;*/
		p = pexp;
		goto done;
	} 
	else 
	{
		int frac1, frac2;

		frac1 = 0;
		for ( ; mant_size > 9; mant_size--) 
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
		for (; mant_size > 0; mant_size--) {
			c = *p++;
			if (c == XP_T('.')) 
			{
				c = *p;
				p++;
			}
			frac2 = 10 * frac2 + (c - XP_T('0'));
		}
		fraction = (1.0e9 * frac1) + frac2;
	}

	/* Skim off the exponent */
	p = pexp;
	if (p < end && (*p == XP_T('E') || *p == XP_T('e'))) 
	{
		p++;

		if (p < end) 
		{
			if (*p == XP_T('-')) 
			{
				exp_negative = 1;
				p++;
			} 
			else 
			{
				if (*p == XP_T('+')) p++;
				exp_negative = 0;
			}
		}
		else exp_negative = 0;

		if (!(p < end && XP_AWK_ISDIGIT (awk, *p))) 
		{
			/*p = pexp;*/
			/*goto done;*/
			goto no_exp;
		}

		while (p < end && XP_AWK_ISDIGIT (awk, *p)) 
		{
			exp = exp * 10 + (*p - XP_T('0'));
			p++;
		}
	}

no_exp:
	if (exp_negative) exp = frac_exp - exp;
	else exp = frac_exp + exp;

	/*
	 * Generate a floating-point number that represents the exponent.
	 * Do this by processing the exponent one bit at a time to combine
	 * many powers of 2 of 10. Then combine the exponent with the
	 * fraction.
	 */
	if (exp < 0) 
	{
		exp_negative = 1;
		exp = -exp;
	} 
	else exp_negative = 0;

	if (exp > MAX_EXPONENT) exp = MAX_EXPONENT;

	dbl_exp = 1.0;

	for (d = powers_of_10; exp != 0; exp >>= 1, d++) 
	{
		if (exp & 01) dbl_exp *= *d;
	}

	if (exp_negative) fraction /= dbl_exp;
	else fraction *= dbl_exp;

done:
	if (endptr != XP_NULL) *endptr = p;
	return (negative)? -fraction: fraction;
}

xp_size_t xp_awk_longtostr (
	xp_long_t value, int radix, const xp_char_t* prefix, 
	xp_char_t* buf, xp_size_t size)
{
	xp_long_t t, rem;
	xp_size_t len, ret, i;
	xp_size_t prefix_len;

	prefix_len = (prefix != XP_NULL)? xp_awk_strlen(prefix): 0;

	t = value;
	if (t == 0)
	{
		/* zero */
		if (buf == XP_NULL) return prefix_len + 1;

		if (size < prefix_len+1) 
		{
			/* buffer too small */
			return (xp_size_t)-1;
		}

		for (i = 0; i < prefix_len; i++) buf[i] = prefix[i];
		buf[prefix_len] = XP_T('0');
		if (size > prefix_len+1) buf[prefix_len+1] = XP_T('\0');
		return 1;
	}

	/* non-zero values */
	len = prefix_len;
	if (t < 0) { t = -t; len++; }
	while (t > 0) { len++; t /= radix; }

	if (buf == XP_NULL)
	{
		/* if buf is not given, return the number of bytes required */
		return len;
	}

	if (size < len) return (xp_size_t)-1; /* buffer too small */
	if (size > len) buf[len] = XP_T('\0');
	ret = len;

	t = value;
	if (t < 0) t = -t;

	while (t > 0) 
	{
		rem = t % radix;
		if (rem >= 10)
			buf[--len] = (xp_char_t)rem + XP_T('a') - 10;
		else
			buf[--len] = (xp_char_t)rem + XP_T('0');
		t /= radix;
	}

	if (value < 0) 
	{
		for (i = 1; i <= prefix_len; i++) 
		{
			buf[i] = prefix[i-1];
			len--;
		}
		buf[--len] = XP_T('-');
	}
	else
	{
		for (i = 0; i < prefix_len; i++) buf[i] = prefix[i];
	}

	return ret;
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
	xp_char_t c1, c2;
	const xp_char_t* end1 = s1 + len1;
	const xp_char_t* end2 = s2 + len2;

	while (s1 < end1)
	{
		c1 = *s1;
		if (s2 < end2) 
		{
			c2 = *s2;
			if (c1 > c2) return 1;
			if (c1 < c2) return -1;
		}
		else return 1;
		s1++; s2++;
	}

	return (s2 < end2)? -1: 0;
}

int xp_awk_strxncasecmp (
	xp_awk_t* awk,
	const xp_char_t* s1, xp_size_t len1, 
	const xp_char_t* s2, xp_size_t len2)
{
	xp_char_t c1, c2;
	const xp_char_t* end1 = s1 + len1;
	const xp_char_t* end2 = s2 + len2;

	while (s1 < end1)
	{
		c1 = XP_AWK_TOUPPER (awk, *s1); 
		if (s2 < end2) 
		{
			c2 = XP_AWK_TOUPPER (awk, *s2);
			if (c1 > c2) return 1;
			if (c1 < c2) return -1;
		}
		else return 1;
		s1++; s2++;
	}

	return (s2 < end2)? -1: 0;
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
	xp_awk_run_t* run, const xp_char_t* s, 
	const xp_char_t* delim, xp_char_t** tok, xp_size_t* tok_len)
{
	return xp_awk_strxntok (
		run, s, xp_awk_strlen(s), 
		delim, xp_awk_strlen(delim), tok, tok_len);
}

xp_char_t* xp_awk_strxtok (
	xp_awk_run_t* run, const xp_char_t* s, xp_size_t len,
	const xp_char_t* delim, xp_char_t** tok, xp_size_t* tok_len)
{
	return xp_awk_strxntok (
		run, s, len, 
		delim, xp_awk_strlen(delim), tok, tok_len);
}

xp_char_t* xp_awk_strntok (
	xp_awk_run_t* run, const xp_char_t* s, 
	const xp_char_t* delim, xp_size_t delim_len,
	xp_char_t** tok, xp_size_t* tok_len)
{
	return xp_awk_strxntok (
		run, s, xp_awk_strlen(s), 
		delim, delim_len, tok, tok_len);
}

xp_char_t* xp_awk_strxntok (
	xp_awk_run_t* run, const xp_char_t* s, xp_size_t len,
	const xp_char_t* delim, xp_size_t delim_len, 
	xp_char_t** tok, xp_size_t* tok_len)
{
	const xp_char_t* p = s, *d;
	const xp_char_t* end = s + len;	
	const xp_char_t* sp = XP_NULL, * ep = XP_NULL;
	const xp_char_t* delim_end = delim + delim_len;
	xp_char_t c; 
	int delim_mode;

#define __DELIM_NULL      0
#define __DELIM_EMPTY     1
#define __DELIM_SPACES    2
#define __DELIM_NOSPACES  3
#define __DELIM_COMPOSITE 4
	if (delim == XP_NULL) delim_mode = __DELIM_NULL;
	else 
	{
		delim_mode = __DELIM_EMPTY;

		for (d = delim; d < delim_end; d++) 
		{
			if (XP_AWK_ISSPACE(run->awk,*d)) 
			{
				if (delim_mode == __DELIM_EMPTY)
					delim_mode = __DELIM_SPACES;
				else if (delim_mode == __DELIM_NOSPACES)
				{
					delim_mode = __DELIM_COMPOSITE;
					break;
				}
			}
			else
			{
				if (delim_mode == __DELIM_EMPTY)
					delim_mode = __DELIM_NOSPACES;
				else if (delim_mode == __DELIM_SPACES)
				{
					delim_mode = __DELIM_COMPOSITE;
					break;
				}
			}
		}
	}		
	
	if (delim_mode == __DELIM_NULL) 
	{ 
		/* when XP_NULL is given as "delim", it trims off the 
		 * leading and trailing spaces characters off the source
		 * string "s" eventually. */

		while (p < end && XP_AWK_ISSPACE(run->awk,*p)) p++;
		while (p < end) 
		{
			c = *p;

			if (!XP_AWK_ISSPACE(run->awk,c)) 
			{
				if (sp == XP_NULL) sp = p;
				ep = p;
			}
			p++;
		}
	}
	else if (delim_mode == __DELIM_EMPTY)
	{
		/* each character in the source string "s" becomes a token. */
		if (p < end)
		{
			c = *p;
			sp = p;
			ep = p++;
		}
	}
	else if (delim_mode == __DELIM_SPACES) 
	{
		/* each token is delimited by space characters. all leading
		 * and trailing spaces are removed. */

		while (p < end && XP_AWK_ISSPACE(run->awk,*p)) p++;
		while (p < end) 
		{
			c = *p;
			if (XP_AWK_ISSPACE(run->awk,c)) break;
			if (sp == XP_NULL) sp = p;
			ep = p++;
		}
		while (p < end && XP_AWK_ISSPACE(run->awk,*p)) p++;
	}
	else if (delim_mode == __DELIM_NOSPACES)
	{
		/* each token is delimited by one of charaters 
		 * in the delimeter set "delim". */
		if (run->global.ignorecase)
		{
			while (p < end) 
			{
				c = XP_AWK_TOUPPER(run->awk, *p);
				for (d = delim; d < delim_end; d++) 
				{
					if (c == XP_AWK_TOUPPER(run->awk,*d)) goto exit_loop;
				}

				if (sp == XP_NULL) sp = p;
				ep = p++;
			}
		}
		else
		{
			while (p < end) 
			{
				c = *p;
				for (d = delim; d < delim_end; d++) 
				{
					if (c == *d) goto exit_loop;
				}

				if (sp == XP_NULL) sp = p;
				ep = p++;
			}
		}
	}
	else /* if (delim_mode == __DELIM_COMPOSITE) */ 
	{
		/* each token is delimited by one of non-space charaters
		 * in the delimeter set "delim". however, all space characters
		 * surrounding the token are removed */
		while (p < end && XP_AWK_ISSPACE(run->awk,*p)) p++;
		if (run->global.ignorecase)
		{
			while (p < end) 
			{
				c = XP_AWK_TOUPPER(run->awk, *p);
				if (XP_AWK_ISSPACE(run->awk,c)) 
				{
					p++;
					continue;
				}
				for (d = delim; d < delim_end; d++) 
				{
					if (c == XP_AWK_TOUPPER(run->awk,*d)) goto exit_loop;
				}
				if (sp == XP_NULL) sp = p;
				ep = p++;
			}
		}
		else
		{
			while (p < end) 
			{
				c = *p;
				if (XP_AWK_ISSPACE(run->awk,c)) 
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

	/* if XP_NULL is returned, this function should not be called anymore */
	if (p >= end) return XP_NULL;
	if (delim_mode == __DELIM_EMPTY || 
	    delim_mode == __DELIM_SPACES) return (xp_char_t*)p;
	return (xp_char_t*)++p;
}

xp_char_t* xp_awk_strxntokbyrex (
	xp_awk_run_t* run, const xp_char_t* s, xp_size_t len,
	void* rex, xp_char_t** tok, xp_size_t* tok_len, int* errnum)
{
	int n;
	xp_char_t* match_ptr;
	xp_size_t match_len, i;
	xp_size_t left = len;
	const xp_char_t* ptr = s;
	const xp_char_t* str_ptr = s;
	xp_size_t str_len = len;

	while (len > 0)
	{
		n = xp_awk_matchrex (
			run->awk, rex, 
			((run->global.ignorecase)? XP_AWK_REX_IGNORECASE: 0),
			ptr, left, (const xp_char_t**)&match_ptr, &match_len, 
			errnum);
		if (n == -1) return XP_NULL;
		if (n == 0)
		{
			/* no match has been found. 
			 * return the entire string as a token */
			*tok = (xp_char_t*)str_ptr;
			*tok_len = str_len;
			*errnum = XP_AWK_ENOERR;
			return XP_NULL; 
		}

		xp_awk_assert (run->awk, n == 1);

		if (match_len == 0)
		{
			ptr++;
			left--;
		}
		else if (run->awk->option & XP_AWK_STRIPSPACES)
		{
			/* match at the beginning of the input string */
			if (match_ptr == s) 
			{
				for (i = 0; i < match_len; i++)
				{
					if (!XP_AWK_ISSPACE(run->awk, match_ptr[i]))
						goto exit_loop;
				}

				/* the match that are all spaces at the 
				 * beginning of the input string is skipped */
				ptr += match_len;
				left -= match_len;
				str_ptr = s + match_len;
				str_len -= match_len;
			}
			else  break;
		}
		else break;
	}

exit_loop:
	if (len == 0)
	{
		*tok = (xp_char_t*)str_ptr;
		*tok_len = str_len;
		*errnum = XP_AWK_ENOERR;
		return XP_NULL; 
	}

	*tok = (xp_char_t*)str_ptr;
	*tok_len = match_ptr - str_ptr;

	for (i = 0; i < match_len; i++)
	{
		if (!XP_AWK_ISSPACE(run->awk, match_ptr[i]))
		{
			*errnum = XP_AWK_ENOERR;
			return match_ptr+match_len;
		}
	}

	*errnum = XP_AWK_ENOERR;

	if (run->awk->option & XP_AWK_STRIPSPACES)
	{
		return (match_ptr+match_len >= s+len)? 
			XP_NULL: (match_ptr+match_len);
	}
	else
	{
		return (match_ptr+match_len > s+len)? 
			XP_NULL: (match_ptr+match_len);
	}
}

int xp_awk_abort (xp_awk_t* awk, 
	const xp_char_t* expr, const xp_char_t* file, int line)
{
	awk->syscas.dprintf (
		XP_T("ASSERTION FAILURE AT FILE %s, LINE %d\n%s\n"),
		file, line, expr);
	awk->syscas.abort ();
	return 0;
}
