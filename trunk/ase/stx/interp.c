/*
 * $Id: interp.c,v 1.7 2005-09-11 13:17:35 bacon Exp $
 */

#include <xp/stx/interp.h>
#include <xp/stx/method.h>
#include <xp/stx/object.h>
#include <xp/stx/array.h>
#include <xp/bas/assert.h>

#define XP_STX_CONTEXT_SIZE      5
#define XP_STX_CONTEXT_STACK     0
#define XP_STX_CONTEXT_STACK_TOP 1
#define XP_STX_CONTEXT_RECEIVER  2
#define XP_STX_CONTEXT_PC        3
#define XP_STX_CONTEXT_METHOD    4

struct xp_stx_context_t
{
	xp_stx_objhdr_t header;
	xp_word_t stack;
	xp_word_t stack_top;
	xp_word_t receiver;
	xp_word_t pc;
	xp_word_t method;
};

typedef struct xp_stx_context_t xp_stx_context_t;

/* data structure for internal vm operation */
struct vmcontext_t
{
	/* from context */
	xp_word_t* stack;
	xp_word_t  stack_size;
	xp_word_t  stack_top;
	xp_word_t  receiver;
	xp_word_t  pc;

	/* from method */
	xp_byte_t* bytecodes;
	xp_word_t  bytecode_size;
	xp_word_t* literals;
};

typedef struct vmcontext_t vmcontext_t;


xp_word_t xp_stx_new_context (xp_stx_t* stx, xp_word_t receiver, xp_word_t method)
{
	xp_word_t context;
	xp_stx_context_t* ctxobj;

	context = xp_stx_alloc_word_object(
		stx, XP_NULL, XP_STX_CONTEXT_SIZE, XP_NULL, 0);
	XP_STX_CLASS(stx,context) = stx->class_context;

	ctxobj = (xp_stx_context_t*)XP_STX_OBJECT(stx,context);
	ctxobj->stack = xp_stx_new_array (stx, 256); /* TODO: initial stack size */
	ctxobj->stack_top = XP_STX_TO_SMALLINT(0);
	ctxobj->receiver = receiver;
	ctxobj->pc = XP_STX_TO_SMALLINT(0);
	ctxobj->method = method;

	return context;
}

/*
static int __push_receiver_variable (
	xp_stx_t* stx, int index, xp_stx_context_t* ctxobj);
static int __push_temporary_variable (
	xp_stx_t* stx, int index, xp_stx_context_t* ctxobj);
*/

int xp_stx_interp (xp_stx_t* stx, xp_word_t context)
{
	xp_stx_context_t* ctxobj;
	xp_stx_method_t* mthobj;
	vmcontext_t vmc;
	int code, next;

	ctxobj = (xp_stx_context_t*)XP_STX_OBJECT(stx,context);
	mthobj = (xp_stx_method_t*)XP_STX_OBJECT(stx,ctxobj->method);

	vmc.stack = XP_STX_DATA(stx,ctxobj->stack);
	vmc.stack_size = XP_STX_SIZE(stx,ctxobj->stack);
	vmc.receiver = ctxobj->receiver;
	vmc.pc = ctxobj->pc;

	vmc.literals = mthobj->literals;
	vmc.bytecodes = XP_STX_DATA(stx, mthobj->bytecodes);
	vmc.bytecode_size = XP_STX_SIZE(stx, mthobj->bytecodes);

	while (vmc.pc < vmc.bytecode_size) {
		code = vmc.bytecodes[vmc.pc++];

		if (code >= 0x00 && code <= 0x3F) {
			/* stack - push */
			int what = code >> 4;
			int index = code & 0x0F;

			switch (what) {
			case 0: /* receiver variable */
				vmc.stack[vmc.stack_top++] = XP_STX_WORD_AT(stx, vmc.receiver, index);
				break;
			case 1: /* temporary variable */
				//vmc.stack[vmc.stack_top++] = XP_STX_WORD_AT(stx, vmc.temporary, index);
				break;
			case 2: /* literal constant */
				break;
			case 3: /* literal variable */
				break;
			}
		}
		else if (code >= 0x40 && code <= 0x5F) {
			/* stack - store */
			int what = code >> 4;
			int index = code & 0x0F;

			switch (what) {
			case 4: /* receiver variable */
				break; 
			case 5: /* temporary location */
				break;
			}
		}

		/* more code .... */

		else if (code >= 0xF0 && code <= 0xFF)  {
			/* primitive */
			next = vmc.bytecodes[vmc.pc++];
			__dispatch_primitive (next);
		}
	}

	return 0;	
}

/*
static int __push_receiver_variable (
	xp_stx_t* stx, int index, xp_stx_context_t* ctxobj)
{
	xp_word_t* stack;
	xp_word_t stack_top;

	xp_assert (XP_STX_IS_WORD_OBJECT(stx, ctxobj->receiver));

	stack_top = XP_STX_FROM_SMALLINT(ctxobj->stack_top);
	stack = XP_STX_DATA(stx, ctxobj->stack);
	stack[stack_top++] = XP_STX_WORD_AT(stx, ctxobj->receiver, index);
	ctxobj->stack_top = XP_STX_TO_SMALLINT(stack_top);

	return 0;
}

static int __push_temporary_variable (
	xp_stx_t* stx, int index, xp_stx_context_t* ctxobj)
{
	xp_word_t* stack;
	xp_word_t stack_top;

	xp_assert (XP_STX_IS_WORD_OBJECT(stx, ctxobj->receiver));

	stack_top = XP_STX_FROM_SMALLINT(ctxobj->stack_top);
	stack = XP_STX_DATA(stx, ctxobj->stack);
	stack[stack_top++] = XP_STX_WORD_AT(stx, ctxobj->receiver, index);
	ctxobj->stack_top = XP_STX_TO_SMALLINT(stack_top);

	return 0;
}
*/

static int __dispatch_primitive (xp_stx_t* stx, int no, xp_stx_context_t* ctxobj)
{
	switch (no) {
	case 0:
			
		break;
	}
	
}
