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
#ifndef SM__AMIPROC_H
#define SM__AMIPROC_H

/* The following functions provide a convenient way to spawn
** a new subprocess with full autoinitialization, access to
** all the normal library functions, etc.
*/

struct AmiProcMsg *AmiProc_Start(int (*FuncPtr)(void *), void *UserData);
int AmiProc_Wait(struct AmiProcMsg *);
int AmiProc_Check(struct AmiProcMsg *start_msg);

#endif
