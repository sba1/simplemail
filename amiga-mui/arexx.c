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
** arexx.c
*/

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <rexx/storage.h>

#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <proto/rexxsyslib.h>

#include "amigasupport.h"
#include "arexx.h"
#include "mainwnd.h"
#include "simplemail.h"

static struct MsgPort *arexx_port;

/* from gui_main.c */
void app_hide(void);
void app_show(void);

/****************************************************************
 Returns the arexx message port if it already exists. Should
 be called in Forbid() state.
*****************************************************************/
struct MsgPort *arexx_find(void)
{
	return FindPort("SIMPLEMAIL.1");
}

/****************************************************************
 Initialize the arexx port, fails if the port already exits
*****************************************************************/
int arexx_init(void)
{
	int rc = 0;
	Forbid();
	if (!arexx_find())
	{
		if ((arexx_port = CreateMsgPort()))
		{
			arexx_port->mp_Node.ln_Name = "SIMPLEMAIL.1";
			AddPort(arexx_port);
			rc = 1;
		}
	}
	Permit();
	
	return rc;
}

/****************************************************************
 Cleanup Arexx Stuff
*****************************************************************/
void arexx_cleanup(void)
{
	if (arexx_port)
	{
		RemPort(arexx_port);
		DeleteMsgPort(arexx_port);
	}
}

/****************************************************************
 Returns the mask of the arexx port
*****************************************************************/
ULONG arexx_mask(void)
{
	if (!arexx_port) return NULL;
	return 1UL << arexx_port->mp_SigBit;
}

/****************************************************************
 MAINTOFRONT Arexx Command
*****************************************************************/
static void arexx_maintofront(struct RexxMsg *rxmsg, STRPTR args)
{
	main_window_open();
}

/****************************************************************
 MAINTOFRONT Arexx Command
*****************************************************************/
static void arexx_mailwrite(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct	{
		STRPTR mailto;
		STRPTR subject;
	} mailwrite_arg;
	memset(&mailwrite_arg,0,sizeof(mailwrite_arg));

	if ((arg_handle = ParseTemplate("MAILTO/K,SUBJECT/K",args,&mailwrite_arg)))
	{
		callback_write_mail_to_str(mailwrite_arg.mailto,mailwrite_arg.subject);
		FreeTemplate(arg_handle);
	}
}

/****************************************************************
 SETMAIL Arexx Command
*****************************************************************/
static void arexx_setmail(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct	{
		LONG *num;
	} setmail_arg;
	memset(&setmail_arg,0,sizeof(setmail_arg));

	if ((arg_handle = ParseTemplate("NUM/N/A",args,&setmail_arg)))
	{
		callback_select_mail(*setmail_arg.num);
		FreeTemplate(arg_handle);
	}
}

/****************************************************************
 SHOW Arexx Command
*****************************************************************/
static void arexx_show(struct RexxMsg *rxmsg, STRPTR args)
{
	app_show();
}

/****************************************************************
 HIDE Arexx Command
*****************************************************************/
static void arexx_hide(struct RexxMsg *rxmsg, STRPTR args)
{
	app_hide();
}

/****************************************************************
 Handle this single arexx message
*****************************************************************/
static int arexx_message(struct RexxMsg *rxmsg)
{
	STRPTR command_line = (STRPTR)ARG0(rxmsg);
	APTR command_handle;

	struct {
		STRPTR command;
		STRPTR args;
	} command;

	command.command = command.args = NULL;
	rxmsg->rm_Result2 = NULL;

	if ((command_handle = ParseTemplate("COMMAND/A,ARGS/F",command_line,(LONG*)&command)))
	{
		if (!Stricmp("MAINTOFRONT",command.command)) arexx_maintofront(rxmsg,command.args);
		else if (!Stricmp("MAILWRITE",command.command)) arexx_mailwrite(rxmsg,command.args);
		else if (!Stricmp("SETMAIL",command.command)) arexx_setmail(rxmsg,command.args);
		else if (!Stricmp("SHOW",command.command)) arexx_show(rxmsg,command.args);
		else if (!Stricmp("HIDE",command.command)) arexx_hide(rxmsg,command.args);

		FreeTemplate(command_handle);
	}
	return 0;
}

/****************************************************************
 Handle the incoming arexx message.
*****************************************************************/
int arexx_handle(void)
{
	int retval = 0;
	struct RexxMsg *msg;

	while ((msg = (struct RexxMsg *)GetMsg(arexx_port)))
	{
		arexx_message(msg);
		ReplyMsg((struct Message*)msg);
	}
	return retval;
}

