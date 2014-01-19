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
void test_me(void)
{
	struct index *idx;

	idx = index_external_create("/tmp/index_external_unittest_index.dat");
	CU_ASSERT(idx != NULL);

	index_external_dispose(idx);
}

/*******************************************************/
