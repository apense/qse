#include <xp/stx/memory.h>
#include <xp/bas/stdio.h>

int xp_main ()
{
	xp_stx_t stx;
	xp_stx_word_t i;

	if (xp_stx_open (&stx, 10) == XP_NULL) {
		xp_printf (XP_TEXT("cannot open memory\n"));
		return -1;
	}

	stx.nil = xp_stx_memory_alloc(&stx.memory, 0);
	stx.true = xp_stx_memory_alloc(&stx.memory, 0);
	stx.false = xp_stx_memory_alloc(&stx.memory, 0);

	for (i = 0; i < 20; i++) {
		xp_printf (XP_TEXT("%d, %d\n"), 
			i, xp_stx_memory_alloc(&stx.memory, 100));
	}

	for (i = 0; i < 5; i++) {
		xp_stx_memory_dealloc (&stx.memory, i);
	}

	for (i = 0; i < 20; i++) {
		xp_printf (XP_TEXT("%d, %d\n"), 
			i, xp_stx_memory_alloc(&stx.memory, 100));
	}

	xp_stx_close (&stx);
	xp_printf (XP_TEXT("End of program\n"));
	return 0;
}

