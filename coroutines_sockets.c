/**
 * coroutines_sockets.c - simple coroutines for SimpleMail.
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
 * @file coroutines_sockets.c
 */

#include "coroutines_sockets.h"

#include <stdlib.h>

#include <netinet/in.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifndef __MORPHOS__
#include <sys/select.h>
#else
#include <proto/exec.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#undef getpid
// libcrypto stubs
pid_t getpid()
{
	return (pid_t) FindTask(NULL);
}

int sigaction(int a, const struct sigaction *b, struct sigaction *c)
{
	return 0;
}

int tcsetattr(int a, int b, const struct termios *c)
{
	return 0;
}

int tcgetattr(int a, struct termios *b)
{
	return 0;
}
#endif

#include "coroutines_internal.h"

/*****************************************************************************/

#define MAX(a,b) ((a)>(b)?(a):(b))

/*****************************************************************************/

struct coroutine_scheduler_fd_data
{
	/** Highest number of descriptors to wait for */
	int nfds;

	/** Set of read fds to wait for */
	fd_set readfds;

	/** Set of write fds to wait for */
	fd_set writefds;
};


/**
 * Prepare the set of fds for the given scheduler.
 *
 * @param scheduler
 */
static void coroutine_schedule_prepare_fds(coroutine_scheduler_t scheduler, struct coroutine_scheduler_fd_data *data)
{
	coroutine_t cor, cor_next;

	fd_set *readfds = &data->readfds;
	fd_set *writefds = &data->writefds;

	FD_ZERO(readfds);
	FD_ZERO(writefds);
	data->nfds = -1;

	cor = coroutines_list_first(&scheduler->waiting_coroutines_list);
	for (;cor;cor = cor_next)
	{
		cor_next =  coroutines_next(cor);

		if (cor->context->socket_fd >= 0)
		{
			data->nfds = MAX(cor->context->socket_fd, data->nfds);

			if (cor->context->write_mode)
			{
				FD_SET(cor->context->socket_fd, writefds);
			} else
			{
				FD_SET(cor->context->socket_fd, readfds);
			}
		}
	}
}

/**
 * Standard wait for event function using select().
 *
 * @param poll
 * @param udata
 */
static int coroutine_wait_for_fd_event(coroutine_scheduler_t sched, int poll, void *udata)
{
	struct coroutine_scheduler_fd_data *data = (struct coroutine_scheduler_fd_data *)udata;

	struct timeval zero_timeout = {0};

	coroutine_schedule_prepare_fds(sched, data);

	if (data->nfds >= 0)
	{
		select(data->nfds+1, &data->readfds, &data->writefds, NULL, poll?&zero_timeout:NULL);
		return 1;
	}
	return 0;
}

/*****************************************************************************/

int coroutine_is_fd_now_ready(coroutine_scheduler_t scheduler, coroutine_t cor)
{
	struct coroutine_scheduler_fd_data *data = (struct coroutine_scheduler_fd_data *)scheduler->wait_for_event_udata;

	if (cor->context->write_mode)
	{
		if (FD_ISSET(cor->context->socket_fd, &data->writefds))
			return 1;
	} else
	{
		if (FD_ISSET(cor->context->socket_fd, &data->readfds))
			return 1;
	}
	return 0;
}

/*****************************************************************************/

void coroutine_await_socket(struct coroutine_basic_context *context, int socket_fd, int write)
{
	context->socket_fd = socket_fd;
	context->write_mode = write;
	context->is_now_ready = coroutine_is_fd_now_ready;
}

/*****************************************************************************/

coroutine_scheduler_t coroutine_scheduler_new(void)
{
	struct coroutine_scheduler_fd_data *data;
	coroutine_scheduler_t sched;

	if (!(data = (struct coroutine_scheduler_fd_data *)malloc(sizeof(*data))))
		return NULL;

	if (!(sched = coroutine_scheduler_new_custom(coroutine_wait_for_fd_event, data)))
	{
		free(data);
		return NULL;
	}
	return sched;
}

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
	coroutine_scheduler_t sched;
	coroutine_t example;

	struct example_context example_context = {0};

	if (!(sched = coroutine_scheduler_new()))
	{
		fprintf(stderr, "Couldn't allocate scheduler!\n");
		return 1;
	}


	example_context.basic_context.socket_fd = -1;
	/* TODO: It is not required to set the scheduler as this is part of coroutine_add() */
	example_context.basic_context.scheduler = sched;

	example = coroutine_add(sched, coroutines_test, &example_context.basic_context);
	assert(example != 0);

	while (coroutine_schedule(sched));

	if (example_context.fd > 0)
		close(example_context.fd);
	return 0;
}


#endif
