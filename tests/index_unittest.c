/**
 * index_unittest - a simple test for the simple string index interface for SimpleMail.
 * Copyright (C) 2012  Sebastian Bauer
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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <CUnit/Basic.h>

#include "index.h"
#include "index_naive.h"

/*******************************************************/

static int test_index_naive_callback_called;

static int test_index_naive_callback(int did, void *userdata)
{
	CU_ASSERT(did==4);
}

static int test_index_naive_callback2(int did, void *userdata)
{
	CU_ASSERT(0);
}

/*******************************************************/

/* @Test */
void test_index_naive(void)
{
	struct index *index;
	int ok;
	int nd;

	index = index_create(&index_naive,"naive-index.dat");
	CU_ASSERT(index != NULL);

	ok = index_put_document(index,4,"This is a very long text.");
	CU_ASSERT(ok != 0);

	ok = index_put_document(index,12,"This is a short text.");
	CU_ASSERT(ok != 0);

	nd = index_find_documents(index,test_index_naive_callback,NULL,1,"very");
	CU_ASSERT(nd == 1);

	ok = index_remove_document(index,4);
	CU_ASSERT(ok != 0);

	nd = index_find_documents(index,test_index_naive_callback2,NULL,1,"very");
	CU_ASSERT(nd == 0);

	index_dispose(index);
}
