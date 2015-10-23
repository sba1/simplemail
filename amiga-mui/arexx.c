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

/**
 * @file arexx.c
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
#include <proto/asl.h>
#include <proto/intuition.h> /* ScreenToXXX() */

#include "addressbook.h"
#include "codesets.h"
#include "configuration.h"
#include "debug.h"
#include "folder.h"
#include "http.h"
#include "mail.h"
#include "support_indep.h"
#include "trans.h"
#include "version.h"

#include "addressbookwnd.h"
#include "amigasupport.h"
#include "arexx.h"
#include "composewnd.h"
#include "mainwnd.h"
#include "readwnd.h"
#include "request.h"
#include "simplemail.h"
#include "support.h"

static struct MsgPort *arexx_port;

static struct MsgPort *arexx_execute_port;
static int arexx_execute_out; /* number of arexx messages standing out */

/* from gui_main.c */
void app_hide(void);
void app_show(void);
void app_quit(void);

/* from mainwnd.c */
struct Screen *main_get_screen(void);

/**
 * Replacement for SetRexxVarFromMsg().
 *
 * @param name the name of the ARexx variable
 * @param value the value that the ARexx variable should have
 * @param message the message defining the ARexx context
 * @return 0 on failure
 */

LONG MySetRexxVarFromMsg(STRPTR name, STRPTR value, struct RexxMsg *message)
{
#ifdef __AMIGAOS4__
	if (IRexxSys) return SetRexxVarFromMsg(name,value,message);
	return 0;
#else
	return SetRexxVar(message,name,value,strlen(value));
#endif
}

/*****************************************************************************/

struct MsgPort *arexx_find(void)
{
	return FindPort("SIMPLEMAIL.1");
}

/*****************************************************************************/

int arexx_init(void)
{
	int rc = 0;
	Forbid();
	if (!arexx_find())
	{
		if ((arexx_execute_port = CreateMsgPort()))
		{
			if ((arexx_port = CreateMsgPort()))
			{
				arexx_port->mp_Node.ln_Name = "SIMPLEMAIL.1";
				AddPort(arexx_port);
				rc = 1;
			}
		}
	}
	Permit();

	return rc;
}

/*****************************************************************************/

void arexx_cleanup(void)
{
	if (arexx_port)
	{
		RemPort(arexx_port);
		DeleteMsgPort(arexx_port);
	}

	while (arexx_execute_out)
	{
		struct RexxMsg *rxmsg;

		WaitPort(arexx_execute_port);
		while ((rxmsg = (struct RexxMsg *)GetMsg(arexx_execute_port)))
		{
			arexx_execute_out--;
			ClearRexxMsg(rxmsg,1);
			DeleteRexxMsg(rxmsg);
		}
	}
	if (arexx_execute_port) DeleteMsgPort(arexx_execute_port);
}

/*****************************************************************************/

ULONG arexx_mask(void)
{
	if (!arexx_port) return 0UL;
	return 1UL << arexx_port->mp_SigBit;
}

/*****************************************************************************/

int arexx_execute_script(char *command)
{
	struct RexxMsg *rxmsg;
	BPTR lock;

	if (!(lock = Lock(command,ACCESS_READ))) return 0;
	if (!(command = NameOfLock(lock)))
	{
		UnLock(lock);
		return 0;
	}

	UnLock(lock);

	if ((rxmsg = CreateRexxMsg(arexx_execute_port, "SMRX", "SIMPLEMAIL.1")))
	{
		rxmsg->rm_Args[0] = command;

		if (FillRexxMsg(rxmsg,1,0))
		{
			struct MsgPort *ap; /* Port of ARexx resident process */

			/* Init Rexx message */
			rxmsg->rm_Action = RXCOMM;/*|RXFF_NOIO;*/

			/* Find port and send message */
			Forbid();
			ap = FindPort("REXX");
			if (ap)
			{
				PutMsg(ap,(struct Message *)rxmsg);
				arexx_execute_out++;
			}
			Permit();

			return 1;
		}
		DeleteRexxMsg(rxmsg);
	}
	return 0;
}

/**
 * Set the result variable.
 *
 * @param rxmsg the message defining the ARexx context.
 * @param string the contents of the result
 */
static void arexx_set_result(struct RexxMsg *rxmsg, STRPTR string)
{
	if (!string) string = "";
	if (rxmsg->rm_Action & (1L << RXFB_RESULT))
	{
		if (rxmsg->rm_Result2) DeleteArgstring((STRPTR)rxmsg->rm_Result2);
		rxmsg->rm_Result2 = (LONG)CreateArgstring( string,strlen(string));
	}
}

/**
 * Sets a given variable as a integer number.
 *
 * @param rxmsg the message defining the ARexx context.
 * @param varname
 * @param num
 */
static void arexx_set_var_int(struct RexxMsg *rxmsg, char *varname, int num)
{
	char num_buf[24];
	sprintf(num_buf,"%d",num);
	MySetRexxVarFromMsg(varname,num_buf,rxmsg);
}


/**
 * MAINTOFRONT Arexx Command.
 *
 * @param rxmsg the message defining the ARexx context.
 * @param args
 */
static void arexx_maintofront(struct RexxMsg *rxmsg, STRPTR args)
{
	main_window_open();
}

static int compose_active_window;

/**
 * MAILWRITE ARexx command.
 *
 * @param rxmsg the message defining the ARexx context.
 * @param args
 */
static void arexx_mailwrite(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct	{
		STRPTR var;
		STRPTR stem;
		ULONG *window;
		ULONG quiet;
		STRPTR mailto;
		STRPTR subject;
		STRPTR body;
		STRPTR *attachments;
	} mailwrite_arg;
	memset(&mailwrite_arg,0,sizeof(mailwrite_arg));

	if ((arg_handle = ParseTemplate("VAR/K,STEM/K,WINDOW/N,QUIET/S,MAILTO/K,SUBJECT/K,BODY/K,ATTACHMENT/K/M",args,&mailwrite_arg)))
	{
		if (mailwrite_arg.window)
		{
			compose_window_activate(*mailwrite_arg.window);
			compose_active_window = *mailwrite_arg.window;
		} else
		{
			int window;
			char *unescaped_body = NULL;

			if (mailwrite_arg.body)
			{
				char *body;
				char *body2;

				if ((body = mystrreplace(mailwrite_arg.body, "**", "*")))
				{
					if ((body2 = mystrreplace(body, "*n", "\n")))
						unescaped_body = body2;
					free(body);
				}
			}

			compose_active_window = window = callback_write_mail_to_str_with_body(mailwrite_arg.mailto,mailwrite_arg.subject,unescaped_body);

			if (unescaped_body)
				free(unescaped_body);

			if (mailwrite_arg.attachments)
				compose_window_attach(window,mailwrite_arg.attachments);

			if (mailwrite_arg.stem)
			{
				int stem_len = strlen(mailwrite_arg.stem);
				char *stem_buf = malloc(stem_len+20);
				if (stem_buf)
				{
					strcpy(stem_buf,mailwrite_arg.stem);
					strcat(stem_buf,"WINDOW");
					arexx_set_var_int(rxmsg,stem_buf,window);
					free(stem_buf);
				}
			} else
			{
				char num_buf[24];
				sprintf(num_buf,"%d",window);
				if (mailwrite_arg.var) MySetRexxVarFromMsg(mailwrite_arg.var,num_buf,rxmsg);
				else arexx_set_result(rxmsg,num_buf);
			}
		}
		FreeTemplate(arg_handle);
	}
}

/**
 * WRITEATTACH ARexx command.
 *
 * @param rxmsg the message defining the ARexx context.
 * @param args the command's arguments
 */
static void arexx_writeattach(struct RexxMsg *rxmsg, STRPTR args)
{
#if 0
	APTR arg_handle;
	ULONG window;

	struct
	{
		STRPTR file;
		STRPTR desc;
		STRPTR encmode;
		STRPTR ctype;
		ULONG *window;
	} writeattach_arg;
	memset(&writeattach_arg,0,sizeof(writeattach_arg));

	if (!(arg_handle = ParseTemplate("FILE/A,DESC,ENCMODE,CTYPE,WINDOW/N",args,&writeattach_arg)))
		return;

	if (writeattach_arg.window)
		window = *writeattach_arg.window;
	else window = -1;

	FreeTemplate(arg_handle);
#endif
}

/**
 * WRITECLOSE ARexx command
 *
 * @param rxmsg the message defining the ARexx context.
 * @param args the command's arguments
 */
static void arexx_writeclose(struct RexxMsg *rxmsg, STRPTR args)
{
	compose_window_close(compose_active_window,COMPOSE_CLOSE_CANCEL);
}

/**
 * SETMAIL ARexx command
 *
 * @param rxmsg the message defining the ARexx context.
 * @param args the command's arguments
 */
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

/**
 * SETMAILFILE ARexx command
 *
 * @param rxmsg the message defining the ARexx context.
 * @param args the command's arguments
 */
static void arexx_setmailfile(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct	{
		STRPTR mailfile;
	} setmailfile_arg;
	memset(&setmailfile_arg,0,sizeof(setmailfile_arg));

	if ((arg_handle = ParseTemplate("MAILFILE/A",args,&setmailfile_arg)))
	{
		struct mail_info *m = folder_find_mail_by_filename(main_get_folder(),setmailfile_arg.mailfile);
		if (m)
		{
			callback_select_mail(folder_get_index_of_mail(main_get_folder(),m));
		}
		FreeTemplate(arg_handle);
	}
}

/**
 * GETSELECTED ARexx command.
 *
 * @param rxmsg the message defining the ARexx context.
 * @param args the command's arguments
 */
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
		struct mail_info *mail;
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
					MySetRexxVarFromMsg(stem_buf,num_buf,rxmsg);

					mail = main_get_mail_first_selected(&handle);
					while (mail)
					{
						sprintf(&stem_buf[stem_len],"NUM.%d",i);
						sprintf(num_buf,"%d",folder_get_index_of_mail(folder,mail));
						MySetRexxVarFromMsg(stem_buf,num_buf,rxmsg);
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

				if (getselected_arg.var) MySetRexxVarFromMsg(getselected_arg.var,str,rxmsg);
				else arexx_set_result(rxmsg,str);

				free(str);
			}
		}
		FreeTemplate(arg_handle);
	}
}

/**
 * GETMAILSTAT ARexx command
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_getmailstat(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct	{
		STRPTR var;
		STRPTR stem;
	} getmailstat_arg;
	memset(&getmailstat_arg,0,sizeof(getmailstat_arg));

	if ((arg_handle = ParseTemplate("VAR/K,STEM/K",args,&getmailstat_arg)))
	{
		int total_msg,total_unread,total_new;

		folder_get_stats(&total_msg,&total_unread,&total_new);

		if (getmailstat_arg.stem)
		{
			int stem_len = strlen(getmailstat_arg.stem);
			char *stem_buf = malloc(stem_len+20);
			if (stem_buf)
			{
				strcpy(stem_buf,getmailstat_arg.stem);

				strcpy(&stem_buf[stem_len],"TOTAL");
				arexx_set_var_int(rxmsg,stem_buf,total_msg);
				strcpy(&stem_buf[stem_len],"NEW");
				arexx_set_var_int(rxmsg,stem_buf,total_new);
				strcpy(&stem_buf[stem_len],"UNREAD");
				arexx_set_var_int(rxmsg,stem_buf,total_unread);

				free(stem_buf);
			}
		} else
		{
			char num_buf[36];
			sprintf(num_buf,"%d %d %d",total_msg,total_new,total_unread);
			if (getmailstat_arg.var) MySetRexxVarFromMsg(getmailstat_arg.var,num_buf,rxmsg);
			else arexx_set_result(rxmsg,num_buf);
		}
		FreeTemplate(arg_handle);
	}
}

/**
 * SHOW ARexx command
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_show(struct RexxMsg *rxmsg, STRPTR args)
{
	app_show();
}

/**
 * HIDE ARexx command
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_hide(struct RexxMsg *rxmsg, STRPTR args)
{
	app_hide();
}

/**
 * QUIT ARexx command
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_quit(struct RexxMsg *rxmsg, STRPTR args)
{
	app_quit();
}

/**
 * FOLDERINFO ARexx command
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_folderinfo(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct	{
		STRPTR var;
		STRPTR stem;
		STRPTR folder;
	} folderinfo_arg;
	memset(&folderinfo_arg,0,sizeof(folderinfo_arg));

	if ((arg_handle = ParseTemplate("VAR/K,STEM/K,FOLDER",args,&folderinfo_arg)))
	{
		struct folder *folder;

		if (folderinfo_arg.folder) folder = folder_find_by_name(folderinfo_arg.folder);
		else folder = main_get_folder();

		if (folder)
		{
			int folder_size = folder_size_of_mails(folder);
			int folder_type;

			if (folder == folder_incoming()) folder_type = 1;
			else if (folder == folder_outgoing()) folder_type = 2;
			else if (folder == folder_sent()) folder_type = 3;
			else if (folder == folder_deleted()) folder_type = 4;
			else if (folder->type == FOLDER_TYPE_SEND) folder_type = 5;
			else if (folder->type == FOLDER_TYPE_SENDRECV) folder_type = 6;
			else if (folder->special == FOLDER_SPECIAL_GROUP) folder_type = 7;
			else if (folder == folder_spam()) folder_type = 8;
			else folder_type = 0;

			if (folderinfo_arg.stem)
			{
				int stem_len = strlen(folderinfo_arg.stem);
				char *stem_buf = malloc(stem_len+20);
				if (stem_buf)
				{
					strcpy(stem_buf,folderinfo_arg.stem);

					strcpy(&stem_buf[stem_len],"NUMBER");
					arexx_set_var_int(rxmsg,stem_buf,folder_position(folder));
					strcpy(&stem_buf[stem_len],"NAME");
					MySetRexxVarFromMsg(stem_buf,folder->name,rxmsg);
					strcpy(&stem_buf[stem_len],"PATH");
					MySetRexxVarFromMsg(stem_buf,folder->path,rxmsg);
					strcpy(&stem_buf[stem_len],"TOTAL");
					arexx_set_var_int(rxmsg,stem_buf,folder->num_mails);
					strcpy(&stem_buf[stem_len],"NEW");
					arexx_set_var_int(rxmsg,stem_buf,folder->new_mails);
					strcpy(&stem_buf[stem_len],"UNREAD");
					arexx_set_var_int(rxmsg,stem_buf,folder->unread_mails);
					strcpy(&stem_buf[stem_len],"SIZE");
					arexx_set_var_int(rxmsg,stem_buf,folder_size);
					strcpy(&stem_buf[stem_len],"TYPE");
					arexx_set_var_int(rxmsg,stem_buf,folder_type);

					free(stem_buf);
				}
			} else
			{
				char *str;
				char num_buf[24];

				sprintf(num_buf,"%d",folder_position(folder));
				str = mystrdup(num_buf);
				str = stradd(str," \"");
				str = stradd(str,folder->name);
				str = stradd(str,"\" \"");
				str = stradd(str,folder->path);
				str = stradd(str,"\" ");
				sprintf(num_buf,"%d",folder->num_mails);
				str = stradd(str,num_buf);
				str = stradd(str," ");
				sprintf(num_buf,"%d",folder->new_mails);
				str = stradd(str,num_buf);
				str = stradd(str," ");
				sprintf(num_buf,"%d",folder->unread_mails);
				str = stradd(str,num_buf);
				str = stradd(str," ");
				sprintf(num_buf,"%d",folder_size);
				str = stradd(str,num_buf);
				str = stradd(str," ");
				sprintf(num_buf,"%d",folder_type);
				str = stradd(str,num_buf);

				if (folderinfo_arg.var) MySetRexxVarFromMsg(folderinfo_arg.var,str,rxmsg);
				else arexx_set_result(rxmsg,str);

				free(str);
			}
		}
		FreeTemplate(arg_handle);
	}
}

/**
 * REQUEST ARexx command
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_request(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct	{
		STRPTR var;
		STRPTR stem;
		STRPTR body;
		STRPTR gadgets;
	} request_arg;
	memset(&request_arg,0,sizeof(request_arg));

	if ((arg_handle = ParseTemplate("VAR/K,STEM/K,BODY/A,GADGETS/A",args,&request_arg)))
	{
		int result;
		result = sm_request(NULL,request_arg.body,request_arg.gadgets,NULL);
		if (request_arg.stem)
		{
			int stem_len = strlen(request_arg.stem);
			char *stem_buf = malloc(stem_len+20);
			if (stem_buf)
			{
				strcpy(stem_buf,request_arg.stem);
				strcat(stem_buf,"RESULT");
				arexx_set_var_int(rxmsg,stem_buf,result);
				free(stem_buf);
			}
		} else
		{
			char num_buf[24];
			sprintf(num_buf,"%d",result);
			if (request_arg.var) MySetRexxVarFromMsg(request_arg.var,num_buf,rxmsg);
			else arexx_set_result(rxmsg,num_buf);
		}
		FreeTemplate(arg_handle);
	}
}

/**
 * REQUESTSTRING ARexx command
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_requeststring(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct	{
		STRPTR var;
		STRPTR stem;
		STRPTR body;
		STRPTR string;
		ULONG secret;
	} requeststring_arg;
	memset(&requeststring_arg,0,sizeof(requeststring_arg));

	if ((arg_handle = ParseTemplate("VAR/K,STEM/K,BODY/A,STRING/K,SECRET/S",args,&requeststring_arg)))
	{
		char *result;
		if ((result = sm_request_string(NULL,requeststring_arg.body,requeststring_arg.string,requeststring_arg.secret)))
		{
			if (requeststring_arg.stem)
			{
				int stem_len = strlen(requeststring_arg.stem);
				char *stem_buf = malloc(stem_len+20);
				if (stem_buf)
				{
					strcpy(stem_buf,requeststring_arg.stem);
					strcat(stem_buf,"STRING");
					MySetRexxVarFromMsg(stem_buf,result,rxmsg);
					free(stem_buf);
				}
			} else
			{
				if (requeststring_arg.var) MySetRexxVarFromMsg(requeststring_arg.var,result,rxmsg);
				else arexx_set_result(rxmsg,result);
			}
			free(result);
		} else
		{
			/* error so set the RC val to 1 */
			rxmsg->rm_Result1 = 1;
		}
		FreeTemplate(arg_handle);
	}
}

/**
 * REQUESTFILE ARexx command
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_requestfile(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct	{
		STRPTR stem;
		STRPTR drawer;
		STRPTR file;
		STRPTR pattern;
		STRPTR title;
		STRPTR positive;
		STRPTR negative;
		STRPTR acceptpattern;
		STRPTR rejectpattern;
		ULONG savemode;
		ULONG multiselect;
		ULONG drawersonly;
		ULONG noicons;
	} requestfile_arg;
	memset(&requestfile_arg,0,sizeof(requestfile_arg));

	if ((arg_handle = ParseTemplate("STEM/K,DRAWER,FILE/K,PATTERN/K,TITLE/K,POSITIVE/K,NEGATIVE/K,ACCEPTPATTERN/K,REJECTPATTERN/K,SAVEMODE/S,MULTISELECT/S,DRAWERSONLY/S,NOICONS/S",args,&requestfile_arg)))
	{
		struct FileRequester *filereq = AllocAslRequest(ASL_FileRequest,NULL);
		if (filereq)
		{
			UBYTE *acceptpattern = NULL,*rejectpattern = NULL;
			int stem_len;
			char *stem_buf;
			char num_buf[32];

			if (requestfile_arg.stem)
			{
				stem_len = strlen(requestfile_arg.stem);
				if ((stem_buf = malloc(stem_len+20)))
				{
					strcpy(stem_buf,requestfile_arg.stem);
				} else stem_len = 0;
			} else
			{
				stem_buf = 0;
				stem_len = 0;
			}


			if (requestfile_arg.acceptpattern)
			{
				int len = strlen(requestfile_arg.acceptpattern)*2+3;
				if ((acceptpattern = (UBYTE*)AllocVec(len,0)))
					ParsePatternNoCase(requestfile_arg.acceptpattern,(STRPTR)acceptpattern,len);
			}

			if (requestfile_arg.rejectpattern)
			{
				int len = strlen(requestfile_arg.rejectpattern)*2+3;
				if ((rejectpattern = (UBYTE*)AllocVec(len,0)))
					ParsePatternNoCase(requestfile_arg.rejectpattern,(STRPTR)rejectpattern,len);
			}

			if (AslRequestTags(filereq,
						ASLFR_Screen, main_get_screen(),
						requestfile_arg.title?ASLFR_TitleText:TAG_IGNORE, requestfile_arg.title,
						requestfile_arg.positive?ASLFR_PositiveText:TAG_IGNORE, requestfile_arg.positive,
						requestfile_arg.negative?ASLFR_NegativeText:TAG_IGNORE, requestfile_arg.negative,
						requestfile_arg.drawer?ASLFR_InitialDrawer:TAG_IGNORE, requestfile_arg.drawer,
						requestfile_arg.file?ASLFR_InitialFile:TAG_IGNORE, requestfile_arg.file,
						requestfile_arg.pattern?ASLFR_InitialPattern:TAG_IGNORE, requestfile_arg.pattern,
						rejectpattern?ASLFR_RejectPattern:TAG_IGNORE, rejectpattern,
						acceptpattern?ASLFR_AcceptPattern:TAG_IGNORE, acceptpattern,
						ASLFR_RejectIcons, requestfile_arg.noicons,
						ASLFR_DrawersOnly, requestfile_arg.drawersonly,
						ASLFR_DoMultiSelect, requestfile_arg.multiselect,
						ASLFR_DoSaveMode, requestfile_arg.savemode,
						requestfile_arg.pattern?ASLFR_DoPatterns:TAG_IGNORE, TRUE,
						TAG_DONE))
			{
				int i;
				char *name;
				STRPTR dirname;
				BPTR dirlock;

				if (stem_buf)
				{
					strcpy(&stem_buf[stem_len],"PATH.COUNT");
					sprintf(num_buf,"%ld",filereq->fr_NumArgs);
					MySetRexxVarFromMsg(stem_buf,num_buf,rxmsg);

					for (i=0;i<filereq->fr_NumArgs;i++)
					{
						sprintf(&stem_buf[stem_len],"PATH.%d",i);
						if ((dirname = NameOfLock(filereq->fr_ArgList[i].wa_Lock)))
						{
							if ((name = mycombinepath(dirname,filereq->fr_ArgList[i].wa_Name)))
							{
								MySetRexxVarFromMsg(stem_buf,name,rxmsg);
								free(name);
							} else MySetRexxVarFromMsg(stem_buf,"",rxmsg);
							FreeVec(dirname);
						} else MySetRexxVarFromMsg(stem_buf,"",rxmsg);
					}

					strcpy(&stem_buf[stem_len],"FILE");
					MySetRexxVarFromMsg(stem_buf,filereq->fr_File,rxmsg);

					strcpy(&stem_buf[stem_len],"DRAWER");
					MySetRexxVarFromMsg(stem_buf,filereq->fr_Drawer,rxmsg);

					strcpy(&stem_buf[stem_len],"PATTERN");
					MySetRexxVarFromMsg(stem_buf,filereq->fr_Pattern,rxmsg);
				}

				if ((dirlock = Lock(filereq->fr_Drawer,ACCESS_READ)))
				{
					if ((dirname = NameOfLock(dirlock)))
					{
						if ((name = mycombinepath(dirname,filereq->fr_File)))
						{
							arexx_set_result(rxmsg,name);
							free(name);
						}
						FreeVec(dirname);
					}
					UnLock(dirlock);
				}
			} else
			{
				if (stem_buf)
				{
					strcpy(&stem_buf[stem_len],"PATH.COUNT");
					MySetRexxVarFromMsg(stem_buf,"0",rxmsg);
				}
				arexx_set_result(rxmsg,"");
			}
		}
		FreeTemplate(arg_handle);
	}
}

/**
 * MAILINFO ARexx command
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_mailinfo(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct	{
		STRPTR stem;
		ULONG *index;
	} mailinfo_arg;
	memset(&mailinfo_arg,0,sizeof(mailinfo_arg));

	if ((arg_handle = ParseTemplate("STEM/K,INDEX/N",args,&mailinfo_arg)))
	{
		struct mail_info *mail = NULL;
		if (mailinfo_arg.index)
		{
			struct folder *f = main_get_folder();
			if (f) mail = folder_find_mail_by_position(f, *mailinfo_arg.index);
		} else mail = main_get_active_mail();

		if (mail)
		{
			char *mail_status;
			char *mail_from = mail_get_from_address(mail);
			char *mail_to = mail_get_to_address(mail);
			char *mail_replyto = mail_get_replyto_address(mail);
			char *mail_date = NULL;
			char mail_flags[9];
			int mail_index;

			switch (mail_get_status_type(mail))
			{
				case	MAIL_STATUS_READ: mail_status = "O";break;
				case	MAIL_STATUS_WAITSEND: mail_status = "W";break;
				case	MAIL_STATUS_SENT: mail_status = "S";break;
				case	MAIL_STATUS_REPLIED: mail_status = "R";break;
				case	MAIL_STATUS_FORWARD: mail_status = "F";break;
				case	MAIL_STATUS_REPLFORW: mail_status = "R";break;
				case	MAIL_STATUS_HOLD: mail_status = "H";break;
				case	MAIL_STATUS_ERROR: mail_status = "E";break;
				case	MAIL_STATUS_SPAM: mail_status = "M";break;
				default: if (mail->flags & MAIL_FLAGS_NEW) mail_status = "N";
								 else mail_status = "U"; break;
			}

			mail_index = folder_get_index_of_mail(main_get_folder(),mail);

			{
				char *date = malloc(LEN_DATSTRING);
				char *time = malloc(LEN_DATSTRING);
				struct DateTime dt;

				if (date && time)
				{
					dt.dat_Stamp.ds_Days = mail->seconds / (60*60*24);
					dt.dat_Stamp.ds_Minute = (mail->seconds % (60*60*24))/60;
					dt.dat_Stamp.ds_Tick = (mail->seconds % 60) * 50;
					dt.dat_Format = FORMAT_USA;
					dt.dat_Flags = 0;
					dt.dat_StrDate = date;
					dt.dat_StrTime = time;
					DateToStr(&dt);
					if ((mail_date = malloc(2*LEN_DATSTRING)))
					sprintf(mail_date,"%s %s",date,time);
				}
				free(date);
				free(time);
			}

			strcpy(mail_flags,"--------");
			if (mail->flags & MAIL_FLAGS_GROUP) mail_flags[0] = 'M';
			if (mail->flags & MAIL_FLAGS_ATTACH) mail_flags[1] = 'A';
			if (mail->flags & MAIL_FLAGS_CRYPT) mail_flags[3] = 'C';
			if (mail->flags & MAIL_FLAGS_SIGNED) mail_flags[4] = 'S';

			if (mailinfo_arg.stem)
			{
				int stem_len = strlen(mailinfo_arg.stem);
				char *stem_buf = malloc(stem_len+20);
				if (stem_buf)
				{
					strcpy(stem_buf,mailinfo_arg.stem);

					strcpy(&stem_buf[stem_len],"INDEX");
					arexx_set_var_int(rxmsg,stem_buf,mail_index);
					strcpy(&stem_buf[stem_len],"STATUS");
					MySetRexxVarFromMsg(stem_buf,mail_status,rxmsg);
					strcpy(&stem_buf[stem_len],"FROM");
					MySetRexxVarFromMsg(stem_buf,mail_from,rxmsg);
					strcpy(&stem_buf[stem_len],"TO");
					MySetRexxVarFromMsg(stem_buf,mail_to,rxmsg);
					strcpy(&stem_buf[stem_len],"REPLYTO");
					MySetRexxVarFromMsg(stem_buf,mail_replyto,rxmsg);
					strcpy(&stem_buf[stem_len],"SUBJECT");
					MySetRexxVarFromMsg(stem_buf,mail->subject,rxmsg);
					strcpy(&stem_buf[stem_len],"FILENAME");
					MySetRexxVarFromMsg(stem_buf,mail->filename,rxmsg);
					strcpy(&stem_buf[stem_len],"SIZE");
					arexx_set_var_int(rxmsg,stem_buf,mail->size);
					strcpy(&stem_buf[stem_len],"DATE");
					MySetRexxVarFromMsg(stem_buf,mail_date,rxmsg);
					strcpy(&stem_buf[stem_len],"FLAGS");
					MySetRexxVarFromMsg(stem_buf,mail_flags,rxmsg);
					strcpy(&stem_buf[stem_len],"FOLDER");
					MySetRexxVarFromMsg(stem_buf,main_get_folder()->name,rxmsg);
					strcpy(&stem_buf[stem_len],"MSGID");
					arexx_set_var_int(rxmsg,stem_buf,0); /* Not supported yet */

					free(stem_buf);
				}
			}
			free(mail_date);
			free(mail_replyto);
			free(mail_to);
			free(mail_from);
		}
		FreeTemplate(arg_handle);
	}
}

/**
 * SETFOLDER ARexx command
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_setfolder(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct	{
		STRPTR folder;
	} setfolder_arg;
	memset(&setfolder_arg,0,sizeof(setfolder_arg));

	if ((arg_handle = ParseTemplate("FOLDER/A",args,&setfolder_arg)))
	{
		struct folder *f = folder_find_by_name(setfolder_arg.folder);
		if (f) main_set_folder_active(f);
		FreeTemplate(arg_handle);
	}
}

/**
 * ADDRGOTO ARexx command
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_addrgoto(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct	{
		STRPTR alias;
	} addrgoto_arg;
	memset(&addrgoto_arg,0,sizeof(addrgoto_arg));

	if ((arg_handle = ParseTemplate("ALIAS/A",args,&addrgoto_arg)))
	{
		addressbookwnd_set_active_alias(addrgoto_arg.alias);
		FreeTemplate(arg_handle);
	}
}

/**
 * ADDRNEW ARexx command
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_addrnew(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct	{
		STRPTR type;
		STRPTR alias;
		STRPTR name;
		STRPTR email;
	} addrnew_arg;
	memset(&addrnew_arg,0,sizeof(addrnew_arg));

	if ((arg_handle = ParseTemplate("TYPE,ALIAS,NAME,EMAIL",args,&addrnew_arg)))
	{
		if (addrnew_arg.type && (toupper((unsigned char)(*addrnew_arg.type)) == 'G'))
		{
			addressbook_add_group(addrnew_arg.alias);
		} else
		{
			struct addressbook_entry_new *entry;
			char *name;

			name = utf8create(addrnew_arg.name, user.config.default_codeset?user.config.default_codeset->name:NULL);

			if ((entry = addressbook_add_entry(name)))
			{
				entry->alias = mystrdup(addrnew_arg.alias);
				entry->email_array = array_add_string(entry->email_array,addrnew_arg.email);
			}

			free(name);
		}

		main_build_addressbook();
		addressbookwnd_refresh();

		FreeTemplate(arg_handle);
	}
}

/**
 * ADDRSAVE ARexx command
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_addrsave(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct	{
		STRPTR filename;
	} addrsave_arg;
	memset(&addrsave_arg,0,sizeof(addrsave_arg));

	if ((arg_handle = ParseTemplate("FILENAME",args,&addrsave_arg)))
	{
		if (addrsave_arg.filename) addressbook_save_as(addrsave_arg.filename);
		else addressbook_save();
		FreeTemplate(arg_handle);
	}
}

/**
 * ADDRLOAD ARexx command
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_addrload(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct	{
		STRPTR filename;
		ULONG append;
	} addrload_arg;
	memset(&addrload_arg,0,sizeof(addrload_arg));

	if ((arg_handle = ParseTemplate("FILENAME,APPEND/S",args,&addrload_arg)))
	{
		addressbook_import_file(addrload_arg.filename,addrload_arg.append);
		main_build_addressbook();
		addressbookwnd_refresh();

		FreeTemplate(arg_handle);
	}

}

/**
 * GETURL ARexx command
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_geturl(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct	{
		STRPTR url;
		STRPTR filename;
	} geturl_arg;
	memset(&geturl_arg,0,sizeof(geturl_arg));

	if ((arg_handle = ParseTemplate("URL/A,FILENAME/A",args,&geturl_arg)))
	{
		void *buf;
		int buf_len;

		if (http_download(geturl_arg.url, &buf, &buf_len))
		{
			FILE *fh;
			if ((fh = fopen(geturl_arg.filename,"wb")))
			{
				fwrite(buf,1,buf_len,fh);
				fclose(fh);
			}
		} else rxmsg->rm_Result1 = 10;
		FreeTemplate(arg_handle);
	}
}

/**
 * NEWMAILFILE ARexx command
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_newmailfile(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct	{
		STRPTR var;
		STRPTR stem;
		STRPTR folder;
	} newmailfile_arg;
	memset(&newmailfile_arg,0,sizeof(newmailfile_arg));

	if ((arg_handle = ParseTemplate("VAR/K,STEM/K,FOLDER",args,&newmailfile_arg)))
	{
		struct folder *folder;

		if (newmailfile_arg.folder) folder = folder_find_by_name(newmailfile_arg.folder);
		else folder = main_get_folder();

		if (folder)
		{
			BPTR lock = Lock(folder->path,ACCESS_READ);
			if (lock)
			{
				BPTR odir = CurrentDir(lock);
				char *name = mail_get_new_name(MAIL_STATUS_UNREAD);
				if (name)
				{
					STRPTR fulldirname = NameOfLock(lock);
					if (fulldirname)
					{
						char *newname = mycombinepath(fulldirname, name);
						if (newname)
						{
							BPTR out;

							/* create the file, so a next call would not return the same result */
							if ((out = Open(newname,MODE_NEWFILE))) Close(out);

							if (newmailfile_arg.stem)
							{
								int stem_len = strlen(newmailfile_arg.stem);
								char *stem_buf = malloc(stem_len+20);
								if (stem_buf)
								{
									strcpy(stem_buf,newmailfile_arg.stem);
									strcat(stem_buf,"FILENAME");
									MySetRexxVarFromMsg(stem_buf,newname,rxmsg);
									free(stem_buf);
								}
							} else
							{
								if (newmailfile_arg.var) MySetRexxVarFromMsg(newmailfile_arg.var,newname,rxmsg);
								else arexx_set_result(rxmsg,newname);
							}

							free(newname);
						}
						FreeVec(fulldirname);
					}
					free(name);
				}
				CurrentDir(odir);
				UnLock(lock);
			}
		}

		FreeTemplate(arg_handle);
	}
}

static int read_active_window;

/**
 * MAILREAD ARexx command
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_mailread(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct	{
		STRPTR var;
		STRPTR stem;
		LONG *window;
		LONG quiet;
	} mailread_arg;
	memset(&mailread_arg,0,sizeof(mailread_arg));

	if ((arg_handle = ParseTemplate("VAR/K,STEM/K,WINDOW/N,QUIET/S",args,&mailread_arg)))
	{
		if (mailread_arg.window)
		{
			read_window_activate(*mailread_arg.window);
			read_active_window = *mailread_arg.window;
		} else
		{
			int window;

			read_active_window = window = callback_read_active_mail();

			if (mailread_arg.stem)
			{
				int stem_len = strlen(mailread_arg.stem);
				char *stem_buf = malloc(stem_len+20);
				if (stem_buf)
				{
					strcpy(stem_buf,mailread_arg.stem);
					strcat(stem_buf,"WINDOW");
					arexx_set_var_int(rxmsg,stem_buf,window);
					free(stem_buf);
				}
			} else
			{
				char num_buf[24];
				sprintf(num_buf,"%d",window);
				if (mailread_arg.var) MySetRexxVarFromMsg(mailread_arg.var,num_buf,rxmsg);
				else arexx_set_result(rxmsg,num_buf);
			}
		}
		FreeTemplate(arg_handle);
	}
}

/**
 * READCLOSE ARexx command
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_readclose(struct RexxMsg *rxmsg, STRPTR args)
{
	read_window_close(read_active_window);
}

/**
 * READINFO ARexx command
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_readinfo(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct	{
		STRPTR var;
		STRPTR stem;
	} readinfo_arg;
	memset(&readinfo_arg,0,sizeof(readinfo_arg));

	if ((arg_handle = ParseTemplate("VAR/K,STEM/K",args,&readinfo_arg)))
	{
		char num_buf[20];
		struct mail_complete *mail;

		if ((mail = read_window_get_displayed_mail(read_active_window)))
		{
			mail = mail_get_root(mail);

			if (readinfo_arg.stem)
			{
				int stem_len = strlen(readinfo_arg.stem);
				char *stem_buf = malloc(stem_len+20);
				if (stem_buf)
				{
					int i, count = 0;
					struct mail_complete *t = mail;

					while (t)
					{
						mail_decode(t);
						t = mail_get_next(t);
						count++;
					}

					strcpy(stem_buf,readinfo_arg.stem);
					strcpy(&stem_buf[stem_len],"FILENAME.COUNT");
					sprintf(num_buf,"%d",count);
					MySetRexxVarFromMsg(stem_buf,num_buf,rxmsg);

					strcpy(&stem_buf[stem_len],"FILETYPE.COUNT");
					sprintf(num_buf,"%d",count);
					MySetRexxVarFromMsg(stem_buf,num_buf,rxmsg);

					strcpy(&stem_buf[stem_len],"FILESIZE.COUNT");
					sprintf(num_buf,"%d",count);
					MySetRexxVarFromMsg(stem_buf,num_buf,rxmsg);

					i = 0;

					while (mail)
					{
						char *char_buf;
						int mail_size;

						if (mail->decoded_data) mail_size = mail->decoded_len;
						else mail_size = mail->text_len;

						sprintf(&stem_buf[stem_len],"FILENAME.%d",i);
						char_buf = mystrdup(mail->content_name);
						if (char_buf)
							MySetRexxVarFromMsg(stem_buf,char_buf,rxmsg);
						free(char_buf);
						char_buf = NULL;

						sprintf(&stem_buf[stem_len],"FILETYPE.%d",i);
						char_buf = mystrdup(mail->content_type);
						char_buf = stradd(char_buf,"/");
						char_buf = stradd(char_buf,mail->content_subtype);
						if (char_buf)
							MySetRexxVarFromMsg(stem_buf,char_buf,rxmsg);
						free(char_buf);
						char_buf = NULL;

						sprintf(&stem_buf[stem_len],"FILESIZE.%d",i);
						sprintf(num_buf,"%d",mail_size);
						MySetRexxVarFromMsg(stem_buf,num_buf,rxmsg);

						mail = mail_get_next(mail);
						i++;
					}
					free(stem_buf);
				}
			}
		}
		FreeTemplate(arg_handle);
	}
}

/**
 * READSAVE ARexx command
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_readsave(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct	{
		LONG *part;
		STRPTR filename;
		LONG overwrite;
	} readsave_arg;
	memset(&readsave_arg,0,sizeof(readsave_arg));

	if ((arg_handle = ParseTemplate("PART/N,FILENAME/A/K,OVERWRITE/S",args,&readsave_arg)))
	{
		struct mail_complete *mail = read_window_get_displayed_mail(read_active_window);
		if (mail && readsave_arg.part)
		{
			int i;
			mail = mail_get_root(mail);
			for (i=0;i<*readsave_arg.part;i++)
				mail = mail_get_next(mail);
		}

		if (mail)
		{
			BPTR fh;
			int ok = 0;

			mail_decode(mail);

			fh = Open(readsave_arg.filename,MODE_OLDFILE);
			if (fh) Close(fh);
			else ok = 1;

			if (ok || readsave_arg.overwrite)
			{
				if ((fh = Open(readsave_arg.filename, MODE_NEWFILE)))
				{
					if (!mail->decoded_data) Write(fh,mail->text + mail->text_begin,mail->text_len);
					Write(fh,mail->decoded_data,mail->decoded_len);
					Close(fh);
				}
			}
		}
		FreeTemplate(arg_handle);
	}
}

/**
 * REQUESTFOLDER ARexx command
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_requestfolder(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct	{
		STRPTR var;
		STRPTR stem;
		STRPTR body;
		ULONG excludeactive;
	} requestfolder_arg;
	memset(&requestfolder_arg,0,sizeof(requestfolder_arg));

	if ((arg_handle = ParseTemplate("VAR/K,STEM/K,BODY/A,EXCLUDEACTIVE/S",args,&requestfolder_arg)))
	{
		struct folder *exclude;
		struct folder *folder;
		if (requestfolder_arg.excludeactive) exclude = main_get_folder();
		else exclude = NULL;

		if ((folder = sm_request_folder(requestfolder_arg.body,exclude)))
		{
			if (requestfolder_arg.stem)
			{
				int stem_len = strlen(requestfolder_arg.stem);
				char *stem_buf = malloc(stem_len+20);
				if (stem_buf)
				{
					strcpy(stem_buf,requestfolder_arg.stem);
					strcat(stem_buf,"FOLDER");
					MySetRexxVarFromMsg(stem_buf,folder->name,rxmsg);
					free(stem_buf);
				}
			} else
			{
				if (requestfolder_arg.var) MySetRexxVarFromMsg(requestfolder_arg.var,folder->name,rxmsg);
				else arexx_set_result(rxmsg,folder->name);
			}
		} else
		{
			/* error so set the RC val to 1 */
			rxmsg->rm_Result1 = 1;
		}
		FreeTemplate(arg_handle);
	}
}

/**
 * MAILADD Arexx Command
 *
 * Adds a mail with FILENAME into the folder. If the mail
 * (FILENAME) is already in the folder it isn't copied. Otherwise
 * it is copied. Returns the filename of the mail.
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_mailadd(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct	{
		STRPTR var;
		STRPTR stem;
		STRPTR filename;
		STRPTR folder;
	} mailadd_arg;
	memset(&mailadd_arg,0,sizeof(mailadd_arg));

	if ((arg_handle = ParseTemplate("VAR/K,STEM/K,FILENAME/A,FOLDER",args,&mailadd_arg)))
	{
		struct folder *folder;
		struct mail_info *mail;
		if (mailadd_arg.folder) folder = folder_find_by_name(mailadd_arg.folder);
		else folder = main_get_folder();

		if ((mail = callback_new_mail_to_folder(mailadd_arg.filename,folder)))
		{
			if (mailadd_arg.stem)
			{
				int stem_len = strlen(mailadd_arg.stem);
				char *stem_buf = malloc(stem_len+20);
				if (stem_buf)
				{
					strcpy(stem_buf,mailadd_arg.stem);
					strcat(stem_buf,"FILENAME");
					MySetRexxVarFromMsg(stem_buf,mail->filename,rxmsg);
					free(stem_buf);
				}
			} else
			{
				if (mailadd_arg.var) MySetRexxVarFromMsg(mailadd_arg.var,mail->filename,rxmsg);
				else arexx_set_result(rxmsg,mail->filename);
			}
		}

		FreeTemplate(arg_handle);
	}
}

/**
 * MAILSETSTATUS Arexx Command
 *
 * Sets the status of a given mail (or selected mail).
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_mailsetstatus(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct
	{
		STRPTR filepath;
		STRPTR status;
	} mailsetstatus_arg;
	memset(&mailsetstatus_arg,0,sizeof(mailsetstatus_arg));

	if ((arg_handle = ParseTemplate("FILENAME,STATUS/A",args,&mailsetstatus_arg)))
	{
		struct mail_info *mail;
		struct folder *f;

		if (mailsetstatus_arg.filepath)
		{
			if ((f = folder_find_by_file(mailsetstatus_arg.filepath)))
			{
				mail = folder_find_mail_by_filename(f,sm_file_part(mailsetstatus_arg.filepath));
			} else mail = NULL;
		} else
		{
			mail = main_get_active_mail();
			f = main_get_folder();
		}

		if (f && mail)
		{
			int new_status;
			switch(*mailsetstatus_arg.status)
			{
				case	'O': new_status = MAIL_STATUS_READ; break;
				case	'W': new_status = MAIL_STATUS_WAITSEND; break;
				case	'R': new_status = MAIL_STATUS_REPLIED; break;
				case	'F': new_status = MAIL_STATUS_FORWARD; break;
				case	'H': new_status = MAIL_STATUS_HOLD; break;
				case	'E': new_status = MAIL_STATUS_ERROR; break;
				case	'M': new_status = MAIL_STATUS_SPAM; break;
				case	'U': new_status = MAIL_STATUS_UNREAD; break;
				default: new_status = -1; break;
			}

			if (new_status != -1)
			{
				folder_set_mail_status(f,mail,new_status);
				main_refresh_mail(mail);
			}
		}
		FreeTemplate(arg_handle);
	}

}

/**
 * MAILFETCH Arexx Command
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_mailfetch(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct	{
		STRPTR accountemail;
		LONG *accountnum;
	} mailfetch_arg;
	memset(&mailfetch_arg,0,sizeof(mailfetch_arg));

	if ((arg_handle = ParseTemplate("ACCOUNTEMAIL/K,ACCOUNTNUM/K/N",args,&mailfetch_arg)))
	{
		if (mailfetch_arg.accountemail)
		{
			mails_dl_single_account(account_find_by_from(mailfetch_arg.accountemail));
		} else
		if (mailfetch_arg.accountnum)
		{
			mails_dl_single_account(account_find_by_number(*mailfetch_arg.accountnum));
		} else mails_dl(0);
		FreeTemplate(arg_handle);
	}
}

/**
 * OPENMESSAGE ARexx command
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_openmessage(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct {
		STRPTR var;
		STRPTR stem;
		STRPTR filename;
		LONG *window;
	} openmessage_arg;
	memset(&openmessage_arg,0,sizeof(openmessage_arg));

	if ((arg_handle = ParseTemplate("VAR/K,STEM/K,FILENAME/A,WINDOW/N",args,&openmessage_arg)))
	{
		int window = -1;

		if (openmessage_arg.window) window = *openmessage_arg.window;

		read_active_window = window = callback_open_message(openmessage_arg.filename,window);

		if (openmessage_arg.stem)
		{
			int stem_len = strlen(openmessage_arg.stem);
			char *stem_buf = malloc(stem_len+20);
			if (stem_buf)
			{
				strcpy(stem_buf,openmessage_arg.stem);
				strcat(stem_buf,"WINDOW");
				arexx_set_var_int(rxmsg,stem_buf,window);
				free(stem_buf);
			}
		} else
		{
			char num_buf[24];
			sprintf(num_buf,"%d",window);
			if (openmessage_arg.var) MySetRexxVarFromMsg(openmessage_arg.var,num_buf,rxmsg);
			else arexx_set_result(rxmsg,num_buf);
		}
		FreeTemplate(arg_handle);
	}
}

/**
 * Returns the version of SimpleMail.
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_version(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct
	{
		ULONG date;
	} version_arg;
	char buf[24];

	memset(&version_arg,0,sizeof(version_arg));

	if ((arg_handle = ParseTemplate("DATE/S",args,&version_arg)))
	{
		if (version_arg.date)
		{
			arexx_set_result(rxmsg, SIMPLEMAIL_DATE);
		} else
		{
			sm_snprintf(buf,sizeof(buf), "%d.%d",VERSION,REVISION);
			arexx_set_result(rxmsg, buf);
		}
		FreeTemplate(arg_handle);
	}
}

/**
 * Moves the active or the specified mail to a given folder.
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_mailmove(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct
	{
		STRPTR srcfolder;
		STRPTR filename;
		STRPTR destfolder;
	} mailmove_arg;

	memset(&mailmove_arg,0,sizeof(mailmove_arg));

	if ((arg_handle = ParseTemplate("SRCFOLDER,FILENAME,DESTFOLDER/A",args,&mailmove_arg)))
	{
		struct folder *src = mailmove_arg.srcfolder?folder_find_by_name(mailmove_arg.srcfolder):main_get_folder();
		struct folder *dest = folder_find_by_name(mailmove_arg.destfolder);

		if (src && dest && (src != dest))
		{
			if (src && mailmove_arg.filename)
			{
				struct mail_info *mail;

				if ((mail = folder_find_mail_by_filename(src,mailmove_arg.filename)))
					callback_move_mail(mail,src,dest);
			} else
			{
				if (!mailmove_arg.filename)
				{
					/* Only accept if source is also not given */
					void *handle = NULL;
					struct mail_info *mail;

					mail = main_get_mail_first_selected(&handle);

					while (mail)
					{
						callback_move_mail(mail,src,dest);
						mail = main_get_mail_next_selected(&handle);
					}
				}
			}
		}

		FreeTemplate(arg_handle);
	}
}


/**
 * Moves the active or the specified mail to a given folder.
 *
 * @param rxmsg the message defining the ARexx context
 * @param args the command's arguments
 */
static void arexx_maildelete(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct
	{
		STRPTR folder;
		STRPTR filename;
		LONG quiet;
	} maildelete_arg;

	memset(&maildelete_arg,0,sizeof(maildelete_arg));

	if ((arg_handle = ParseTemplate("FOLDER,FILENAME,QUIET/S",args,&maildelete_arg)))
	{
		struct folder *f = maildelete_arg.folder?folder_find_by_name(maildelete_arg.folder):main_get_folder();

		if (f)
		{
			if (maildelete_arg.filename)
			{
				struct mail_info *mail;

				if ((mail = folder_find_mail_by_filename(f,maildelete_arg.filename)))
					callback_delete_mail(mail);
			} else
			{
				if (maildelete_arg.quiet)
					callback_delete_mails_silent(0);
				else
					callback_delete_mails();
			}
		}

		FreeTemplate(arg_handle);
	}
}

/**
 * Handle this single ARexx message.
 *
 * @param rxmsg
 * @return
 */
static int arexx_message(struct RexxMsg *rxmsg)
{
	STRPTR command_line = (STRPTR)ARG0(rxmsg);
	APTR command_handle;

	struct {
		STRPTR command;
		STRPTR args;
	} command;

	command.command = command.args = NULL;
	rxmsg->rm_Result1 = 0;
	rxmsg->rm_Result2 = 0;

	SM_DEBUGF(5,("Received ARexx command: \"%s\"\n",command_line));

	if ((command_handle = ParseTemplate("COMMAND/A,ARGS/F",command_line,(LONG*)&command)))
	{
		if (!Stricmp("MAINTOFRONT",command.command)) arexx_maintofront(rxmsg,command.args);
		else if (!Stricmp("MAILWRITE",command.command)) arexx_mailwrite(rxmsg,command.args);
		else if (!Stricmp("WRITEATTACH",command.command)) arexx_writeattach(rxmsg,command.args);
		else if (!Stricmp("WRITECLOSE",command.command)) arexx_writeclose(rxmsg,command.args);
		else if (!Stricmp("SETMAIL",command.command)) arexx_setmail(rxmsg,command.args);
		else if (!Stricmp("SETMAILFILE",command.command)) arexx_setmailfile(rxmsg,command.args);
		else if (!Stricmp("SHOW",command.command)) arexx_show(rxmsg,command.args);
		else if (!Stricmp("HIDE",command.command)) arexx_hide(rxmsg,command.args);
		else if (!Stricmp("QUIT",command.command)) arexx_quit(rxmsg,command.args);
		else if (!Stricmp("GETSELECTED",command.command)) arexx_getselected(rxmsg,command.args);
		else if (!Stricmp("GETMAILSTAT",command.command)) arexx_getmailstat(rxmsg,command.args);
		else if (!Stricmp("FOLDERINFO",command.command)) arexx_folderinfo(rxmsg,command.args);
		else if (!Stricmp("REQUEST",command.command)) arexx_request(rxmsg,command.args);
		else if (!Stricmp("REQUESTSTRING",command.command)) arexx_requeststring(rxmsg,command.args);
		else if (!Stricmp("REQUESTFILE",command.command)) arexx_requestfile(rxmsg,command.args);
		else if (!Stricmp("MAILINFO",command.command)) arexx_mailinfo(rxmsg,command.args);
		else if (!Stricmp("SETFOLDER",command.command)) arexx_setfolder(rxmsg,command.args);
		else if (!Stricmp("ADDRGOTO",command.command)) arexx_addrgoto(rxmsg,command.args);
		else if (!Stricmp("ADDRNEW",command.command)) arexx_addrnew(rxmsg,command.args);
		else if (!Stricmp("ADDRSAVE",command.command)) arexx_addrsave(rxmsg,command.args);
		else if (!Stricmp("ADDRLOAD",command.command)) arexx_addrload(rxmsg,command.args);
		else if (!Stricmp("GETURL",command.command)) arexx_geturl(rxmsg,command.args);
		else if (!Stricmp("NEWMAILFILE",command.command)) arexx_newmailfile(rxmsg,command.args);
		else if (!Stricmp("MAILREAD",command.command)) arexx_mailread(rxmsg,command.args);
		else if (!Stricmp("READCLOSE",command.command)) arexx_readclose(rxmsg,command.args);
		else if (!Stricmp("READINFO",command.command)) arexx_readinfo(rxmsg,command.args);
		else if (!Stricmp("READSAVE",command.command)) arexx_readsave(rxmsg,command.args);
		else if (!Stricmp("SCREENTOBACK",command.command)) {struct Screen *scr = (struct Screen *)main_get_screen(); if (scr) ScreenToBack(scr);}
		else if (!Stricmp("SCREENTOFRONT",command.command)) {struct Screen *scr = (struct Screen *)main_get_screen(); if (scr) ScreenToFront(scr);}
		else if (!Stricmp("REQUESTFOLDER",command.command)) arexx_requestfolder(rxmsg,command.args);
		else if (!Stricmp("MAILADD",command.command)) arexx_mailadd(rxmsg,command.args);
		else if (!Stricmp("MAILSETSTATUS",command.command)) arexx_mailsetstatus(rxmsg,command.args);
		else if (!Stricmp("MAILLISTFREEZE",command.command)) main_freeze_mail_list();
		else if (!Stricmp("MAILLISTTHAW",command.command)) main_thaw_mail_list();
		else if (!Stricmp("MAILFETCH",command.command)) arexx_mailfetch(rxmsg,command.args);
		else if (!Stricmp("OPENMESSAGE",command.command)) arexx_openmessage(rxmsg,command.args);
		else if (!Stricmp("VERSION",command.command)) arexx_version(rxmsg,command.args);
		else if (!Stricmp("MAILMOVE",command.command)) arexx_mailmove(rxmsg,command.args);
		else if (!Stricmp("MAILDELETE",command.command)) arexx_maildelete(rxmsg,command.args);
		else rxmsg->rm_Result1 = 20;

		FreeTemplate(command_handle);
	}
	return 0;
}

/*****************************************************************************/

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

