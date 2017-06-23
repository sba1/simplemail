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
 * @file arrays.h
 */

#ifndef SM__ARRAYS_H
#define SM__ARRAYS_H

struct array
{
	int num_el;
	int num_el_allocated;
	void **els;
};

/**
 * Initialize the array.
 *
 * @param array
 * @return whether successful not.
 */
int array_init(struct array *array);

/**
 * Deinitialized the array. All memory directly associated with the array
 * is given back. The array itself it is not affected.
 *
 * @param array
 */
void array_deinit(struct array *array);

/**
 * Add an element to the end of the array.
 *
 * @param array
 * @param elm
 * @return the position of the array or -1 on the failure case.
 */
int array_add(struct array *array, void *elm);

/**
 * Returns the element at the given index.
 *
 * @param array
 * @param idx
 */
void *array_get(struct array *array, int idx);

/*****************************************************************************/

struct iarray
{
	int num_el;
	int num_el_allocated;
	int *els;
};

int iarray_init(struct iarray *array);

/**
 * Deinitialized the array. All memory directly associated with the array
 * is given back. The array itself it is not affected.
 *
 * @param array
 */
void iarray_deinit(struct iarray *array);

/**
 * Add an element to the end of the array.
 *
 * @param array
 * @param elm
 * @return the position of the array or -1 on the failure case.
 */
int iarray_add(struct iarray *array, int elm);

/**
 * Returns the element at the given index.
 *
 * @param array
 * @param idx
 */
int iarray_get(struct iarray *array, int idx);

#endif
