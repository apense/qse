/*
 * $Id$
 *
 * {License}
 */

#include <ase/cmn/sll.h>
#include "mem.h"

#define sll_t    ase_sll_t
#define node_t   ase_sll_node_t
#define copier_t ase_sll_copier_t
#define freeer_t ase_sll_freeer_t
#define comper_t ase_sll_comper_t
#define walker_t ase_sll_walker_t

#define HEAD(s) ASE_SLL_HEAD(s)
#define TAIL(s) ASE_SLL_TAIL(s)
#define SIZE(s) ASE_SLL_SIZE(s)

#define DPTR(n) ASE_SLL_DPTR(n)
#define DLEN(n) ASE_SLL_DLEN(n)
#define NEXT(n) ASE_SLL_NEXT(n)

#define TOB(sll,len) ((len)*(sll)->scale)

#define SIZEOF(x) ASE_SIZEOF(x)
#define size_t    ase_size_t
#define mmgr_t    ase_mmgr_t

static int comp_data (sll_t* sll, 
	const void* dptr1, size_t dlen1, 
	const void* dptr2, size_t dlen2)
{
	if (dlen1 == dlen2) return ASE_MEMCMP (dptr1, dptr2, TOB(sll,dlen1));
	/* it just returns 1 to indicate that they are different. */
	return 1;
}

static node_t* alloc_node (sll_t* sll, void* dptr, size_t dlen)
{
	node_t* n;

	if (sll->copier == ASE_SLL_COPIER_SIMPLE)
	{
		n = ASE_MMGR_ALLOC (sll->mmgr, SIZEOF(node_t));
		if (n == ASE_NULL) return ASE_NULL;
		DPTR(n) = dptr;
	}
	else if (sll->copier == ASE_SLL_COPIER_INLINE)
	{
		n = ASE_MMGR_ALLOC (sll->mmgr, 
			SIZEOF(node_t) + TOB(sll,dlen));
		if (n == ASE_NULL) return ASE_NULL;

		ASE_MEMCPY (n + 1, dptr, TOB(sll,dlen));
		DPTR(n) = n + 1;
	}
	else
	{
		n = ASE_MMGR_ALLOC (sll->mmgr, SIZEOF(node_t));
		if (n == ASE_NULL) return ASE_NULL;
		DPTR(n) = sll->copier (sll, dptr, dlen);
		if (DPTR(n) == ASE_NULL) 
		{
			ASE_MMGR_FREE (sll->mmgr, n);
			return ASE_NULL;
		}
	}

	DLEN(n) = dlen; 
	NEXT(n) = ASE_NULL;	

	return n;
}

sll_t* ase_sll_open (mmgr_t* mmgr, size_t ext)
{
	sll_t* sll;

	if (mmgr == ASE_NULL) 
	{
		mmgr = ASE_MMGR_GETDFL();

		ASE_ASSERTX (mmgr != ASE_NULL,
			"Set the memory manager with ASE_MMGR_SETDFL()");

		if (mmgr == ASE_NULL) return ASE_NULL;
	}

	sll = ASE_MMGR_ALLOC (mmgr, SIZEOF(sll_t) + ext);
	if (sll == ASE_NULL) return ASE_NULL;

	if (ase_sll_init (sll, mmgr) == ASE_NULL)
	{
		ASE_MMGR_FREE (mmgr, sll);
		return ASE_NULL;
	}

	return sll;
}

void ase_sll_close (sll_t* sll)
{
	ase_sll_fini (sll);
	ASE_MMGR_FREE (sll->mmgr, sll);
}

sll_t* ase_sll_init (sll_t* sll, mmgr_t* mmgr)
{
	/* do not zero out the extension */
	ASE_MEMSET (sll, 0, SIZEOF(*sll));

	sll->mmgr = mmgr;
	sll->size = 0;
	sll->scale = 1;

	sll->comper = comp_data;
	sll->copier = ASE_SLL_COPIER_SIMPLE;
	return sll;
}

void ase_sll_fini (sll_t* sll)
{
	ase_sll_clear (sll);
}

void* ase_sll_getextension (sll_t* sll)
{
	return sll + 1;
}

mmgr_t* ase_sll_getmmgr (sll_t* sll)
{
	return sll->mmgr;
}

void ase_sll_setmmgr (sll_t* sll, mmgr_t* mmgr)
{
	sll->mmgr = mmgr;
}

int ase_sll_getscale (sll_t* sll)
{
	return sll->scale;
}

void ase_sll_setscale (sll_t* sll, int scale)
{
	ASE_ASSERTX (scale > 0 && scale <= ASE_TYPE_MAX(ase_byte_t), 
		"The scale should be larger than 0 and less than or equal to the maximum value that the ase_byte_t type can hold");

	if (scale <= 0) scale = 1;
	if (scale > ASE_TYPE_MAX(ase_byte_t)) scale = ASE_TYPE_MAX(ase_byte_t);

	sll->scale = scale;
}

copier_t ase_sll_getcopier (sll_t* sll)
{
	return sll->copier;
}

void ase_sll_setcopier (sll_t* sll, copier_t copier)
{
	if (copier == ASE_NULL) copier = ASE_SLL_COPIER_SIMPLE;
	sll->copier = copier;
}

freeer_t ase_sll_getfreeer (sll_t* sll)
{
	return sll->freeer;
}

void ase_sll_setfreeer (sll_t* sll, freeer_t freeer)
{
	sll->freeer = freeer;
}

comper_t ase_sll_getcomper (sll_t* sll)
{
	return sll->comper;
}

void ase_sll_setcomper (sll_t* sll, comper_t comper)
{
	if (comper == ASE_NULL) comper = comp_data;
	sll->comper = comper;
}

size_t ase_sll_getsize (sll_t* sll)
{
	return SIZE(sll);
}

node_t* ase_sll_gethead (sll_t* sll)
{
	return HEAD(sll);
}

node_t* ase_sll_gettail (sll_t* sll)
{
	return TAIL(sll);
}

node_t* ase_sll_search (sll_t* sll, node_t* pos, const void* dptr, size_t dlen)
{
	pos = (pos == ASE_NULL)? pos = sll->head: NEXT(pos);

	while (pos != ASE_NULL)
	{
		if (sll->comper (sll, DPTR(pos), DLEN(pos), dptr, dlen) == 0)
		{
			return pos;
		}
		pos = NEXT(pos);
	}	

	return ASE_NULL;
}

node_t* ase_sll_insert (
	sll_t* sll, node_t* pos, void* dptr, size_t dlen)
{
	node_t* n = alloc_node (sll, dptr, dlen);
	if (n == ASE_NULL) return ASE_NULL;

	if (pos == ASE_NULL)
	{
		/* insert at the end */
		if (HEAD(sll) == ASE_NULL)
		{
			ASE_ASSERT (TAIL(sll) == ASE_NULL);
			HEAD(sll) = n;
		}
		else NEXT(TAIL(sll)) = n;

		TAIL(sll) = n;
	}
	else
	{
		/* insert in front of the positional node */
		NEXT(n) = pos;
		if (pos == HEAD(sll)) HEAD(sll) = n;
		else
		{
			/* take note of performance penalty */
			node_t* n2 = HEAD(sll);
			while (NEXT(n2) != pos) n2 = NEXT(n2);
			NEXT(n2) = n;
		}
	}

	SIZE(sll)++;
	return n;
}

void ase_sll_delete (sll_t* sll, node_t* pos)
{
	if (pos == ASE_NULL) return; /* not a valid node */

	if (pos == HEAD(sll))
	{
		/* it is simple to delete the head node */
		HEAD(sll) = NEXT(pos);
		if (HEAD(sll) == ASE_NULL) TAIL(sll) = ASE_NULL;
	}
	else 
	{
		/* but deletion of other nodes has significant performance
		 * penalty as it has look for the predecessor of the 
		 * target node */
		node_t* n2 = HEAD(sll);
		while (NEXT(n2) != pos) n2 = NEXT(n2);

		NEXT(n2) = NEXT(pos);

		/* update the tail node if necessary */
		if (pos == TAIL(sll)) TAIL(sll) = n2;
	}

	if (sll->freeer != ASE_NULL)
	{
		/* free the actual data */
		sll->freeer (sll, DPTR(pos), DLEN(pos));
	}

	/* free the node */
	ASE_MMGR_FREE (sll->mmgr, pos);

	/* decrement the number of elements */
	SIZE(sll)--;
}

void ase_sll_clear (sll_t* sll)
{
	while (HEAD(sll) != ASE_NULL) ase_sll_delete (sll, HEAD(sll));
	ASE_ASSERT (TAIL(sll) == ASE_NULL);
}

node_t* ase_sll_pushhead (sll_t* sll, void* data, size_t size)
{
	return ase_sll_insert (sll, HEAD(sll), data, size);
}

node_t* ase_sll_pushtail (sll_t* sll, void* data, size_t size)
{
	return ase_sll_insert (sll, ASE_NULL, data, size);
}

void ase_sll_pophead (sll_t* sll)
{
	ase_sll_delete (sll, HEAD(sll));
}

void ase_sll_poptail (sll_t* sll)
{
	ase_sll_delete (sll, TAIL(sll));
}

void ase_sll_walk (sll_t* sll, walker_t walker, void* arg)
{
	node_t* n = HEAD(sll);

	while (n != ASE_NULL)
	{
		if (walker(sll,n,arg) == ASE_SLL_WALK_STOP) return;
		n = NEXT(n);
	}
}

void* ase_sll_copysimple (sll_t* sll, void* dptr, size_t dlen)
{
	return dptr;
}

void* ase_sll_copyinline (sll_t* sll, void* dptr, size_t dlen)
{
	/* this is a dummy copier */
	return ASE_NULL;
}