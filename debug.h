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

void set_debug_level(int);

#ifdef NDEBUG
#define SM_DEBUGF(level,x)
#else
extern int __debuglevel;
#define SM_DEBUGF(level,x) do { if (level <= __debuglevel) ARCH_DEBUG(x); } while (0)
#endif

#endif

