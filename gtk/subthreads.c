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
#include <unistd.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "subthreads.h"

static GCond *thread_cond;
static GMutex *thread_mutex;

static int input_added;
static GMutex *input_mutex;
static GMutex *input_mutex2;
static GCond *input_cond;
static int input_msg;

int init_threads(void)
{
	if (!g_thread_supported ()) g_thread_init (NULL);
	if (!(thread_cond = g_cond_new())) return 0;
	if (!(thread_mutex = g_mutex_new())) return 0;
	return 1;
}

void cleanup_threads(void)
{
}

int thread_parent_task_can_contiue(void)
{
	g_mutex_lock(thread_mutex);
	g_cond_signal(thread_cond);
	g_mutex_unlock(thread_mutex);
	return 1;
}

static void thread_idle(gpointer data)
{
	GTimeVal time;

//	printf("1: idle\n");

	g_mutex_lock(input_mutex);

	if (input_msg)
	{
		printf("message arrived\n");
		input_msg = 0;
		g_cond_signal(input_cond);
	}
	g_mutex_unlock(input_mutex);

//	printf("1: endidle\n");

	g_thread_yield();

//	printf("1: endidle2\n");

}

int thread_start(int (*entry)(void*), void *eudata)
{
	if (!input_added)
	{
		input_mutex = g_mutex_new();
		input_cond = g_cond_new();
		gtk_idle_add(thread_idle,NULL);
		input_added = 1;
	}

	if ((g_thread_create(entry,eudata,TRUE,NULL)))
	{
		g_mutex_lock(thread_mutex);
		g_cond_wait(thread_cond,thread_mutex);
		g_mutex_unlock(thread_mutex);
		return 1;
	}
	return 0;
}

void thread_abort(void)
{
}

int thread_call_parent_function_sync(void *function, int argcount, ...)
{
	printf("sync\n");
#if 0
	int rc;
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
#endif

	return 0;
}

int thread_call_parent_function_async(void *function, int argcount, ...)
{
//	printf("async\n");
//	write(pipes[1],"test",4);


	g_mutex_lock(input_mutex);
//	printf("%d\n",input_msg);
	input_msg = 1;
	g_cond_wait(input_cond,input_mutex);
//	printf("%d\n",input_msg);
	g_cond_free(input_cond);
	input_cond = NULL;
	g_mutex_unlock(input_mutex);

	g_thread_yield();

#if 0
	int rc;
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
#endif
	return 0;
}

/* Call the function asynchron and duplicate the first argument which us threaded at a string */
int thread_call_parent_function_async_string(void *function, int argcount, ...)
{
	printf("async string\n");

#if 0
	int rc;
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
#endif
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
			return NULL;
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
