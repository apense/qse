/*
 * $Id: misc.c 116 2008-03-03 11:15:37Z baconevi $
 *
 * {License}
 */

#if defined(__BORLANDC__)
#pragma hdrstop
#define Library
#endif

#include <ase/cmn/types.h>
#include <ase/cmn/macros.h>

#ifndef NDEBUG
int ase_assert_failed (
	const ase_char_t* expr, const ase_char_t* desc, 
	const ase_char_t* file, ase_size_t line)
{
	if (desc == ASE_NULL)
	{
		ase_assert_printf (
			ASE_T("ASSERTION FAILURE AT FILE %s LINE %lu\n%s\n"),
			file, (unsigned long)line, expr);
	}
	else
	{
		ase_assert_printf (
			ASE_T("ASSERTION FAILURE AT FILE %s LINE %lu\n%s\n\nDESCRIPTION:\n%s\n"),
			file, (unsigned long)line, expr, desc);

	}

	ase_assert_abort ();
	return 0;
}

#endif

