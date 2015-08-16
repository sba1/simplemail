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

#include "coroutines.h"

#include <CUnit/Basic.h>

/*****************************************************************************/

static const int MAX_COUNT = 10;

struct count_context
{
	struct coroutine_basic_context basic_context;

	int count;
};

static coroutine_return_t count(struct coroutine_basic_context *arg)
{
	struct count_context *c = (struct count_context *)arg;

	COROUTINE_BEGIN(c);

	for (c->count=0; c->count < MAX_COUNT; c->count++)
	{
		COROUTINE_YIELD(c);
	}

	COROUTINE_END(c);
}

/* @Test */
void test_simple_coroutines_loop(void)
{
	struct count_context count_context1 = {0};
	struct count_context count_context2 = {0};

	coroutine_scheduler_t scheduler = coroutine_scheduler_new();
	CU_ASSERT(scheduler != NULL);

	coroutine_add(scheduler, count, &count_context1.basic_context);
	coroutine_add(scheduler, count, &count_context2.basic_context);

	coroutine_schedule(scheduler);

	CU_ASSERT_EQUAL(count_context1.count, MAX_COUNT);
	CU_ASSERT_EQUAL(count_context2.count, MAX_COUNT);

	coroutine_scheduler_dispose(scheduler);
}

