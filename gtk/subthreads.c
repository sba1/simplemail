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
** subthreads.c
*/

#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "lists.h"
#include "support_indep.h"
#include "subthreads.h"

static GCond *thread_cond;
static GMutex *thread_mutex;

static int input_added;

/* Sockets for IPC */
static int sockets[2];

/** List of all threads */
static struct list thread_list;

/** Mutex for accessing thread list */
static GMutex *thread_list_mutex;

struct ipc_message
{
	int async;
	int string;
	int rc;
	void *function;
	int argcount;
	void *arg1;
	void *arg2;
	void *arg3;
	void *arg4;
};

struct thread_s
{
	struct node node;
	GThread *thread;
};

int init_threads(void)
{
	if (!g_thread_supported ()) g_thread_init (NULL);
	if (!(thread_cond = g_cond_new())) return 0;
	if (!(thread_mutex = g_mutex_new())) return 0;
	list_init(&thread_list);
	if (!(thread_list_mutex = g_mutex_new())) return 0;

	socketpair(PF_LOCAL,SOCK_DGRAM,0,sockets);

	return 1;
}

void cleanup_threads(void)
{
	g_mutex_free(thread_list_mutex);
}

int thread_parent_task_can_contiue(void)
{
	g_mutex_lock(thread_mutex);
	g_cond_signal(thread_cond);
	g_mutex_unlock(thread_mutex);
	return 1;
}

static void thread_input(gpointer data, gint source, GdkInputCondition condition)
{
	int len;
	struct ipc_message msg;

	len = read(source,&msg,sizeof(msg));

	if (len == sizeof(msg))
	{
		int rc = 0;

		switch (msg.argcount)
		{
			case	0: rc = ((int (*)(void))msg.function)();break;
			case	1: rc = ((int (*)(void*))msg.function)(msg.arg1);break;
			case	2: rc = ((int (*)(void*,void*))msg.function)(msg.arg1,msg.arg2);break;
			case	3: rc = ((int (*)(void*,void*,void*))msg.function)(msg.arg1,msg.arg2,msg.arg3);break;
			case	4: rc = ((int (*)(void*,void*,void*,void*))msg.function)(msg.arg1,msg.arg2,msg.arg3,msg.arg4);break;
		}

		if (msg.async)
		{
			if (msg.string)
			{
				free(msg.arg1);
			}
		}	else
		{
			/* synchron call, deliver return code */
			msg.rc = rc;
			write(sockets[0],&msg,sizeof(msg));
		}
	}
}

/***************************************************************************************/

thread_t thread_add(char *thread_name, int (*entry)(void *), void *eudata)
{
	struct thread_s *t;

	if (!(t = malloc(sizeof(*t)))) return NULL;
	memset(t,0,sizeof(*t));

	if ((t->thread = g_thread_create((GThreadFunc)entry,eudata,TRUE,NULL)))
	{
		g_mutex_lock(thread_mutex);
		g_cond_wait(thread_cond,thread_mutex);
		g_mutex_unlock(thread_mutex);

		g_mutex_lock(thread_list_mutex);
		list_insert_tail(&thread_list, &t->node);
		g_mutex_unlock(thread_list_mutex);
		return t;
	}
	return NULL;
}

/***************************************************************************************/

int thread_start(int (*entry)(void*), void *udata)
{
	if (!input_added)
	{
		gtk_input_add_full(sockets[0],GDK_INPUT_READ,thread_input, NULL, NULL, NULL);
		input_added = 1;
	}

	if ((g_thread_create((GThreadFunc)entry,udata,TRUE,NULL)))
	{
		g_mutex_lock(thread_mutex);
		g_cond_wait(thread_cond,thread_mutex);
		g_mutex_unlock(thread_mutex);
		return 1;
	}
	return 0;
}

/***************************************************************************************/

void thread_abort(thread_t thread)
{
	fprintf(stderr, "%s() not implemented yet!\n", __PRETTY_FUNCTION__);
	exit(1);
}

/***************************************************************************************/

/* Call the function synchron, calls timer_callback on the calling process context */
int thread_call_parent_function_sync_timer_callback(void (*timer_callback)(void*), void *timer_data, int millis, void *function, int argcount, ...)
{
	fprintf(stderr, "%s() not implemented yet!\n", __PRETTY_FUNCTION__);
	exit(1);
}

/**************************************************************************
 Call a function in the context of the given thread synchron

 NOTE: Should call thread_handle()
**************************************************************************/
int thread_call_function_sync(thread_t thread, void *function, int argcount, ...)
{
	fprintf(stderr, "%s() not implemented yet!\n", __PRETTY_FUNCTION__);
	exit(1);

/*	int rc;
	void *arg1,*arg2,*arg3,*arg4;
	va_list argptr;

	va_start(argptr,argcount);

	arg1 = va_arg(argptr, void *);
	arg2 = va_arg(argptr, void *);
	arg3 = va_arg(argptr, void *);
	arg4 = va_arg(argptr, void *);

	switch (argcount)
	{
		case	0: return ((int (*)(void))function)();break;
		case	1: return ((int (*)(void*))function)(arg1);break;
		case	2: return ((int (*)(void*,void*))function)(arg1,arg2);break;
		case	3: return ((int (*)(void*,void*,void*))function)(arg1,arg2,arg3);break;
		case	4: return ((int (*)(void*,void*,void*,void*))function)(arg1,arg2,arg3,arg4);break;
	}
*/
	return 0;
}

/**************************************************************************
 Waits until aborted and calls timer_callback periodically. It's possible
 to execute functions on the threads context while in this function.
**************************************************************************/
int thread_wait(void (*timer_callback(void*)), void *timer_data, int millis)
{
	fprintf(stderr, "%s() not implemented yet!\n", __PRETTY_FUNCTION__);
	exit(1);
	return 0;
}

/**************************************************************************
 Pusges an function call in the function queue of the callers task context.
 Return 1 for success else 0.
**************************************************************************/
int thread_push_function(void *function, int argcount, ...)
{
	int rc = 0;
	fprintf(stderr, "%s() not implemented yet!\n", __PRETTY_FUNCTION__);
	exit(1);
	return rc;
}


int thread_call_parent_function_sync(int *success, void *function, int argcount, ...)
{
	struct ipc_message msg;
	va_list argptr;

	va_start(argptr,argcount);
	memset(&msg,0,sizeof(msg));
	msg.async = 0;
	msg.function = function;
	msg.argcount = argcount;
	if (argcount--) msg.arg1 = va_arg(argptr, void *);
	if (argcount--) msg.arg2 = va_arg(argptr, void *);
	if (argcount--) msg.arg3 = va_arg(argptr, void *);
	if (argcount--) msg.arg4 = va_arg(argptr, void *);
	write(sockets[1],&msg,sizeof(msg));
	va_end(argptr);
	read(sockets[1],&msg,sizeof(msg));
	return msg.rc;
}

int thread_call_parent_function_async(void *function, int argcount, ...)
{
	struct ipc_message msg;
	va_list argptr;

	va_start(argptr,argcount);
	memset(&msg,0,sizeof(msg));
	msg.async = 1;
	msg.function = function;
	msg.argcount = argcount;
	if (argcount--) msg.arg1 = va_arg(argptr, void *);
	if (argcount--) msg.arg2 = va_arg(argptr, void *);
	if (argcount--) msg.arg3 = va_arg(argptr, void *);
	if (argcount--) msg.arg4 = va_arg(argptr, void *);
	write(sockets[1],&msg,sizeof(msg));
	va_end(argptr);

	return 0;
}

/* Call the function asynchron and duplicate the first argument which us threaded at a string */
int thread_call_parent_function_async_string(void *function, int argcount, ...)
{
	struct ipc_message msg;
	va_list argptr;

	va_start(argptr,argcount);
	memset(&msg,0,sizeof(msg));
	msg.async = 1;
	msg.string = 1;
	msg.function = function;
	msg.argcount = argcount;
	if (argcount--) msg.arg1 = mystrdup(va_arg(argptr, char *));
	if (argcount--) msg.arg2 = va_arg(argptr, void *);
	if (argcount--) msg.arg3 = va_arg(argptr, void *);
	if (argcount--) msg.arg4 = va_arg(argptr, void *);
	write(sockets[1],&msg,sizeof(msg));
	va_end(argptr);
	return 0;
}

int thread_aborted(void)
{
	return 0;
}


struct semaphore_s
{
	GMutex *mutex;
};

semaphore_t thread_create_semaphore(void)
{
	semaphore_t sem = malloc(sizeof(struct semaphore_s));
	if (sem)
	{
		if (!(sem->mutex = g_mutex_new()))
		{
			free(sem);
			return NULL;
		}
	}
	return sem;
}

void thread_dispose_semaphore(semaphore_t sem)
{
	g_mutex_free(sem->mutex);
	free(sem);
}

void thread_lock_semaphore(semaphore_t sem)
{
	g_mutex_lock(sem->mutex);
}

int thread_attempt_lock_semaphore(semaphore_t sem)
{
	return g_mutex_trylock(sem->mutex);
}

void thread_unlock_semaphore(semaphore_t sem)
{
	g_mutex_unlock(sem->mutex);
}
