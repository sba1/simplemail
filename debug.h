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
** debug.h
*/

#ifndef SM__SMDEBUG_H
#define SM__SMDEBUG_H

#ifndef SM__ARCH_H
#include "arch.h"
#endif

int debug_init(void);
void debug_deinit(void);

void debug_set_level(int);
void debug_set_out(char *);
void debug_set_modules(char *modules);

/**
 * Return all known modules that can be debugged, i.e., that emit some
 * debug logging.
 *
 * @return the NULL terminated array of all recognized submodules.
 */
const char * const *debug_get_loggable_modules(void);

/* For resource tracking */
void debug_track(void *res, char *cl, char *args, char *filename, char *function, int line);
void debug_untrack(void *res, char *cl, char *call, char *args, char *filename, char *function, int line);

int debug_check(const char *file, int line);

void __debug_begin(void);
void __debug_print(const char *fmt, ...);
void __debug_end(void);

#ifdef NODEBUG
#define SM_DEBUGF(level,x)
#define SM_ENTER
#define SM_LEAVE
#define SM_RETURN(val,type)
#else
extern int __debuglevel;
#ifdef __GNUC__
#define SM_DEBUGF(level,x) do { static const char filename[] __attribute__((used, section("DEBUGMODULES"))) = "MODULE:" __FILE__; if (level <= __debuglevel && debug_check(__FILE__,__LINE__)) { __debug_begin(); __debug_print("%s/%d [%s()] (%s) => ",__FILE__,__LINE__,__PRETTY_FUNCTION__,ARCH_DEBUG_EXTRA); __debug_print x; __debug_end(); }} while (0)
#else
#ifdef __SASC
#define SM_DEBUGF(level,x) do { if (level <= __debuglevel && debug_check(__FILE__,__LINE__)) { __debug_begin(); __debug_print("%s/%d [%s()] (%s) => ",__FILE__,__LINE__,__FUNC__,ARCH_DEBUG_EXTRA); __debug_print x; __debug_end(); }} while (0)
#else
#define SM_DEBUGF(level,x) do { if (level <= __debuglevel && debug_check(__FILE__,__LINE__)) { __debug_begin(); __debug_print("%s/%d (%s) => ",__FILE__,__LINE__,ARCH_DEBUG_EXTRA); __debug_print x; __debug_end(); }} while (0)
#endif
#endif
#define SM_ENTER SM_DEBUGF(20,("Entered function\n"))
#define SM_LEAVE SM_DEBUGF(20,("Leave function\n"))
#define SM_RETURN(val,type) do {SM_DEBUGF(20,("Leave (" type ")\n",val)); return (val);} while (0)
#endif

#ifndef NODEBUG
#if defined(__SASC) && defined(DEBUG_RESTRACK)

#ifndef _STDLIB_H
#include <stdlib.h>
#endif

int sm_snprintf(char *buf, int n, const char *fmt, ...);

static void *__malloc_tracked(size_t size, char *file, char *function, int line)
{
	void *m = malloc(size);
	char argstr[10];
	sm_snprintf(argstr,sizeof(argstr),"%d",size);
	if (m) debug_track(m,"malloc", argstr, file,function,line);
	return m;
}
#define malloc(size) __malloc_tracked(size, __FILE__,__FUNC__,__LINE__)

static void __free_tracked(void *mem, char *file, char *function, int line)
{
	char argstr[10];
	if (!mem) return;
	sm_snprintf(argstr,sizeof(argstr),"%p",mem);
	debug_untrack(mem,"malloc","free",argstr,file,function,line);
	free(mem);
}

#define free(mem) __free_tracked(mem, __FILE__,__FUNC__,__LINE__)

static void *__realloc_tracked(void *mem, size_t size, char *file, char *function, int line)
{
	char argstr[16];
	void *m;
	sm_snprintf(argstr,sizeof(argstr),"%p,%d",mem,size);
	m = realloc(mem,size);
	if (m)
	{
		if (mem)
			debug_untrack(mem,"malloc","realloc",argstr,file,function,line);
		debug_track(m,"malloc",argstr,file,function,line);
	}
	return m;
}

#define realloc(mem,size) __realloc_tracked(mem, size, __FILE__, __FUNC__, __LINE__)

#endif
#endif

#endif

