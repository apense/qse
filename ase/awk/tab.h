/*
 * $Id: tab.h,v 1.1 2006-01-29 18:28:14 bacon Exp $
 */

#ifndef _XP_AWK_TAB_H_
#define _XP_AWK_TAB_H_

#ifdef __STAND_ALONE
#include <xp/awk/sa.h>
#else
#include <xp/types.h>
#include <xp/macros.h>
#endif

// TODO: you have to turn this into a hash table.
//	 as of now, this is an arrayed table.

typedef struct xp_awk_tab_t xp_awk_tab_t;

struct xp_awk_tab_t
{
	xp_char_t** buf;
	xp_size_t size;
	xp_size_t capa;
	xp_bool_t __dynamic;	
};

#ifdef __cplusplus
extern "C" {
#endif

xp_awk_tab_t* xp_awk_tab_open (xp_awk_tab_t* tab);
void xp_awk_tab_close (xp_awk_tab_t* tab);

xp_size_t xp_awk_tab_getsize (xp_awk_tab_t* tab);
xp_size_t xp_awk_tab_getcapa (xp_awk_tab_t* tab);
xp_awk_tab_t* xp_awk_tab_setcapa (xp_awk_tab_t* tab, xp_size_t capa);

void xp_awk_tab_clear (xp_awk_tab_t* tab);

xp_size_t xp_awk_tab_insdatum (
	xp_awk_tab_t* tab, xp_size_t index, const xp_char_t* value);
xp_size_t xp_awk_tab_remrange (
	xp_awk_tab_t* tab, xp_size_t index, xp_size_t count);

xp_size_t xp_awk_tab_adddatum (xp_awk_tab_t* tab, const xp_char_t* value);
xp_size_t xp_awk_tab_find (
	xp_awk_tab_t* tab, const xp_char_t* value, xp_size_t index);

#ifdef __cplusplus
}
#endif

#endif
