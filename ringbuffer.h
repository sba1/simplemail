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
 * @file ringbuffer.h
 */

#ifndef SM__RINGBUFFER_H
#define SM__RINGBUFFER_H

#include <stdlib.h>

struct ringbuffer;

typedef struct ringbuffer * ringbuffer_t;

/**
 * Type of a callback that is invoked before a ring buffer item is freed.
 *
 * @param rb the ring buffer
 * @param mem the address of the item that is going to be freed
 * @param size the size of the ring buffer item that is going to be freed. This
 *  may be a little bit larger than the requested one.
 * @param userdata user data supplied in ringbuffer_create().
 */
typedef void (*ringbuffer_free_callback_t)(ringbuffer_t rb, void *mem, int size, void *userdata);

/**
 * Prepare the ring buffer.
 *
 * @param size of the ring buffer.
 * @param free_callback the callback that is invoked for freed items
 * @return an instance to the ring buffer.
 */
ringbuffer_t ringbuffer_create(size_t size, ringbuffer_free_callback_t free_callback, void *userdata);

/**
 * Free all memory associated with the given ring buffer.
 *
 * @param rb the ring buffer to be disposed
 */
void ringbuffer_dispose(ringbuffer_t rb);

/**
 * Allocate size bytes of memory from the ring buffer.
 *
 * @param size of the ring buffer item to be allocated.
 * @return the memory address of the item payload.
 */
void *ringbuffer_alloc(ringbuffer_t rb, size_t size);

#endif /* SM__RINGBUFFER_H */
