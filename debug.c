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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "debug.h"
#include "subthreads.h"
#include "support.h" /* sm_put_on_serial_line() */

#ifndef DEFAULT_DEBUG_LEVEL
#define DEFAULT_DEBUG_LEVEL 0
#endif

int __debuglevel = DEFAULT_DEBUG_LEVEL;


static semaphore_t debug_sem;
static FILE *debug_file;

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
 Use this if debug output should be sequentiel
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
