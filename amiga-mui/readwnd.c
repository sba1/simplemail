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
** readwnd.c
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <dos/exall.h>
#include <workbench/icon.h>
#include <libraries/asl.h>
#include <libraries/iffparse.h> /* MAKE_ID */
#include <libraries/mui.h>
#include <libraries/gadtools.h>
#include <mui/nlistview_mcc.h>
#include <mui/nlisttree_mcc.h>
#include "simplehtml_mcc.h"
#include <clib/alib_protos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/openurl.h>
#include <proto/wb.h>
#include <proto/icon.h>

#include "configuration.h"
#include "folder.h"
#include "http.h"
#include "mail.h"
#include "simplemail.h"
#include "smintl.h"
#include "support_indep.h"
#include "text2html.h"

#include "amigasupport.h"
#include "compiler.h"
#include "datatypesclass.h"
#include "iconclass.h"
#include "mainwnd.h" /* remove this */
#include "muistuff.h"
#include "picturebuttonclass.h"
#include "readwnd.h"
#include "support.h"
#include "print.h"

void display_about(void);
static void save_contents(struct Read_Data *data, struct mail *mail);
static void save_contents_to(struct Read_Data *data, struct mail *mail, char *drawer, char *file);
static int read_window_display_mail(struct Read_Data *data, struct mail *mail);

#define MAX_READ_OPEN 10
static struct Read_Data *read_open[MAX_READ_OPEN];

#define PAGE_HTML			0
#define PAGE_DATATYPE	1

struct Read_Data /* should be a customclass */
{
	Object *wnd;
	Object *prev_button;
	Object *next_button;
	Object *print_button;

	Object *contents_page;

	Object *datatype_datatypes;
	Object *html_simplehtml;

	Object *attachments_group;

	Object *attachments_last_selected;
	Object *attachment_standard_menu; /* standard context menu */
	Object *attachment_html_menu; /* for html files */
	int attachment_show_me;

	struct FileRequester *file_req;
	int num; /* the number of the window */
	struct mail *mail; /* the mail which is displayed, a copy of the ref_mail */

	struct mail *ref_mail; /* The reference to the original mail which is in the folder */
	char *folder_path; /* the path of the folder */

	struct MyHook simplehtml_load_hook; /* load hook for the SimpleHTML Object */
	/* more to add */
};

/******************************************************************
 Cleanups temporary files created for the mail. Returns 1 if can
 continue.
*******************************************************************/
static int read_cleanup(struct Read_Data *data)
{
	struct ExAllControl *eac;
	BPTR dirlock;
	char filename[100];
	int rc = 1;
	struct mail *mail = data->mail;

	if (!data->mail) return 1;
	strcpy(filename,"T:");
	strcat(filename,mail->filename);
	dirlock = Lock(filename,ACCESS_READ);
	if (!dirlock) return 1;

	if ((eac = (struct ExAllControl *)AllocDosObject(DOS_EXALLCONTROL,NULL)))
	{
		APTR EAData = AllocVec(1024,0);
		if (EAData)
		{
			int more;
			BPTR olddir = CurrentDir(dirlock);

			eac->eac_LastKey = 0;
			do
			{
				struct ExAllData *ead;
				more = ExAll(dirlock, EAData, 1024, ED_NAME, eac);
				if ((!more) && (IoErr() != ERROR_NO_MORE_ENTRIES)) break;
				if (eac->eac_Entries == 0) continue;

	      ead = (struct ExAllData *)EAData;
	      do
	      {
		DeleteFile(ead->ed_Name); /* TODO: Check the result */
					ead = ead->ed_Next;
				} while (ead);
			} while (more);
			CurrentDir(olddir);
			FreeVec(EAData);
		}
		FreeDosObject(DOS_EXALLCONTROL,eac);
	}
	UnLock(dirlock);
	DeleteFile(filename);
	return rc;
}

/******************************************************************
 Open the contents of an icon (requires version 44 of the os)
*******************************************************************/
static void open_contents(struct Read_Data *data, struct mail *mail)
{
	if (WorkbenchBase->lib_Version >= 44 && IconBase->lib_Version >= 44)
	{
		BPTR fh;
		BPTR newdir;
		BPTR olddir;
		char filename[100];

		/* Write out the file, create an icon, start it via wb.library */
		if (mail->content_name)
		{
			strcpy(filename,"T:");
			strcat(filename,data->mail->filename);
			if ((newdir = CreateDir(filename))) UnLock(newdir);

			if ((newdir = Lock(filename, ACCESS_READ)))
			{
				olddir = CurrentDir(newdir);

				if ((fh = Open(mail->content_name,MODE_NEWFILE)))
				{
					struct DiskObject *dobj;
					void *cont;
					int cont_len;

					mail_decode(mail);
					mail_decoded_data(mail,&cont,&cont_len);

					Write(fh,cont,cont_len);
					Close(fh);

					if ((dobj = GetIconTags(mail->content_name,ICONGETA_FailIfUnavailable,FALSE,TAG_DONE)))
					{
						int ok_to_open = 1;
						if (dobj->do_Type == WBTOOL)
						{
							ok_to_open = sm_request(NULL,_("Are you sure that you want to start this executable?"),_("*_Yes|_Cancel"));
						}

						if (ok_to_open) PutIconTagList(mail->content_name,dobj,NULL);
						FreeDiskObject(dobj);

						if (ok_to_open)
							OpenWorkbenchObjectA(mail->content_name,NULL);
					}
				}

				CurrentDir(olddir);
				UnLock(newdir);
			}
		}
	}
}

/******************************************************************
 inserts the text of the mail into the given nlist object
*******************************************************************/
static void insert_text(struct Read_Data *data, struct mail *mail)
{
	char *buf;
	char *buf_end;

	if (!mail->text) return;

	if (mail->decoded_data)
	{
		if (stricmp(mail->content_type,"text"))
		{
			SetAttrs(data->datatype_datatypes,
						MUIA_DataTypes_Buffer, mail->decoded_data,
						MUIA_DataTypes_BufferLen, mail->decoded_len,
						TAG_DONE);

			set(data->contents_page, MUIA_Group_ActivePage, PAGE_DATATYPE);
			set(data->print_button, MUIA_Disabled, !xget(data->datatype_datatypes,MUIA_DataTypes_SupportsPrint));
			return;
		}
		buf = mail->decoded_data;
		buf_end = buf + mail->decoded_len;
	} else
	{
		buf = mail->text + mail->text_begin;
		buf_end = buf + mail->text_len;
	}

	if (!stricmp(mail->content_subtype, "html"))
	{
		SetAttrs(data->html_simplehtml,
				MUIA_SimpleHTML_Buffer, buf,
				MUIA_SimpleHTML_BufferLen, buf_end - buf,
				TAG_DONE);

		set(data->contents_page, MUIA_Group_ActivePage, PAGE_HTML);
		set(data->print_button, MUIA_Disabled,TRUE);
	} else
	{
		char *html_mail;
		char *font_buf;

		SetAttrs(data->html_simplehtml,
				MUIA_SimpleHTML_Buffer,data->mail->html_header,
				MUIA_SimpleHTML_BufferLen,strstr(data->mail->html_header,"</BODY></HTML>") - data->mail->html_header,
				TAG_DONE);

		if (font_buf = mystrdup(user.config.read_fixedfont))
		{
			char *end = strchr(font_buf,'/');
			if (end)
			{
				int size = atoi(end+1);
				*end = 0;

				DoMethod(data->html_simplehtml,MUIM_SimpleHTML_FontSubst,"fixedmail",3,font_buf,size);
			}
			free(font_buf);
		}

		if (font_buf = mystrdup(user.config.read_propfont))
		{
			char *end = strchr(font_buf,'/');
			if (end)
			{
				int size = atoi(end+1);
				*end = 0;

				DoMethod(data->html_simplehtml,MUIM_SimpleHTML_FontSubst,"normal",2,font_buf,size);
				DoMethod(data->html_simplehtml,MUIM_SimpleHTML_FontSubst,"normal",3,font_buf,size);
				DoMethod(data->html_simplehtml,MUIM_SimpleHTML_FontSubst,"normal",4,font_buf,size);
			}
			free(font_buf);
		}


		html_mail = text2html(buf, buf_end - buf,
													TEXT2HTML_ENDBODY_TAG|TEXT2HTML_FIXED_FONT|(user.config.read_wordwrap?0:TEXT2HTML_NOWRAP),"<FONT FACE=\"fixedmail\" SIZE=\"+1\">");

		DoMethod(data->html_simplehtml, MUIM_SimpleHTML_AppendBuffer, html_mail, strlen(html_mail));
		set(data->wnd, MUIA_Window_DefaultObject, data->html_simplehtml);

		set(data->contents_page, MUIA_Group_ActivePage, PAGE_HTML);
		set(data->print_button, MUIA_Disabled, FALSE);
	}
}

/******************************************************************
 An Icon has selected
*******************************************************************/
static void icon_selected(int **pdata)
{
	struct Read_Data *data = (struct Read_Data*)(pdata[0]);
	Object *icon = (Object*)(pdata[1]);
	struct mail *mail;

	if (icon == data->attachments_last_selected) return;
	if (data->attachments_last_selected)
		set(data->attachments_last_selected, MUIA_Selected, 0);

	if ((mail = (struct mail*)xget(icon,MUIA_UserData)))
	{
		mail_decode(mail);
		insert_text(data,mail);
	}
	data->attachments_last_selected = icon;
}

/******************************************************************
 Open the icon
*******************************************************************/
static void icon_open(int **pdata)
{
	struct Read_Data *data = (struct Read_Data*)(pdata[0]);
	struct mail *mail = (struct mail *)(pdata[1]);

	open_contents(data,mail);
}

/******************************************************************
 The icon has been dropped on a wb drawer
*******************************************************************/
static void icon_drop(int **pdata)
{
	struct Read_Data *data = (struct Read_Data*)(pdata[0]);
	struct mail *mail = (struct mail *)(pdata[1]);
	Object *icon = (Object*)(pdata[2]);
	char *path = (char*)xget(icon,MUIA_Icon_DropPath);

	save_contents_to(data, mail, path, mail->content_name);
}

/******************************************************************
 A context menu item has been selected
*******************************************************************/
static void context_menu_trigger(int **pdata)
{
	struct Read_Data *data = (struct Read_Data*)(pdata[0]);
	struct mail *mail = (struct mail *)(pdata[1]);
	Object *item = (Object*)(pdata[2]);

	if (item && mail)
	{
		switch (xget(item,MUIA_UserData))
		{
			case	1: /* save attachment */
						save_contents(data,mail);
						break;

			case	2: /* save whole document */
						break;

			case	3: /* Open via workbench.library */
						open_contents(data,mail);
						break;
			case	4: /* print as text */
				print_mail(mail, FALSE);
				break;
		}
	}
}

/******************************************************************
 inserts the mime informations (uses ugly recursion)
*******************************************************************/
static void insert_mail(struct Read_Data *data, struct mail *mail)
{
	int i;

	if (!data->attachments_group) return;

	if (mail->num_multiparts == 0)
	{
		Object *group, *icon, *context_menu;
		void *buffer = NULL;
		unsigned int buffer_len = 488;

		buffer = mail_decode_bytes(mail,&buffer_len);

		context_menu = data->attachment_standard_menu;
		if (!mystricmp(mail->content_subtype,"html"))
			context_menu = data->attachment_html_menu;

		group = VGroup,
			Child, icon = IconObject,
					MUIA_Draggable, TRUE,
					MUIA_Icon_MimeType, mail->content_type,
					MUIA_Icon_MimeSubType, mail->content_subtype,
					MUIA_Icon_Buffer, buffer,
					MUIA_Icon_BufferLen, buffer_len,
					MUIA_UserData, mail,
					MUIA_ContextMenu, context_menu,
					End,
			Child, TextObject,
					MUIA_Background, MUII_TextBack,
					MUIA_Font, MUIV_Font_Tiny,
					MUIA_Text_Contents, mail->content_name,
					MUIA_Text_PreParse, "\33c",
					End,
			End;

		if (icon)
		{
			DoMethod(data->attachments_group, OM_ADDMEMBER, group);
			DoMethod(icon, MUIM_Notify, MUIA_ContextMenuTrigger, MUIV_EveryTime, App, 6, MUIM_CallHook, &hook_standard, context_menu_trigger, data, mail, MUIV_TriggerValue);
			DoMethod(icon, MUIM_Notify, MUIA_Icon_DoubleClick, TRUE, App, 5, MUIM_CallHook, &hook_standard, icon_open, data, mail);
			DoMethod(icon, MUIM_Notify, MUIA_Selected, TRUE, App, 5, MUIM_CallHook, &hook_standard, icon_selected, data, icon);
			DoMethod(icon, MUIM_Notify, MUIA_Icon_DropPath, MUIV_EveryTime, App, 6, MUIM_CallHook, &hook_standard, icon_drop, data, mail, icon);
		}
	}

	for (i=0;i<mail->num_multiparts;i++)
	{
		insert_mail(data,mail->multipart_array[i]);
	}
}

/******************************************************************
 Save the contents of a given mail
*******************************************************************/
static void save_contents(struct Read_Data *data, struct mail *mail)
{
	if (!mail) return;
	if (!mail->num_multiparts)
	{
		if (MUI_AslRequestTags(data->file_req,
					mail->content_name?ASLFR_InitialFile:TAG_IGNORE,mail->content_name,
					TAG_DONE))
		{
			save_contents_to(data,mail,data->file_req->fr_Drawer,data->file_req->fr_File);
		}
	}
}

/******************************************************************
 Save the contents of a given mail to a given dest
*******************************************************************/
static void save_contents_to(struct Read_Data *data, struct mail *mail, char *drawer, char *file)
{
	BPTR dlock;
	mail_decode(mail);

	if (!file) file = "Unnamed";

	if ((dlock = Lock(drawer,ACCESS_READ)))
	{
		BPTR olock;
		BPTR flock;
		BPTR fh;
		int goon = 1;

		olock = CurrentDir(dlock);

		if ((flock = Lock(file,ACCESS_READ)))
		{
			UnLock(flock);
			goon = sm_request(NULL,_("File %s already exists in %s.\nDo you really want to replace it?"),("_Yes|_No"),file,drawer);
		}

		if (goon)
		{
			if (!mystricmp(mail->content_type,"text") && mystricmp(mail->content_subtype, "html"))
			{
				void *cont; /* mails content */
				int cont_len;

				char *charset;
				char *user_charset;

				if (!(charset = mail->content_charset)) charset = "ISO-8859-1";
				user_charset = user.config.default_codeset?user.config.default_codeset->name:"ISO-8858-1";

				/* Get the contents */
				mail_decoded_data(mail,&cont,&cont_len);
					
				if (mystricmp(user_charset,charset))
				{
					/* The character sets differ so now we create the two strings to see if they differ */
					char *str; /* That's the string encoded in the mails charset */
					char *user_str; /* That's the string encoded in the users charset */

					/* encode now */
					str = utf8tostrcreate((utf8 *)cont, codesets_find(charset));
					user_str = utf8tostrcreate((utf8 *)cont, codesets_find(user_charset));

					if (str && user_str)
					{
						char *towrite;

						/* Let's see if they are different */
						if (strcmp(str,user_str))
						{
							/* Yes, so inform the user */
							char gadgets[320];
							int selection;

							sprintf(gadgets,_("_Orginal (%s)|_Converted (%s)| _UTF8|_Cancel"),charset,user_charset);
							selection = sm_request(NULL,_("The orginal charset of the attached file differs from yours.\nIn which charset do you want the file being saved?"),gadgets);

							switch (selection)
							{
								case 1: towrite = str; break;
								case 2: towrite = user_str; break;
								case 3: towrite = NULL; goon = 1; break; /* will be writeout as utf8 */
								default: towrite = NULL; goon = 0; break; /* cancel */
							}
						} else towrite = user_str;

						if (towrite)
						{
							/* Now write out the stuff */
							if ((fh = Open(file, MODE_NEWFILE)))
							{
								char *comment = mail_get_from_address(mail_get_root(mail));
								Write(fh,towrite,strlen(towrite));
								Close(fh);

								if (comment)
								{
									SetComment(file,comment);
									free(comment);
								}
								goon = 0;
							}
						}
					}
				}
			}
		}
		
    if (goon)
    {
			if ((fh = Open(file, MODE_NEWFILE)))
			{
				char *comment = mail_get_from_address(mail_get_root(mail));
				void *cont;
				int cont_len;

				mail_decoded_data(mail,&cont,&cont_len);

				Write(fh,cont,cont_len);
				Close(fh);

				if (comment)
				{
					SetComment(file,comment);
					free(comment);
				}
			}
		}

		CurrentDir(olock);
		UnLock(dlock);
	}
}

/******************************************************************
 Show the raw text with multiview
*******************************************************************/
static void show_raw(int **pdata)
{
	struct Read_Data *data = (struct Read_Data*)(pdata[0]);
	BPTR lock;
	int len;
	char *buf;
	char *dirname;

	if (!(lock = Lock(data->folder_path,ACCESS_READ)))
		return;

	dirname = NameOfLock(lock);
	UnLock(lock);
	if (!dirname) return;

	len = strlen(dirname)+strlen(data->ref_mail->filename) + 6;
	if (!(buf = malloc(len+40)))
	{
		FreeVec(dirname);
		return;
	}

	strcpy(buf,"SYS:Utilities/Multiview \"");
	strcat(buf,dirname);
	AddPart(buf+27,data->ref_mail->filename,len);
	strcat(buf+27,"\"");
	sm_system(buf,NULL);
	free(buf);
	FreeVec(dirname);
}

/******************************************************************
 Returns the currently displayed mail
*******************************************************************/
struct mail *read_get_displayed_mail(struct Read_Data *data)
{
	struct mail *mail;
	if (data->attachments_last_selected)
	{
		mail = (struct mail*)xget(data->attachments_last_selected,MUIA_UserData);
	} else {
		if (!data->mail->num_multiparts) mail = data->mail;
		else mail = NULL;
	}
	return mail;
}

/******************************************************************
 Print the visible mail
*******************************************************************/
static void menu_print(int **pdata)
{
	struct Read_Data *data = (struct Read_Data*)(pdata[0]);
	struct mail *mail = read_get_displayed_mail(data);

	if (mail)
	{
		if (xget(data->contents_page, MUIA_Group_ActivePage) ==  PAGE_DATATYPE)
		{
			DoMethod(data->datatype_datatypes, MUIM_DataTypes_Print);
		} else
		{
			set(App, MUIA_Application_Sleep, TRUE);
			print_mail(mail, TRUE);
			set(App, MUIA_Application_Sleep, FALSE);
		}
	}
}

/******************************************************************
 Shows a given mail (part)
*******************************************************************/
static void show_mail(struct Read_Data *data, struct mail *m)
{
	if (!m) return;

	if (!data->attachments_group || !data->mail->num_multiparts)
	{
		mail_decode(m);
		insert_text(data, m);
	} else
	{
		DoMethod(data->attachments_group, MUIM_SetUDataOnce, m, MUIA_Selected, 1);
	}
}

/******************************************************************
 This close and disposed the window (note: this must not be called
 within a normal callback hook (because the object is disposed in
 this function)!
*******************************************************************/
static void read_window_dispose(struct Read_Data **pdata)
{
	struct Read_Data *data = *pdata;
	read_cleanup(data);
	set(data->wnd,MUIA_Window_Open,FALSE);
	DoMethod(App,OM_REMMEMBER,data->wnd);
	MUI_DisposeObject(data->wnd);

	if (data->attachment_html_menu) MUI_DisposeObject(data->attachment_html_menu);
	if (data->attachment_standard_menu) MUI_DisposeObject(data->attachment_standard_menu);

	if (data->file_req) MUI_FreeAslRequest(data->file_req);
	if (data->folder_path) free(data->folder_path);
	mail_free(data->mail);
	if (data->num < MAX_READ_OPEN) read_open[data->num] = NULL;
	free(data);
}

/******************************************************************
 The save button has been clicked
*******************************************************************/
static void save_button_pressed(struct Read_Data **pdata)
{
	struct mail *mail;
	struct Read_Data *data = *pdata;

	if (!(mail = read_get_displayed_mail(data))) return;

	save_contents(data,mail);
}

/******************************************************************
 The prev button has been pressed. This should be made somehow
 gui independend later
*******************************************************************/
static void prev_button_pressed(struct Read_Data **pdata)
{
	struct Read_Data *data = *pdata;
	struct mail *prev = folder_find_prev_mail_by_filename(data->folder_path, data->ref_mail->filename);
	if (prev)
	{
		read_cleanup(data);
		if (data->mail) mail_free(data->mail);
		data->mail = NULL;
		read_window_display_mail(data, prev);

		/* Update flags if needed */
		if (mail_get_status_type(prev) == MAIL_STATUS_UNREAD)
		{
			struct folder *f = folder_find_by_path(data->folder_path);
			if (f)
			{
				folder_set_mail_status(f,prev,MAIL_STATUS_READ | (prev->status & (~MAIL_STATUS_MASK)));
				if (prev->flags & MAIL_FLAGS_NEW && f->new_mails) f->new_mails--;
				prev->flags &= ~MAIL_FLAGS_NEW;
				main_refresh_folder(f);
			}
		}

		main_set_active_mail(prev);
	}
}

/******************************************************************
 The next button has been pressed This should be made somehow
 gui independend later
*******************************************************************/
static void next_button_pressed(struct Read_Data **pdata)
{
	struct Read_Data *data = *pdata;
	struct mail *next = folder_find_next_mail_by_filename(data->folder_path, data->ref_mail->filename);
	if (next)
	{
		read_cleanup(data);
		if (data->mail) mail_free(data->mail);
		data->mail = NULL;
		read_window_display_mail(data, next);

		/* Update flags if needed */
		if (mail_get_status_type(next) == MAIL_STATUS_UNREAD)
		{
			struct folder *f = folder_find_by_path(data->folder_path);
			if (f)
			{
				folder_set_mail_status(f,next,MAIL_STATUS_READ | (next->status & (~MAIL_STATUS_MASK)));
				if (next->flags & MAIL_FLAGS_NEW && f->new_mails) f->new_mails--;
				next->flags &= ~MAIL_FLAGS_NEW;
				main_refresh_folder(f);
			}
		}

	 /* will also refresh the mail, in case of updated flags */
		main_set_active_mail(next);
	}
}

/******************************************************************
 The delete button has been pressed
*******************************************************************/
static void delete_button_pressed(struct Read_Data **pdata)
{
	struct Read_Data *data = *pdata;
	struct mail *next = folder_find_next_mail_by_filename(data->folder_path, data->ref_mail->filename);
	if (!next) next = folder_find_prev_mail_by_filename(data->folder_path, data->ref_mail->filename);

	if (callback_delete_mail(data->ref_mail))
	{
		if (!next)
		{
			set(data->wnd, MUIA_Window_Open, FALSE);
			DoMethod(App, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, read_window_dispose, data);
		}
		read_cleanup(data);
		if (data->mail) mail_free(data->mail);
		data->mail = NULL;

		if (next) read_window_display_mail(data, next);
	}
}

/******************************************************************
 The reply button has been pressed
*******************************************************************/
static void reply_button_pressed(struct Read_Data **pdata)
{
	struct Read_Data *data = *pdata;
	if (!data->ref_mail) return;

	callback_reply_this_mail(data->folder_path, 1, &data->ref_mail);
}

/******************************************************************
 The forward button has been pressed
*******************************************************************/
static void forward_button_pressed(struct Read_Data **pdata)
{
	struct Read_Data *data = *pdata;
	if (!data->ref_mail) return;

	callback_forward_this_mail(data->folder_path, 1, &data->ref_mail);
}

/******************************************************************
 A an uri has been clicked
*******************************************************************/
static void uri_clicked(void **msg)
{
	struct Read_Data *data = (struct Read_Data*)msg[0];
	char *uri = (char*)msg[1];

	if (!mystrnicmp(uri,"mailto:",7))
	{
		callback_write_mail_to_str(uri+7,NULL);
	} else
	{
		struct Library *OpenURLBase;

		if ((OpenURLBase = OpenLibrary("openurl.library",0)))
		{
			URL_OpenA(uri,NULL);
			CloseLibrary(OpenURLBase);
		}
	}
}

/******************************************************************
 SimpleHTML Load Hook. Returns 1 if uri can be loaded by the hook
 otherwise 0. -1 means reject this object totaly
*******************************************************************/
__asm int simplehtml_load_func(register __a0 struct Hook *h, register __a1 struct MUIP_SimpleHTML_LoadHook *msg)
{
	struct Read_Data *data = (struct Read_Data*)h->h_Data;
	char *uri = msg->uri;
	struct mail *mail;

	if (!(mail = read_get_displayed_mail(data))) return -1;

	if (!mystrnicmp("http://",uri,7) && mail_allowed_to_download(data->mail))
	{
		void *buf;
		int buf_len;
		int rc = 0;

		if (http_download(uri,&buf,&buf_len))
		{
			if ((msg->buffer = (void*)DoMethod(data->html_simplehtml, MUIM_SimpleHTML_AllocateMem, buf_len)))
			{
				msg->buffer_len = buf_len;
				CopyMem(buf,msg->buffer,buf_len);
				rc = 1;
			}

			free(buf);
		}
		return rc;
	}

	if (!(mail = mail_find_compound_object(mail,uri))) return -1;
	mail_decode(mail);
	if (!mail->decoded_data || !mail->decoded_len) return -1;
	msg->buffer = (void*)DoMethod(data->html_simplehtml, MUIM_SimpleHTML_AllocateMem, mail->decoded_len);
	if (!msg->buffer) return 0;
	CopyMem(mail->decoded_data,msg->buffer,mail->decoded_len);
	msg->buffer_len = mail->decoded_len;
	return 1;
}


/******************************************************************
 Display the mail
*******************************************************************/
static int read_window_display_mail(struct Read_Data *data, struct mail *mail)
{
	BPTR lock;

	if (!data->folder_path) return 0;

	set(App, MUIA_Application_Sleep, TRUE);
	data->ref_mail = mail;

	if ((lock = Lock(data->folder_path,ACCESS_READ))) /* maybe it's better to use an absoulte path here */
	{
		BPTR old_dir = CurrentDir(lock);

		if ((data->mail = mail_create_from_file(mail->filename)))
		{
			int dont_show = 0;
			mail_read_contents(data->folder_path,data->mail);
			mail_create_html_header(data->mail);

			if (!data->mail->num_multiparts || (data->mail->num_multiparts == 1 && !data->mail->multipart_array[0]->num_multiparts))
			{
				/* mail has only one part */
				set(data->attachments_group, MUIA_ShowMe, FALSE);
				dont_show = 1;
			} else
			{
				DoMethod((Object*)xget(data->attachments_group,MUIA_Parent), MUIM_Group_InitChange);
			}

			DoMethod(data->attachments_group, MUIM_Group_InitChange);
			DisposeAllChilds(data->attachments_group);
			data->attachments_last_selected = NULL;
			insert_mail(data,data->mail);
			DoMethod(data->attachments_group, OM_ADDMEMBER, HSpace(0));
			DoMethod(data->attachments_group, MUIM_Group_ExitChange);

			if (!dont_show)
			{
				set(data->attachments_group, MUIA_ShowMe, TRUE);
				DoMethod((Object*)xget(data->attachments_group,MUIA_Parent), MUIM_Group_ExitChange);
			}

			show_mail(data,mail_find_initial(data->mail));

			CurrentDir(old_dir);
			set(App, MUIA_Application_Sleep, FALSE);
			return 1;
		}
		CurrentDir(old_dir);
	}

	DoMethod(data->attachments_group, MUIM_Group_InitChange);
	DisposeAllChilds(data->attachments_group);
	data->attachments_last_selected = NULL;
	DoMethod(data->attachments_group, OM_ADDMEMBER, HSpace(0));
	DoMethod(data->attachments_group, MUIM_Group_ExitChange);
	set(App, MUIA_Application_Sleep, FALSE);
	return 0;
}

/******************************************************************
 Opens a read window. Returns the number of the readwindow or -1
 for an error
*******************************************************************/
int read_window_open(char *folder, struct mail *mail)
{
	Object *wnd, *html_simplehtml, *html_vert_scrollbar, *html_horiz_scrollbar, *contents_page;
	Object *datatype_vert_scrollbar, *datatype_horiz_scrollbar;
	Object *attachments_group;
	Object *datatype_datatypes;
	Object *prev_button, *next_button, *print_button, *save_button, *delete_button, *reply_button, *forward_button;
	Object *space;
	Object *read_menu;
	int num;

	enum {
		MENU_PROJECT,
		MENU_PROJECT_ABOUT = 1,
		MENU_PROJECT_ABOUTMUI,
		MENU_PROJECT_QUIT,
		MENU_MAIL_RAW,
		MENU_MAIL_PRINT
	};

	static const struct NewMenu nm_untranslated[] =
	{
		{NM_TITLE, N_("Project"), NULL, 0, 0, NULL},
		{NM_ITEM, N_("?:About..."), NULL, 0, 0, (APTR)MENU_PROJECT_ABOUT},
		{NM_ITEM, N_("About MUI..."), NULL, 0, 0, (APTR)MENU_PROJECT_ABOUTMUI},
		{NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
		{NM_ITEM, N_("Q:Quit"), NULL, 0, 0, (APTR)MENU_PROJECT_QUIT},
		{NM_TITLE, N_("Mail"), NULL, 0, 0, NULL},
		{NM_ITEM, N_("Show raw format..."), NULL, 0, 0, (APTR)MENU_MAIL_RAW},
		{NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
		{NM_ITEM, N_("Print visible mailpart"), NULL, 0, 0, (APTR)MENU_MAIL_PRINT},
		{NM_END, NULL, NULL, 0, 0, NULL}
	};

	struct NewMenu *nm;
	int i;

	for (num=0; num < MAX_READ_OPEN; num++)
		if (!read_open[num]) break;

	if (num == MAX_READ_OPEN) return -1;

	/* translate the menu entries */
	if (!(nm = malloc(sizeof(nm_untranslated)))) return -1;
	memcpy(nm,nm_untranslated,sizeof(nm_untranslated));

	for (i=0;i<ARRAY_LEN(nm_untranslated)-1;i++)
	{
		if (nm[i].nm_Label && nm[i].nm_Label != NM_BARLABEL)
		{
			nm[i].nm_Label = mystrdup(_(nm[i].nm_Label));
			if (nm[i].nm_Label[1] == ':') nm[i].nm_Label[1] = 0;
		}
	}

	read_menu = MUI_MakeObject(MUIO_MenustripNM, nm, MUIO_MenustripNM_CommandKeyCheck);

	wnd = WindowObject,
		MUIA_HelpNode, "RE_W",
		(num < MAX_READ_OPEN)?MUIA_Window_ID:TAG_IGNORE, MAKE_ID('R','E','A',num),
		MUIA_Window_Title, _("SimpleMail - Read Message"),
		MUIA_Window_Menustrip, read_menu,

		WindowContents, VGroup,
			Child, HGroupV,
				Child, HGroup,
					MUIA_VertWeight, 0,
					Child, HGroup,
						MUIA_Group_Spacing, 0,
						MUIA_Weight, 100,
						Child, prev_button = MakePictureButton(_("_Prev"),"PROGDIR:Images/MailPrev"),
						Child, next_button = MakePictureButton(_("_Next"),"PROGDIR:Images/MailNext"),
						End,
					Child, HGroup,
						MUIA_Group_Spacing, 0,
						MUIA_Weight, 100,
/*						Child, MakePictureButton("Show","PROGDIR:Images/MailShow"),*/
						Child, save_button = MakePictureButton(_("_Save"),"PROGDIR:Images/MailSave"),
						Child, print_button = MakePictureButton(_("Pr_int"),"PROGDIR:Images/Print"),
						End,
					Child, HGroup,
						MUIA_Group_Spacing, 0,
						MUIA_Weight, 150,
						Child, delete_button = MakePictureButton(_("_Delete"),"PROGDIR:Images/MailDelete"),
/*						Child, MakePictureButton("_Move","PROGDIR:Images/MailMove"),*/
						Child, reply_button = MakePictureButton(_("_Reply"),"PROGDIR:Images/MailReply"),
						Child, forward_button = MakePictureButton(_("_Forward"),"PROGDIR:Images/MailForward"),
						End,
					Child, HVSpace,
					End,
				End,
			Child, contents_page = PageGroup,
				MUIA_Group_ActivePage, PAGE_HTML,

				Child, VGroup,
					MUIA_Group_Spacing, 0,
					Child, HGroup,
						MUIA_Group_Spacing, 0,
						Child, html_simplehtml = SimpleHTMLObject,TextFrame,End,
						Child, html_vert_scrollbar = ScrollbarObject,End,
						End,
					Child, html_horiz_scrollbar = ScrollbarObject, MUIA_Group_Horiz, TRUE, End,
					End,
				Child, VGroup,
					MUIA_Group_Spacing, 0,
					Child, HGroup,
						MUIA_Group_Spacing, 0,
						Child, datatype_datatypes = DataTypesObject, TextFrame, End,
						Child, datatype_vert_scrollbar = ScrollbarObject,End,
						End,
					Child, datatype_horiz_scrollbar = ScrollbarObject, MUIA_Group_Horiz, TRUE, End,
					End,
				End,
			Child, HGroup,
				MUIA_Group_Spacing, 0,
				Child, attachments_group = HGroupV,
					End,
				Child, space = HSpace(0),
				End,
			End,
		End;

	if (wnd)
	{
		struct Read_Data *data = (struct Read_Data*)malloc(sizeof(struct Read_Data));
		if (data)
		{
			Object *open_contents_item;
			Object *save_contents_item;
			Object *save_contents2_item;
			Object *save_document_item;
			Object *printtxt_contents_item;


			data->attachment_standard_menu = MenustripObject,
				Child, MenuObjectT(_("Attachment")),
					Child, open_contents_item = MenuitemObject,
						MUIA_Menuitem_Title, _("Open"),
						MUIA_UserData, 3,
						End,
					Child, save_contents_item = MenuitemObject,
						MUIA_Menuitem_Title, _("Save As..."),
						MUIA_UserData, 1,
						End,
					Child, MenuitemObject,
						MUIA_Menuitem_Title, _("Print as text"),
						MUIA_UserData, 4,
						End,
					End,
				End;

			data->attachment_html_menu = MenustripObject,
				Child, MenuObjectT(_("Attachment")),
					Child, save_contents2_item = MenuitemObject,
						MUIA_Menuitem_Title, _("Save as..."),
						MUIA_UserData, 1,
						End,
					Child, save_document_item = MenuitemObject,
						MUIA_Menuitem_Title, _("Save whole document as..."),
						MUIA_UserData, 2,
						End,
					End,
				End;

			set(space, MUIA_HorizWeight, 1);

			data->wnd = wnd;
			data->folder_path = mystrdup(folder);
			data->prev_button = prev_button;
			data->next_button = next_button;
			data->print_button = print_button;
			data->contents_page = contents_page;
			data->datatype_datatypes = datatype_datatypes;
			data->html_simplehtml = html_simplehtml;
			data->file_req = MUI_AllocAslRequestTags(ASL_FileRequest, ASLFR_DoSaveMode, TRUE, TAG_DONE);
			data->attachments_group = attachments_group;
			data->num = num;
			read_open[num] = data;

			init_myhook(&data->simplehtml_load_hook, (HOOKFUNC)simplehtml_load_func, data);

			SetAttrs(data->html_simplehtml,
					MUIA_SimpleHTML_HorizScrollbar, html_horiz_scrollbar,
					MUIA_SimpleHTML_VertScrollbar, html_vert_scrollbar,
					MUIA_SimpleHTML_LoadHook, &data->simplehtml_load_hook,
					TAG_DONE);

			SetAttrs(data->datatype_datatypes,
					MUIA_DataTypes_HorizScrollbar, datatype_horiz_scrollbar,
					MUIA_DataTypes_VertScrollbar, datatype_vert_scrollbar,
					TAG_DONE);

			DoMethod(data->html_simplehtml, MUIM_Notify, MUIA_SimpleHTML_URIClicked, MUIV_EveryTime, App, 5, MUIM_CallHook, &hook_standard, uri_clicked, data, MUIV_TriggerValue);

			DoMethod(prev_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, prev_button_pressed, data);
			DoMethod(next_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, next_button_pressed, data);
			DoMethod(save_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, save_button_pressed, data);
			DoMethod(print_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, menu_print, data);
			DoMethod(delete_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, delete_button_pressed, data);
			DoMethod(reply_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, reply_button_pressed, data);
			DoMethod(forward_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, forward_button_pressed, data);
			DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, read_window_dispose, data);

			/* Menu notifies */
			DoMethod(wnd, MUIM_Notify, MUIA_Window_MenuAction, MENU_PROJECT_ABOUT, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, display_about);
			DoMethod(wnd, MUIM_Notify, MUIA_Window_MenuAction, MENU_PROJECT_ABOUTMUI, App, 2, MUIM_Application_AboutMUI, 0);
			DoMethod(wnd, MUIM_Notify, MUIA_Window_MenuAction, MENU_PROJECT_QUIT, App, 2, MUIM_Application_ReturnID,  MUIV_Application_ReturnID_Quit);
			DoMethod(wnd, MUIM_Notify, MUIA_Window_MenuAction, MENU_MAIL_RAW, App, 4, MUIM_CallHook, &hook_standard, show_raw, data);
			DoMethod(wnd, MUIM_Notify, MUIA_Window_MenuAction, MENU_MAIL_PRINT, App, 4, MUIM_CallHook, &hook_standard, menu_print, data);

			set(App, MUIA_Application_Sleep, TRUE);

			if (read_window_display_mail(data,mail))
			{
				DoMethod(App,OM_ADDMEMBER,wnd);
				set(wnd,MUIA_Window_Open,TRUE);
				set(App, MUIA_Application_Sleep, FALSE);
				return num;
			}

			free(data);
			set(App, MUIA_Application_Sleep, FALSE);
		}
		MUI_DisposeObject(wnd);
	}
	return -1;
}

/******************************************************************
 Activate a read window
*******************************************************************/
void read_window_activate(int num)
{
	if (num < 0 || num >= MAX_READ_OPEN) return;
	if (read_open[num] && read_open[num]->wnd) set(read_open[num]->wnd,MUIA_Window_Open,TRUE);
}

/******************************************************************
 Closes a read window
*******************************************************************/
void read_window_close(int num)
{
	if (num < 0 || num >= MAX_READ_OPEN) return;
	if (read_open[num] && read_open[num]->wnd)
		read_window_dispose(&read_open[num]);
}

/******************************************************************
 Returns the displayed mail of the given window
*******************************************************************/
struct mail *read_window_get_displayed_mail(int num)
{
	if (num < 0 || num >= MAX_READ_OPEN) return NULL;
	if (read_open[num])
		return read_get_displayed_mail(read_open[num]);

	return NULL;
}
