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

#include "ringbuffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include <CUnit/Basic.h>

/*****************************************************************************/

/* @Test */
void test_ringbuffer_create_and_dispose(void)
{
	ringbuffer_t rb;

	CU_ASSERT((rb = ringbuffer_create(1000, NULL, NULL)) != NULL);
	ringbuffer_dispose(rb);
}

/*****************************************************************************/

/* @Test */
void test_ringbuffer_single_entry_that_fits(void)
{
	void *mem;
	ringbuffer_t rb;

	CU_ASSERT((rb = ringbuffer_create(1000, NULL, NULL)) != NULL);
	CU_ASSERT((mem = ringbuffer_alloc(rb, 100)) != NULL);
	ringbuffer_dispose(rb);
}

/*****************************************************************************/

/* @Test */
void test_ringbuffer_single_entry_that_does_not_fit(void)
{
	void *mem;
	ringbuffer_t rb;

	CU_ASSERT((rb = ringbuffer_create(1000, NULL, NULL)) != NULL);
	CU_ASSERT((mem = ringbuffer_alloc(rb, 10000)) == NULL);
	ringbuffer_dispose(rb);
}

/*****************************************************************************/

static void test_ringbuffer_three_entries_only_two_fit_initially_free_callback(ringbuffer_t rb, void *mem, int size, void *userdata)
{
	int *called = (int*)userdata;
	*called = *called + 1;

	printf("Free mem@%p\n", mem);
}

/* @Test */
void test_ringbuffer_three_entries_only_two_fit_initially(void)
{
	void *mem;
	ringbuffer_t rb;
	int called = 0;

	CU_ASSERT((rb = ringbuffer_create(1000, test_ringbuffer_three_entries_only_two_fit_initially_free_callback, &called)) != NULL);
	CU_ASSERT((ringbuffer_alloc(rb, 400)) != NULL);
	CU_ASSERT((ringbuffer_alloc(rb, 500)) != NULL);
	CU_ASSERT((ringbuffer_alloc(rb, 600)) != NULL);
	CU_ASSERT(called == 2);
	ringbuffer_dispose(rb);
}

/*****************************************************************************/

/* @Test */
void test_ringbuffer_three_entries_only_two_fit_initially_subsequent_frees(void)
{
	void *mem;
	ringbuffer_t rb;
	int called = 0;

	CU_ASSERT((rb = ringbuffer_create(1000, test_ringbuffer_three_entries_only_two_fit_initially_free_callback, &called)) != NULL);
	CU_ASSERT((ringbuffer_alloc(rb, 400)) != NULL);
	CU_ASSERT((ringbuffer_alloc(rb, 500)) != NULL);
	CU_ASSERT((ringbuffer_alloc(rb, 600)) != NULL);
	CU_ASSERT((ringbuffer_alloc(rb, 600)) != NULL);
	CU_ASSERT(called == 3);
	ringbuffer_dispose(rb);
}
