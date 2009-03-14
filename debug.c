/***************************************************************************
 SimpleMail - Copyright (C) 2000 Hynek Schlawack and Sebastian Bauer

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
***************************************************************************/

/**
 * @file debug.c
 */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "archdebug.h"
#include "debug.h"
#include "hash.h"
#include "subthreads.h"
#include "support.h" /* sm_put_on_serial_line() */
#include "support_indep.h"

#ifndef DEFAULT_DEBUG_LEVEL
#define DEFAULT_DEBUG_LEVEL 0
#endif

int __debuglevel = DEFAULT_DEBUG_LEVEL;

static semaphore_t debug_sem;
static FILE *debug_file;

/** Contains the names of modules of which debugging information should be considered */
static struct hash_table debug_modules_table;

/** Indicates whether table should be really used */
static int debug_use_modules_table;

/** List that keeps track of all resources */
static struct list tracking_list;

static int tracking_elements;

static int tracking_initialized;
static int inside_tracking;

struct tracked_resource
{
	struct node node;

	/** @brief the tracked resource. First part of the key. */
	void *resource;

	/** @brief the class of the tracked resource. Second part of the key */
	char *class;

	char *args;

	/** @brief the origin of the resource allocation */
	char *filename;
	char *function;
	int line;

	/** @brief architecture dependent debug information */
	struct bt *bt;
};

/*******************************************************
 Sets the debug level
********************************************************/
void debug_set_level(int ndl)
{
	SM_DEBUGF(0,("Debuglevel set to %d\n",ndl));
	__debuglevel = ndl;
}

/*******************************************************
 Sets the debug output
********************************************************/
void debug_set_out(char *out)
{
	if (debug_file) fclose(debug_file);
	debug_file = fopen(out,"a+");
}

/*******************************************************
 Private function. Used to insert a module.
********************************************************/
static void debug_insert_module(char *mod)
{
	int len;
	char *newmod;

	len = strlen(mod);
	if (len > 2)
	{
		if (mod[len-1]=='c' && mod[len-2]=='.')
			hash_table_insert(&debug_modules_table,mod,1);
	}

	if ((newmod = malloc(len+3)))
	{
		strcpy(newmod,mod);
		strcat(newmod,".c");
		hash_table_insert(&debug_modules_table,newmod,1);
	}
}

/**
 * Sets the modules (modules is comma separated)
 * @param modules
 */
void debug_set_modules(char *modules)
{
	char *mod;

	if ((mod = mystrdup(modules)))
	{
		char *begin = mod;
		char *end;

		hash_table_init(&debug_modules_table,9,NULL);

		while ((end = strchr(begin,',')))
		{
			*end = 0;
			debug_insert_module(begin);
			begin = end+1;
		}
		debug_insert_module(begin);

		debug_use_modules_table = 1;
	}
}

/**
 * Initialize debug sub system.
 *
 * @return
 */
int debug_init(void)
{
	if (!(debug_sem = thread_create_semaphore()))
		return 0;
	list_init(&tracking_list);
	tracking_initialized = 1;
	return 1;
}

/**
 * Deinitializes the debug subsystem.
 */
void debug_deinit(void)
{
	if (tracking_initialized)
	{
		struct tracked_resource *tr;

		int num = 0;

		inside_tracking = 1;

		if (tracking_elements > 0)
			__debug_print("There were %d resources that got not freed!\n",tracking_elements);

		tr = (struct tracked_resource*)list_last(&tracking_list);
		while (tr && num < 6)
		{
			char *call = tr->class;
			char *more;
			char *contents = (char*)tr->resource;
			if (!call) call = tr->class;

			__debug_print("Resource %p of call %s(%s) not freed! Origin: %s/%d\n",tr->resource, tr->class,tr->args,tr->filename,tr->line);
			if ((more = arch_debug_bt2string(tr->bt)))
			{
				/* Print out architecture dependent string, but line for line */
				char *lf;
				char *str = more;

				while ((lf = strchr(str,'\n')))
				{
					*lf = 0;
					__debug_print(str);
					__debug_print("\n");
					str = lf + 1;
				}

				free(more);
			}

			tr = (struct tracked_resource*)node_prev(&tr->node);
			num++;
		}

		inside_tracking = 0;
	}

	thread_dispose_semaphore(debug_sem);
}

/**
 * Returns the tracked resource or NULL if the resource couldn't be found.
 *
 * @param res
 * @param class
 * @return
 */
struct tracked_resource *debug_find_tracked_resource(void *res, char *class)
{
	struct tracked_resource *tr;

	tr = (struct tracked_resource*)list_last(&tracking_list);
	while (tr)
	{
		if (tr->resource == res)
		{
			if ((tr->class == class) ||
				(tr->class && class && !strcmp(tr->class,class)))
			{
				return tr;
			}
		}
		tr = (struct tracked_resource*)node_prev(&tr->node);
	}

	return NULL;
}

/**
 * Track the resource.
 *
 * @param res
 * @param class the name of the tracked function.
 * @param arguments to the function.
 * @param filename
 * @param function
 * @param line
 */
void debug_track(void *res, char *class, char *args, char *filename, char *function, int line)
{
	if (tracking_initialized)
	{
		thread_lock_semaphore(debug_sem);
		if (!inside_tracking)
		{
			struct tracked_resource *tr;

			inside_tracking = 1;

			if ((tr = (struct tracked_resource*)malloc(sizeof(*tr))))
			{
				tr->resource = res;
				tr->class = class;
				tr->args = strdup(args);
				tr->filename = filename;
				tr->function = function;
				tr->line = line;
				tr->bt = arch_debug_get_bt();
				list_insert_tail(&tracking_list,&tr->node);
				tracking_elements++;
			}

			inside_tracking = 0;
		}
		thread_unlock_semaphore(debug_sem);
	}
}

/**
 * Remove the given resource from the tracking list.
 *
 * @param res
 * @param class the name of the tracked function
 * @param call
 * @param args
 * @param filename
 * @param function
 * @param line
 */
void debug_untrack(void *res, char *class, char *call, char *args, char *filename, char *function, int line)
{
	if (tracking_initialized)
	{
		thread_lock_semaphore(debug_sem);
		if (!inside_tracking)
		{
			struct tracked_resource *tr;

			inside_tracking = 1;

			if ((tr = debug_find_tracked_resource(res,class)))
			{
				node_remove(&tr->node);
				arch_debug_free_bt(tr->bt);
				free(tr->args);
				free(tr);
				tracking_elements--;
			} else
			{
				char *cl = class;
				if (!cl) cl = "UNKNOWN";

				__debug_print("Trying to untrack untracked resource at %p of class %s. The call was %s(%s) at %s/%d\n",res,class,call,args,filename,line);
			}

			inside_tracking = 0;
		}
		thread_unlock_semaphore(debug_sem);
	}
}

/**
 * Check, whether we want to debug this.
 *
 * @param file
 * @param line
 * @return
 */
int debug_check(const char *file, int line)
{
	if (!debug_use_modules_table)
		return 1;

	if (hash_table_lookup(&debug_modules_table,file))
		return 1;
	return 0;
}

/**
 * Use this if debug output should be sequential
 */
void __debug_begin(void)
{
	thread_lock_semaphore(debug_sem);
}

/**
 * Use this after debugging output had been emitting.
 */
void __debug_end(void)
{
	thread_unlock_semaphore(debug_sem);
}

/**
 * Emits debugging
 *
 * @param fmt
 */
void __debug_print(const char *fmt, ...)
{
  va_list ap;
  char buf[512];

  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  if (debug_file) fputs(buf,debug_file);
  else sm_put_on_serial_line(buf);
}
