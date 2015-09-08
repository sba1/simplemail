/**
 * coroutines_internal.h - header file for simple coroutines for SimpleMail.
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
 * @file coroutines_internal.h
 */

#ifndef SM__COROUTINES_INTERNAL_H
#define SM__COROUTINES_INTERNAL_H

#ifndef SM__COROUTINES_H
#include "coroutines.h"
#endif

#ifndef SM__LISTS_H
#include "lists.h"
#endif

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
	/** Contains all ready coroutines. Elements are of type coroutine_t */
	struct coroutines_list coroutines_ready_list;

	/** Contains all waiting coroutines. Elements are of type coroutine_t */
	struct coroutines_list waiting_coroutines_list;

	/** Contains all finished coroutines. Elements are of type coroutine_t */
	struct coroutines_list finished_coroutines_list;

	/**
	 * Function that is invoked to wait or poll for a next event
	 *
	 * @return if there were events that potentially were blocked
	 */
	int (*wait_for_event)(coroutine_scheduler_t sched, int poll, void *udata);

	/** User data passed to wait_for_event() */
	void *wait_for_event_udata;
};

/**
 * Initializes a coroutines list.
 *
 * @param list the list to initialize.
 */
void coroutines_list_init(struct coroutines_list *list);

/**
 * Returns the first element of a coroutines list.
 *
 * @param list the coroutines list.
 * @return the first element or NULL if the list is empty
 */
coroutine_t coroutines_list_first(struct coroutines_list *list);

/**
 * Returns the next coroutine within the same list.
 *
 * @param c the coroutine whose successor shall be determined
 * @return the successor of c or NULL if c was the last element
 */
coroutine_t coroutines_next(coroutine_t c);

#endif
