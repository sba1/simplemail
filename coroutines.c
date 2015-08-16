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
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "lists.h"

/*****************************************************************************/

/**
 * A simple coroutine.
 */
struct coroutine
{
	/** Embedded node structure for adding it to lists */
	struct node node;

	/** The context parameter of the coroutine */
	struct coroutine_basic_context *context;

	/** The actual entry of the coroutine */
	coroutine_entry_t entry;
};

/**
 * A simple list of elements of type coroutine_t.
 */
struct coroutines_list
{
	struct list list;
};

/**
 * A simple scheduler for coroutines.
 */
struct coroutine_scheduler
{
	/** Contains all active coroutines. Elements are of type coroutine_t */
	struct coroutines_list coroutines_list;

	/** Contains all waiting coroutines. Elements are of type coroutine_t */
	struct coroutines_list waiting_coroutines_list;
};

/*****************************************************************************/

/**
 * Initializes a coroutines list.
 *
 * @param list the list to initialize.
 */
static void coroutines_list_init(struct coroutines_list *list)
{
	list_init(&list->list);
}

/**
 * Returns the first element of a coroutines list.
 *
 * @param list the coroutines list.
 * @return the first element or NULL if the list is empty
 */
static coroutine_t coroutines_list_first(struct coroutines_list *list)
{
	return (coroutine_t)list_first(&list->list);
}

/**
 * Returns the next coroutine within the same list.
 *
 * @param c the coroutine whose successor shall be determined
 * @return the successor of c or NULL if c was the last element
 */
static coroutine_t coroutines_next(coroutine_t c)
{
	return (coroutine_t)node_next(&c->node);
}

/*****************************************************************************/

coroutine_scheduler_t coroutine_scheduler_new(void)
{
	coroutine_scheduler_t scheduler;

	if (!(scheduler = (coroutine_scheduler_t)malloc(sizeof(*scheduler))))
		return NULL;

	coroutines_list_init(&scheduler->coroutines_list);
	coroutines_list_init(&scheduler->waiting_coroutines_list);
	return scheduler;
}

/*****************************************************************************/

void coroutine_scheduler_dispose(coroutine_scheduler_t scheduler)
{
	free(scheduler);
}

/*****************************************************************************/

void coroutine_await_socket(struct coroutine_basic_context *context, int socket_fd, int write)
{
	context->socket_fd = socket_fd;
	context->write = write;
}

/*****************************************************************************/

coroutine_t coroutine_add(coroutine_scheduler_t scheduler, coroutine_entry_t entry, struct coroutine_basic_context *context)
{
	coroutine_t coroutine;

	if (!(coroutine = malloc(sizeof(*coroutine))))
		return NULL;
	coroutine->entry = entry;
	coroutine->context = context;
	list_insert_tail(&scheduler->coroutines_list.list, &coroutine->node);
	return coroutine;
}

/*****************************************************************************/

void coroutine_schedule(coroutine_scheduler_t scheduler)
{
	struct timeval zero_timeout = {0};
	struct coroutines_list finished_coroutines_list;

	coroutines_list_init(&finished_coroutines_list);

	while (coroutines_list_first(&scheduler->coroutines_list) || coroutines_list_first(&scheduler->waiting_coroutines_list))
	{
		coroutine_return_t cor_ret;
		coroutine_t cor = coroutines_list_first(&scheduler->coroutines_list);
		coroutine_t cor_next;

		fd_set readfds, writefds;
		int nfds;

		/* Execute all non-waiting coroutines */
		for (;cor;cor = cor_next)
		{
			cor_next =  coroutines_next(cor);
			cor_ret = cor->entry(cor->context);
			switch (cor_ret)
			{
				case	COROUTINE_DONE:
						node_remove(&cor->node);
						list_insert_tail(&finished_coroutines_list.list, &cor->node);
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

		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		nfds = -1;

		cor = coroutines_list_first(&scheduler->waiting_coroutines_list);
		for (;cor;cor = cor_next)
		{
			coroutine_t f;

			cor_next =  coroutines_next(cor);

			if (cor->context->socket_fd >= 0)
			{
				if (cor->context->socket_fd > nfds)
				{
					nfds = cor->context->socket_fd;
				}

				if (cor->context->write)
				{
					FD_SET(cor->context->socket_fd, &writefds);
				} else
				{
					FD_SET(cor->context->socket_fd, &readfds);
				}
			}

			/* Check if we are waiting for another coroutine to be finished
			 * FIXME: This needs only be done once when the coroutine that we
			 * are waiting for is done */
			f = coroutines_list_first(&finished_coroutines_list);
			while (f)
			{
				if ((f = cor->context->other))
				{
					/* Move from waiting to active queue */
					node_remove(&cor->node);
					list_insert_tail(&scheduler->coroutines_list.list, &cor->node);
					break;
				}
				f = coroutines_next(f);
			}
		}

		if (nfds >= 0)
		{
			int polling = !!list_first(&scheduler->coroutines_list.list);
			select(nfds+1, &readfds, &writefds, NULL, polling?&zero_timeout:NULL);

			cor = coroutines_list_first(&scheduler->waiting_coroutines_list);
			for (;cor;cor = cor_next)
			{
				cor_next =  coroutines_next(cor);
				if (cor->context->socket_fd < 0)
					continue;

				if (FD_ISSET(cor->context->socket_fd, &readfds))
				{
					node_remove(&cor->node);
					list_insert_tail(&scheduler->coroutines_list.list, &cor->node);
				}
			}
		}
	}

}

/*---------------------------------------------------------------------------*/



/*****************************************************************************/

#ifdef TEST

#include <assert.h>
#include <stdio.h>

struct count_context
{
	struct coroutine_basic_context basic_context;

	int count;
};

static coroutine_return_t count(struct coroutine_basic_context *arg)
{
	struct count_context *c = (struct count_context *)arg;

	COROUTINE_BEGIN(c);

	for (c->count=0; c->count < 10; c->count++)
	{
		printf("count = %d\n", c->count);
		COROUTINE_YIELD(c);
	}

	COROUTINE_END(c);
}

struct example_context
{
	struct coroutine_basic_context basic_context;

	int fd;
	struct sockaddr_in local_addr;
	socklen_t local_addrlen;
	struct sockaddr_in remote_addr;
	socklen_t remote_addrlen;

	struct count_context count_context;
};

static coroutine_return_t coroutines_test(struct coroutine_basic_context *arg)
{
	struct example_context *c = (struct example_context *)arg;

	COROUTINE_BEGIN(c);

	c->local_addr.sin_family = AF_INET;
	c->local_addr.sin_port = htons(1234);
	c->local_addr.sin_addr.s_addr = INADDR_ANY;
	c->local_addrlen = sizeof(c->local_addr);
	c->remote_addrlen = sizeof(c->remote_addr);

	c->fd = socket(AF_INET, SOCK_STREAM, 0);
	assert(c->fd != 0);

	int rc = bind(c->fd, (struct sockaddr*)&c->local_addr, c->local_addrlen);
	assert(rc == 0);

	rc = listen(c->fd, 50);
	assert(rc == 0);

	rc = fcntl(c->fd, F_SETFL, O_NONBLOCK);
	assert(rc == 0);

	COROUTINE_AWAIT_SOCKET(c, c->fd, 0);

	printf("back again\n");
	rc = accept(c->fd, (struct sockaddr*)&c->remote_addr, &c->remote_addrlen);
	printf("%d\n",rc);

	if (rc >= 0)
	{
		close(rc);
	}

	/* Now invoking the other coroutine */
	c->count_context.basic_context.socket_fd = -1;
	c->count_context.basic_context.scheduler = c->basic_context.scheduler;

	coroutine_t other = coroutine_add(c->basic_context.scheduler, count, &c->count_context.basic_context);
	assert(other != NULL);

	COROUTINE_AWAIT_OTHER(c, other);

	printf("now ending main coroutine\n");
	COROUTINE_END(c);
}


int main(int argc, char **argv)
{
	coroutine_t example;

	struct coroutine_scheduler scheduler = {0};

	struct example_context example_context = {0};

	coroutines_list_init(&scheduler.coroutines_list);
	coroutines_list_init(&scheduler.waiting_coroutines_list);

	example_context.basic_context.socket_fd = -1;
	/* TODO: It is not required to set the scheduler as this is part of coroutine_add() */
	example_context.basic_context.scheduler = &scheduler;

	example = coroutine_add(&scheduler, coroutines_test, &example_context.basic_context);
	assert(example != 0);

	coroutine_schedule(&scheduler);

	if (example_context.fd > 0)
		close(example_context.fd);
	return 0;
}


#endif
