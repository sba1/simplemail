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

/* Define DONT_USE_THREADS to disable the multithreading */
/*#define DONT_USE_THREADS*/

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>
#include <clib/alib_protos.h>

#include "lists.h"
#include "compiler.h"
#include "subthreads.h"

#include "subthreads_amiga.h" /* struct thread_s */

/* #define MYDEBUG */
#include "debug.h"

/* TODO:
    add thread_call_function_async_callback() which calls the functions asynchron but 
    if the function returns another function is called on the calling process
*/

/*** Timer ***/

struct timer
{
	struct MsgPort *timer_port;
	struct timerequest *timer_req;
	struct timerequest *new_timer_req;
	int timer_send;
	int open;
};

/**************************************************************************
 Cleanup timer
**************************************************************************/
static void timer_cleanup(struct timer *timer)
{
	if (timer->timer_send)
	{
		AbortIO((struct IORequest*)timer->new_timer_req);
		WaitIO((struct IORequest*)timer->new_timer_req);
	}
	if (timer->new_timer_req) FreeVec(timer->new_timer_req);
	if (timer->open) CloseDevice((struct IORequest *)timer->timer_req);
	if (timer->timer_req) DeleteIORequest((struct IORequest*)timer->timer_req);
	if (timer->timer_port) DeleteMsgPort(timer->timer_port);
}

/**************************************************************************
 Initialize timer
**************************************************************************/
static int timer_init(struct timer *timer)
{
	memset(timer,0,sizeof(*timer));
	if ((timer->timer_port = CreateMsgPort()))
	{
		if ((timer->timer_req = (struct timerequest *) CreateIORequest(timer->timer_port, sizeof(struct timerequest))))
		{
			if (!OpenDevice(TIMERNAME, UNIT_VBLANK, (struct IORequest *)timer->timer_req, 0))
			{
				timer->open = 1;
				timer->new_timer_req = (struct timerequest *) AllocVec(sizeof(struct timerequest), MEMF_PUBLIC);
				if (timer->new_timer_req) return 1;
			}
		}
	}
	timer_cleanup(timer);
	return 0;
}

/**************************************************************************
 Return mask of timer signal
**************************************************************************/
static ULONG timer_mask(struct timer *timer)
{
	return 1UL << timer->timer_port->mp_SigBit;
}


/**************************************************************************
 Send the given timer if not already being sent
**************************************************************************/
static void timer_send_if_not_sent(struct timer *timer, int millis)
{
	if (!timer->timer_send)
	{
	    *timer->new_timer_req = *timer->timer_req;
			timer->new_timer_req->tr_node.io_Command = TR_ADDREQUEST;
	    timer->new_timer_req->tr_time.tv_secs = millis/1000;
	    timer->new_timer_req->tr_time.tv_micro = millis%1000;
		  SendIO((struct IORequest *)timer->new_timer_req);
			timer->timer_send = 1;
	}
}

/*** ThreadMessage ***/

struct ThreadMessage
{
	struct Message msg;
	int startup; /* message is startup message */
	thread_t thread; /* The thread which it is the startup message for */
	int (*function)(void);
	int async;
	int argcount;
	void *arg1,*arg2,*arg3,*arg4;
	int result;
};

/* the port allocated by the main task */
static struct MsgPort *thread_port;

/* the default thread */
static thread_t default_thread;

/* list of all subthreads */
struct list thread_list;
struct thread_node
{
	struct node node;
	thread_t thread;
};

static void thread_handle_execute_function_message(struct ThreadMessage *tmsg);

/**************************************************************************
 Remove the thread which has replied tis given tmsg
**************************************************************************/
static void thread_remove(struct ThreadMessage *tmsg)
{
	struct thread_node *node = (struct thread_node*)list_first(&thread_list);
	while (node)
	{
		if (node->thread == tmsg->thread)
		{
			if (node->thread->is_default) default_thread = NULL;
			D(bug("Got startup message of 0x%lx back\n",node->thread));
			node_remove(&node->node);
			FreeVec(tmsg);
			FreeVec(node);
			return;
		}
		node = (struct thread_node*)node_next(&node->node);
	}
}

/**************************************************************************
 Iniitalize the thread system
**************************************************************************/
int init_threads(void)
{
	static struct thread_s main_thread;
	if ((thread_port = CreateMsgPort()))
	{
		main_thread.process = (struct Process*)FindTask(NULL);
		main_thread.is_main = 1;
		main_thread.thread_port = thread_port;
		NewList(&main_thread.push_list);
		FindTask(NULL)->tc_UserData = &main_thread;

		list_init(&thread_list);
		return 1;
	}
	return 0;
}

/**************************************************************************
 Cleanup the thread system. Will abort every thread
**************************************************************************/
void cleanup_threads(void)
{
	/* It's safe to call this if no thread is actually running */
	struct thread_node *node = (struct thread_node*)list_first(&thread_list);
	while (node)
	{
		/* FIXME: This could cause a possible race condition if the task is already removed
		 * but not yet removed in the list */
		Signal(&node->thread->process->pr_Task, SIGBREAKF_CTRL_C);
		node = (struct thread_node*)node_next(&node->node);
	}
	thread_abort(NULL);

	/* wait until every task has been removed */
	while (list_first(&thread_list))
	{
		struct ThreadMessage *tmsg;

		WaitPort(thread_port);
		while ((tmsg = (struct ThreadMessage *)GetMsg(thread_port)))
		{
			if (tmsg->startup)
				thread_remove(tmsg);
		}
	}

	if (thread_port)
	{
		DeleteMsgPort(thread_port);
		thread_port = NULL;
	}
}

/**************************************************************************
 Returns the mask of the thread port of the current process
**************************************************************************/
unsigned long thread_mask(void)
{
	return (1UL << thread_port->mp_SigBit);
}

/**************************************************************************
 Handle a new message in the send to the current process
**************************************************************************/
void thread_handle(void)
{
	struct ThreadMessage *tmsg;

	while ((tmsg = (struct ThreadMessage *)GetMsg(thread_port)))
	{
		D(bug("Received Message: 0x%lx\n",tmsg));

		if (tmsg->startup)
		{
			thread_remove(tmsg);
			continue;
		}

		thread_handle_execute_function_message(tmsg);
	}
}

/* the entry point for the subthread */
static SAVEDS void thread_entry(void)
{
	struct Process *proc;
	struct ThreadMessage *msg;
	int (*entry)(void *);
	thread_t thread;

	BPTR dirlock;

	D(bug("Waiting for startup message\n"));

	proc = (struct Process*)FindTask(NULL);	
	WaitPort(&proc->pr_MsgPort);
	msg = (struct ThreadMessage *)GetMsg(&proc->pr_MsgPort);

	/* Set the task's UserData field to strore per thread data */
	thread = msg->thread;
	if (thread->thread_port = CreateMsgPort())
	{
		D(bug("Subthreaded created port at 0x%lx\n",thread->thread_port));

		NewList(&thread->push_list);

		proc->pr_Task.tc_UserData = thread;
		entry = (int(*)(void*))msg->function;

		dirlock = DupLock(proc->pr_CurrentDir);
		if (dirlock)
		{
			BPTR odir = CurrentDir(dirlock);
			entry(msg->arg1);
			UnLock(CurrentDir(odir));
		}
		DeleteMsgPort(thread->thread_port);
		thread->thread_port = NULL;
	}

	Forbid();
	D(bug("Replying startup message\n"));
	ReplyMsg((struct Message*)msg);
}

/**************************************************************************
 Informs the parent task that it can continue
**************************************************************************/
int thread_parent_task_can_contiue(void)
{
#ifndef DONT_USE_THREADS
	struct ThreadMessage *msg = (struct ThreadMessage *)AllocVec(sizeof(struct ThreadMessage),MEMF_PUBLIC|MEMF_CLEAR);
	D(bug("Thread can continue\n"));
	if (msg)
	{
		struct Process *p = (struct Process*)FindTask(NULL);
		msg->msg.mn_ReplyPort = &p->pr_MsgPort;
		msg->msg.mn_Length = sizeof(struct ThreadMessage);

		PutMsg(thread_port,&msg->msg);
		/* Message is freed by parent task */
		return 1;
	}
	return 0;
#else
	return 1;
#endif
}

/**************************************************************************
 Runs the given function in a newly created thread under the given name
**************************************************************************/
static thread_t thread_start_new(char *thread_name, int (*entry)(void*), void *eudata)
{
#ifndef DONT_USE_THREADS
	struct thread_s *thread = (struct thread_s*)AllocVec(sizeof(*thread),MEMF_PUBLIC|MEMF_CLEAR);
	if (thread)
	{
		struct ThreadMessage *msg;

		if ((msg = (struct ThreadMessage *)AllocVec(sizeof(*msg),MEMF_PUBLIC|MEMF_CLEAR)))
		{
			BPTR in,out;
			msg->msg.mn_Node.ln_Type = NT_MESSAGE;
			msg->msg.mn_ReplyPort = thread_port;
			msg->msg.mn_Length = sizeof(*msg);
			msg->startup = 1;
			msg->thread = thread;
			msg->function = (int (*)(void))entry;
			msg->arg1 = eudata;
	
			in = Open("CONSOLE:",MODE_NEWFILE);
			if (!in) in = Open("NIL:",MODE_NEWFILE);
			out = Open("CONSOLE:",MODE_NEWFILE);
			if (!out) out = Open("NIL:",MODE_NEWFILE);
	
			if (in && out)
			{
				thread->process = CreateNewProcTags(
							NP_Entry,      thread_entry,
							NP_StackSize,  16384,
							NP_Name,       thread_name,
							NP_Priority,   -1,
							NP_Input,      in,
							NP_Output,     out,
							TAG_END);
	
				if (thread->process)
				{
					struct ThreadMessage *thread_msg;
					D(bug("Thread started at 0x%lx\n",thread));
					PutMsg(&thread->process->pr_MsgPort,(struct Message*)msg);
					WaitPort(thread_port);
					thread_msg = (struct ThreadMessage *)GetMsg(thread_port);
					if (thread_msg == msg)
					{
						/* This was the startup message, so something has failed */
						D(bug("Got startup message back. Something went wrong\n"));
						FreeVec(thread_msg);
						FreeVec(thread);
						return NULL;
					} else
					{
						/* This was the "parent task can continue message", we don't reply it
						 * but we free it here (although it wasn't allocated by this task) */
						D(bug("Got \"parent can continue\"\n"));
						FreeVec(thread_msg);
						return thread;
					}
				}
			}
			if (in) Close(in);
			if (out) Close(out);
			FreeVec(msg);
		}
	}
	return NULL;
#else
	entry(eudata);
	return NULL;
#endif
}

/**************************************************************************
 Runs a given function in a newly created thread under the given name which
 in linked into a internal list.
**************************************************************************/
thread_t thread_add(char *thread_name, int (*entry)(void *), void *eudata)
{
	struct thread_node *thread_node = (struct thread_node*)AllocVec(sizeof(struct thread_node),MEMF_PUBLIC|MEMF_CLEAR);
	if (thread_node)
	{
		thread_t thread = thread_start_new(thread_name, entry, eudata);
		if (thread)
		{
			thread_node->thread = thread;
			list_insert_tail(&thread_list,&thread_node->node);
			return thread;
		}
		FreeVec(thread_node);
	}
	return NULL;
}


/**************************************************************************
 Start a thread as a default sub thread. This function will be removed
 in the future.
**************************************************************************/
int thread_start(int (*entry)(void*), void *eudata)
{
	/* We allow only one subtask for the moment */
	if (default_thread) return 0;
	if ((default_thread = thread_add("SimpleMail - Default subthread", entry, eudata)))
	{
		default_thread->is_default = 1;
		return 1;
	}
	return 0;
}

/**************************************************************************
 Abort the given thread or if NULL is given the default subthread.
**************************************************************************/
void thread_abort(thread_t thread_to_abort)
{
	if (!thread_to_abort)
	{
		if (default_thread)
			Signal(&default_thread->process->pr_Task, SIGBREAKF_CTRL_C);
	} else
	{
		/* FIXME: This could cause a possible race condition if the task is already removed
		 * but not yet removed in the list */
		Signal(&thread_to_abort->process->pr_Task, SIGBREAKF_CTRL_C);
	}
}


/**************************************************************************
 Returns ThreadMessage filled with the given parameters. You can manipulate
 the returned message to be async or something else
**************************************************************************/
static struct ThreadMessage *thread_create_message(void *function, int argcount, va_list argptr)
{
	struct ThreadMessage *tmsg = (struct ThreadMessage *)AllocVec(sizeof(struct ThreadMessage),MEMF_PUBLIC|MEMF_CLEAR);
	if (tmsg)
	{
		struct MsgPort *subthread_port = ((struct thread_s*)(FindTask(NULL)->tc_UserData))->thread_port;

		tmsg->msg.mn_ReplyPort = subthread_port;
		tmsg->msg.mn_Length = sizeof(struct ThreadMessage);
		tmsg->function = (int (*)(void))function;
		tmsg->argcount = argcount;

		if (argcount--)
		{
			tmsg->arg1 = va_arg(argptr, void *);
			if (argcount--)
			{
				tmsg->arg2 = va_arg(argptr, void *);
				if (argcount--)
				{
					tmsg->arg3 = va_arg(argptr, void *);
					if (argcount--)
					{
						tmsg->arg4 = va_arg(argptr, void *);
					}
				}
			}
		}
		tmsg->async = 0;
	}
	return tmsg;
}

/**************************************************************************
 This will handle the execute function message
**************************************************************************/
static void thread_handle_execute_function_message(struct ThreadMessage *tmsg)
{
	if (tmsg->startup) return;

	if (tmsg->function)
	{
		switch(tmsg->argcount)
		{
			case	0: tmsg->result = tmsg->function();break;
			case	1: tmsg->result = ((int (*)(void*))tmsg->function)(tmsg->arg1);break;
			case	2: tmsg->result = ((int (*)(void*,void*))tmsg->function)(tmsg->arg1,tmsg->arg2);break;
			case	3: tmsg->result = ((int (*)(void*,void*,void*))tmsg->function)(tmsg->arg1,tmsg->arg2,tmsg->arg3);break;
			case	4: tmsg->result = ((int (*)(void*,void*,void*,void*))tmsg->function)(tmsg->arg1,tmsg->arg2,tmsg->arg3,tmsg->arg4);break;
		}
	}
	if (tmsg->async)
	{
		D(bug("Freeing Message at 0x%lx\n",tmsg));
		if (tmsg->async == 2 && tmsg->argcount >= 1 && tmsg->arg1) FreeVec(tmsg->arg1);
		FreeVec(tmsg);
	}
	else
	{
		D(bug("Repling Message at 0x%lx\n",tmsg));
		ReplyMsg(&tmsg->msg);
	}
}

/**************************************************************************
 Call a function in context of the parent task synchron
**************************************************************************/
int thread_call_parent_function_sync(void *function, int argcount, ...)
{
#ifndef DONT_USE_THREADS
	va_list argptr;
	int rc = 0;
	struct ThreadMessage *tmsg;

	va_start(argptr,argcount);

	if ((tmsg = thread_create_message(function, argcount, argptr)))
	{
		struct MsgPort *subthread_port = tmsg->msg.mn_ReplyPort;
		int ready = 0;

		PutMsg(thread_port,&tmsg->msg);

		while (!ready)
		{
			struct Message *msg;
			WaitPort(subthread_port);

			while ((msg = GetMsg(subthread_port)))
			{
				if (msg == &tmsg->msg) ready = 1;
				else
				{
					thread_handle_execute_function_message((struct ThreadMessage*)msg);
				}
			}
		}

		rc = tmsg->result;
		FreeVec(tmsg);
	}

	va_end (arg_ptr);
	return rc;
#else
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

	return 0;
#endif
}

/**************************************************************************
 Call a function in the context of the given thread synchron
 
 NOTE: Should call thread_handle()
**************************************************************************/
int thread_call_function_sync(thread_t thread, void *function, int argcount, ...)
{
#ifndef DONT_USE_THREADS
	va_list argptr;
	int rc = 0;
	struct ThreadMessage *tmsg;

	va_start(argptr,argcount);

	if ((tmsg = thread_create_message(function, argcount, argptr)))
	{
		struct MsgPort *subthread_port = tmsg->msg.mn_ReplyPort;
		int ready = 0;

		PutMsg(thread->thread_port,&tmsg->msg);

		while (!ready)
		{
			struct Message *msg;
			WaitPort(subthread_port);

			while ((msg = GetMsg(subthread_port)))
			{
				if (msg == &tmsg->msg) ready = 1;
				else
				{
					thread_handle_execute_function_message((struct ThreadMessage*)msg);
				}
			}
		}

		rc = tmsg->result;
		FreeVec(tmsg);
	}

	va_end (arg_ptr);
	return rc;
#else
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

	return 0;
#endif
}

/* Call the function synchron, calls timer_callback on the calling process context */
int thread_call_parent_function_sync_timer_callback(void (*timer_callback(void*)), void *timer_data, int millis, void *function, int argcount, ...)
{
#ifndef DONT_USE_THREADS
	va_list argptr;
	int rc = 0;
	struct ThreadMessage *tmsg;

	va_start(argptr,argcount);

	if ((tmsg = thread_create_message(function, argcount, argptr)))
	{
		struct MsgPort *subthread_port = tmsg->msg.mn_ReplyPort;
		struct timer timer;

		if (timer_init(&timer))
		{
			int ready = 0;

			/* we only accept positive values */
			if (millis < 0) millis = 0;

			/* now send the message */
			PutMsg(thread_port,&tmsg->msg);
	
			/* while the parent task should execute the message
			 * we regualiy call the given callback function */
			while (!ready)
			{
				ULONG timer_m = timer_mask(&timer);
				ULONG proc_m = 1UL << subthread_port->mp_SigBit;
				ULONG mask;

				if (millis)
					timer_send_if_not_sent(&timer,millis);
	
				mask = Wait(timer_m|proc_m);
				if (mask & timer_m)
				{
					if (timer_callback)
						timer_callback(timer_data);
					timer.timer_send = 0;
				}
	
				if (mask & proc_m)
				{
					struct Message *msg;
					while ((msg = GetMsg(subthread_port)))
					{
						if (msg == &tmsg->msg) ready = 1;
					}
				}
			}
			rc = tmsg->result;
			timer_cleanup(&timer);
		}
		FreeVec(tmsg);
	}
	va_end (arg_ptr);
	return rc;
#else

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

	return 0;
#endif
}


/**************************************************************************
 Waits until aborted and calls timer_callback periodically. It's possible
 to execute functions on the threads context while in this function. 
**************************************************************************/
void thread_wait(void (*timer_callback(void*)), void *timer_data, int millis)
{
	struct timer timer;
	if (timer_init(&timer))
	{
		thread_t this_thread = ((struct thread_s*)(FindTask(NULL)->tc_UserData));
		struct MsgPort *this_thread_port = this_thread->thread_port;
		if (millis < 0) millis = 0;

		while (1)
		{
			ULONG proc_m = 1UL << this_thread_port->mp_SigBit;
			ULONG timer_m = timer_mask(&timer);
			ULONG mask;
			struct ThreadMessage *tmsg;

			if (millis) timer_send_if_not_sent(&timer,millis);
	
			mask = Wait(timer_m|proc_m|SIGBREAKF_CTRL_C);
			if (mask & timer_m)
			{
				if (timer_callback)
					timer_callback(timer_data);
				timer.timer_send = 0;
			}

			if (mask & proc_m)
			{
				struct ThreadMessage *tmsg;
				while ((tmsg = (struct ThreadMessage*)GetMsg(this_thread_port)))
				{
					 thread_handle_execute_function_message(tmsg);
				}
			}

			/* Now perform any pending push calls */
			while ((tmsg = (struct ThreadMessage*)RemHead(&this_thread->push_list)))
			{
				if (tmsg->function)
				{
					switch(tmsg->argcount)
					{
						case	0: tmsg->function();break;
						case	1: ((int (*)(void*))tmsg->function)(tmsg->arg1);break;
						case	2: ((int (*)(void*,void*))tmsg->function)(tmsg->arg1,tmsg->arg2);break;
						case	3: ((int (*)(void*,void*,void*))tmsg->function)(tmsg->arg1,tmsg->arg2,tmsg->arg3);break;
						case	4: ((int (*)(void*,void*,void*,void*))tmsg->function)(tmsg->arg1,tmsg->arg2,tmsg->arg3,tmsg->arg4);break;
					}
				}
				FreeVec(tmsg);
			}

			if (mask & SIGBREAKF_CTRL_C) break;
		}
		timer_cleanup(&timer);
	}
}

/**************************************************************************
 Pusges an function call in the function queue of the callers task context.
 Return 1 for success else 0.
**************************************************************************/
int thread_push_function(void *function, int argcount, ...)
{
	int rc = 0;
	struct ThreadMessage *tmsg;
	va_list argptr;

	va_start(argptr,argcount);

	if ((tmsg = thread_create_message(function, argcount, argptr)))
	{
		thread_t this_thread = ((struct thread_s*)(FindTask(NULL)->tc_UserData));
		AddTail(&this_thread->push_list,&tmsg->msg.mn_Node);
	}

	va_end (arg_ptr);
	return rc;
}

/* Call the function asynchron */
int thread_call_parent_function_async(void *function, int argcount, ...)
{
#ifndef DONT_USE_THREADS
	struct ThreadMessage *tmsg = (struct ThreadMessage *)AllocVec(sizeof(struct ThreadMessage),MEMF_PUBLIC|MEMF_CLEAR);
	if (tmsg)
	{
		struct Process *p = (struct Process*)FindTask(NULL);
		va_list argptr;

		va_start(argptr,argcount);

		tmsg->msg.mn_ReplyPort = &p->pr_MsgPort;
		tmsg->msg.mn_Length = sizeof(struct ThreadMessage);
		tmsg->function = (int (*)(void))function;
		tmsg->argcount = argcount;
		tmsg->arg1 = va_arg(argptr, void *);/*(*(&argcount + 1));*/
		tmsg->arg2 = va_arg(argptr, void *);/*(void*)(*(&argcount + 2));*/
		tmsg->arg3 = va_arg(argptr, void *);/*(void*)(*(&argcount + 3));*/
		tmsg->arg4 = va_arg(argptr, void *);/*(void*)(*(&argcount + 4));*/
		tmsg->async = 1;

		va_end (arg_ptr);

		PutMsg(thread_port,&tmsg->msg);
		return 1;
	}

	return 0;
#else
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

	return 0;
#endif
}

/* Call the function asynchron and duplicate the first argument which us threaded at a string */
int thread_call_parent_function_async_string(void *function, int argcount, ...)
{
#ifndef DONT_USE_THREADS
	struct ThreadMessage *tmsg = (struct ThreadMessage *)AllocVec(sizeof(struct ThreadMessage),MEMF_PUBLIC|MEMF_CLEAR);
	if (tmsg)
	{
		struct Process *p = (struct Process*)FindTask(NULL);
		va_list argptr;

		va_start(argptr,argcount);

		tmsg->msg.mn_ReplyPort = &p->pr_MsgPort;
		tmsg->msg.mn_Length = sizeof(struct ThreadMessage);
		tmsg->function = (int (*)(void))function;
		tmsg->argcount = argcount;
		tmsg->arg1 = va_arg(argptr, void *);/*(*(&argcount + 1));*/
		tmsg->arg2 = va_arg(argptr, void *);/*(void*)(*(&argcount + 2));*/
		tmsg->arg3 = va_arg(argptr, void *);/*(void*)(*(&argcount + 3));*/
		tmsg->arg4 = va_arg(argptr, void *);/*(void*)(*(&argcount + 4));*/
		tmsg->async = 2;

		va_end (arg_ptr);

		if (tmsg->arg1 && argcount >= 1)
		{
			STRPTR str = AllocVec(strlen((char*)tmsg->arg1)+1,MEMF_PUBLIC);
			if (str)
			{
				strcpy(str,(char*)tmsg->arg1);
				tmsg->arg1 = (void*)str;
			} else
			{
				FreeVec(tmsg);
				return 0;
			}
		}

		PutMsg(thread_port,&tmsg->msg);
		return 1;
	}

	return 0;
#else
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

	return 0;
#endif
}

/* Check if thread is aborted and return 1 if so */
int thread_aborted(void)
{
	return !!CheckSignal(SIGBREAKF_CTRL_C);
}

struct semaphore_s
{
	struct SignalSemaphore sem;
};

semaphore_t thread_create_semaphore(void)
{
	semaphore_t sem = malloc(sizeof(struct semaphore_s));
	if (sem)
	{
		InitSemaphore(&sem->sem);
	}
	return sem;
}

void thread_dispose_semaphore(semaphore_t sem)
{
	free(sem);
}

void thread_lock_semaphore(semaphore_t sem)
{
	ObtainSemaphore(&sem->sem);
}

int thread_attempt_lock_semaphore(semaphore_t sem)
{
	return (int)AttemptSemaphore(&sem->sem);
}

void thread_unlock_semaphore(semaphore_t sem)
{
	ReleaseSemaphore(&sem->sem);
}

