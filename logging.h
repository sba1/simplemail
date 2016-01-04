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
 * @file logging.h
 */
#ifndef SM__LOGGING_H
#define SM__LOGGING_H

typedef enum
{
	INFO,
	ERROR
} logging_severity_t;

typedef enum
{
	INT32, /* A signed integer value */
	POINTER, /* A pointer */
	STRING_STATIC, /* String that is not copied */
	STRING, /* String that is copied */
	PASSWORD, /* A password string that is not kept in the log */
	LAST, /* Special marker to indicate the end of the argument list */
} logging_datatype_t;

typedef struct logg_s *logg_t;

/**
 * Initialize the log subsystem.
 *
 * @return whether successful.
 */
int logg_init(void);

/**
 * Shuts down the logging subsystem.
 */
void logg_dispose(void);

/**
 * Appends the given info to the internal log. This is a varargs function. The
 * final entries are pairs of logging_datatype_t and the actual data. The last
 * entries' type should be LAST.
 *
 * @param severity the severity of the log
 * @param tid the thread id
 * @param filename the filename of the caller. The memory pointed to must be
 *  valid until the log is disposed.
 * @param function the function name of the caller. The memory pointed to must be
 *  valid until the log is disposed.
 * @param line the line number
 * @param text the text of the logging. This is currently copied.
 */
void logg(logging_severity_t severity, int tid, const char *filename, const char *function, int line,
	const char *text, ...);

/**
 * Given the log entry, get the next one.
 *
 * @param current
 * @return the next log entry or NULL if there are no more log entries.
 *
 * @note the pointer returned here may become invalid when logg() is called next.
 */
logg_t logg_next(logg_t current);

/**
 * Return the text associated to the logg.
 *
 * @param logg the logg entry
 * @return the text
 */
const char *logg_text(logg_t logg);

/**
 * Return the seconds part of the time stamp of the log entry.
 *
 * @param logg the logg entry
 * @return the seconds
 */
unsigned int logg_seconds(logg_t logg);

/**
 * Return the milli seconds part of the time stamp of the log entry.
 *
 * @param logg the logg entry
 * @return the milli seconds
 */
unsigned int logg_millis(logg_t logg);

/*****************************************************************************/

#define SM_LOG_INFO(level,text) \
	do { \
		static const char filename[] __attribute__((used, section("LOGMODULES"))) = "LOGMODULE:" __FILE__;\
		logg(INFO, 0, __FILE__, __PRETTY_FUNCTION__, __LINE__, text);\
	} while (0)

/*****************************************************************************/

/**
 * Function type for logging related update callbacks.
 *
 * @param userdata
 */
typedef void (*logg_update_callback_t)(void *userdata);

typedef struct logg_listener_s *logg_listener_t;

/**
 * Add a new listener for receiving update notifications.
 *
 * @param logg_update_callback
 * @param userdata
 * @return the listner or NULL on a failure.
 */
logg_listener_t logg_add_update_listener(logg_update_callback_t logg_update_callback, void *userdata);

/**
 * Remove the given listener.
 *
 * @param listener
 */
void logg_remove_update_listener(logg_listener_t listener);

#endif
