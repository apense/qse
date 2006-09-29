/*
 * $Id: extio.h,v 1.13 2006-09-29 11:18:13 bacon Exp $
 */

#ifndef _XP_AWK_EXTIO_H_
#define _XP_AWK_EXTIO_H_

#ifndef _XP_AWK_AWK_H_
#error Never include this file directly. Include <xp/awk/awk.h> instead
#endif

#ifdef __cplusplus
extern "C"
#endif

int xp_awk_readextio (
	xp_awk_run_t* run, int in_type, 
	const xp_char_t* name, xp_awk_str_t* buf);

int xp_awk_writeextio_val (
	xp_awk_run_t* run, int out_type, 
	const xp_char_t* name, xp_awk_val_t* v);

int xp_awk_writeextio_str (
	xp_awk_run_t* run, int out_type, 
	const xp_char_t* name, xp_char_t* str, xp_size_t len);

int xp_awk_flushextio (
	xp_awk_run_t* run, int out_type, const xp_char_t* name);

int xp_awk_nextextio_read (
	xp_awk_run_t* run, int in_type, const xp_char_t* name);

/* TODO:
int xp_awk_nextextio_write (
	xp_awk_run_t* run, int out_type, const xp_char_t* name);
*/

int xp_awk_closeextio_read (
	xp_awk_run_t* run, int in_type, const xp_char_t* name);
int xp_awk_closeextio_write (
	xp_awk_run_t* run, int out_type, const xp_char_t* name);
int xp_awk_closeextio (xp_awk_run_t* run, const xp_char_t* name);

void xp_awk_clearextio (xp_awk_run_t* run);

#ifdef __cplusplus
}
#endif

#endif
