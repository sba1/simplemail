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
#include "folder.h"
#include "mainwnd.h"
#include "simplemail.h"
#include "support_indep.h"

static struct MsgPort *arexx_port;

/* from gui_main.c */
void app_hide(void);
void app_show(void);

char *stradd(char *src, const char *str1);

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
 Sets the RESULT variable
*****************************************************************/
static void arexx_set_result(struct RexxMsg *rxmsg, STRPTR string)
{
	if (!string) string = "";
	if (rxmsg->rm_Action & (1L << RXFB_RESULT))
	{
		if (rxmsg->rm_Result2) DeleteArgstring((STRPTR)rxmsg->rm_Result2);
		rxmsg->rm_Result2 = (LONG)CreateArgstring( string,strlen(string));
	}
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
 GETSELECTED Arexx Command
*****************************************************************/
static void arexx_getselected(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct	{
		STRPTR var;
		STRPTR stem;
	} getselected_arg;
	memset(&getselected_arg,0,sizeof(getselected_arg));

	if ((arg_handle = ParseTemplate("VAR/K,STEM/K",args,&getselected_arg)))
	{
		int num = 0;
		struct mail *mail;
		void *handle = NULL;
		struct folder *folder;

		folder = main_get_folder();
		mail = main_get_mail_first_selected(&handle);
		num = 0;

		/* Count the number of mails */
		while (mail)
		{
			num++;
			mail = main_get_mail_next_selected(&handle);
		}

		if (folder)
		{
			char num_buf[20];

			if (getselected_arg.stem)
			{
				int stem_len = strlen(getselected_arg.stem);
				char *stem_buf = malloc(stem_len+20);
				if (stem_buf)
				{
					int i = 0;

					strcpy(stem_buf,getselected_arg.stem);
					strcpy(&stem_buf[stem_len],"NUM.COUNT");
					sprintf(num_buf,"%d",num);
					SetRexxVar(rxmsg,stem_buf,num_buf,strlen(num_buf));

					mail = main_get_mail_first_selected(&handle);
					while (mail)
					{
						sprintf(&stem_buf[stem_len],"NUM.%d",i);
						sprintf(num_buf,"%d",folder_get_index_of_mail(folder,mail));
						SetRexxVar(rxmsg,stem_buf,num_buf,strlen(num_buf));
						i++;
						mail = main_get_mail_next_selected(&handle);
					}
					free(stem_buf);
				}
			} else
			{
				char *str;

				sprintf(num_buf,"%d",num);
				str = mystrdup(num_buf);

				mail = main_get_mail_first_selected(&handle);

				/* Count the number of mails */
				while (mail)
				{
					sprintf(num_buf, " %d", folder_get_index_of_mail(folder,mail));
					str = stradd(str,num_buf);
					mail = main_get_mail_next_selected(&handle);
				}

				if (getselected_arg.var) SetRexxVar(rxmsg,getselected_arg.var,str,strlen(str));
				else arexx_set_result(rxmsg,str);

				free(str);
			}
		}
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
		else if (!Stricmp("GETSELECTED",command.command)) arexx_getselected(rxmsg,command.args);

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

