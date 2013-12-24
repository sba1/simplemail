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

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "debug.h"
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

	GMainContext *context;
	GMainLoop *main_loop;
};

static struct thread_s main_thread;

/***************************************************************************************/

int init_threads(void)
{
	/* TODO: Proper bailout on failure */
	if (!g_thread_supported ()) g_thread_init (NULL);
	if (!(thread_cond = g_cond_new())) return 0;
	if (!(thread_mutex = g_mutex_new())) return 0;
	list_init(&thread_list);
	if (!(thread_list_mutex = g_mutex_new())) return 0;

	socketpair(PF_LOCAL,SOCK_DGRAM,0,sockets);

	main_thread.thread = g_thread_self();
	if (!(main_thread.context = g_main_context_new()))
		return 0;
	if (!(main_thread.main_loop = g_main_loop_new(main_thread.context, FALSE)))
		return 0;

	return 1;
}

/***************************************************************************************/

void cleanup_threads(void)
{
	struct thread_s *t;

	SM_ENTER;

	while (1)
	{
		g_mutex_lock(thread_list_mutex);
		if ((t = (struct thread_s*)list_first(&thread_list)))
		{
			GThread *gt;
			gt = t->thread;
			thread_abort(t);
			g_mutex_unlock(thread_list_mutex);
			g_thread_join(gt);
		} else
		{
			g_mutex_unlock(thread_list_mutex);
			break;
		}
	}

	g_main_loop_unref(main_thread.main_loop);
	g_main_context_unref(main_thread.context);

	g_mutex_free(thread_list_mutex);

	SM_LEAVE;
}

/***************************************************************************************/

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

/** Structure that is passed to newly created threads via thread_add() */
struct thread_add_data
{
	int (*entry)(void *);
	void *eudata;
	struct thread_s *thread;
};

/**
 * The entry function of thread_add().
 *
 * @param udata
 * @return
 */
static gpointer thread_add_entry(gpointer udata)
{
	struct thread_add_data *tad = (struct thread_add_data*)udata;
	struct thread_s *t = tad->thread;

	/* TODO: Catch errors and inform parent task */
	t->context = g_main_context_new();
	t->main_loop = g_main_loop_new(t->context, FALSE);

	tad->entry(tad->eudata);

	g_mutex_lock(thread_list_mutex);
	node_remove(&t->node);
	g_mutex_unlock(thread_list_mutex);

	g_main_loop_unref(t->main_loop);
	g_main_context_unref(t->context);
	return NULL;
}

thread_t thread_add(char *thread_name, int (*entry)(void *), void *eudata)
{
	struct thread_s *t;
	struct thread_add_data tad;

	if (!(t = malloc(sizeof(*t)))) return NULL;
	memset(t,0,sizeof(*t));

	tad.entry = entry;
	tad.eudata = eudata;
	tad.thread = t;

	g_mutex_lock(thread_list_mutex);
	list_insert_tail(&thread_list, &t->node);
	g_mutex_unlock(thread_list_mutex);

	if ((t->thread = g_thread_create(thread_add_entry,&tad,TRUE,NULL)))
	{
		g_mutex_lock(thread_mutex);
		g_cond_wait(thread_cond,thread_mutex);
		g_mutex_unlock(thread_mutex);
		return t;
	}
	/* TODO: Remove thread */
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

static gboolean thread_abort_entry(gpointer udata)
{
	struct thread_s *t = (struct thread_s*)udata;
	g_main_loop_quit(t->main_loop);
	return 0;
}

void thread_abort(thread_t thread)
{
	g_main_context_invoke(thread->context, thread_abort_entry, thread);
}

/***************************************************************************************/

int thread_call_parent_function_sync_timer_callback(void (*timer_callback)(void*), void *timer_data, int millis, void *function, int argcount, ...)
{
	fprintf(stderr, "%s() not implemented yet!\n", __PRETTY_FUNCTION__);
	exit(1);
}

/***************************************************************************************/

#define THREAD_CALL_FUNCTION_SYNC_DATA_NUM_ARGS 6

struct thread_call_function_sync_data
{
	int (*function)(void);
	int argcount;
	void *arg[THREAD_CALL_FUNCTION_SYNC_DATA_NUM_ARGS];

	uintptr_t rc;

	thread_t caller;
};

/* FIXME: Note that if the args are not passed in a register, but e.g., on the stack this doesn't need to
 * work depending on the ABI
 */
static gboolean thread_call_function_sync_entry(gpointer user_data)
{
	struct thread_call_function_sync_data *data = (struct thread_call_function_sync_data*)user_data;
	uintptr_t rc;

	switch (data->argcount)
	{
		case	0: rc = data->function(); break;
		case	1: rc = ((int (*)(void*))data->function)(data->arg[0]);break;
		case	2: rc = ((int (*)(void*,void*))data->function)(data->arg[0],data->arg[1]);break;
		case	3: rc = ((int (*)(void*,void*,void*))data->function)(data->arg[0],data->arg[1],data->arg[2]);break;
		case	4: rc = ((int (*)(void*,void*,void*,void*))data->function)(data->arg[0],data->arg[1],data->arg[2],data->arg[3]);break;
		case	5: rc = ((int (*)(void*,void*,void*,void*,void*))data->function)(data->arg[0],data->arg[1],data->arg[2],data->arg[3],data->arg[4]);break;
		case	6: rc = ((int (*)(void*,void*,void*,void*,void*,void*))data->function)(data->arg[0],data->arg[1],data->arg[2],data->arg[3],data->arg[4],data->arg[5]);break;
	}
	data->rc = rc;
	g_main_loop_quit(data->caller->main_loop);

	return 0;
}

static int thread_call_function_sync_v(thread_t thread, uintptr_t *rc, void *function, int argcount, va_list argptr)
{
	struct thread_call_function_sync_data data;
	int i;

	assert(argcount < THREAD_CALL_FUNCTION_SYNC_DATA_NUM_ARGS);

	data.function = (int (*)(void))function;
	data.argcount = argcount;
	data.caller = thread_get();

	for (i=0; i < argcount; i++)
		data.arg[i] = va_arg(argptr, void *);

	g_main_context_invoke(thread->context, thread_call_function_sync_entry, &data);
	g_main_loop_run(data.caller->main_loop);

	if (rc) *rc = data.rc;

	return 0;
}

int thread_call_function_sync(thread_t thread, void *function, int argcount, ...)
{
	int rc;

	va_list argptr;

	va_start(argptr,argcount);
	rc = thread_call_function_sync_v(thread, NULL, function, argcount, argptr);
	va_end(argptr);
	return rc;
}

/***************************************************************************************/

/* FIXME: Note that if the args are not passed in a register, but e.g., on the stack this doesn't need to
 * work depending on the ABI
 */
static gboolean thread_call_function_async_entry(gpointer user_data)
{
	struct thread_call_function_sync_data *data = (struct thread_call_function_sync_data*)user_data;

	switch (data->argcount)
	{
		case	0: data->function(); break;
		case	1: ((int (*)(void*))data->function)(data->arg[0]);break;
		case	2: ((int (*)(void*,void*))data->function)(data->arg[0],data->arg[1]);break;
		case	3: ((int (*)(void*,void*,void*))data->function)(data->arg[0],data->arg[1],data->arg[2]);break;
		case	4: ((int (*)(void*,void*,void*,void*))data->function)(data->arg[0],data->arg[1],data->arg[2],data->arg[3]);break;
		case	5: ((int (*)(void*,void*,void*,void*,void*))data->function)(data->arg[0],data->arg[1],data->arg[2],data->arg[3],data->arg[4]);break;
		case	6: ((int (*)(void*,void*,void*,void*,void*,void*))data->function)(data->arg[0],data->arg[1],data->arg[2],data->arg[3],data->arg[4],data->arg[5]);break;
	}
	free(data);
	return 0;
}

int thread_call_function_async(thread_t thread, void *function, int argcount, ...)
{
	struct thread_call_function_sync_data *data;
	int i;

	va_list argptr;

	assert(argcount < THREAD_CALL_FUNCTION_SYNC_DATA_NUM_ARGS);

	if (!(data = malloc(sizeof(*data))))
		return 0;
	memset(data, 0, sizeof(*data));

	va_start(argptr,argcount);

	data->function = (int (*)(void))function;
	data->argcount = argcount;

	for (i=0; i < argcount; i++)
		data->arg[i] = va_arg(argptr, void *);

	va_end (argptr);

	g_main_context_invoke(thread->context, thread_call_function_async_entry, data);

	return 1;
}

/***************************************************************************************/

thread_t thread_get_main(void)
{
	return (thread_t)&main_thread;
}

/***************************************************************************************/

thread_t thread_get(void)
{
	struct thread_s *t;
	GThread *gt = g_thread_self();

	if (gt == main_thread.thread)
		return &main_thread;

	g_mutex_lock(thread_list_mutex);
	t = (struct thread_s*)list_first(&thread_list);
	while (t)
	{
		if (t->thread == gt) break;
		t = (struct thread_s*)node_next(&t->node);
	}
	g_mutex_unlock(thread_list_mutex);

	assert(t);
	return t;
}

/***************************************************************************************/

struct thread_wait_timer_entry_data
{
	void (*timer_callback)(void*);
	void *timer_data;
};

static gboolean thread_wait_timer_entry(gpointer udata)
{
	struct thread_wait_timer_entry_data *data = (struct thread_wait_timer_entry_data*)udata;
	data->timer_callback(data->timer_data);
	return 1;
}

int thread_wait(void (*timer_callback(void*)), void *timer_data, int millis)
{
	struct thread_wait_timer_entry_data data;
	struct thread_s *t;

	memset(&data, 0, sizeof(data));

	t = thread_get();

	data.timer_callback = (void (*)(void*))timer_callback;
	data.timer_data = timer_data;

	if (timer_callback)
	{
		GSource *s = g_timeout_source_new(millis);
		g_source_set_callback(s, thread_wait_timer_entry, &data, NULL);
		g_source_attach(s, t->context);
		g_source_unref(s);
	}

	g_main_loop_run(t->main_loop);

	return 0;
}

/***************************************************************************************/

static gboolean thread_push_function_entry(gpointer user_data)
{
	struct thread_call_function_sync_data *data = (struct thread_call_function_sync_data*)user_data;

	switch (data->argcount)
	{
		case	0: data->function(); break;
		case	1: ((int (*)(void*))data->function)(data->arg[0]);break;
		case	2: ((int (*)(void*,void*))data->function)(data->arg[0],data->arg[1]);break;
		case	3: ((int (*)(void*,void*,void*))data->function)(data->arg[0],data->arg[1],data->arg[2]);break;
		case	4: ((int (*)(void*,void*,void*,void*))data->function)(data->arg[0],data->arg[1],data->arg[2],data->arg[3]);break;
		case	5: ((int (*)(void*,void*,void*,void*,void*))data->function)(data->arg[0],data->arg[1],data->arg[2],data->arg[3],data->arg[4]);break;
		case	6: ((int (*)(void*,void*,void*,void*,void*,void*))data->function)(data->arg[0],data->arg[1],data->arg[2],data->arg[3],data->arg[4],data->arg[5]);break;
	}
	free(data);
	return FALSE;
}

int thread_push_function(void *function, int argcount, ...)
{
	struct thread_call_function_sync_data *data;
	int i;

	va_list argptr;

	if (!(data = malloc(sizeof(*data))))
		return 0;
	memset(data, 0, sizeof(*data));

	assert(argcount < THREAD_CALL_FUNCTION_SYNC_DATA_NUM_ARGS);

	va_start(argptr,argcount);

	data->function = (int (*)(void))function;
	data->argcount = argcount;

	for (i=0; i < argcount; i++)
		data->arg[i] = va_arg(argptr, void *);

	va_end (argptr);


	GSource *s = g_timeout_source_new(1);
	g_source_set_callback(s, thread_push_function_entry, data, NULL);
	g_source_attach(s, thread_get()->context);
	g_source_unref(s);

	return 1;
}

/***************************************************************************************/

int thread_push_function_delayed(int millis, void *function, int argcount, ...)
{
	struct thread_call_function_sync_data *data;
	int i;

	va_list argptr;

	if (!(data = malloc(sizeof(*data))))
		return 0;
	memset(data, 0, sizeof(*data));

	assert(argcount < THREAD_CALL_FUNCTION_SYNC_DATA_NUM_ARGS);

	va_start(argptr,argcount);

	data->function = (int (*)(void))function;
	data->argcount = argcount;

	for (i=0; i < argcount; i++)
		data->arg[i] = va_arg(argptr, void *);

	va_end (argptr);


	GSource *s = g_timeout_source_new(millis);
	g_source_set_callback(s, thread_push_function_entry, data, NULL);
	g_source_attach(s, thread_get()->context);
	g_source_unref(s);

	return 1;
}

/***************************************************************************************/

int thread_call_parent_function_sync(int *success, void *function, int argcount, ...)
{
	uintptr_t rc;
	int s;

	va_list argptr;

	va_start(argptr,argcount);
	s = thread_call_function_sync_v(&main_thread, &rc, function, argcount, argptr);
	va_end(argptr);
	if (success)
		*success = s;

	return rc;
}

/***************************************************************************************/

int thread_call_parent_function_async(void *function, int argcount, ...)
{
	fprintf(stderr,"not implemented!");
	exit(1);
}

/***************************************************************************************/

/* FIXME: Note that if the args are not passed in a register, but e.g., on the stack this doesn't need to
 * work depending on the ABI
 */
static gboolean thread_call_function_async_string_entry(gpointer user_data)
{
	struct thread_call_function_sync_data *data = (struct thread_call_function_sync_data*)user_data;

	switch (data->argcount)
	{
		case	0: data->function(); break;
		case	1: ((int (*)(void*))data->function)(data->arg[0]);break;
		case	2: ((int (*)(void*,void*))data->function)(data->arg[0],data->arg[1]);break;
		case	3: ((int (*)(void*,void*,void*))data->function)(data->arg[0],data->arg[1],data->arg[2]);break;
		case	4: ((int (*)(void*,void*,void*,void*))data->function)(data->arg[0],data->arg[1],data->arg[2],data->arg[3]);break;
		case	5: ((int (*)(void*,void*,void*,void*,void*))data->function)(data->arg[0],data->arg[1],data->arg[2],data->arg[3],data->arg[4]);break;
		case	6: ((int (*)(void*,void*,void*,void*,void*,void*))data->function)(data->arg[0],data->arg[1],data->arg[2],data->arg[3],data->arg[4],data->arg[5]);break;
	}

	if (data->argcount)
		free(data->arg[0]);
	free(data);

	return 0;
}

int thread_call_parent_function_async_string(void *function, int argcount, ...)
{
	struct thread_call_function_sync_data *data;
	int i;

	struct thread_s *thread;

	va_list argptr;

	thread = &main_thread;

	assert(argcount < THREAD_CALL_FUNCTION_SYNC_DATA_NUM_ARGS);

	if (!(data = malloc(sizeof(*data))))
		return 0;
	memset(data, 0, sizeof(*data));

	va_start(argptr,argcount);

	data->function = (int (*)(void))function;
	data->argcount = argcount;

	if (argcount)
		data->arg[0] = strdup(va_arg(argptr, char *));

	for (i=1; i < argcount; i++)
		data->arg[i] = va_arg(argptr, void *);

	va_end (argptr);

	g_main_context_invoke(thread->context, thread_call_function_async_string_entry, data);
}

/***************************************************************************************/

int thread_aborted(void)
{
	return 0;
}

/***************************************************************************************/

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
