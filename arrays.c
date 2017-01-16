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
 * @file arrays.c
 */

#include "arrays.h"

#include <string.h>
#include <stdlib.h>

/*****************************************************************************/

int array_init(struct array *array)
{
	memset(array,0,sizeof(*array));
	return 1;
}

/*****************************************************************************/

void array_deinit(struct array *array)
{
	free(array->els);
}

/*****************************************************************************/

int array_add(struct array *array, void *el)
{
	if (array->num_el == array->num_el_allocated)
	{
		int new_num_el_allocated = array->num_el_allocated * 3 / 4 + 4;
		void *new_els = realloc(array->els, new_num_el_allocated * sizeof(void*));
		if (!new_els)
		{
			return -1;
		}
		array->els = new_els;
		array->num_el_allocated = new_num_el_allocated;
	}

	array->els[array->num_el] = el;
	return array->num_el++;
}

/*****************************************************************************/

void *array_get(struct array *array, int idx)
{
	return array->els[idx];
}

/*****************************************************************************/

int iarray_init(struct iarray *array)
{
	memset(array,0,sizeof(*array));
	return 1;
}


/*****************************************************************************/

void iarray_deinit(struct iarray *array)
{
	free(array->els);
}

/*****************************************************************************/

int iarray_add(struct iarray *array, int elm)
{
	if (array->num_el == array->num_el_allocated)
	{
		int new_num_el_allocated = array->num_el_allocated * 3 / 4 + 4;
		void *new_els = realloc(array->els, new_num_el_allocated * sizeof(void*));
		if (!new_els)
		{
			return -1;
		}
		array->els = new_els;
		array->num_el_allocated = new_num_el_allocated;
	}

	array->els[array->num_el] = elm;
	return array->num_el++;
}

/*****************************************************************************/

int iarray_get(struct iarray *array, int idx)
{
	return array->els[idx];
}
