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
/* #define DONT_USE_THREADS */

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>

#include "subthreads.h"
#include "lists.h"

//#define MYDEBUG
#include "debug.h"

struct ThreadMessage
{
	struct Message msg;
	int startup; /* message is startup message */
	int (*function)(void);
	int async;
	int argcount;
	void *arg1,*arg2,*arg3,*arg4;
	int result;
};

/* the port allocated by the main task */
static struct MsgPort *thread_port;

/* the thread, we currently allow only a single one */
static struct Process *thread;


struct list thread_list;
struct thread_node
{
	struct node node;
	thread_t thread;
};

struct thread_s
{
	struct Process *process;
};

int init_threads(void)
{
	if ((thread_port = CreateMsgPort()))
	{
		list_init(&thread_list);
		return 1;
	}
	return 0;
}

void cleanup_threads(void)
{
	/* It's safe to call this if no thread is actually running */
	thread_abort();

	if (thread)
	{
		int ready = 0;
		while (!ready)
		{
			struct ThreadMessage *tmsg;

			WaitPort(thread_port);

			while ((tmsg = (struct ThreadMessage *)GetMsg(thread_port)))
			{
				if (tmsg->startup)
					ready = 1;
			}
		}
		thread = NULL;
	}

	if (thread_port)
	{
		DeleteMsgPort(thread_port);
		thread_port = NULL;
	}
}

unsigned long thread_mask(void)
{
	return (1UL << thread_port->mp_SigBit);
}

void thread_handle(void)
{
	struct ThreadMessage *tmsg;

	while ((tmsg = (struct ThreadMessage *)GetMsg(thread_port)))
	{
		if (tmsg->startup)
		{
			D(bug("Got startup message back\n"));
			FreeVec(tmsg);
			thread = NULL;
			continue;
		}
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
			if (tmsg->async == 2 && tmsg->argcount >= 1 && tmsg->arg1) FreeVec(tmsg->arg1);
			FreeVec(tmsg);
		}
		else ReplyMsg(&tmsg->msg);
	}
}

/* the entry point for the subthread */
static __saveds void thread_entry(void)
{
	struct Process *proc;
	struct ThreadMessage *msg;
	int (*entry)(void *);

	BPTR dirlock;

	D(bug("Waiting for startup message\n"));

	proc = (struct Process*)FindTask(NULL);	
	WaitPort(&proc->pr_MsgPort);
	msg = (struct ThreadMessage *)GetMsg(&proc->pr_MsgPort);

	entry = (int(*)(void*))msg->function;


	dirlock = DupLock(proc->pr_CurrentDir);
	if (dirlock)
	{
		BPTR odir = CurrentDir(dirlock);
		entry(msg->arg1);
		UnLock(CurrentDir(odir));
	}

	Forbid();
	D(bug("Replying startup message\n"));
	ReplyMsg((struct Message*)msg);
}


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

thread_t thread_start_new(int (*entry)(void*), void *eudata)
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
							NP_Name,       "SimpleMail - Subthread",
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

thread_t thread_add(int (*entry)(void *), void *eudata)
{
	struct thread_node *thread_node = (struct thread_node*)AllocVec(sizeof(struct thread_node),MEMF_PUBLIC|MEMF_CLEAR);
	if (thread_node)
	{
		thread_t thread = thread_start_new(entry, eudata);
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

int thread_start(int (*entry)(void*), void *eudata)
{
#ifndef DONT_USE_THREADS
	struct ThreadMessage *msg;

	/* We allow only one subtask for the moment */
	if (thread) return 0;

	if ((msg = (struct ThreadMessage *)AllocVec(sizeof(*msg),MEMF_PUBLIC|MEMF_CLEAR)))
	{
		BPTR in,out;
		msg->msg.mn_Node.ln_Type = NT_MESSAGE;
		msg->msg.mn_ReplyPort = thread_port;
		msg->msg.mn_Length = sizeof(*msg);
		msg->startup = 1;
		msg->function = (int (*)(void))entry;
		msg->arg1 = eudata;

		in = Open("CONSOLE:",MODE_NEWFILE);
		if (!in) in = Open("NIL:",MODE_NEWFILE);
		out = Open("CONSOLE:",MODE_NEWFILE);
		if (!out) out = Open("NIL:",MODE_NEWFILE);

		if (in && out)
		{
			thread = CreateNewProcTags(
						NP_Entry,      thread_entry,
						NP_StackSize,  16384,
						NP_Name,       "SimpleMail - Subthread",
						NP_Priority,   -1,
						NP_Input,      in,
						NP_Output,     out,
						TAG_END);

			if (thread)
			{
				struct ThreadMessage *thread_msg;
				D(bug("Thread started at 0x%lx\n",thread));
				PutMsg(&thread->pr_MsgPort,(struct Message*)msg);
				WaitPort(thread_port);
				thread_msg = (struct ThreadMessage *)GetMsg(thread_port);
				if (thread_msg == msg)
				{
					/* This was the startup message, so something has failed */
					D(bug("Got startup message back. Something went wrong\n"));
					FreeVec(thread_msg);
					return 0;
				} else
				{
					/* This was the "parent task can continue message", we don't reply it
					 * but we free it here (although it wasn't allocated by this task) */
					D(bug("Got \"parent can continue\"\n"));
					FreeVec(thread_msg);
					return 1;
				}
			}
		}
		if (in) Close(in);
		if (out) Close(out);
		FreeVec(msg);
	}
	return 0;
#else
	entry(eudata);
	return 1;
#endif
}

void thread_abort(void)
{
	Forbid();
	if (thread)
	{
		Signal(&thread->pr_Task, SIGBREAKF_CTRL_C);
	}
	Permit();
}

/* Call the function synchron */
int thread_call_parent_function_sync(void *function, int argcount, ...)
{
#ifndef DONT_USE_THREADS
	int rc = 0;

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
		tmsg->async = 0;

		va_end (arg_ptr);

		PutMsg(thread_port,&tmsg->msg);
		WaitPort(&p->pr_MsgPort);
		GetMsg(&p->pr_MsgPort);
		rc = tmsg->result;
		FreeVec(tmsg);
	}

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
	int rc = 0;

	struct ThreadMessage *tmsg = (struct ThreadMessage *)AllocVec(sizeof(struct ThreadMessage),MEMF_PUBLIC|MEMF_CLEAR);
	if (tmsg)
	{
		struct MsgPort *timer_port;
		struct MsgPort *subthread_port;
		struct timerequest *timer_req;

		if (subthread_port = CreateMsgPort())
		{
			if (timer_port = CreateMsgPort())
			{
				if (timer_req = (struct timerequest *) CreateIORequest(timer_port, sizeof(struct timerequest)))
				{
					if (!OpenDevice(TIMERNAME, UNIT_VBLANK, (struct IORequest *) timer_req, 0))
					{
						struct timerequest *new_timer_req = (struct timerequest *) AllocVec(sizeof(struct timerequest), MEMF_PUBLIC);
						if (new_timer_req)
						{
							int timer_send = 0;
							struct Process *p = (struct Process*)FindTask(NULL);
							va_list argptr;
	
							/* we only accept positive values */
							if (millis < 1) millis = 1;
	
							va_start(argptr,argcount);
	
							tmsg->msg.mn_ReplyPort = subthread_port;
							tmsg->msg.mn_Length = sizeof(struct ThreadMessage);
							tmsg->function = (int (*)(void))function;
							tmsg->argcount = argcount;
							tmsg->arg1 = va_arg(argptr, void *);/*(*(&argcount + 1));*/
							tmsg->arg2 = va_arg(argptr, void *);/*(void*)(*(&argcount + 2));*/
							tmsg->arg3 = va_arg(argptr, void *);/*(void*)(*(&argcount + 3));*/
							tmsg->arg4 = va_arg(argptr, void *);/*(void*)(*(&argcount + 4));*/
							tmsg->async = 0;
	
							va_end (arg_ptr);
	
							/* now send the message */
							PutMsg(thread_port,&tmsg->msg);
	
							/* while the parent task should execute the message
							 * we regualiy call the given callback function */
							while (1)
							{
								ULONG timer_mask = 1UL << timer_port->mp_SigBit;
								ULONG proc_mask = 1UL << subthread_port->mp_SigBit;
								ULONG mask;
	
								if (!timer_send)
								{
							    *new_timer_req = *timer_req;
							    new_timer_req->tr_node.io_Command = TR_ADDREQUEST;
							    new_timer_req->tr_time.tv_secs = millis/1000;
							    new_timer_req->tr_time.tv_micro = millis%1000;
						  	  SendIO((struct IORequest *)new_timer_req);
						  	  timer_send = 1;
							  }
	
								mask = Wait(timer_mask|proc_mask);
								if (mask & timer_mask)
								{
									if (timer_callback)
									{
										timer_callback(timer_data);
									}
									timer_send = 0;
								}
	
								if (mask & proc_mask)
								{
									/* the parent task has finished */
									break;
								}
							}
			
							GetMsg(&p->pr_MsgPort);
							rc = tmsg->result;
										
							if (timer_send)
							{
								AbortIO((struct IORequest*)new_timer_req);
								WaitIO((struct IORequest*)new_timer_req);
							}
							FreeVec(new_timer_req);
						}
			      CloseDevice((struct IORequest *) timer_req);
					}
			    DeleteIORequest(timer_req);
				}
		    DeleteMsgPort(timer_port);
			}
			DeleteMsgPort(subthread_port);
		}
		FreeVec(tmsg);
	}
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

