/*
 * $Id: val.h,v 1.11 2006-03-28 11:32:58 bacon Exp $
 */

#ifndef _XP_AWK_VAL_H_
#define _XP_AWK_VAL_H_

#ifndef _XP_AWK_AWK_H_
#error Never include this file directly. Include <xp/awk/awk.h> instead
#endif

enum
{
	XP_AWK_VAL_NIL  = 0,
	XP_AWK_VAL_INT  = 1,
	XP_AWK_VAL_REAL = 2,
	XP_AWK_VAL_STR  = 3,
	XP_AWK_VAL_MAP  = 4
};

typedef struct xp_awk_val_t      xp_awk_val_t;
typedef struct xp_awk_val_nil_t  xp_awk_val_nil_t;
typedef struct xp_awk_val_int_t  xp_awk_val_int_t;
typedef struct xp_awk_val_real_t xp_awk_val_real_t;
typedef struct xp_awk_val_str_t  xp_awk_val_str_t;
typedef struct xp_awk_val_map_t  xp_awk_val_map_t;

#define XP_AWK_VAL_HDR \
	int type: 3; \
	int ref: 29

struct xp_awk_val_t
{
	XP_AWK_VAL_HDR;	
};

/* XP_AWK_VAL_NIL */
struct xp_awk_val_nil_t
{
	XP_AWK_VAL_HDR;
};

/* XP_AWK_VAL_INT */
struct xp_awk_val_int_t
{
	XP_AWK_VAL_HDR;
	xp_long_t val;
};

/* XP_AWK_VAL_REAL */
struct xp_awk_val_real_t
{
	XP_AWK_VAL_HDR;
	xp_real_t val;
};

/* XP_AWK_VAL_STR */
struct xp_awk_val_str_t
{
	XP_AWK_VAL_HDR;
	xp_char_t* buf;
	xp_size_t  len;
};

/* XP_AWK_VAL_MAP */
struct xp_awk_val_map_t
{
	XP_AWK_VAL_HDR;
	//xp_awk_vap_t vap;
};

#ifdef __cplusplus
extern "C" {
#endif

extern xp_awk_val_t* xp_awk_val_nil;

xp_awk_val_t* xp_awk_makeintval (xp_awk_t* awk, xp_long_t v);
xp_awk_val_t* xp_awk_makestrval (const xp_char_t* str, xp_size_t len);
//xp_awk_val_t* xp_awk_makemapval ();

xp_bool_t xp_awk_isbuiltinval (xp_awk_val_t* val);
void xp_awk_freeval (xp_awk_t* awk, xp_awk_val_t* val);
void xp_awk_refupval (xp_awk_val_t* val);
void xp_awk_refdownval (xp_awk_t* awk, xp_awk_val_t* val);
void xp_awk_refdownval_nofree (xp_awk_t* awk, xp_awk_val_t* val);

xp_awk_val_t* xp_awk_cloneval (xp_awk_t* awk, xp_awk_val_t* val);
xp_bool_t xp_awk_isvaltrue (xp_awk_val_t* val);
void xp_awk_printval (xp_awk_val_t* val);

#ifdef __cplusplus
}
#endif

#endif
