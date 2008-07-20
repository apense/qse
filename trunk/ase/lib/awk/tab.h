/*
 * $Id: tab.h 115 2008-03-03 11:13:15Z baconevi $
 *
 * {License}
 */

#ifndef _ASE_AWK_TAB_H_
#define _ASE_AWK_TAB_H_

#ifndef _ASE_AWK_AWK_H_
#error Never include this file directly. Include <ase/awk/awk.h> instead
#endif

/* TODO: you have to turn this into a hash table.
	 as of now, this is an arrayed table. */

typedef struct ase_awk_tab_t ase_awk_tab_t;

struct ase_awk_tab_t
{
	struct
	{
		struct
		{
			ase_char_t* ptr;
			ase_size_t  len;
		} name;
	}* buf;
	ase_size_t size;
	ase_size_t capa;
	ase_awk_t* awk;
	ase_bool_t __dynamic;	
};

#ifdef __cplusplus
extern "C" {
#endif

ase_awk_tab_t* ase_awk_tab_open (ase_awk_tab_t* tab, ase_awk_t* awk);
void ase_awk_tab_close (ase_awk_tab_t* tab);

ase_size_t ase_awk_tab_getsize (ase_awk_tab_t* tab);
ase_size_t ase_awk_tab_getcapa (ase_awk_tab_t* tab);
ase_awk_tab_t* ase_awk_tab_setcapa (ase_awk_tab_t* tab, ase_size_t capa);

void ase_awk_tab_clear (ase_awk_tab_t* tab);

ase_size_t ase_awk_tab_insert (
	ase_awk_tab_t* tab, ase_size_t index, 
	const ase_char_t* str, ase_size_t len);

ase_size_t ase_awk_tab_remove (
	ase_awk_tab_t* tab, ase_size_t index, ase_size_t count);

ase_size_t ase_awk_tab_add (
	ase_awk_tab_t* tab, const ase_char_t* str, ase_size_t len);
ase_size_t ase_awk_tab_adduniq (
	ase_awk_tab_t* tab, const ase_char_t* str, ase_size_t len);

ase_size_t ase_awk_tab_find (
	ase_awk_tab_t* tab, ase_size_t index,
	const ase_char_t* str, ase_size_t len);
ase_size_t ase_awk_tab_rfind (
	ase_awk_tab_t* tab, ase_size_t index,
	const ase_char_t* str, ase_size_t len);
ase_size_t ase_awk_tab_rrfind (
	ase_awk_tab_t* tab, ase_size_t index,
	const ase_char_t* str, ase_size_t len);

ase_size_t ase_awk_tab_findx (
	ase_awk_tab_t* tab, ase_size_t index,
	const ase_char_t* str, ase_size_t len, 
	void(*transform)(ase_size_t, ase_cstr_t*,void*), void* arg);
ase_size_t ase_awk_tab_rfindx (
	ase_awk_tab_t* tab, ase_size_t index,
	const ase_char_t* str, ase_size_t len, 
	void(*transform)(ase_size_t, ase_cstr_t*,void*), void* arg);
ase_size_t ase_awk_tab_rrfindx (
	ase_awk_tab_t* tab, ase_size_t index,
	const ase_char_t* str, ase_size_t len, 
	void(*transform)(ase_size_t, ase_cstr_t*,void*), void* arg);

#ifdef __cplusplus
}
#endif

#endif