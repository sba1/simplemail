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

#include <libraries/asl.h>
#include <libraries/iffparse.h> /* MAKE_ID */
#include <libraries/mui.h>
#include <mui/nlistview_mcc.h>
#include <mui/nlisttree_mcc.h>
#include "simplehtml_mcc.h"
#include <clib/alib_protos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "configuration.h"
#include "mail.h"
#include "simplemail.h"
#include "support.h"

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

/******************************************************************
 Shows a given mail (part)
*******************************************************************/
static void show_mail(struct Read_Data *data, struct mail *m)
{
	if (!data->attachments_group)
	{
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

/******************************************************************
 Opens a read window
*******************************************************************/
void read_window_open(char *folder, char *filename)
{
	Object *wnd,*header_list,*text_list, *html_simplehtml, *html_vert_scrollbar, *html_horiz_scrollbar, *contents_page, *save_button;
	Object *attachments_group;
	Object *datatype_datatypes;
	Object *text_listview;
	int num;

	for (num=0; num < MAX_READ_OPEN; num++)
		if (!read_open[num]) break;

	init_hook(&header_display_hook,(HOOKFUNC)header_display);

	wnd = WindowObject,
		(num < MAX_READ_OPEN)?MUIA_Window_ID:TAG_IGNORE, MAKE_ID('R','E','A',num),
    MUIA_Window_Title, "SimpleMail - Read Message",
        
		WindowContents, VGroup,
			Child, HGroup,
				MUIA_VertWeight,33,
				Child, NListviewObject,
					MUIA_CycleChain, 1,
					MUIA_HorizWeight,300,
					MUIA_NListview_NList, header_list = NListObject,
						MUIA_NList_DisplayHook, &header_display_hook,
						MUIA_NList_Format, "P=" MUIX_R MUIX_PH ",",
						End,
					End,
				End,
			Child, BalanceObject,End,
			Child, contents_page = PageGroup,
				MUIA_Group_ActivePage, PAGE_TEXT,
				Child, VGroup,
					Child, text_listview = NListviewObject,
						MUIA_CycleChain, 1,
						MUIA_NListview_NList, text_list = ReadListObject,
							End,
						End,
					End,
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
					Child, datatype_datatypes = DataTypesObject, TextFrame, End,
					Child, save_button = MakeButton("Save"),
					End,
				End,
			Child, attachments_group = HGroupV,
				End,
			End,
		End;
	
	if (wnd)
	{
		struct Read_Data *data = (struct Read_Data*)malloc(sizeof(struct Read_Data));
		if (data)
		{
			BPTR lock = Lock(folder,ACCESS_READ); /* maybe it's better to use an absoulte path here */
			if (lock)
			{
				BPTR old_dir = CurrentDir(lock);
				data->wnd = wnd;
				
				if ((data->mail = mail_create_from_file(filename)))
				{
					Object *save_contents_item;
					Object *save_contents2_item;
					Object *save_document_item;

					mail_read_contents(folder,data->mail);

					data->attachment_standard_menu = MenustripObject,
						Child, MenuObjectT("Attachment"),
							Child, save_contents_item = MenuitemObject,
								MUIA_Menuitem_Title, "Save As...",
								MUIA_UserData, 1,
								End,
							End,
						End;

					data->attachment_html_menu = MenustripObject,
						Child, MenuObjectT("Attachment"),
							Child, save_contents2_item = MenuitemObject,
								MUIA_Menuitem_Title, "Save as...",
								MUIA_UserData, 1,
								End,
							Child, save_document_item = MenuitemObject,
								MUIA_Menuitem_Title, "Save whole document as...",
								MUIA_UserData, 2,
								End,
							End,
						End;

					if (!data->mail->num_multiparts)
					{
						/* mail has only one part */
						set(attachments_group, MUIA_ShowMe, FALSE);
						attachments_group = NULL;
					}

					data->text_list = text_list;
					data->contents_page = contents_page;
					data->datatype_datatypes = datatype_datatypes;
					data->html_simplehtml = html_simplehtml;
					data->file_req = MUI_AllocAslRequestTags(ASL_FileRequest, ASLFR_DoSaveMode, TRUE, TAG_DONE);
					data->attachments_group = attachments_group;
					data->num = num;
					data->attachments_last_selected = NULL;
					read_open[num] = 1;

					init_myhook(&data->simplehtml_load_hook, (HOOKFUNC)simplehtml_load_func, data);

					SetAttrs(data->html_simplehtml,
							MUIA_SimpleHTML_HorizScrollbar, html_horiz_scrollbar,
							MUIA_SimpleHTML_VertScrollbar, html_vert_scrollbar,
							MUIA_SimpleHTML_LoadHook, &data->simplehtml_load_hook,
							TAG_DONE);

					set(text_list, MUIA_ContextMenu, data->attachment_standard_menu);
					DoMethod(text_list, MUIM_Notify, MUIA_ContextMenuTrigger, MUIV_EveryTime, App, 6, MUIM_CallHook, &hook_standard, context_menu_trigger, data, data->mail, MUIV_TriggerValue);

					insert_headers(header_list,data->mail);
					insert_mail(data,data->mail);

					if (attachments_group)
					{
						DoMethod(attachments_group, OM_ADDMEMBER, HSpace(0));
					}

					DoMethod(save_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, save_button_pressed, data);
					DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, read_window_close, data);
					DoMethod(App,OM_ADDMEMBER,wnd);

					show_mail(data,mail_find_initial(data->mail));
					set(wnd,MUIA_Window_DefaultObject, data->text_list);
					set(wnd,MUIA_Window_Open,TRUE);
					CurrentDir(old_dir);
					return;
				}
				CurrentDir(old_dir);
			}
			free(data);
		}
		MUI_DisposeObject(wnd);
	}
}
