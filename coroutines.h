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
 * @file coroutines.h
 *
 * This implements the coroutines basic functionality. This is highly inspired
 * by http://www.chiark.greenend.org.uk/~sgtatham/coroutines.html.
 */

#ifndef SM__COROUTINES_H
#define SM__COROUTINES_H

#ifndef SM__LISTS_H
#include "lists.h"
#endif

struct coroutine;
struct coroutine_basic_context;

/** The return values of a coroutine */
typedef enum {
	COROUTINE_YIELD,
	COROUTINE_WAIT,
	COROUTINE_DONE
} coroutine_return_t;

/** A single coroutine */
typedef struct coroutine *coroutine_t;
typedef coroutine_return_t (*coroutine_entry_t)(struct coroutine_basic_context *arg);

struct coroutine_scheduler;
typedef struct coroutine_scheduler *coroutine_scheduler_t;

/**
 * The basic context of a coroutine. This should be embedded in a higher
 * context.
 */
struct coroutine_basic_context
{
	/** The scheduler that is responsible for this context */
	coroutine_scheduler_t scheduler;

	/** The state that will be executed next for this coroutine */
	int next_state;

	/** The socket fd for a waiting coroutine */
	int socket_fd;

	/** Whether we wait for a reading or writing fd */
	int write_mode;

	/** Another coroutine we are waiting for */
	coroutine_t other;

	/** Function that checks if a switch from wait to ready is possible */
	int (*is_now_ready)(coroutine_scheduler_t scheduler, coroutine_t cor);
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

/**
 * Create a new scheduler for coroutines with a custom wait for event callback.
 *
 * @return the scheduler nor NULL for an error.
 */
coroutine_scheduler_t coroutine_scheduler_new_custom(int (*wait_for_event)(coroutine_scheduler_t sched, int poll, void *udata), void *udata);

/**
 * Execute the current set of ready coroutines.
 *
 * @param scheduler
 */
void coroutine_schedule_ready(coroutine_scheduler_t scheduler);

/**
 * Dispose the given scheduler. Does not check if there are any coroutines
 * left.
 *
 * @param scheduler defines the scheduler to be disposed.
 */
void coroutine_scheduler_dispose(coroutine_scheduler_t scheduler);

/**
 * Add a new coroutine to the scheduler.
 *
 * @param scheduler the schedule that should take care of the coroutine.
 * @param entry the coroutine's entry
 * @param context the coroutine's context
 * @return the coroutine just added or NULL for an error.
 */
coroutine_t coroutine_add(coroutine_scheduler_t scheduler, coroutine_entry_t entry, struct coroutine_basic_context *context);


/**
 * Checks whether the given blocked coroutine becomes now active due to some fd conditions.
 *
 * @param scheduler
 * @param cor
 * @return 1 if cor should become active, 0 if it should stay blocked.
 */
int coroutine_is_fd_now_ready(coroutine_scheduler_t scheduler, coroutine_t cor);

/**
 * Schedule all coroutines.
 *
 * @param scheduler the scheduler
 */
void coroutine_schedule(coroutine_scheduler_t scheduler);

#endif
