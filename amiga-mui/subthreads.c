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

#ifndef __USE_OLD_TIMEVAL__
#define __USE_OLD_TIMEVAL__
#endif

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
#include "debug.h"
#include "subthreads.h"

#include "subthreads_amiga.h" /* struct thread_s */

/* #define MYDEBUG */
#include "amigadebug.h"

/* TODO:
    add thread_call_function_async_callback() which calls the functions asynchron but
    if the function returns another function is called on the calling process
*/

/**
 * The context for the timer.
 */
struct timer
{
	struct MsgPort *timer_port;
	struct timerequest *timer_req;
	struct timerequest *new_timer_req;
	int timer_send;
	int open;
};

/**
 * Cleanup timer.
 * Reverse the effects of timer_init.
 *
 * @param timer
 */
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

/***************************************************************************************/

/**
 * Initialize timer the given timer.
 *
 * @param timer
 * @return
 */
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

/***************************************************************************************/

/**
 * Return mask of timer signal of the associated timer.
 *
 * @param timer
 * @return
 */
static ULONG timer_mask(struct timer *timer)
{
	return 1UL << timer->timer_port->mp_SigBit;
}

/***************************************************************************************/

/**
 * Send the given timer if not already being sent.
 *
 * @param timer
 * @param millis
 */
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

/***************************************************************************************/


/**
 * The message that is passed around here.
 */
struct ThreadMessage
{
	struct Message msg;
	int startup; /* message is startup message */
	thread_t thread; /* The thread which it is the startup message for */
	int parent_task_can_continue;
	int (*function)(void);
	int async;
	int argcount;
	void *arg1,*arg2,*arg3,*arg4,*arg5,*arg6;
	int result;
	int called;
};

/*** TimerMessage ***/
struct TimerMessage
{
	struct timerequest time_req;
	struct ThreadMessage *thread_msg;
	struct MinNode node;
};

/** the thread port allocated by the main task */
static struct MsgPort *main_thread_port;

/* the main thread */
static struct thread_s main_thread;

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

/***************************************************************************************/

/**
 * Remove the thread which has replied its given tmsg
 *
 * @param tmsg
 */
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

/***************************************************************************************/

/**
 * Initialize the timer for the given thread.
 *
 * @param thread
 * @return
 */
static int thread_init_timer(struct thread_s *thread)
{
	if ((thread->timer_port = CreateMsgPort()))
	{
		if ((thread->timer_req = (struct timerequest *) CreateIORequest(thread->timer_port, sizeof(struct timerequest))))
		{
			if (!OpenDevice(TIMERNAME, UNIT_VBLANK, (struct IORequest *)thread->timer_req, 0))
			{
				NewList((struct List*)&thread->timer_request_list);
				return 1;
			}
			DeleteIORequest((struct IORequest*)thread->timer_req);
			thread->timer_req = NULL;
		}
		DeleteMsgPort(thread->timer_port);
		thread->timer_port = NULL;
	}
	return 0;
}

/***************************************************************************************/

/**
 * Cleanup pending timer of the given thread.
 *
 * @param thread
 */
static void thread_cleanup_timer(struct thread_s *thread)
{
	struct MinNode *node;
	while ((node = (struct MinNode*)RemTail((struct List*)&thread->timer_request_list)))
	{
		struct TimerMessage *dummy = NULL;
		struct TimerMessage *timer_msg = (struct TimerMessage*)((UBYTE*)node - ((UBYTE*)&dummy->node - (UBYTE*)dummy));

		AbortIO((struct IORequest*)&timer_msg->time_req);
		WaitIO((struct IORequest*)&timer_msg->time_req);

		if (timer_msg->thread_msg) FreeVec(timer_msg->thread_msg);
		FreeVec(timer_msg);
	}

	CloseDevice((struct IORequest*)thread->timer_req);
	DeleteIORequest((struct IORequest*)thread->timer_req);
	DeleteMsgPort(thread->timer_port);
}

/***************************************************************************************/

int init_threads(void)
{
	if ((main_thread_port = CreateMsgPort()))
	{
		main_thread.process = (struct Process*)FindTask(NULL);
		main_thread.is_main = 1;
		main_thread.thread_port = main_thread_port;

		thread_init_timer(&main_thread);

		NewList((struct List*)&main_thread.push_list);
		FindTask(NULL)->tc_UserData = &main_thread;

		list_init(&thread_list);
		return 1;
	}
	return 0;
}

/***************************************************************************************/

void cleanup_threads(void)
{
	struct TimerMessage *timeout;
	struct thread_node *node;

	ULONG thread_m;
	ULONG timer_m;

	SM_ENTER;

	if (main_thread_port)
	{
		node = (struct thread_node*)list_first(&thread_list);
		while (node)
		{
			thread_abort(node->thread);
			node = (struct thread_node*)node_next(&node->node);
		}

		thread_m = 1UL << main_thread_port->mp_SigBit;
		timer_m = 1UL << main_thread.timer_port->mp_SigBit;
		timeout = NULL;

		/* wait until every task has been removed */
		while ((node = (struct thread_node*)list_first(&thread_list)))
		{
			struct ThreadMessage *tmsg;
			ULONG mask;

			if (!timeout)
			{
				if ((timeout = (struct TimerMessage*)AllocVec(sizeof(*timeout),MEMF_PUBLIC|MEMF_CLEAR)))
				{
					timeout->time_req = *main_thread.timer_req;
					timeout->time_req.tr_node.io_Command = TR_ADDREQUEST;
					timeout->time_req.tr_time.tv_secs = 0;
					timeout->time_req.tr_time.tv_micro = 500000;
					SendIO(&timeout->time_req.tr_node);

					/* Enqueue the timer_msg in our request list */
					AddTail((struct List*)&main_thread.timer_request_list,(struct Node*)&timeout->node);
				}
			}

			mask = Wait(thread_m|timer_m);

			if (mask & timer_m)
			{
				struct TimerMessage *timer;

				while ((timer = (struct TimerMessage*)GetMsg(main_thread.timer_port)))
				{
					if (timer == timeout)
					{
						SM_DEBUGF(15,("Timeout occurred, aborting thread another time\n"));
						/* time out occurred, abort the current task another time */
						timeout = NULL;
						thread_abort(node->thread);
					}
					Remove((struct Node*)&timer->node);
					FreeVec(timer);
				}
			}

			while ((tmsg = (struct ThreadMessage *)GetMsg(main_thread_port)))
			{
				if (tmsg->startup)
					thread_remove(tmsg);
				else
				{
					SM_DEBUGF(10,("Got non startup message (async=%ld)\n",tmsg->async));

					if (!tmsg->async)
					{
						tmsg->called = 0;
						ReplyMsg(&tmsg->msg);
					} else
					{
						FreeVec(tmsg);
					}
				}
			}
		}

		SM_DEBUGF(15,("Zero subthreads left\n"));
		thread_cleanup_timer(thread_get());

		DeleteMsgPort(main_thread_port);
		main_thread_port = NULL;
	}

	SM_LEAVE;
}

/***************************************************************************************/

ULONG thread_mask(void)
{
	struct thread_s *thread = thread_get();

	return (1UL << thread->thread_port->mp_SigBit) | (1UL << thread->timer_port->mp_SigBit);
}

/***************************************************************************************/

void thread_handle(ULONG mask)
{
	struct thread_s *thread = thread_get();

	if (mask & (1UL << thread->thread_port->mp_SigBit))
	{
		struct ThreadMessage *tmsg;
		while ((tmsg = (struct ThreadMessage *)GetMsg(thread->thread_port)))
		{
			SM_DEBUGF(20,("Received message: 0x%lx\n",tmsg));

			if (tmsg->startup)
			{
				thread_remove(tmsg);
				continue;
			}

			thread_handle_execute_function_message(tmsg);

			/* Set the signal so we are going to be called again */
			SetSignal((1UL << thread->thread_port->mp_SigBit),(1UL << thread->thread_port->mp_SigBit));
			break;
		}
	}

	if (mask & (1UL << thread->timer_port->mp_SigBit))
	{
		struct TimerMessage *timer_msg;

		while ((timer_msg = (struct TimerMessage*)GetMsg(thread->timer_port)))
		{
			struct ThreadMessage *tmsg = timer_msg->thread_msg;

			if (tmsg)
			{
				/* Now execute the thread message */
				if (tmsg->function)
				{
					switch(tmsg->argcount)
					{
						case	0: tmsg->function();break;
						case	1: ((int (*)(void*))tmsg->function)(tmsg->arg1);break;
						case	2: ((int (*)(void*,void*))tmsg->function)(tmsg->arg1,tmsg->arg2);break;
						case	3: ((int (*)(void*,void*,void*))tmsg->function)(tmsg->arg1,tmsg->arg2,tmsg->arg3);break;
						case	4: ((int (*)(void*,void*,void*,void*))tmsg->function)(tmsg->arg1,tmsg->arg2,tmsg->arg3,tmsg->arg4);break;
						case	5: ((int (*)(void*,void*,void*,void*,void*))tmsg->function)(tmsg->arg1,tmsg->arg2,tmsg->arg3,tmsg->arg4,tmsg->arg5);break;
						case	6: ((int (*)(void*,void*,void*,void*,void*,void*))tmsg->function)(tmsg->arg1,tmsg->arg2,tmsg->arg3,tmsg->arg4,tmsg->arg5,tmsg->arg6);break;
					}
				}
				FreeVec(tmsg);
			}
			Remove((struct Node*)&timer_msg->node);
			FreeVec(timer_msg);
		}
	}
}

/***************************************************************************************/

/**
 * Entry point for a new thread.
 */
static SAVEDS void thread_entry(void)
{
	struct Process *proc;
	struct ThreadMessage *msg;
	int (*entry)(void *);
	thread_t thread;

	BPTR dirlock;

#ifdef __AMIGAOS4__
	void allow_access_to_private_data(void); /* from startup-os4.c */
#endif

	D(bug("Waiting for startup message\n"));

	proc = (struct Process*)FindTask(NULL);
	WaitPort(&proc->pr_MsgPort);
	msg = (struct ThreadMessage *)GetMsg(&proc->pr_MsgPort);

	/* Set the task's UserData field to store per thread data */
	thread = msg->thread;

#ifdef __AMIGAOS4__
	allow_access_to_private_data();
#endif

	if ((thread->thread_port = CreateMsgPort()))
	{
		D(bug("Subthreaded created port at 0x%lx\n",thread->thread_port));

		NewList((struct List*)&thread->push_list);
		if (thread_init_timer(thread))
		{
			/* Store the thread pointer as userdata */
			proc->pr_Task.tc_UserData = thread;

			/* get task's entry function */
			entry = (int(*)(void*))msg->function;

			dirlock = DupLock(proc->pr_CurrentDir);
			if (dirlock)
			{
				BPTR odir = CurrentDir(dirlock);
				entry(msg->arg1);
				UnLock(CurrentDir(odir));

				thread->process = NULL;
			}
			thread_cleanup_timer(thread);
		}
		DeleteMsgPort(thread->thread_port);
		thread->thread_port = NULL;
	}

	Forbid();
	D(bug("Replying startup message\n"));
	ReplyMsg((struct Message*)msg);
}

/***************************************************************************************/

int thread_parent_task_can_contiue(void)
{
	struct ThreadMessage *msg = (struct ThreadMessage *)AllocVec(sizeof(struct ThreadMessage),MEMF_PUBLIC|MEMF_CLEAR);
	D(bug("Thread can continue\n"));
	if (msg)
	{
		/* No reply port needed as this message is async */
		msg->msg.mn_Length = sizeof(struct ThreadMessage);
		msg->async = 1;
		msg->parent_task_can_continue = 1;

		SM_DEBUGF(10,("Sending \"parent task can continue\"\n"));

		PutMsg(main_thread_port,&msg->msg);
		/* Message is freed by parent task */
		return 1;
	}
	return 0;
}

/***************************************************************************************/

/**
 * Runs the given function in a newly created thread under the given name.
 *
 * @param thread_name
 * @param entry
 * @param eudata
 * @return
 */static thread_t thread_start_new(char *thread_name, int (*entry)(void*), void *eudata)
{
	struct thread_s *thread = (struct thread_s*)AllocVec(sizeof(*thread),MEMF_PUBLIC|MEMF_CLEAR);
	if (thread)
	{
		struct ThreadMessage *msg;

		if ((msg = (struct ThreadMessage *)AllocVec(sizeof(*msg),MEMF_PUBLIC|MEMF_CLEAR)))
		{
			BPTR in,out;
			msg->msg.mn_Node.ln_Type = NT_MESSAGE;
			msg->msg.mn_ReplyPort = main_thread_port;
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
#ifdef __MORPHOS__
				static const struct TagItem extraTags[] =
				{
					{NP_CodeType,   MACHINE_PPC},
					{TAG_DONE,0}
				};
#else
				static const struct TagItem extraTags[] =
				{
					{TAG_DONE,0}
				};
#endif
				thread->process = CreateNewProcTags(
							NP_Entry,      thread_entry,
							NP_StackSize,  20480,
							NP_Name,       thread_name,
							NP_Priority,   -1,
							NP_Input,      in,
							NP_Output,     out,
							TAG_MORE, extraTags);

				if (thread->process)
				{
					SM_DEBUGF(20,("Thread %s started at 0x%lx. Sent message 0x%lx\n",thread_name,thread,msg));
					PutMsg(&thread->process->pr_MsgPort,(struct Message*)msg);

					do
					{
						int cnt = 0;

						Wait(1UL << main_thread_port->mp_SigBit);

						/* Warning: We are accessing the message port directly and scan through all messages (without
						 * removing them), this may not be merely ugly but also a hack */
						Forbid();
						{
							struct Node *node;

							node =  main_thread_port->mp_MsgList.lh_Head;
							while (node->ln_Succ)
							{
								struct ThreadMessage *tmsg = (struct ThreadMessage*)node;

								if (tmsg == msg)
								{
									Remove(node);
									Permit();

									/* This was the startup message, so something has failed */
									SM_DEBUGF(10,("Got startup message of a newly created task back. Something went wrong\n"));
									FreeVec(tmsg);
									FreeVec(thread);
									/* Set the state of this message port to "hot" again */
									SetSignal((1UL << main_thread_port->mp_SigBit),(1UL << main_thread_port->mp_SigBit));
									return NULL;
								}

								if (tmsg->parent_task_can_continue)
								{
									Remove(node);
									Permit();

									/* This was the "parent task can continue message", we don't reply it
									 * but we free it here (although it wasn't allocated by this task) */
									SM_DEBUGF(20,("Got \"parent can continue\"\n"));
									FreeVec(tmsg);
									/* Set the state of this message port to "hot" again */
									SetSignal((1UL << main_thread_port->mp_SigBit),(1UL << main_thread_port->mp_SigBit));
									return thread;
								}
								cnt++;
								node = node->ln_Succ;
							}
						}
						Permit();
					} while(1);
				}
			}
			if (in) Close(in);
			if (out) Close(out);
			FreeVec(msg);
		}
	}
	return NULL;
}

/***************************************************************************************/

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

/***************************************************************************************/

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

/***************************************************************************************/

void thread_abort(thread_t thread_to_abort)
{
	if (!thread_to_abort) thread_to_abort = default_thread;
	Forbid();
	SM_DEBUGF(15,("Aborting thread at %p %p\n",thread_to_abort,thread_to_abort->process));
	if (thread_to_abort->process)
		Signal(&thread_to_abort->process->pr_Task, SIGBREAKF_CTRL_C);
	Permit();
}

/***************************************************************************************/

void thread_signal(thread_t thread_to_signal)
{
	if (!thread_to_signal) thread_to_signal = default_thread;
	Forbid();
	SM_DEBUGF(15,("Signaling thread at %p %p\n",thread_to_signal,thread_to_signal->process));
	if (thread_to_signal->process)
		Signal(&thread_to_signal->process->pr_Task, SIGBREAKF_CTRL_D);
	Permit();
}

/***************************************************************************************/

/**
 * Returns ThreadMessage filled with the given parameters. You can manipulate
 * the returned message to be async or something else
 *
 * @param function
 * @param argcount
 * @param argptr
 * @return
 */
static struct ThreadMessage *thread_create_message(void *function, int argcount, va_list argptr)
{
	struct ThreadMessage *tmsg = (struct ThreadMessage *)AllocVec(sizeof(struct ThreadMessage),MEMF_PUBLIC|MEMF_CLEAR);
	if (tmsg)
	{
		struct MsgPort *subthread_port = thread_get()->thread_port;

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
						if (argcount--)
						{
							tmsg->arg5 = va_arg(argptr, void *);
							if (argcount--)
							{
								tmsg->arg6 = va_arg(argptr, void *);
							}
						}
					}
				}
			}
		}
		tmsg->async = 0;
		tmsg->called = 0;
	}
	return tmsg;
}

/***************************************************************************************/

/**
 * This will handle the execute function message.
 *
 * @param tmsg
 */
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
			case	5: tmsg->result = ((int (*)(void*,void*,void*,void*,void*))tmsg->function)(tmsg->arg1,tmsg->arg2,tmsg->arg3,tmsg->arg4,tmsg->arg5);break;
			case	6: tmsg->result = ((int (*)(void*,void*,void*,void*,void*,void*))tmsg->function)(tmsg->arg1,tmsg->arg2,tmsg->arg3,tmsg->arg4,tmsg->arg5,tmsg->arg6);break;
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
		tmsg->called = 1;
		ReplyMsg(&tmsg->msg);
	}
}

/***************************************************************************************/

int thread_call_parent_function_sync(int *success, void *function, int argcount, ...)
{
	va_list argptr;
	int rc = 0;
	struct ThreadMessage *tmsg;

	SM_ENTER;

	va_start(argptr,argcount);

	if ((tmsg = thread_create_message(function, argcount, argptr)))
	{
		struct MsgPort *subthread_port = tmsg->msg.mn_ReplyPort;
		int ready = 0;

		PutMsg(main_thread_port,&tmsg->msg);

		while (!ready)
		{
			struct Message *msg;

			SM_DEBUGF(20,("Going to sleep\n"));
			WaitPort(subthread_port);
			SM_DEBUGF(20,("Awaken\n"));

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
		if (success) *success = tmsg->called;
		FreeVec(tmsg);
	} else
	{
		if (success) *success = 0;
	}

	va_end (argptr);
	SM_RETURN(rc,"%ld");
	return rc;
}

/***************************************************************************************/

/* TODO: Should call thread_handle()m needs better return values, and should
 * be optimized in case thread == thread_get()
 */
int thread_call_function_sync(thread_t thread, void *function, int argcount, ...)
{
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

			SM_DEBUGF(20,("Going to sleep\n"));
			WaitPort(subthread_port);
			SM_DEBUGF(20,("Awaken\n"));

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

	va_end (argptr);
	return rc;
}

/***************************************************************************************/

int thread_call_function_async(thread_t thread, void *function, int argcount, ...)
{
	va_list argptr;
	int rc = 0;
	struct ThreadMessage *tmsg;

	va_start(argptr,argcount);

	if ((tmsg = thread_create_message(function, argcount, argptr)))
	{
		tmsg->async = 1;

		PutMsg(thread->thread_port,&tmsg->msg);
		rc = 1;
	}
	va_end (argptr);
	return rc;
}

/***************************************************************************************/

int thread_call_parent_function_sync_timer_callback(void (*timer_callback)(void*), void *timer_data, int millis, void *function, int argcount, ...)
{
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
			PutMsg(main_thread_port,&tmsg->msg);

			/* while the parent task should execute the message
			 * we regualiy call the given callback function */
			while (!ready)
			{
				ULONG timer_m = timer_mask(&timer);
				ULONG proc_m = 1UL << subthread_port->mp_SigBit;
				ULONG mask;

				if (millis)
					timer_send_if_not_sent(&timer,millis);

				SM_DEBUGF(20,("Going to sleep\n"));
				mask = Wait(timer_m|proc_m);
				SM_DEBUGF(20,("Awaken\n"));
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
						else
						{
							thread_handle_execute_function_message((struct ThreadMessage*)msg);
						}
					}
				}
			}
			rc = tmsg->result;
			timer_cleanup(&timer);
		}
		FreeVec(tmsg);
	}
	va_end (argptr);
	return rc;
}

/***************************************************************************************/

int thread_wait(void (*timer_callback(void*)), void *timer_data, int millis)
{
	struct timer timer;
	int rc = 0;

	if (timer_init(&timer))
	{
		thread_t this_thread = thread_get();
		struct MsgPort *this_thread_port = this_thread->thread_port;
		if (millis < 0) millis = 0;

		while (1)
		{
			ULONG proc_m = 1UL << this_thread_port->mp_SigBit;
			ULONG timer_m = timer_mask(&timer);
			ULONG mask;
			struct ThreadMessage *tmsg;

			if (millis) timer_send_if_not_sent(&timer,millis);

			if (!IsListEmpty((struct List *)&this_thread->push_list))
				SM_DEBUGF(5,("List is not empty!\n"));
			SM_DEBUGF(20,("Sleeping\n"));
			mask = Wait(timer_m|proc_m|SIGBREAKF_CTRL_C|SIGBREAKF_CTRL_D);
			SM_DEBUGF(20,("Awaking\n"));
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
			while ((tmsg = (struct ThreadMessage*)RemHead((struct List*)&this_thread->push_list)))
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
						case	5: ((int (*)(void*,void*,void*,void*,void*))tmsg->function)(tmsg->arg1,tmsg->arg2,tmsg->arg3,tmsg->arg4,tmsg->arg5);break;
						case	6: ((int (*)(void*,void*,void*,void*,void*,void*))tmsg->function)(tmsg->arg1,tmsg->arg2,tmsg->arg3,tmsg->arg4,tmsg->arg5,tmsg->arg6);break;
					}
				}
				FreeVec(tmsg);
			}

			if (mask & SIGBREAKF_CTRL_C)
			{
				rc = 0;
				break;
			}
			if (mask & SIGBREAKF_CTRL_D)
			{
				rc = 1;
				break;
			}
		}
		timer_cleanup(&timer);
	}

	return rc;
}

/***************************************************************************************/

int thread_push_function(void *function, int argcount, ...)
{
	int rc = 0;
	struct ThreadMessage *tmsg;
	va_list argptr;

	va_start(argptr,argcount);

	if ((tmsg = thread_create_message(function, argcount, argptr)))
	{
		thread_t this_thread = thread_get();
		AddTail((struct List*)&this_thread->push_list,&tmsg->msg.mn_Node);
		rc = 1;
	}

	va_end (argptr);
	return rc;
}

/***************************************************************************************/

int thread_push_function_delayed(int millis, void *function, int argcount, ...)
{
	int rc = 0;
	struct ThreadMessage *tmsg;
	va_list argptr;

	va_start(argptr,argcount);

	if ((tmsg = thread_create_message(function, argcount, argptr)))
	{
		thread_t thread = thread_get();
		struct TimerMessage *timer_msg = AllocVec(sizeof(struct TimerMessage),MEMF_PUBLIC);
		if (timer_msg)
		{
			timer_msg->time_req = *thread->timer_req;
			timer_msg->time_req.tr_node.io_Command = TR_ADDREQUEST;
			timer_msg->time_req.tr_time.tv_secs = millis / 1000;
			timer_msg->time_req.tr_time.tv_micro = (millis % 1000)*1000;
			timer_msg->thread_msg = tmsg;
			SendIO(&timer_msg->time_req.tr_node);

			/* Enqueue the timer_msg in our request list */
			AddTail((struct List*)&thread->timer_request_list,(struct Node*)&timer_msg->node);

			rc = 1;
		} else FreeVec(tmsg);
	}

	va_end (argptr);
	return rc;
}

/***************************************************************************************/

int thread_call_parent_function_async_string(void *function, int argcount, ...)
{
	struct ThreadMessage *tmsg = (struct ThreadMessage *)AllocVec(sizeof(struct ThreadMessage),MEMF_PUBLIC|MEMF_CLEAR);
	if (tmsg)
	{
		va_list argptr;

		va_start(argptr,argcount);

		/* Note that async messages are never replied, therefore no reply port is necessary */
		tmsg->msg.mn_Length = sizeof(struct ThreadMessage);
		tmsg->function = (int (*)(void))function;
		tmsg->argcount = argcount;
		tmsg->arg1 = va_arg(argptr, void *);
		tmsg->arg2 = va_arg(argptr, void *);
		tmsg->arg3 = va_arg(argptr, void *);
		tmsg->arg4 = va_arg(argptr, void *);
		tmsg->arg5 = va_arg(argptr, void *);
		tmsg->arg6 = va_arg(argptr, void *);
		tmsg->async = 2;

		va_end (argptr);

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

		PutMsg(main_thread_port,&tmsg->msg);
		return 1;
	}

	return 0;
}

/***************************************************************************************/

thread_t thread_get_main(void)
{
	return &main_thread;
}

/***************************************************************************************/

thread_t thread_get(void)
{
	return (struct thread_s*)(FindTask(NULL)->tc_UserData);
}


/***************************************************************************************/

int thread_aborted(void)
{
	int aborted = !!CheckSignal(SIGBREAKF_CTRL_C);
	SM_DEBUGF(15,("Aborted=%ld\n",aborted));
	return aborted;
}

/***************************************************************************************/

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

