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
#include <proto/asl.h>
#include <proto/intuition.h> /* ScreenToXXX() */

#include "addressbook.h"
#include "folder.h"
#include "http.h"
#include "mail.h"
#include "support_indep.h"

#include "addressbookwnd.h"
#include "amigasupport.h"
#include "arexx.h"
#include "composewnd.h"
#include "mainwnd.h"
#include "readwnd.h"
#include "simplemail.h"
#include "support.h"

static struct MsgPort *arexx_port;

static struct MsgPort *arexx_execute_port;
static int arexx_execute_out; /* number of arexx messages standing out */

/* from gui_main.c */
void app_hide(void);
void app_show(void);

/* from mainwnd.c */
struct Screen *main_get_screen(void);

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

/****************************************************************
 Returns the mask of the arexx port
*****************************************************************/
ULONG arexx_mask(void)
{
	if (!arexx_port) return NULL;
	return 1UL << arexx_port->mp_SigBit;
}

/****************************************************************
 Returns the mask of the arexx port
*****************************************************************/
int arexx_execute_script(char *command)
{
	struct RexxMsg *rxmsg;
	BPTR lock;
	int rc = 0;

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

static int compose_active_window;

/****************************************************************
 MAILWRITE Arexx Command
*****************************************************************/
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
	} mailwrite_arg;
	memset(&mailwrite_arg,0,sizeof(mailwrite_arg));

	if ((arg_handle = ParseTemplate("VAR/K,STEM/K,WINDOW/N,QUIET/S,MAILTO/K,SUBJECT/K",args,&mailwrite_arg)))
	{
		if (mailwrite_arg.window)
		{
			compose_window_activate(*mailwrite_arg.window);
			compose_active_window = *mailwrite_arg.window;
		} else
		{
			int window;

			compose_active_window = window = callback_write_mail_to_str(mailwrite_arg.mailto,mailwrite_arg.subject);

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
				if (mailwrite_arg.var) SetRexxVar(rxmsg,mailwrite_arg.var,num_buf,strlen(num_buf));
				else arexx_set_result(rxmsg,num_buf);
			}
		}
		FreeTemplate(arg_handle);
	}
}

/****************************************************************
 WRITECLOSE ARexx Command
*****************************************************************/
static void arexx_writeclose(struct RexxMsg *rxmsg, STRPTR args)
{
	compose_window_close(compose_active_window,COMPOSE_CLOSE_CANCEL);
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
 REQUESTFILE Arexx Command
*****************************************************************/
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
					ParsePatternNoCase(requestfile_arg.acceptpattern,acceptpattern,len);
			}

			if (requestfile_arg.rejectpattern)
			{
				int len = strlen(requestfile_arg.rejectpattern)*2+3;
				if ((rejectpattern = (UBYTE*)AllocVec(len,0)))
					ParsePatternNoCase(requestfile_arg.rejectpattern,rejectpattern,len);
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
					sprintf(num_buf,"%d",filereq->fr_NumArgs);
					SetRexxVar(rxmsg,stem_buf,num_buf,strlen(num_buf));

					for (i=0;i<filereq->fr_NumArgs;i++)
					{
						sprintf(&stem_buf[stem_len],"PATH.%d",i);
						if ((dirname = NameOfLock(filereq->fr_ArgList[i].wa_Lock)))
						{
							if ((name = mycombinepath(dirname,filereq->fr_ArgList[i].wa_Name)))
							{
								SetRexxVar(rxmsg,stem_buf,name,strlen(name));
								free(name);
							} else SetRexxVar(rxmsg,stem_buf,"",0);
							FreeVec(dirname);
						} else SetRexxVar(rxmsg,stem_buf,"",0);
					}

					strcpy(&stem_buf[stem_len],"FILE");
					SetRexxVar(rxmsg,stem_buf,filereq->fr_File,mystrlen(filereq->fr_File));

					strcpy(&stem_buf[stem_len],"DRAWER");
					SetRexxVar(rxmsg,stem_buf,filereq->fr_Drawer,mystrlen(filereq->fr_Drawer));

					strcpy(&stem_buf[stem_len],"PATTERN");
					SetRexxVar(rxmsg,stem_buf,filereq->fr_Pattern,mystrlen(filereq->fr_Pattern));
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
					SetRexxVar(rxmsg,stem_buf,"0",1);
				}
				arexx_set_result(rxmsg,"");
			}
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
 ADDRGOTO Arexx Command
*****************************************************************/
static void arexx_addrgoto(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct	{
		STRPTR alias;
	} addrgoto_arg;
	memset(&addrgoto_arg,0,sizeof(addrgoto_arg));

	if ((arg_handle = ParseTemplate("ALIAS/A",args,&addrgoto_arg)))
	{
		addressbook_set_active(addrgoto_arg.alias);
		FreeTemplate(arg_handle);
	}
}

/****************************************************************
 ADDRNEW Arexx Command
*****************************************************************/
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
		struct addressbook_entry *entry;
		if (addrnew_arg.type && (toupper((unsigned char)(*addrnew_arg.type)) == 'G'))
		{
			entry = addressbook_new_group(NULL);
		} else
		{
			entry = addressbook_new_person(NULL,addrnew_arg.name,addrnew_arg.email);
		}

		if (addrnew_arg.alias) addressbook_set_alias(entry,addrnew_arg.alias);
		main_build_addressbook();
		addressbookwnd_refresh();

		FreeTemplate(arg_handle);
	}
}

/****************************************************************
 ADDRSAVE Arexx Command
 TODO: FILENAME
*****************************************************************/
static void arexx_addrsave(struct RexxMsg *rxmsg, STRPTR args)
{
	APTR arg_handle;

	struct	{
		STRPTR filename;
	} addrsave_arg;
	memset(&addrsave_arg,0,sizeof(addrsave_arg));

	if ((arg_handle = ParseTemplate("FILENAME",args,&addrsave_arg)))
	{
		addressbook_save();
		FreeTemplate(arg_handle);
	}
}

/****************************************************************
 ADDRLOAD ARexx Command
*****************************************************************/
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

/****************************************************************
 GETURL Arexx Command
*****************************************************************/
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

/****************************************************************
 NEWMAILFILE ARexx Command
*****************************************************************/
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
									SetRexxVar(rxmsg,stem_buf,newname,strlen(newname));
									free(stem_buf);
								}
							} else
							{
								if (newmailfile_arg.var) SetRexxVar(rxmsg,newmailfile_arg.var,newname,strlen(newname));
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

/****************************************************************
 MAILREAD ARexx Command
*****************************************************************/
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

			read_active_window = window = callback_read_mail();

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
				if (mailread_arg.var) SetRexxVar(rxmsg,mailread_arg.var,num_buf,strlen(num_buf));
				else arexx_set_result(rxmsg,num_buf);
			}
		}
		FreeTemplate(arg_handle);
	}
}

/****************************************************************
 READCLOSE ARexx Command
*****************************************************************/
static void arexx_readclose(struct RexxMsg *rxmsg, STRPTR args)
{
	read_window_close(read_active_window);
}

/****************************************************************
 READINFO ARexx Command
*****************************************************************/
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
		struct mail *mail;

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
					struct mail *t = mail;

					while (t)
					{
						mail_decode(t);
						t = mail_get_next(t);
						count++;
					}

					strcpy(stem_buf,readinfo_arg.stem);
					strcpy(&stem_buf[stem_len],"FILENAME.COUNT");
					sprintf(num_buf,"%d",count);
					SetRexxVar(rxmsg,stem_buf,num_buf,strlen(num_buf));

					strcpy(&stem_buf[stem_len],"FILETYPE.COUNT");
					sprintf(num_buf,"%d",count);
					SetRexxVar(rxmsg,stem_buf,num_buf,strlen(num_buf));

					strcpy(&stem_buf[stem_len],"FILESIZE.COUNT");
					sprintf(num_buf,"%d",count);
					SetRexxVar(rxmsg,stem_buf,num_buf,strlen(num_buf));

					i = 0;

					while (mail)
					{
						char *type_buf;
						int mail_size;

						if (mail->decoded_data) mail_size = mail->decoded_len;
						else mail_size = mail->text_len;

						sprintf(&stem_buf[stem_len],"FILENAME.%d",i);
						sprintf(num_buf,"%d",count);
						SetRexxVar(rxmsg,stem_buf,num_buf,strlen(num_buf));

						sprintf(&stem_buf[stem_len],"FILETYPE.%d",i);
						type_buf = mystrdup(mail->content_type);
						type_buf = stradd(type_buf,"/");
						type_buf = stradd(type_buf,mail->content_subtype);
						if (type_buf)
							SetRexxVar(rxmsg,stem_buf,type_buf,strlen(type_buf));
						free(type_buf);

						sprintf(&stem_buf[stem_len],"FILESIZE.%d",i);
						sprintf(num_buf,"%d",mail_size);
						SetRexxVar(rxmsg,stem_buf,num_buf,strlen(num_buf));

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

/****************************************************************
 READSAVE ARexx Command
*****************************************************************/
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
		struct mail *mail = read_window_get_displayed_mail(read_active_window);
		if (readsave_arg.part)
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

/****************************************************************
 REQUESTFOLDER Arexx Command
*****************************************************************/
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
					SetRexxVar(rxmsg,stem_buf,folder->name,strlen(folder->name));
					free(stem_buf);
				}
			} else
			{
				if (requestfolder_arg.var) SetRexxVar(rxmsg,requestfolder_arg.var,folder->name,strlen(folder->name));
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

/****************************************************************
 MAILADD Arexx Command

 Adds a mail with FILENAME into the folder. If the mail
 (FILENAME) is already in the folder it isn't copied. Otherwise
 it is copied. Returns the filename of the mail.
*****************************************************************/
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
		struct mail *mail;
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
					SetRexxVar(rxmsg,stem_buf,mail->filename,strlen(mail->filename));
					free(stem_buf);
				}
			} else
			{
				if (mailadd_arg.var) SetRexxVar(rxmsg,mailadd_arg.var,mail->filename,strlen(mail->filename));
				else arexx_set_result(rxmsg,mail->filename);
			}
		}

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
		else if (!Stricmp("WRITECLOSE",command.command)) arexx_writeclose(rxmsg,command.args);
		else if (!Stricmp("SETMAIL",command.command)) arexx_setmail(rxmsg,command.args);
		else if (!Stricmp("SETMAILFILE",command.command)) arexx_setmailfile(rxmsg,command.args);
		else if (!Stricmp("SHOW",command.command)) arexx_show(rxmsg,command.args);
		else if (!Stricmp("HIDE",command.command)) arexx_hide(rxmsg,command.args);
		else if (!Stricmp("GETSELECTED",command.command)) arexx_getselected(rxmsg,command.args);
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
		else if (!Stricmp("MAILLISTFREEZE",command.command)) main_freeze_mail_list();
		else if (!Stricmp("MAILLISTTHAW",command.command)) main_thaw_mail_list();

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

