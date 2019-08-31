/**
 * string_lists_unittest - a simple test for the gadgets interface for SimpleMail.
 * Copyright (C) 2019 Sebastian Bauer
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

#include "string_lists.h"

#include <stdlib.h>

/* @Test */
void test_string_lists_basics(void)
{
	struct string_list l;
	struct string_node *n;

	string_list_init(&l);
	CU_ASSERT(string_list_length(&l) == 0);

	n = string_list_insert_tail_always_len(&l, "Hello", 4);
	CU_ASSERT(string_list_length(&l) == 1);
	CU_ASSERT(n == string_list_first(&l));
	CU_ASSERT_STRING_EQUAL(n->string, "Hell");

	n = string_list_insert_tail_always(&l, "Hello");
	CU_ASSERT(string_list_length(&l) == 2);
	CU_ASSERT(n == string_list_last(&l));
	CU_ASSERT_STRING_EQUAL(n->string, "Hello");

	n = string_list_insert_tail_always(&l, "");
	CU_ASSERT(string_list_length(&l) == 3);
	CU_ASSERT(n == string_list_last(&l));
	CU_ASSERT_STRING_EQUAL(n->string, "");

	n = string_list_insert_tail_always(&l, "");
	CU_ASSERT(string_list_length(&l) == 4);
	CU_ASSERT(n == string_list_last(&l));
	CU_ASSERT_STRING_EQUAL(n->string, "");

	n = string_list_insert_tail(&l, "");
	CU_ASSERT(string_list_length(&l) == 4);
	CU_ASSERT(n == NULL);

	string_list_clear(&l);
}
