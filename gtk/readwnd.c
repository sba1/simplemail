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

#include <sys/stat.h>
#include <unistd.h>

#include <gtk/gtk.h>


#include "configuration.h"
#include "lists.h"
#include "mail.h"
#include "simplemail.h"
#include "support.h"
#include "support_indep.h"

#include "readwnd.h"

#define MAX_READ_OPEN 10
static struct Read_Data *read_open[MAX_READ_OPEN];

struct Read_Data
{
	GtkWidget *wnd;
	GtkWidget *text_view;
	GtkWidget *text_scrolled_window;

	int num; /* the number of the window */
	struct mail *mail; /* the mail which is displayed, a copy of the ref_mail */

	struct mail *ref_mail; /* The reference to the original mail which is in the folder */
	char *folder_path; /* the path of the folder */


	GSList *mail_list; /* linear list of all mails */
};


#if 0

#include "compiler.h"
#include "datatypesclass.h"
#include "iconclass.h"
#include "muistuff.h"
#include "readlistclass.h"
#include "readwnd.h"

static struct Hook header_display_hook;

static void save_contents(struct Read_Data *data, struct mail *mail);

#define MAX_READ_OPEN 10
static int read_open[MAX_READ_OPEN];

#define PAGE_TEXT			0
#define PAGE_HTML			1
#define PAGE_DATATYPE	2

struct Read_Data /* should be a customclass */
{
	Object *wnd;
	Object *contents_page;
	Object *datatype_datatypes;
	Object *text_list;
	Object *html_simplehtml;
	Object *attachments_group;

	Object *attachments_last_selected;
	Object *attachment_standard_menu; /* standard context menu */
	Object *attachment_html_menu; /* for html files */

	struct FileRequester *file_req;
	int num; /* the number of the window */
	struct mail *mail; /* the mail which is displayed */

	struct MyHook simplehtml_load_hook; /* load hook for the SimpleHTML Object */
	/* more to add */
};

STATIC ASM VOID header_display(register __a2 char **array, register __a1 struct header *header)
{
	if (header)
	{
		*array++ = header->name;
		*array = header->contents;
	}
}

/******************************************************************
 inserts the headers of the mail into the given nlist object
*******************************************************************/
static void insert_headers(Object *header_list, struct mail *mail)
{
	struct header *header = (struct header*)list_first(&mail->header_list);
	while (header)
	{
		DoMethod(header_list,MUIM_NList_InsertSingle, header, MUIV_NList_Insert_Bottom);
		header = (struct header*)node_next(&header->node);
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
	} else
	{
		set(data->text_list, MUIA_NList_Quiet, TRUE);
		DoMethod(data->text_list,MUIM_NList_Clear);

		while (buf < buf_end)
		{
			if (user.config.read_wordwrap)  DoMethod(data->text_list,MUIM_NList_InsertSingleWrap, buf, MUIV_NList_Insert_Bottom,WRAPCOL0,ALIGN_LEFT);
			else DoMethod(data->text_list,MUIM_NList_InsertSingle, buf, MUIV_NList_Insert_Bottom);

			if ((buf = strchr(buf,10))) buf++;
			else break;
		}
		set(data->text_list, MUIA_NList_Quiet, FALSE);
		set(data->contents_page, MUIA_Group_ActivePage, PAGE_TEXT);
	}
}

/******************************************************************
 An Icon has selected
*******************************************************************/
void icon_selected(int **pdata)
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
 A context menu item has been selected
*******************************************************************/
void context_menu_trigger(int **pdata)
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
		}
	}
}
#endif

/******************************************************************
 inserts the mime informations (uses ugly recursion)
*******************************************************************/
static void insert_mail(struct Read_Data *data, struct mail *mail)
{
	int i;

	if (mail->num_multiparts == 0)
	{
		g_slist_append(data->mail_list,mail);
#if 0
		Object *group, *icon, *context_menu;

		context_menu = data->attachment_standard_menu;
		if (!mystricmp(mail->content_subtype,"html"))
			context_menu = data->attachment_html_menu;

		group = VGroup,
			Child, icon = IconObject,
					MUIA_InputMode, MUIV_InputMode_Immediate,
					MUIA_Icon_MimeType, mail->content_type,
					MUIA_Icon_MimeSubType, mail->content_subtype,
					MUIA_UserData, mail,
					MUIA_ContextMenu, context_menu,
					End,
			Child, TextObject,
					MUIA_Font, MUIV_Font_Tiny,
					MUIA_Text_Contents, mail->filename,
					MUIA_Text_PreParse, "\33c",
					End,
			End;

		if (icon)
		{
			DoMethod(data->attachments_group, OM_ADDMEMBER, group);
			DoMethod(icon, MUIM_Notify, MUIA_ContextMenuTrigger, MUIV_EveryTime, App, 6, MUIM_CallHook, &hook_standard, context_menu_trigger, data, mail, MUIV_TriggerValue);
		}


		DoMethod(icon, MUIM_Notify, MUIA_Selected, TRUE, App, 5, MUIM_CallHook, &hook_standard, icon_selected, data, icon);
#endif
	}

	for (i=0;i<mail->num_multiparts;i++)
	{
		insert_mail(data,mail->multipart_array[i]);
	}
}

#if 0

/******************************************************************
 Save the contents of a given mail
*******************************************************************/
static void save_contents(struct Read_Data *data, struct mail *mail)
{
	if (!mail->num_multiparts)
	{
		if (MUI_AslRequestTags(data->file_req,
					mail->filename?ASLFR_InitialFile:TAG_IGNORE,mail->filename,
					TAG_DONE))
		{
			BPTR dlock;
			STRPTR drawer = data->file_req->fr_Drawer;

			mail_decode(mail);

			if ((dlock = Lock(drawer,ACCESS_READ)))
			{
				BPTR olock;
				BPTR fh;

				olock = CurrentDir(dlock);

				if ((fh = Open(data->file_req->fr_File, MODE_NEWFILE)))
				{
					Write(fh,mail->decoded_data,mail->decoded_len);
					Close(fh);
				}

				CurrentDir(olock);
				UnLock(dlock);
			}
		}
	}
}
#endif

/******************************************************************
 Shows a given mail (part)
*******************************************************************/
static void show_mail(struct Read_Data *data, struct mail *m)
{
	char *buf;
	int buf_len;
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	mail_decode(m);
	mail_decoded_data(m,(void**)&buf,&buf_len);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->text_view));

	gtk_text_buffer_get_iter_at_line(buffer,&iter,0);

	gtk_text_buffer_insert(buffer,&iter,buf,buf_len);

#if 0
	if (!data->attachments_group)
	{
		mail_decode(m);
		insert_text(data, m);
	} else
	{
		DoMethod(data->attachments_group, MUIM_SetUDataOnce, m, MUIA_Selected, 1);
	}
#endif
}

#if 0
/******************************************************************
 This close and disposed the window (note: this must not be called
 within a normal callback hook (because the object is disposed in
 this function)!
*******************************************************************/
static void read_window_close(struct Read_Data **pdata)
{
	struct Read_Data *data = *pdata;
	set(data->wnd,MUIA_Window_Open,FALSE);
	DoMethod(App,OM_REMMEMBER,data->wnd);
	MUI_DisposeObject(data->wnd);

	if (data->attachment_html_menu) MUI_DisposeObject(data->attachment_html_menu);
	if (data->attachment_standard_menu) MUI_DisposeObject(data->attachment_standard_menu);

	if (data->file_req) MUI_FreeAslRequest(data->file_req);
	mail_free(data->mail);
	if (data->num < MAX_READ_OPEN) read_open[data->num] = 0;
	free(data);
}

/******************************************************************
 The save button has been clicked
*******************************************************************/
static void save_button_pressed(struct Read_Data **pdata)
{
	struct mail *mail;
	struct Read_Data *data = *pdata;
	if (!data->attachments_last_selected) return;
	if (!(mail = (struct mail*)xget(data->attachments_last_selected,MUIA_UserData))) return;
	save_contents(data,mail);
}

/******************************************************************
 SimpleHTML Load Hook. Returns 1 if uri can be loaded by the hook
 otherwise 0.
*******************************************************************/
__asm int simplehtml_load_func(register __a0 struct Hook *h, register __a1 struct MUIP_SimpleHTML_LoadHook *msg)
{
	struct Read_Data *data = (struct Read_Data*)h->h_Data;
	char *uri = msg->uri;
	struct mail *mail;

	if (!data->attachments_last_selected) return 0;
	if (!(mail = (struct mail*)xget(data->attachments_last_selected,MUIA_UserData))) return 0;
	if (!(mail = mail_find_compound_object(mail,uri))) return 0;
	mail_decode(mail);
	if (!mail->decoded_data || !mail->decoded_len) return 0;
	msg->buffer = (void*)DoMethod(data->html_simplehtml, MUIM_SimpleHTML_AllocateMem, mail->decoded_len);
	if (!msg->buffer) return 0;
	CopyMem(mail->decoded_data,msg->buffer,mail->decoded_len);
	msg->buffer_len = mail->decoded_len;
	return 1;
}

#endif

/******************************************************************
 Read window dispose
*******************************************************************/
void read_window_dispose(GtkObject *object, gpointer user_data)
{
	struct Read_Data *data = (struct Read_Data*)user_data;

	if (data->folder_path) free(data->folder_path);
	mail_free(data->mail);

	if (data->num < MAX_READ_OPEN) read_open[data->num] = NULL;
	free(data);
	gtk_widget_hide_all(GTK_WIDGET(object));
}

/******************************************************************
 Display the mail
*******************************************************************/
static int read_window_display_mail(struct Read_Data *data, struct mail *m)
{
	char path[512];

	if (!data->folder_path) return 0;

	data->ref_mail = m;

	getcwd(path,sizeof(path));
	if (chdir(data->folder_path)==-1) return 0;

	if ((data->mail = mail_create_from_file(m->filename)))
	{
		int dont_show = 0; /* attachments */
		chdir(path); /* should be a absolute path */
		mail_read_contents(data->folder_path,data->mail);
		mail_create_html_header(data->mail);

		if (!data->mail->num_multiparts || (data->mail->num_multiparts == 1 && !data->mail->multipart_array[0]->num_multiparts))
		{
			/* mail has only one part */
			dont_show = 1;
		} else
		{
		}

		if (data->mail_list) g_slist_free(data->mail_list);
		data->mail_list = g_slist_alloc();

		insert_mail(data,data->mail);
		show_mail(data,mail_find_initial(data->mail));
	} else chdir(path);

	return 1;

	#if 0

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
	#endif
}

/******************************************************************
 Opens a read window
*******************************************************************/
int read_window_open(char *folder, struct mail *mail)
{
	int num;
	struct Read_Data *data;

	for (num=0; num < MAX_READ_OPEN; num++)
		if (!read_open[num]) break;

	if (num == MAX_READ_OPEN) return -1;

	if ((data = (struct Read_Data*)malloc(sizeof(struct Read_Data))))
	{
		memset(data,0,sizeof(struct Read_Data));
		data->folder_path = mystrdup(folder);

		data->num = num;
		read_open[num] = data;

		data->wnd = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title(GTK_WINDOW(data->wnd), "SimpleMail - Read mail");
		gtk_window_set_default_size(GTK_WINDOW(data->wnd),640,400);
		gtk_window_set_position(GTK_WINDOW(data->wnd),GTK_WIN_POS_CENTER);
		gtk_signal_connect(GTK_OBJECT(data->wnd), "destroy",GTK_SIGNAL_FUNC (read_window_dispose), data);

		data->text_scrolled_window = gtk_scrolled_window_new(NULL,NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(data->text_scrolled_window),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);

		data->text_view = gtk_text_view_new();
		g_object_set(data->text_view, "editable", FALSE, NULL);
		gtk_container_add(GTK_CONTAINER(data->text_scrolled_window), data->text_view);
		gtk_container_add(GTK_CONTAINER(data->wnd), data->text_scrolled_window);


		read_window_display_mail(data,mail);

		gtk_widget_show_all(data->wnd);
	}
	return num;
}

