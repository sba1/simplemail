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

struct test_ringbuffer_three_entries_only_two_fit_initially_free_callback_data
{
	int num_expected_mem;
	void *expected_mem[2];
	int called;
};

static void test_ringbuffer_three_entries_only_two_fit_initially_free_callback(ringbuffer_t rb, void *mem, int size, void *userdata)
{
	struct test_ringbuffer_three_entries_only_two_fit_initially_free_callback_data *data = (struct test_ringbuffer_three_entries_only_two_fit_initially_free_callback_data*)userdata;
	data->called++;
	if (data->num_expected_mem)
	{
		CU_ASSERT(mem == data->expected_mem[data->num_expected_mem - 1]);
		data->num_expected_mem--;
	}
}

/* @Test */
void test_ringbuffer_three_entries_only_two_fit_initially(void)
{
	struct test_ringbuffer_three_entries_only_two_fit_initially_free_callback_data data = {0};
	ringbuffer_t rb;
	void *mem[3];

	CU_ASSERT((rb = ringbuffer_create(1000, test_ringbuffer_three_entries_only_two_fit_initially_free_callback, &data)) != NULL);
	CU_ASSERT((mem[0] = ringbuffer_alloc(rb, 400)) != NULL);
	CU_ASSERT((mem[1] = ringbuffer_alloc(rb, 500)) != NULL);

	/* Next allocation should cause all items to be freed */
	data.expected_mem[1] = mem[0];
	data.expected_mem[0] = mem[1];
	data.num_expected_mem = 2;
	CU_ASSERT((mem[2] = ringbuffer_alloc(rb, 600)) != NULL);
	CU_ASSERT(data.called == 2);
	ringbuffer_dispose(rb);
}

/*****************************************************************************/

/* @Test */
void test_ringbuffer_three_entries_only_two_fit_initially_subsequent_frees(void)
{
	struct test_ringbuffer_three_entries_only_two_fit_initially_free_callback_data data = {0};
	ringbuffer_t rb;
	void *mem[4];

	CU_ASSERT((rb = ringbuffer_create(1000, test_ringbuffer_three_entries_only_two_fit_initially_free_callback, &data)) != NULL);
	CU_ASSERT((mem[0] = ringbuffer_alloc(rb, 400)) != NULL);
	CU_ASSERT((mem[1] = ringbuffer_alloc(rb, 500)) != NULL);


	/* Next allocation should cause all items to be freed */
	data.expected_mem[1] = mem[0];
	data.expected_mem[0] = mem[1];
	data.num_expected_mem = 2;
	CU_ASSERT((mem[2] = ringbuffer_alloc(rb, 600)) != NULL);

	/* Next allocation should cause the previous item to be freed */
	data.expected_mem[0] = mem[2];
	data.num_expected_mem = 1;
	CU_ASSERT((mem[3] = ringbuffer_alloc(rb, 600)) != NULL);

	CU_ASSERT(data.called == 3);
	ringbuffer_dispose(rb);
}

/*****************************************************************************/

/* @Test */
void test_ringbuffer_empty_traversing(void)
{
	ringbuffer_t rb;

	CU_ASSERT((rb = ringbuffer_create(1000, NULL, NULL)) != NULL);
	CU_ASSERT((NULL == ringbuffer_next(rb, NULL)));

	ringbuffer_dispose(rb);
}

/*****************************************************************************/

/* @Test */
void test_ringbuffer_traversing(void)
{
	ringbuffer_t rb;
	void *mem[16];
	void *entry;
	int count=0;
	int i;
	int last_payload;

	CU_ASSERT((rb = ringbuffer_create(1000, NULL, NULL)) != NULL);

	/* Depending on the actual implementation, this could evict one or more entries */
	for (i=0; i < 11; i++)
	{
		CU_ASSERT((mem[i] = ringbuffer_alloc(rb, 100)) != NULL);
		*((int*)mem[i]) = i;
	}

	entry = NULL;
	while ((entry = ringbuffer_next(rb, entry)))
	{
		last_payload = *(int*)entry;
		count++;
	}
	CU_ASSERT(ringbuffer_entries(rb) == count);
	CU_ASSERT(count >= 7);
	CU_ASSERT(last_payload == 10);
	ringbuffer_dispose(rb);
}

/*****************************************************************************/

/* @Test */
void test_ringbuffer_get_entry_by_id(void)
{
	ringbuffer_t rb;
	void *mem[2];
	unsigned int id[2];
	unsigned int max_id = 0;
	int i;

	CU_ASSERT((rb = ringbuffer_create(1000, NULL, NULL)) != NULL);

	CU_ASSERT(ringbuffer_get_entry_by_id(rb, 0) == NULL);
	CU_ASSERT(ringbuffer_get_entry_by_id(rb, 0x12345) == NULL);

	for (i=0; i < 2; i++)
	{
		CU_ASSERT((mem[i] = ringbuffer_alloc(rb, 8)) != NULL);
		id[i] = ringbuffer_entry_id(mem[i]);
		if (id[i] > max_id) max_id = id[i];
	}

	CU_ASSERT(ringbuffer_get_entry_by_id(rb, id[0]) == mem[0]);
	CU_ASSERT(ringbuffer_get_entry_by_id(rb, id[1]) == mem[1]);
	CU_ASSERT(ringbuffer_get_entry_by_id(rb, max_id + 1) == NULL);

	/* Now allocate new entries so that the existing ones are evicted.
	 * TODO: Use the hook to make sure that they have been removed from the
	 *  ringbuffer.
	 */
	for (i=0; i < 10; i++)
	{
		CU_ASSERT((ringbuffer_alloc(rb, 100)) != NULL);
	}

	CU_ASSERT(ringbuffer_get_entry_by_id(rb, id[0]) == NULL);
	CU_ASSERT(ringbuffer_get_entry_by_id(rb, id[1]) == NULL);

	ringbuffer_dispose(rb);
}
