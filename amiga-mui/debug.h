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
** $Id$
*/

#ifndef __DEBUG_H
#define __DEBUG_H

/* Debug Macros */

#include <dos.h>
#include <proto/exec.h>

#ifdef _AROS

#undef DEBUG
#define DEBUG 1
#include <aros/debug.h>

#else /* _AROS */

#define bug kprintf

#ifdef MYDEBUG
void kprintf(char *string, ...);
#define D(x) {kprintf("%s/%ld %ld bytes (%s): ", __FILE__, __LINE__, (ULONG)getreg(REG_A7) - (ULONG)FindTask(NULL)->tc_SPLower/*Upper + 4096*/, FindTask(NULL)->tc_Node.ln_Name);(x);};
#else
#define D(x) ;

#endif /* MYDEBUG */

#endif /*_AROS */

#endif /* __DEBUG_H */
