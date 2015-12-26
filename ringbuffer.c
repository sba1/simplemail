/***************************************************************************
 SimpleMail - Copyright (C) 2000 Hynek Schlawack and Sebastian Bauer

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
***************************************************************************/

/**
 * @file ringbuffer.c
 *
 * A simple ring buffer implementation for items with different sizes.
 */

#include "ringbuffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*****************************************************************************/

struct ringbuffer
{
	/** First physical item in the ring buffer */
	unsigned char *mem;

	/** Space of the ring buffer */
	unsigned int size;

	/** First address outside the the ring buffer (i.e., exclusive) */
	unsigned char *memend;

	/** Item that can be allocated next */
	unsigned char *next_alloc;

	/** Item that can be freed next */
	unsigned char *next_free;

	/** Free callback */
	ringbuffer_free_callback_t free_callback;

	/** User data */
	void *userdata;
};

/*****************************************************************************/

ringbuffer_t ringbuffer_create(size_t size, ringbuffer_free_callback_t free_callback, void *userdata)
{
	ringbuffer_t rb;

	/* Down align the size */
	size &= ~(4-1);
	if (!size) return NULL;

	if (!(rb = malloc(sizeof(*rb))))
		return NULL;

	memset(rb, 0, sizeof(*rb));

	if (!(rb->mem = malloc(size)))
	{
		ringbuffer_dispose(rb);
		return NULL;
	}
	printf("mem@%p\n", rb->mem);
	rb->size = size;
	rb->next_alloc = rb->mem;
	rb->memend = rb->mem + size;
	rb->free_callback = free_callback;
	rb->userdata = userdata;
	return rb;
}

/*****************************************************************************/

void ringbuffer_dispose(ringbuffer_t rb)
{
	if (!rb) return;
	free(rb->mem);
	free(rb);
}

/*****************************************************************************/

void *ringbuffer_alloc(ringbuffer_t rb, size_t size)
{
	unsigned char *addr;

	/* Up align the size */
	size = (size + sizeof(size_t) + 4 - 1) & (~(4 - 1));

	/* Give up if size is larger than buffer */
	if (size > rb->size)
		return NULL;

	if (rb->next_alloc + size >= rb->memend)
	{
		/* New buffer doesn't fit in the remaining space, wrap, but "tag" the
		 * element beforehand */
		if (rb->next_alloc + sizeof(size_t) < rb->memend)
		{
			*(size_t*)rb->next_alloc = 0;
		}
		rb->next_alloc = rb->mem;
	}

	if (rb->next_free)
	{
		while (rb->next_alloc <= rb->next_free && rb->next_alloc + size >= rb->next_free)
		{
			size_t free_size = *(size_t*)rb->next_free;
			rb->free_callback(rb, rb->next_free + sizeof(size_t), free_size, rb->userdata);
			rb->next_free += free_size + sizeof(size_t);
		}
		/* TODO: Wrap next_free if size is 0 */
	} else
	{
		rb->next_free = rb->next_alloc;
	}

	/* Remember truely occupied size */
	*((size_t*)rb->next_alloc) = size;
	addr = rb->next_alloc + sizeof(size_t);
	rb->next_alloc = addr + size;
	return addr;
}
