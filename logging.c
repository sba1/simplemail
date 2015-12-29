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

/*****************************************************************************/

static ringbuffer_t logg_rb;

struct logg_s
{
	logging_severity_t severity;
	int tid;
	const char *filename;
	const char *function;
	char *text;
	int line;
};

typedef struct logg_s *logg_t;

/*****************************************************************************/

int logg_init(void)
{
	/* Logging is optional */
	logg_rb = ringbuffer_create(64*1024, NULL, NULL);
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

	if (!logg_rb) return;

	size = sizeof(logg_t) + strlen(text) + 1;
	if (!(logg = ringbuffer_alloc(logg_rb, size)))
		return;

	logg->severity = severity;
	logg->tid = tid;
	logg->filename = filename;
	logg->function = function;
	logg->text = (char*)(logg + 1);
	logg->line = line;
	strcpy(logg->text, text);
}

/*****************************************************************************/

logg_t logg_next(logg_t current)
{
	return ringbuffer_next(logg_rb, current);
}
