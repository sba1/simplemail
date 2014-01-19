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

#include "index_external.c"

/*******************************************************/

/* @Test */
void test_number_of_leaves_match_inserted_strings(void)
{
	struct index_external *idx;
	int rc;
	int i;
	long offset;

	idx = (struct index_external *)index_external_create_with_opts("/tmp/index_external_unittest_index.dat", 512);
	CU_ASSERT(idx != NULL);

	for (i=0;i<idx->max_elements_per_node+1;i++)
	{
		char buf[16];
		snprintf(buf, sizeof(buf), "%03dtest", i);

		rc = index_external_append_string(idx, buf, &offset);
		CU_ASSERT(rc != 0);

		rc = bnode_insert_string(idx, i, offset, buf);
		CU_ASSERT(rc != 0);
	}

	CU_ASSERT(count_index_leaves(idx, idx->root_node, 0) == idx->max_elements_per_node+1);
	printf("%d %d %d %d\n", count_index_leaves(idx, idx->root_node, 0), count_index(idx, idx->root_node, 0), idx->max_elements_per_node, idx->number_of_blocks);


//	dump_index(idx, idx->root_node, 0);

	index_external_dispose(&idx->index);
}

/*******************************************************/
