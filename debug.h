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

void __debug_begin(void);
void __debug_print(const char *fmt, ...);
void __debug_end(void);

#ifdef NODEBUG
#define SM_DEBUGF(level,x)
#else
extern int __debuglevel;
#ifdef __GNUC__
#define SM_DEBUGF(level,x) do { if (level <= __debuglevel) { __debug_begin(); __debug_print("%s/%d [%s()] (%s) => ",__FILE__,__LINE__,__PRETTY_FUNCTION__,ARCH_DEBUG_EXTRA); __debug_print x; __debug_end(); }} while (0)
#else
#define SM_DEBUGF(level,x) do { if (level <= __debuglevel) { __debug_begin(); __debug_print("%s/%d (%s) => ",__FILE__,__LINE__,ARCH_DEBUG_EXTRA); __debug_print x; __debug_end(); }} while (0)
#endif
#define SM_ENTER SM_DEBUGF(20,("Entered function\n"))
#define SM_LEAVE SM_DEBUGF(20,("Leave function\n"))
#define SM_RETURN(val,type) do {SM_DEBUGF(20,("Leave (" type ")\n",val)); return (val);} while (0)
#endif

#endif

