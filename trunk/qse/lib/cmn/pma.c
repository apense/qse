/*
 * $Id$
 *
    Copyright 2006-2011 Chung, Hyung-Hwan.
    This file is part of QSE.

    QSE is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as
    published by the Free Software Foundation, either version 3 of
    the License, or (at your option) any later version.

    QSE is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with QSE. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 tre-mem.c - TRE memory allocator

 This software is released under a BSD-style license.
 See the file LICENSE for details and copyright.
 */

/*
 This memory allocator is for allocating small memory blocks efficiently
 in terms of memory overhead and execution speed.  The allocated blocks
 cannot be freed individually, only all at once.  There can be multiple
 allocators, though.
 */

#include <qse/cmn/pma.h>
#include "mem.h"

#define ALIGN(x,size) ((((x) + (size) - 1) / (size)) * (size))

QSE_IMPLEMENT_COMMON_FUNCTIONS (pma)

qse_pma_t* qse_pma_init (qse_pma_t* pma, qse_mmgr_t* mmgr)
{
	if (mmgr == QSE_NULL) mmgr = QSE_MMGR_GETDFL();

	QSE_MEMSET (pma, 0, QSE_SIZEOF(*pma));
	pma->mmgr = mmgr;

	return pma;	
}

/* Frees the memory allocator and all memory allocated with it. */
void qse_pma_fini (qse_pma_t* pma)
{
	qse_pma_blk_t* tmp, * l = pma->blocks;

	while (l != QSE_NULL)
	{
		QSE_MMGR_FREE (pma->mmgr, l->data);
		tmp = l->next;
		QSE_MMGR_FREE (pma->mmgr, l);
		l = tmp;
	}
}

/* Returns a new memory allocator or NULL if out of memory. */
qse_pma_t* qse_pma_open (qse_mmgr_t* mmgr, qse_size_t xtnsize) 
{
	qse_pma_t* pma;

	pma = (qse_pma_t*)QSE_MMGR_ALLOC (mmgr, QSE_SIZEOF(*pma) + xtnsize);
	if (pma == QSE_NULL) return QSE_NULL;

	if (qse_pma_init (pma, mmgr) == QSE_NULL)
	{
		QSE_MMGR_FREE (mmgr, pma);
		return QSE_NULL;
	}

	return pma;
}

void qse_pma_close (qse_pma_t* pma)
{
	qse_pma_fini (pma);
	QSE_MMGR_FREE (pma->mmgr, pma);
}

/* Allocates a block of `size' bytes from `mem'.  Returns a pointer to the
 allocated block or NULL if an underlying malloc() failed. */
void* qse_pma_alloc (qse_pma_t* pma, qse_size_t size)
{
	void *ptr;

	if (pma->failed) return QSE_NULL;

	if (pma->n < size)
	{
		/* We need more memory than is available in the current block.
		 Allocate a new block. */

		qse_pma_blk_t* l;
		int block_size;
		if (size * 8 > QSE_PMA_BLOCK_SIZE)
			block_size = size * 8;
		else
			block_size = QSE_PMA_BLOCK_SIZE;

		l = QSE_MMGR_ALLOC (pma->mmgr, QSE_SIZEOF(*l));
		if (l == QSE_NULL)
		{
			pma->failed = 1;
			return QSE_NULL;
		}
		l->data = QSE_MMGR_ALLOC (pma->mmgr, block_size);
		if (l->data == QSE_NULL)
		{
			QSE_MMGR_FREE (pma->mmgr, l);
			pma->failed = 1;
			return QSE_NULL;
		}
		l->next = QSE_NULL;
		if (pma->current != QSE_NULL) pma->current->next = l;
		if (pma->blocks == QSE_NULL) pma->blocks = l;
		pma->current = l;
		pma->ptr = l->data;
		pma->n = block_size;
	}

	/* Make sure the next pointer will be aligned. */
	size += ALIGN((long)(pma->ptr + size), QSE_SIZEOF(long));

	/* Allocate from current block. */
	ptr = pma->ptr;
	pma->ptr += size;
	pma->n -= size;

	return ptr;
}

void* qse_pma_realloc (qse_pma_t* pma, void* blk, qse_size_t size)
{
	/* do nothing. you can't resize an individual memory chunk */
	return QSE_NULL;
}

void qse_pma_free (qse_pma_t* pma, void* blk)
{
	/* do nothing. you can't free an individual memory chunk */
}

