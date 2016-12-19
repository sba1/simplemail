/**
 * coroutines.c - simple coroutines for SimpleMail.
 * Copyright (C) 2015  Sebastian Bauer
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
 * @file coroutines.c
 */

#include "coroutines.h"

#include <stdio.h>
#include <stdlib.h>

#include "coroutines_internal.h"
#include "lists.h"

/*****************************************************************************/

void coroutines_list_init(struct coroutines_list *list)
{
	list_init(&list->list);
}

/*****************************************************************************/

coroutine_t coroutines_list_first(struct coroutines_list *list)
{
	return (coroutine_t)list_first(&list->list);
}


/*****************************************************************************/

coroutine_t coroutines_list_remove_head(struct coroutines_list *list)
{
	return (coroutine_t)list_remove_head(&list->list);
}

/*****************************************************************************/

coroutine_t coroutines_next(coroutine_t c)
{
	return (coroutine_t)node_next(&c->node);
}

/*****************************************************************************/

coroutine_scheduler_t coroutine_scheduler_new_custom(int (*wait_for_event)(coroutine_scheduler_t sched, int poll, void *udata), void *udata)
{
	coroutine_scheduler_t scheduler;

	if (!(scheduler = (coroutine_scheduler_t)malloc(sizeof(*scheduler))))
		return NULL;

	coroutines_list_init(&scheduler->coroutines_ready_list);
	coroutines_list_init(&scheduler->waiting_coroutines_list);
	coroutines_list_init(&scheduler->finished_coroutines_list);

	scheduler->wait_for_event = wait_for_event;
	scheduler->wait_for_event_udata = udata;

	return scheduler;
}

/*****************************************************************************/

void coroutine_scheduler_dispose(coroutine_scheduler_t scheduler)
{
	free(scheduler);
}

/*****************************************************************************/

coroutine_t coroutine_add(coroutine_scheduler_t scheduler, coroutine_entry_t entry, struct coroutine_basic_context *context)
{
	coroutine_t coroutine;

	if (!(coroutine = malloc(sizeof(*coroutine))))
		return NULL;
	coroutine->entry = entry;
	coroutine->context = context;
	context->scheduler = scheduler;
	list_insert_tail(&scheduler->coroutines_ready_list.list, &coroutine->node);
	return coroutine;
}

/*****************************************************************************/

int coroutine_schedule_ready(coroutine_scheduler_t scheduler)
{
	coroutine_t cor = coroutines_list_first(&scheduler->coroutines_ready_list);
	coroutine_t cor_next;

	/* Execute all non-waiting coroutines */
	for (;cor;cor = cor_next)
	{
		coroutine_return_t cor_ret;

		cor_next =  coroutines_next(cor);
		cor_ret = cor->entry(cor->context);
		switch (cor_ret)
		{
			case	COROUTINE_DONE:
					node_remove(&cor->node);
					list_insert_tail(&scheduler->finished_coroutines_list.list, &cor->node);
					break;

			case	COROUTINE_YIELD:
					/* Nothing special to do here */
					break;

			case	COROUTINE_WAIT:
					node_remove(&cor->node);
					list_insert_tail(&scheduler->waiting_coroutines_list.list,&cor->node);
					break;
		}
	}

	cor = coroutines_list_first(&scheduler->waiting_coroutines_list);
	for (;cor;cor = cor_next)
	{
		coroutine_t f;

		cor_next =  coroutines_next(cor);

		/* Check if we are waiting for another coroutine to be finished
		 * FIXME: This needs only be done once when the coroutine that we
		 * are waiting for is done */
		f = coroutines_list_first(&scheduler->finished_coroutines_list);
		while (f)
		{
			if (f == cor->context->other)
			{
				/* Move from waiting to ready queue */
				node_remove(&cor->node);
				list_insert_tail(&scheduler->coroutines_ready_list.list, &cor->node);
				break;
			}
			f = coroutines_next(f);
		}
	}

	/* Finally, free coroutines that have been just finished */
	while ((cor = (coroutine_t)coroutines_list_remove_head(&scheduler->finished_coroutines_list)))
	{
		if (cor->context->free_after_done)
		{
			free(cor->context);
		}
		free(cor);
	}

	return !!coroutines_list_first(&scheduler->coroutines_ready_list);
}

/**
 * Returns whether there are any unfinished coroutines.
 *
 * @param scheduler
 * @return
 */
static int coroutine_has_unfinished_coroutines(coroutine_scheduler_t scheduler)
{
	return coroutines_list_first(&scheduler->coroutines_ready_list)
			|| coroutines_list_first(&scheduler->waiting_coroutines_list);
}

/*****************************************************************************/

int coroutine_schedule(coroutine_scheduler_t scheduler)
{
	int polling;

	coroutine_t cor, cor_next;

	polling = coroutine_schedule_ready(scheduler);

	if (scheduler->wait_for_event)
	{
		scheduler->wait_for_event(scheduler, polling, scheduler->wait_for_event_udata);
	}

	cor = coroutines_list_first(&scheduler->waiting_coroutines_list);
	for (;cor;cor = cor_next)
	{
		cor_next =  coroutines_next(cor);

		if (!cor->context->is_now_ready)
			continue;

		if (!cor->context->is_now_ready(scheduler, cor))
			continue;

		node_remove(&cor->node);
		list_insert_tail(&scheduler->coroutines_ready_list.list, &cor->node);
	}

	return coroutine_has_unfinished_coroutines(scheduler);
}
