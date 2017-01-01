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

#include "hash.h"

#include <stdio.h>
#include <stdlib.h>

#include <CUnit/Basic.h>

/*****************************************************************************/

static void test_hash_callback(struct hash_entry *entry, void *data)
{
	int *num = (int*)data;
	*num = *num + 1;
}

/* @Test */
void test_whether_hash_works(void)
{
	struct hash_table ht;
	char str[32];
	int i;

	CU_ASSERT_EQUAL(hash_table_init(&ht, 5, NULL), 1);

	/* Insert elements */
	for (i=0; i < 10000; i++)
	{
		char *dup;

		snprintf(str, sizeof(str), "str%d", i);
		dup = strdup(str);
		CU_ASSERT(dup != NULL);

		hash_table_insert(&ht, dup, i);
	}

	/* Check if we can find them again */
	for (i=0;i < 10000; i++)
	{
		struct hash_entry *he;

		snprintf(str, sizeof(str), "str%d", i);

		he = hash_table_lookup(&ht, str);
		CU_ASSERT(he != NULL);

		CU_ASSERT_STRING_EQUAL(he->string, str);
		CU_ASSERT_EQUAL(he->data, i);
	}

	/* Test wheter we can iterate over the elements */
	i = 0;
	hash_table_call_for_each_entry(&ht, test_hash_callback, &i);

	CU_ASSERT_EQUAL(i, 10000);

	/* Clearing before clean is not necessary but this is a test so we do
	 * this redundant call.
	 */
	hash_table_clear(&ht);
	hash_table_clean(&ht);
}
