#define TEST

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

/*---------------------------------------------------------------------------*/

struct coroutine;
struct coroutine_basic_context;

typedef enum {
	COROUTINE_YIELD,
	COROUTINE_WAIT,
	COROUTINE_DONE
} coroutine_return_t;


typedef struct coroutine *coroutine_t;
typedef coroutine_return_t (*coroutine_entry_t)(struct coroutine_basic_context *arg);

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
 * A simple scheduler for coroutines.
 */
struct coroutine_scheduler
{
	fd_set rfds;

	/* List of type coroutine_t */
	struct list coroutines_list;

	/** First active coroutine context */
	struct coroutine_basic_context *first;
};


/**
 * The basic context of a coroutine. This should be embedded in a higher
 * context.
 */
struct coroutine_basic_context
{
	/** A next coroutine as in a single-linked list */
	struct coroutine_basic_context *next;

	/** The scheduler that is responsible for this context */
	struct coroutine_scheduler *scheduler;

	/** The state that will be executed next for this coroutine */
	int next_state;

	/** The socket fd for a waiting coroutine */
	int socket_fd;

	/** Whether we wait for a reading or writing fd */
	int write;

	/** Another coroutine we are waiting for */
	coroutine_t other;
};

#define COROUTINE_BEGIN(context) \
	switch(context->basic_context.next_state)\
	{ \
		case 0:

/**
 * Insert a simple preemption point. Continue on the next possible event.
 */
#define COROUTINE_YIELD(context)\
			context->basic_context.next_state = __LINE__;\
			return COROUTINE_YIELD;\
		case __LINE__:\

/**
 * Insert a preemption point but don't continue until the given socket is ready
 * to be read or written.
 */
#define COROUTINE_AWAIT_SOCKET(context, sfd, write)\
			context->basic_context.next_state = __LINE__;\
			coroutine_await_socket(&context->basic_context, sfd, write);\
			return COROUTINE_WAIT;\
		case __LINE__:\
			context->basic_context.socket_fd = -1;

/**
 * Insert a preemption point but don't continue until the given coroutine
 * is done.
 */
#define COROUTINE_AWAIT_OTHER(context, other)\
			context->basic_context.next_state = __LINE__;\
			context->basic_context.other = other;\
			return COROUTINE_WAIT;\
		case __LINE__:\
			context->basic_context.other = NULL;

#define COROUTINE_END(context) \
	}\
	return COROUTINE_DONE;

static void coroutine_await_socket(struct coroutine_basic_context *context, int socket_fd, int write)
{
	context->socket_fd = socket_fd;
	context->write = write;
	FD_SET(socket_fd, &context->scheduler->rfds);

	/* Enqueue */
	context->next = context->scheduler->first;
	context->scheduler->first = context;
}

/**
 * Add a new coroutine to the scheduler.
 *
 * @param scheduler the schedule that should take care of the coroutine.
 * @param entry the coroutine's entry
 * @param context the coroutine's context
 * @return the coroutine just added or NULL for an error.
 */
static coroutine_t coroutine_add(struct coroutine_scheduler *scheduler, coroutine_entry_t entry, struct coroutine_basic_context *context)
{
	coroutine_t coroutine;

	if (!(coroutine = malloc(sizeof(*coroutine))))
		return NULL;
	coroutine->entry = entry;
	coroutine->context = context;
	list_insert_tail(&scheduler->coroutines_list, &coroutine->node);
	return coroutine;
}

/**
 * Schedule all coroutines.
 *
 * @param scheduler the scheduler
 */
static void coroutine_schedule(struct coroutine_scheduler *scheduler)
{
	struct timeval zero_timeout = {0};

	fd_set readfds;
	FD_ZERO(&readfds);

	while (list_first(&scheduler->coroutines_list))
	{
		int any_non_waiting = 0;

		coroutine_return_t cor_ret;
		coroutine_t cor = (coroutine_t)list_first(&scheduler->coroutines_list);
		coroutine_t cor_next;

		/* Execute all non-waiting coroutines */
		for (;cor;cor = cor_next)
		{
			cor_next =  (coroutine_t)node_next(&cor->node);
			/* If this is a waiting coroutine, skip it if no corresponding
			 * event has been occurred
			 */
			if (cor->context->socket_fd != -1)
			{
				if (!FD_ISSET(cor->context->socket_fd, &readfds))
					continue;
			}

			cor_ret = cor->entry(cor->context);
			switch (cor_ret)
			{
				case	COROUTINE_DONE:
						node_remove(&cor->node);
						break;

				case	COROUTINE_YIELD:
						any_non_waiting = 1;
						break;
			}
		}

		/* Now wait or poll descriptors */
		{
			struct coroutine_basic_context *ctx;
			int nfds = -1;

			readfds = scheduler->rfds;

			/* First, determine number of file descriptors. TODO: cache */
			ctx = scheduler->first;
			while (ctx)
			{
				if (ctx->socket_fd != -1)
				{
					if (ctx->socket_fd > nfds)
						nfds = ctx->socket_fd;
				}
				ctx = ctx->next;
			}

			if (nfds >= 0)
			{
				printf("about to select (%s)\n", any_non_waiting?"polling":"blocking");
				select(nfds+1, &readfds, NULL, NULL, any_non_waiting?&zero_timeout:NULL);
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

	coroutine_add(c->basic_context.scheduler, count, &c->count_context.basic_context);

	COROUTINE_END(c);
}


int main(int argc, char **argv)
{
	coroutine_t example;

	struct coroutine_scheduler scheduler = {0};

	struct example_context example_context = {0};

	list_init(&scheduler.coroutines_list);
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
