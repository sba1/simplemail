/**
 * logging.h - Logging framework for SimpleMail.
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
 * @file logging.c
 */

#include "logging.h"

#include <string.h>

#include "lists.h"
#include "ringbuffer.h"
#include "support_indep.h"

/*****************************************************************************/

static ringbuffer_t logg_rb;

/**
 * All loggs with smaller id are ignored when browsing via logg_next().
 */
static unsigned int logg_start_id;

/**
 * The global logg options.
 */
static logg_options_t logg_options;

#if __STDC_VERSION__ >= 201112L
_Static_assert(SEVERITY_LAST <= 4, "Please fix bit width of severity field in logg_s struct.");
#endif

struct logg_s
{
	logging_severity_t severity:2;
	unsigned int line:30;

	unsigned short tid;
	unsigned short millis;
	unsigned int seconds;
	const char *filename;
	const char *function;
	char *text;
};

typedef struct logg_s *logg_t;

/**
 * Lock.
 */
static void logg_lock(void)
{
	if (logg_options.lock)
		logg_options.lock(logg_options.userdata);
}

/**
 * Unlock.
 */
static void logg_unlock(void)
{
	if (logg_options.unlock)
		logg_options.unlock(logg_options.userdata);
}

/*****************************************************************************/

void logg_clear(void)
{
	unsigned int last_id = 0;
	logg_t logg = NULL;

	while ((logg = logg_next(logg)))
		last_id = logg_id(logg);
	logg_start_id = last_id;
}

/*****************************************************************************/

logg_t logg_next(logg_t current)
{
	logg_t next;

	if (!current)
	{
		logg_t first;

		logg_lock();

		first = ringbuffer_next(logg_rb, NULL);
		while (first)
		{
			if (ringbuffer_entry_id(first) >= logg_start_id)
				return first;
		}

		logg_unlock();
		return NULL;
	}
	if (!(next = ringbuffer_next(logg_rb, current)))
		logg_unlock();
	return next;
}

/*****************************************************************************/

const char *logg_text(logg_t logg)
{
	return logg->text;
}

/*****************************************************************************/

unsigned int logg_seconds(logg_t logg)
{
	return logg->seconds;
}

/*****************************************************************************/

unsigned int logg_millis(logg_t logg)
{
	return logg->millis;
}

/*****************************************************************************/

unsigned int logg_id(logg_t logg)
{
	return ringbuffer_entry_id(logg);
}


/*****************************************************************************/

const char *logg_filename(logg_t logg)
{
	return logg->filename;
}

/*****************************************************************************/

const char *logg_function(logg_t logg)
{
	return logg->function;
}

/*****************************************************************************/

int logg_line(logg_t logg)
{
	return logg->line;
}

/*****************************************************************************/

struct logg_listener_s
{
	struct node node;
	logg_update_callback_t callback;
	void *userdata;
};

static struct list logg_update_listener_list;

/*****************************************************************************/

logg_listener_t logg_add_update_listener(logg_update_callback_t logg_update_callback, void *userdata)
{
	logg_listener_t l = malloc(sizeof(*l));
	if (!l) return NULL;
	memset(l, 0, sizeof(*l));

	l->callback = logg_update_callback;
	l->userdata = userdata;
	logg_lock();
	list_insert_tail(&logg_update_listener_list, &l->node);
	logg_unlock();
	return l;
}

/*****************************************************************************/

void logg_remove_update_listener(logg_listener_t listener)
{
	logg_lock();
	node_remove(&listener->node);
	logg_unlock();
	free(listener);
}

/*****************************************************************************/

static void logg_call_update_listener(void)
{
	logg_listener_t l = (logg_listener_t)list_first(&logg_update_listener_list);
	while (l)
	{
		logg_listener_t n = (logg_listener_t)node_next(&l->node);
		l->callback(l->userdata);
		l = n;
	}
}

/*****************************************************************************/

static void logg_rb_free_callback(ringbuffer_t rb, void *mem, int size, void *userdata)
{
	/* No need to lock as this is only called as a consequence when calling the
	 * logg() which already locked.
	 */
	logg_call_update_listener();
}

/*****************************************************************************/

void logg(logging_severity_t severity, int tid, const char *filename, const char *function, int line,
	const char *text, ...)
{
	logg_t logg;
	size_t size;
	unsigned int seconds;
	unsigned int mics;

	if (!logg_rb) return;
	if (!text) return;

	size = sizeof(*logg) + strlen(text) + 1;
	logg_lock();
	if (!(logg = ringbuffer_alloc(logg_rb, size)))
	{
		logg_unlock();
		return;
	}

	sm_get_current_time(&seconds, &mics);

	logg->severity = severity;
	logg->tid = tid;
	logg->filename = filename;
	logg->function = function;
	logg->text = (char*)(logg + 1);
	logg->line = line;
	logg->millis = mics / 1000;
	logg->seconds = seconds;
	strcpy(logg->text, text);

	logg_call_update_listener();

	logg_unlock();
}

/*****************************************************************************/

int logg_init(logg_options_t *options)
{
	/* Logging is optional */
	logg_rb = ringbuffer_create(64*1024, logg_rb_free_callback, NULL);
	list_init(&logg_update_listener_list);

	if (options)
	{
		logg_options = *options;
	} else
	{
		memset(&logg_options, 0, sizeof(logg_options));
	}
	return 1;
}

/*****************************************************************************/

void logg_dispose(void)
{
	if (logg_rb)
	{
		ringbuffer_dispose(logg_rb);
		logg_rb = NULL;
	}
}
