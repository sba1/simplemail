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

#include "ringbuffer.h"
#include "support_indep.h"

/*****************************************************************************/

static ringbuffer_t logg_rb;
static unsigned int logg_id;

struct logg_s
{
	logging_severity_t severity;
	unsigned short tid;
	unsigned short millis;
	unsigned int seconds;
	const char *filename;
	const char *function;
	char *text;
	int line;
	int id;
};

typedef struct logg_s *logg_t;

/*****************************************************************************/

static void logg_rb_free_callback(ringbuffer_t rb, void *mem, int size, void *userdata)
{
}

/*****************************************************************************/

int logg_init(void)
{
	/* Logging is optional */
	logg_rb = ringbuffer_create(64*1024, logg_rb_free_callback, NULL);
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

/*****************************************************************************/

void logg(logging_severity_t severity, int tid, const char *filename, const char *function, int line,
	const char *text, ...)
{
	logg_t logg;
	size_t size;
	unsigned int seconds;
	unsigned int mics;

	if (!logg_rb) return;

	size = sizeof(logg_t) + strlen(text) + 1;
	if (!(logg = ringbuffer_alloc(logg_rb, size)))
		return;

	sm_get_current_time(&seconds, &mics);

	logg->severity = severity;
	logg->tid = tid;
	logg->filename = filename;
	logg->function = function;
	logg->text = (char*)(logg + 1);
	logg->line = line;
	logg->id = logg_id++;
	logg->millis = mics / 1000;
	logg->seconds = seconds;
	strcpy(logg->text, text);
}

/*****************************************************************************/

logg_t logg_next(logg_t current)
{
	return ringbuffer_next(logg_rb, current);
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
