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

#ifndef SM__SUBTHREADS_AMIGA_H
#define SM__SUBTHREADS_AMIGA_H

#ifndef EXEC_LISTS_H
#include <exec/lists.h>
#endif

struct thread_s
{
	struct Process *process;
	int is_main;
	int is_default;

	struct MsgPort *thread_port;

	struct Library *socketlib;
	int socketlib_opencnt;

	struct List push_list;
};

#endif
