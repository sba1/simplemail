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
#include <proto/exec.h>
#include <proto/dos.h>

#include "subthreads.h"

#include "amiproc.h"

/*#define MYDEBUG*/
#include "debug.h"

struct ThreadMessage
{
	struct Message msg;
	int (*function)(void);
	int async;
	int argcount;
	void *arg1,*arg2,*arg3,*arg4;
	int result;
};

struct ThreadUData
{
	struct MsgPort *server_port;
	int (*entry)(void*);
	void *udata;
};

struct MsgPort *thread_port;

static struct AmiProcMsg *subthread;

int init_threads(void)
{
	if ((thread_port = CreateMsgPort()))
	{
		return 1;
	}
	return 0;
}

void cleanup_threads(void)
{
	if (subthread) AmiProc_Wait(subthread);
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

static int thread_entry(struct ThreadUData *udata)
{
	int retval;

	thread_port = udata->server_port;

	retval = udata->entry(udata->udata);

	FreeVec(udata);
	return retval;
}

int thread_parent_task_can_contiue(void)
{
#ifndef DONT_USE_THREADS
	struct ThreadMessage *tmsg = (struct ThreadMessage *)AllocVec(sizeof(struct ThreadMessage),MEMF_PUBLIC|MEMF_CLEAR);
	if (tmsg)
	{
		struct Process *p = (struct Process*)FindTask(NULL);
		tmsg->msg.mn_ReplyPort = &p->pr_MsgPort;
		tmsg->msg.mn_Length = sizeof(struct ThreadMessage);

		PutMsg(thread_port,&tmsg->msg);
		WaitPort(&p->pr_MsgPort);
		GetMsg(&p->pr_MsgPort);

		FreeVec(tmsg);
		return 1;
	}
	return 0;
#else
	return 1;
#endif
}

int thread_start(int (*entry)(void*), void *eudata)
{
#ifndef DONT_USE_THREADS
	struct ThreadUData *udata;

	if (subthread)
	{
		if (!AmiProc_Check(subthread)) return 0;
		subthread = NULL;
	}

	if ((udata = (struct ThreadUData *)AllocVec(sizeof(*udata),MEMF_PUBLIC|MEMF_CLEAR))); /* Will be free'd in the subtask */
	{
		udata->server_port = thread_port;
		udata->entry = entry;
		udata->udata = eudata;

		if ((subthread = AmiProc_Start( (int (*)(void *))thread_entry, udata)))
		{
			WaitPort(thread_port); /* Should also wait for the startup message in case something fails */
			ReplyMsg(GetMsg(thread_port));
			return 1;
		}
	}
	return 0;
#else
	entry(eudata);
	return 1;
#endif
}

void thread_abort(void)
{
	if (subthread)
	{
		/* a design lack, should be really improved */
		Forbid();

		if (!AmiProc_Check(subthread))
		{
			AmiProc_Abort(subthread);
		} else subthread = NULL;

		Permit();
	}
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

