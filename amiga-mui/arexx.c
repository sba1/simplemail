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

#include "folder.h"
#include "mail.h"

#include "amigasupport.h"
#include "arexx.h"
#include "mainwnd.h"
#include "simplemail.h"
#include "support.h"
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
 Sets a given variable as a integer number
*****************************************************************/
static void arexx_set_var_int(struct RexxMsg *rxmsg, char *varname, int num)
{
	char num_buf[24];
	sprintf(num_buf,"%d",num);
	SetRexxVar(rxmsg,varname,num_buf,strlen(num_buf));
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
 SETMAILFILE Arexx Command
*****************************************************************/
static void arexx_setmailfile(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct	{
		STRPTR mailfile;
	} setmailfile_arg;
	memset(&setmailfile_arg,0,sizeof(setmailfile_arg));

	if ((arg_handle = ParseTemplate("MAILFILE/A",args,&setmailfile_arg)))
	{
		struct mail *m = folder_find_mail_by_filename(main_get_folder(),setmailfile_arg.mailfile);
		if (m)
		{
			callback_select_mail(folder_get_index_of_mail(main_get_folder(),m));
		}
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
 FOLDERINFO Arexx Command
*****************************************************************/
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
					SetRexxVar(rxmsg,stem_buf,folder->name,mystrlen(folder->name));
					strcpy(&stem_buf[stem_len],"PATH");
					SetRexxVar(rxmsg,stem_buf,folder->path,mystrlen(folder->path));
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

				if (folderinfo_arg.var) SetRexxVar(rxmsg,folderinfo_arg.var,str,strlen(str));
				else arexx_set_result(rxmsg,str);

				free(str);
			}
		}
		FreeTemplate(arg_handle);
	}
}

/****************************************************************
 REQUEST Arexx Command
*****************************************************************/
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
			if (request_arg.var) SetRexxVar(rxmsg,request_arg.var,num_buf,strlen(num_buf));
			else arexx_set_result(rxmsg,num_buf);
		}
		FreeTemplate(arg_handle);
	}
}

/****************************************************************
 REQUESTSTRING Arexx Command
*****************************************************************/
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
					SetRexxVar(rxmsg,stem_buf,result,strlen(result));
					free(stem_buf);
				}
			} else
			{
				if (requeststring_arg.var) SetRexxVar(rxmsg,requeststring_arg.var,result,strlen(result));
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

/****************************************************************
 MAILINFO Arexx Command
*****************************************************************/
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
		struct mail *mail = NULL;
		if (mailinfo_arg.index)
		{
			struct folder *f = main_get_folder();
			if (f) mail = folder_find_mail(f, *mailinfo_arg.index);
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
			if (mail->flags & MAIL_FLAGS_SIGNED) mail_flags[3] = 'S';

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
					SetRexxVar(rxmsg,stem_buf,mail_status,1);
					strcpy(&stem_buf[stem_len],"FROM");
					SetRexxVar(rxmsg,stem_buf,mail_from,mystrlen(mail_from));
					strcpy(&stem_buf[stem_len],"TO");
					SetRexxVar(rxmsg,stem_buf,mail_to,mystrlen(mail_to));
					strcpy(&stem_buf[stem_len],"REPLYTO");
					SetRexxVar(rxmsg,stem_buf,mail_replyto,mystrlen(mail_replyto));
					strcpy(&stem_buf[stem_len],"SUBJECT");
					SetRexxVar(rxmsg,stem_buf,mail->subject,mystrlen(mail->subject));
					strcpy(&stem_buf[stem_len],"FILENAME");
					SetRexxVar(rxmsg,stem_buf,mail->filename,mystrlen(mail->filename));
					strcpy(&stem_buf[stem_len],"SIZE");
					arexx_set_var_int(rxmsg,stem_buf,mail->size);
					strcpy(&stem_buf[stem_len],"DATE");
					SetRexxVar(rxmsg,stem_buf,mail_date,strlen(mail_date));
					strcpy(&stem_buf[stem_len],"FLAGS");
					SetRexxVar(rxmsg,stem_buf,mail_flags,strlen(mail_flags));
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

/****************************************************************
 MAILINFO Arexx Command
*****************************************************************/
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
		else if (!Stricmp("SETMAILFILE",command.command)) arexx_setmailfile(rxmsg,command.args);
		else if (!Stricmp("SHOW",command.command)) arexx_show(rxmsg,command.args);
		else if (!Stricmp("HIDE",command.command)) arexx_hide(rxmsg,command.args);
		else if (!Stricmp("GETSELECTED",command.command)) arexx_getselected(rxmsg,command.args);
		else if (!Stricmp("FOLDERINFO",command.command)) arexx_folderinfo(rxmsg,command.args);
		else if (!Stricmp("REQUEST",command.command)) arexx_request(rxmsg,command.args);
		else if (!Stricmp("REQUESTSTRING",command.command)) arexx_requeststring(rxmsg,command.args);
		else if (!Stricmp("MAILINFO",command.command)) arexx_mailinfo(rxmsg,command.args);
		else if (!Stricmp("SETFOLDER",command.command)) arexx_setfolder(rxmsg,command.args);

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

