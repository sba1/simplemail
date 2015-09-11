/**
 * coroutines_sockets.h - header for simple coroutines for SimpleMail.
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
 * @file coroutines_sockets.h
 */

#ifndef SM__COROUTINES_SOCKETS_H_
#define SM__COROUTINES_SOCKETS_H

#include "coroutines.h"


/**
 * Insert a preemption point but don't continue until the given socket is ready
 * to be read or written.
 */
#define COROUTINE_AWAIT_SOCKET(context, sfd, write)\
			context->basic_context.next_state = __LINE__;\
			coroutine_await_socket(&context->basic_context, sfd, write);\
			return COROUTINE_WAIT;\
		case __LINE__:\
			context->basic_context.socket_fd = -1; \
			context->basic_context.is_now_ready = NULL;

/**
 * Create a new scheduler for coroutines.
 *
 * @return the scheduler nor NULL for an error.
 */
coroutine_scheduler_t coroutine_scheduler_new(void);

/**
 * Prepare the waiting state.
 *
 * @param context
 * @param socket_fd
 * @param write
 */
void coroutine_await_socket(struct coroutine_basic_context *context, int socket_fd, int write);

/**
 * Checks whether the given blocked coroutine becomes now active due to some fd conditions.
 *
 * @param scheduler
 * @param cor
 * @return 1 if cor should become active, 0 if it should stay blocked.
 */
int coroutine_is_fd_now_ready(coroutine_scheduler_t scheduler, coroutine_t cor);

#endif
