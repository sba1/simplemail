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

#ifndef SM__SUBTHREADS_H
#define SM__SUBTHREADS_H

int init_threads(void);
void cleanup_threads(void);
int thread_parent_task_can_contiue(void);
int thread_start(int (*entry)(void*), void *udata);
void thread_abort(void);
int thread_call_parent_function_sync(void *function, int argcount, ...);

void thread_handle(void);
unsigned long thread_mask(void);

#endif
