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

#include <exec/memory.h>
#include <exec/execbase.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include <dos.h>
#include <string.h>

#include "amiproc.h"

// Functions defined in the SAS/C® library to run autoinitializer
// and autoterminater functions
void __stdargs __fpinit(void);
void __stdargs __fpterm(void);

// Data items defined in the SAS/C® library and possibly overridden
// by user code.  __stack gives the desired stack size, __priority
// gives the desired priority, and __procname gives the desired name
// for any new processes.
extern long __stack, __priority;
extern char *__procname;

extern char __far RESLEN;               /* size of init data   */
extern char __far RESBASE;              /* Base of global data */
extern char __far NEWDATAL;             /* size of global data */
extern const char __far LinkerDB;       /* Original A4 value   */
extern struct DosLibrary *DOSBase;
extern char *_ProgramName;
extern struct ExecBase *SysBase;
BPTR __curdir;

static struct DosLibrary *MyDOSBase;

#define DATAWORDS ((ULONG)&NEWDATAL)     /* magic to get right type of reloc */ 

static long _CloneData(void)
{
   ULONG *newa4;
   ULONG *origa4;
   ULONG *reloc;
   ULONG nrelocs;
   struct WBStartup *wbtmp = _WBenchMsg;
   char *pntmp = _ProgramName;
   BPTR cdtmp = __curdir;

   // Allocate the new data section
   newa4 = (ULONG *)AllocMem((ULONG)&RESLEN, MEMF_PUBLIC);
   if(newa4 == NULL) return NULL;

   // Get original A4 value
   // This points to the UNMODIFIED near global data section
   // allocated by the cres.o startup.  This line of code 
   // will also generate a linker warning; ignore it.
   origa4 = (ULONG *)((ULONG)&LinkerDB - (ULONG)&RESBASE);

   // Copy over initialized data
   memcpy(newa4, origa4, DATAWORDS*4);
   
   // Zero uninitialized data
   memset(newa4+DATAWORDS, 0, (((ULONG)&RESLEN)-DATAWORDS*4));

   // Perform relocations
   // The number of relocs is stashed immediately after the
   // initialized data in the original data section.  The
   // relocs themselves follow.
   origa4 += DATAWORDS;
   for(nrelocs = *origa4++; nrelocs>0; nrelocs--)
   {
      reloc = (ULONG *)((ULONG)newa4 + *origa4++);
      *reloc += (ULONG)newa4;
   }

   // If your code has >32k of near data, RESBASE will be 32k.
   // Otherwise, it will be 0.  The A4 pointer must point into
   // the middle of the data section if you have >32k of data
   // because the A4-relative addressing mode can handle +/-32k
   // of data, not 64k of positive data.
   newa4 += (ULONG)&RESBASE;
   putreg(REG_A4, (long)newa4);

   // Set up a couple of externs that the startup code normally
   // does for you...
   SysBase = *(struct ExecBase **)4;
   MyDOSBase = DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 0L);
   _WBenchMsg = wbtmp;       // Copied from old data section
   _ProgramName = pntmp;     // Copied from old data section
   __curdir = DupLock(cdtmp);// cdtmp copied from old data section

   return (long)newa4;
}

static void _FreeData(void)
{
   // Free the current directory lock
   UnLock(__curdir);

   // Close the local static copy of MyDOSBase
   CloseLibrary((struct Library *)MyDOSBase);
   
   // Free the new data section we allocated
   FreeMem((void *)getreg(REG_A4), (ULONG)&RESLEN);
}

struct AmiProcMsg								/* startup message sent to child */
{
	struct Message msg;
	int (*fp)(void *);              /* function we're going to call */
	void *global_data;              /* global data reg (A4)         */
	long return_code;               /* return code from process     */
	void *UserData;                 /* User-supplied data pointer   */
	struct Process *child;					/* The child process itselv     */
};

static void process_starter(void)
{
   struct Process *proc;
   struct AmiProcMsg *mess;
   struct ExecBase *SysBase = *((struct ExecBase **)4);
   __regargs int (*fp)(void *, void *, void *);
         
   proc = (struct Process *)FindTask((char *)NULL);

   /* get the startup message */
   WaitPort(&proc->pr_MsgPort);
   mess = (struct AmiProcMsg *)GetMsg(&proc->pr_MsgPort);

   /* gather necessary info from message */
   fp = (__regargs int (*)(void *, void *, void *))mess->fp;

   /* replace this with the proper #asm for Aztec */
   putreg(REG_A4, (long)mess->global_data);
   
   /* Allocate a new data section */
   putreg(REG_A4, _CloneData());

   /* Run autoinitializers.  This has the effect of setting up    */
   /* the standard C and C++ libraries (including stdio), running */
   /* constructors for C++ externs and statics, and running user  */
   /* autoinit functions.                                         */
   __fpinit();

   /* Call the desired function */
   /* We pass the UserData parameter three times in order to satisfy */
   /* both PARM=REG and PARM=STACK function pointers.  Since we have */
   /* declared the local 'fp' to be regargs, the three parms will go */
   /* into A0, A1 and the stack.  If 'fp' points to a regargs func,  */
   /* it will get its parm from A0 and all is well.  If 'fp' points  */
   /* to a stdargs func, it will get its parameter from the stack and*/
   /* all is still well.                                             */
   mess->return_code = (*fp)(mess->UserData, mess->UserData, mess->UserData);

   /* Run autoterminators to clean up. */
   __fpterm();
   
   /* Free the recently-allocated data section */
#ifndef _DCC
   _FreeData();
#endif
   
   /* Forbid so the child can finish completely, before */
   /* the parent cleans up.                             */
   Forbid();

   /* Reply so process who spawned us knows we're done */   
   ReplyMsg((struct Message *)mess);

   /* We finish without Permit()ing, but it's OK since our task    */
   /* will end after the RTS, which will break the Forbid() anyway */
}


// AmiProc_Start - launch a new process with a specified function
// pointer as the entry point.
struct AmiProcMsg *AmiProc_Start(int (*fp)(void *), void *UserData)
{
	struct Process *process;
	struct MsgPort *child_port;
	struct AmiProcMsg *start_msg;
	BPTR in, out;
	int stack = (__stack > 4000 ? __stack : 4000);
	char *procname = (__procname ? __procname : "New Process");
	
	if (!(start_msg = (struct AmiProcMsg *)AllocMem(sizeof(struct AmiProcMsg), MEMF_PUBLIC|MEMF_CLEAR)))
	{
		return NULL;
	}
	
	if (!(start_msg->msg.mn_ReplyPort = CreateMsgPort()))
	{
		FreeMem((void *)start_msg, sizeof(*start_msg));
		return NULL;
	}

	if(!(in  = Open("CONSOLE:", MODE_OLDFILE))) in  = Open("NIL:", MODE_NEWFILE);
	if(!(out = Open("CONSOLE:", MODE_OLDFILE))) out = Open("NIL:", MODE_NEWFILE);

	/* Flush the data cache in case we're on a 68040 */
	CacheClearU();

	process = CreateNewProcTags(NP_Entry,     process_starter,
															NP_StackSize,  stack,
															NP_Name,       procname,
															NP_Priority,   __priority,
															NP_Input,      in,
															NP_Output,     out,
															TAG_END);

	if (!process)
	{
		DeleteMsgPort(start_msg->msg.mn_ReplyPort);
		FreeMem((void *)start_msg, sizeof(*start_msg));
		return NULL;
	}

	child_port = process ? &process->pr_MsgPort : NULL;

	/* Fill in the rest of the startup message */
	start_msg->msg.mn_Length = sizeof(struct AmiProcMsg);
	start_msg->msg.mn_Node.ln_Type = NT_MESSAGE;
	start_msg->child = process;
   
	/* replace this with the proper #asm for Aztec */
	start_msg->global_data = (void *)getreg(REG_A4);  /* save global data reg (A4) */
	start_msg->fp = fp;                               /* Fill in function pointer */
	start_msg->UserData = UserData;   
   
	/* send startup message to child */
	PutMsg(child_port, (struct Message *)start_msg);
	return start_msg;
}

int AmiProc_Wait(struct AmiProcMsg *start_msg)
{
    struct AmiProcMsg *msg;
    int ret;
           
    /* Wait for child to reply, signifying that it is finished */
    while ((msg = (struct AmiProcMsg *)
                   WaitPort(start_msg->msg.mn_ReplyPort)) != start_msg) 
          ReplyMsg((struct Message *)msg);

    /* get return code */
    ret = msg->return_code;

    /* Free up remaining resources */
    DeleteMsgPort(start_msg->msg.mn_ReplyPort);
    FreeMem((void *)start_msg, sizeof(*start_msg));
    return(ret);
}

int AmiProc_Check(struct AmiProcMsg *start_msg)
{
	struct AmiProcMsg *msg;
	int finished = 0;

	while ((msg = (struct AmiProcMsg *)GetMsg(start_msg->msg.mn_ReplyPort)))
	{
		if (msg != start_msg) ReplyMsg((struct Message *)msg);
		else finished = 1;
	}

	/* Free up remaining resources */
	if (finished)
	{
		DeleteMsgPort(start_msg->msg.mn_ReplyPort);
		FreeMem((void *)start_msg, sizeof(*start_msg));
	}
	return finished;
}

void AmiProc_Abort(struct AmiProcMsg *start_msg)
{
	Signal(&start_msg->child->pr_Task, SIGBREAKF_CTRL_C);
}
