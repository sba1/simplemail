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
** subthreads.h
*/

#ifndef SM__SUBTHREADS_H
#define SM__SUBTHREADS_H

#define THREAD_FUNCTION(x) ((int (*)(void*))x)

int init_threads(void);
void cleanup_threads(void);

/* thread handling */
struct thread_s;
typedef struct thread_s * thread_t; /* opaque type */

int thread_parent_task_can_contiue(void);
int thread_start(int (*entry)(void*), void *udata);
thread_t thread_add(char *thread_name, int (*entry)(void *), void *eudata);
void thread_abort(thread_t thread); /* NULL means default thread */
int thread_aborted(void);
int thread_call_parent_function_sync(void *function, int argcount, ...);
int thread_call_parent_function_async(void *function, int argcount, ...);
int thread_call_parent_function_async_string(void *function, int argcount, ...);
int thread_call_parent_function_sync_timer_callback(void (*timer_callback(void*)), void *timer_data, int millis, void *function, int argcount, ...);

/* semaphore handling */
struct semaphore_s;
typedef struct semaphore_s * semaphore_t; /* opaque type */

semaphore_t thread_create_semaphore(void);
void thread_dispose_semaphore(semaphore_t sem);
int thread_attempt_lock_semaphore(semaphore_t sem);
void thread_lock_semaphore(semaphore_t sem);
void thread_unlock_semaphore(semaphore_t sem);

/* Only releavant in AmigaOS so this should be moved to somewhere else */
void thread_handle(void);
unsigned long thread_mask(void);

#endif
