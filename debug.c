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

/*
** smdebug.c
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
 
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

/* Contains the names of modules of which debugging information should be considered at last */ 
static struct hash_table debug_modules_table;

/* Indicates whether table should be really used */
static int debug_use_modules_table;

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

/*******************************************************
 Sets the modules (modules is comma)
********************************************************/
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

/*******************************************************
 Initialize debug
********************************************************/
int debug_init(void)
{
	if (!(debug_sem = thread_create_semaphore()))
		return 0;
	return 1;
}

/*******************************************************
 Deinitialize debug
********************************************************/
void debug_deinit(void)
{
	thread_dispose_semaphore(debug_sem);
}

/*******************************************************
 Alows finer debug controll.
********************************************************/
int debug_check(const char *file, int line)
{
	if (!debug_use_modules_table)
		return 1;

	if (hash_table_lookup(&debug_modules_table,file))
		return 1;
	return 0;
}

/*******************************************************
 Use this if debug output should be sequential
********************************************************/
void __debug_begin(void)
{
	thread_lock_semaphore(debug_sem);
}

/*******************************************************
 
********************************************************/
void __debug_end(void)
{
	thread_unlock_semaphore(debug_sem);
}

/*******************************************************
 Sets the debug output
********************************************************/
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
