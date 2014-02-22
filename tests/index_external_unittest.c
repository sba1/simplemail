/**
 * index_external_unittest.c - unit tests for SimpleMail's external string index implementation
 * Copyright (C) 2013  Sebastian Bauer
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file index_external_unittest.c
 */

#include <CUnit/Basic.h>

#define DEBUG_FUNCTIONS
#include "index_external.c"

/*******************************************************/

/* @Test */
void test_number_of_leaves_match_inserted_strings(void)
{
	struct index_external *idx;
	int rc;
	int i;
	int number_of_distinct_strings;

	idx = (struct index_external *)index_external_create_with_opts("/tmp/index_external_unittest_index.dat", 512);
	CU_ASSERT(idx != NULL);

	number_of_distinct_strings = idx->max_elements_per_node + 1;
	for (i=0;i<number_of_distinct_strings;i++)
	{
		char buf[16];
		long offset;
		snprintf(buf, sizeof(buf), "%03dtest", i);

		rc = index_external_append_string(idx, buf, &offset);
		CU_ASSERT(rc != 0);

		rc = bnode_insert_string(idx, i, offset, buf);
		CU_ASSERT(rc != 0);
	}

	CU_ASSERT(count_index_leaves(idx, idx->root_node, 0) == number_of_distinct_strings);

	for (i=0;i<number_of_distinct_strings;i++)
	{
		char buf[16];
		struct bnode_path bp;

		snprintf(buf, sizeof(buf), "%03dtest", i);

		memset(&bp, 0, sizeof(bp));
		rc = bnode_lookup(idx, buf, &bp);
		CU_ASSERT(rc != 0);
		CU_ASSERT(bp.max_level == 1);
	}

	/* Insert strings again */
	for (i=0;i<number_of_distinct_strings;i++)
	{
		char buf[16];
		long offset;
		snprintf(buf, sizeof(buf), "%03dtest", i);

		rc = index_external_append_string(idx, buf, &offset);
		CU_ASSERT(rc != 0);

		rc = bnode_insert_string(idx, i, offset, buf);
		CU_ASSERT(rc != 0);
	}

	CU_ASSERT(count_index_leaves(idx, idx->root_node, 0) == 2 * number_of_distinct_strings);

	/* Simple Lookups */
	for (i=0;i<number_of_distinct_strings;i++)
	{
		char buf[16];
		struct bnode_path bp;

		snprintf(buf, sizeof(buf), "%03dtest", i);

		memset(&bp, 0, sizeof(bp));
		rc = bnode_lookup(idx, buf, &bp);
		CU_ASSERT(rc != 0);
		CU_ASSERT(bp.max_level == 1);

		printf("buf: %s %d\n", buf, bp.node[1].key_index);
	}

	/* Test for the iteration like API */
	for (i=0;i<number_of_distinct_strings;i++)
	{
		char buf[16];
		struct bnode_string_iter_data *iter;

		snprintf(buf, sizeof(buf), "%03dtest", i);

		iter = bnode_find_string_iter(idx, buf, NULL);
		CU_ASSERT(iter != NULL);

		if (iter == NULL)
		{
			printf("For \"%s\" iter is NULL!\n", buf);
		} else
		{
			CU_ASSERT(bnode_string_iter_data_get_did(iter) == i);

			iter = bnode_find_string_iter(idx, buf, iter);
			CU_ASSERT(iter != NULL);

			if (iter == NULL)
			{
				printf("For \"%s\" 2nd iter is NULL!\n", buf);
			} else
			{
				CU_ASSERT(bnode_string_iter_data_get_did(iter) == i);
			}
		}
	}
	printf("%d %d %d %d\n", count_index_leaves(idx, idx->root_node, 0), count_index(idx, idx->root_node, 0), idx->max_elements_per_node, idx->number_of_blocks);


	dump_index(idx, idx->root_node, 0);

	index_external_dispose(&idx->index);
}

/*******************************************************/
