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

#include "mail.h"
#include "simplemail.h"

#include "compiler.h"
#include "datatypesclass.h"
#include "muistuff.h"
#include "readlistclass.h"
#include "readwnd.h"

static struct Hook header_display_hook;
static struct Hook mime_construct_hook;
static struct Hook mime_destruct_hook;
static struct Hook mime_display_hook;
static struct Hook mime_findname_hook;

#define MAX_READ_OPEN 10
static int read_open[MAX_READ_OPEN];

#define PAGE_TEXT			0
#define PAGE_HTML			1
#define PAGE_DATATYPE	2

struct Read_Data /* should be a customclass */
{
	Object *wnd;
	Object *mime_tree;
	Object *contents_page;
	Object *datatype_datatypes;
	Object *text_list;
	Object *html_simplehtml;
	struct FileRequester *file_req;
	int num; /* the number of the window */
	struct mail *mail; /* the mail which is displayed */
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

struct mime_entry
{
	struct mail *mail;
	int save_button_no;
};

STATIC ASM struct mime_entry *mime_construct(register __a1 struct MUIP_NListtree_ConstructMessage *msg)
{
	struct mime_entry *new_mime_entry = (struct mime_entry*)malloc(sizeof(struct mime_entry));
	if (new_mime_entry)
	{
		struct mime_entry *mime_entry = (struct mime_entry*)msg->UserData;
		if (mime_entry) *new_mime_entry = *mime_entry;
		else memset(new_mime_entry,0,sizeof(struct mime_entry));
	}
	return new_mime_entry;
}

STATIC ASM VOID mime_destruct(register __a1 struct MUIP_NListtree_DestructMessage *msg)
{
	struct mime_entry *mime_entry = (struct mime_entry*)msg->UserData;
	free(mime_entry);
}

STATIC ASM VOID mime_display(register __a0 struct Hook *h, register __a1 struct MUIP_NListtree_DisplayMessage *msg)
{
	if (msg->TreeNode)
	{
		struct mime_entry *mime_entry = (struct mime_entry*)msg->TreeNode->tn_User;
		struct mail *mail = mime_entry->mail;
		static char buf[256];
		sprintf(buf,"%s/%s",mail->content_type,mail->content_subtype);
		*msg->Array++ = buf;
		*msg->Array++ = mail->content_transfer_encoding;
		if (mime_entry->save_button_no)
		{
			static char buf2[128];
			sprintf(buf2, "\033o[%ld@%ld]", mime_entry->save_button_no,mime_entry->save_button_no-1);
			*msg->Array++ = buf2;
		} else *msg->Array++ = NULL;
	} else
	{
	}
}

STATIC ASM int mime_findname(register __a0 struct Hook *h, register __a1 struct MUIP_NListtree_FindNameMessage *msg)
{
	struct mime_entry *entry = (struct mime_entry *)msg->UserData;
	if (entry->mail == (struct mail*)msg->Name) return 0;
	return -1;
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
			DoMethod(data->text_list,MUIM_NList_InsertSingle, buf, MUIV_NList_Insert_Bottom);
			if ((buf = strchr(buf,10))) buf++;
			else break;
		}
		set(data->text_list, MUIA_NList_Quiet, FALSE);
		set(data->contents_page, MUIA_Group_ActivePage, PAGE_TEXT);
	}
}

/******************************************************************
 inserts the mime informations (uses ugly recursion)
*******************************************************************/
static void insert_mime(Object *mime_tree, struct mail *mail, struct MUI_NListtree_TreeNode *listnode)
{
	struct MUI_NListtree_TreeNode *treenode;
	int i;
	struct mime_entry mime_entry;

	mime_entry.mail = mail;

	if (mail->num_multiparts == 0)
	{
		/* Add a save button because the contents could be saved */
		mime_entry.save_button_no = xget(mime_tree,MUIA_NList_Entries)+1;
		DoMethod(mime_tree,MUIM_NList_UseImage,MakeButton("Save"),mime_entry.save_button_no,-1);
	} else mime_entry.save_button_no = 0;

	treenode = (struct MUI_NListtree_TreeNode *)DoMethod(mime_tree,MUIM_NListtree_Insert,"",&mime_entry,listnode,MUIV_NListtree_Insert_PrevNode_Tail,(mail->num_multiparts==0)?0:(TNF_LIST|TNF_OPEN));
	if (!treenode) return;

	for (i=0;i<mail->num_multiparts;i++)
	{
		insert_mime(mime_tree,mail->multipart_array[i],treenode);
	}
}

/******************************************************************
 Save the contents of a given treenode
*******************************************************************/
static void save_contents(struct Read_Data *data, struct MUI_NListtree_TreeNode *treenode)
{
	if (!(treenode->tn_Flags & TNF_LIST))
	{
		struct mail *mail = ((struct mime_entry*)treenode->tn_User)->mail;

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
	struct MUI_NListtree_TreeNode *treenode = (struct MUI_NListtree_TreeNode *)
		DoMethod(data->mime_tree, MUIM_NListtree_FindName, MUIV_NListtree_FindName_ListNode_Root,
						m,0);
	if (treenode)
	{
		set(data->mime_tree, MUIA_NListtree_Active, treenode);
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
	if (data->file_req) MUI_FreeAslRequest(data->file_req);
	mail_free(data->mail);
	if (data->num < MAX_READ_OPEN) read_open[data->num] = 0;
	free(data);
}

/******************************************************************
 A new mime part has been activated
*******************************************************************/
static void mime_tree_active(struct Read_Data **pdata)
{
	struct Read_Data *data = *pdata;
	struct MUI_NListtree_TreeNode *treenode = (struct MUI_NListtree_TreeNode *)xget(data->mime_tree,MUIA_NListtree_Active);
	if (treenode)
	{
		if (!(treenode->tn_Flags & TNF_LIST))
		{
			mail_decode(((struct mime_entry*)treenode->tn_User)->mail);
			insert_text(data,((struct mime_entry*)treenode->tn_User)->mail);
		}
	}
}

/******************************************************************
 A button inside the mime tree has been clicked
*******************************************************************/
static void mime_tree_button(struct Read_Data **pdata)
{
	struct Read_Data *data = *pdata;
	int pos = xget(data->mime_tree, MUIA_NList_ButtonClick);
	if (pos >= 0)
	{
		struct MUI_NListtree_TreeNode *treenode;
		DoMethod(data->mime_tree, MUIM_NList_GetEntry, pos, &treenode);

		if (treenode)
		{
			save_contents(data,treenode);
		}
	}
}

/******************************************************************
 The save button has been clicked
*******************************************************************/
static void save_button_pressed(struct Read_Data **pdata)
{
	struct Read_Data *data = *pdata;
	struct MUI_NListtree_TreeNode *treenode = (struct MUI_NListtree_TreeNode *)xget(data->mime_tree,MUIA_NListtree_Active);
	if (treenode)
	{
		save_contents(data,treenode);
	}
}

/******************************************************************
 Opens a read window
*******************************************************************/
void read_window_open(char *folder, char *filename)
{
	Object *wnd,*header_list,*text_list, *html_simplehtml, *html_vert_scrollbar, *html_horiz_scrollbar, *mime_tree, *contents_page, *save_button;
	Object *datatype_datatypes;
	int num;

	for (num=0; num < MAX_READ_OPEN; num++)
		if (!read_open[num]) break;

	init_hook(&header_display_hook,(HOOKFUNC)header_display);
	init_hook(&mime_construct_hook,(HOOKFUNC)mime_construct);
	init_hook(&mime_destruct_hook,(HOOKFUNC)mime_destruct);
	init_hook(&mime_display_hook,(HOOKFUNC)mime_display);
	init_hook(&mime_findname_hook,(HOOKFUNC)mime_findname);

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
				Child, BalanceObject, End,
				Child, NListviewObject,
					MUIA_CycleChain, 1,
					MUIA_NListview_NList, mime_tree = NListtreeObject,
						MUIA_NListtree_ConstructHook, &mime_construct_hook,
						MUIA_NListtree_DestructHook, &mime_destruct_hook,
						MUIA_NListtree_DisplayHook, &mime_display_hook,
						MUIA_NListtree_FindNameHook, &mime_findname_hook,
						MUIA_NListtree_DragDropSort, FALSE,
						MUIA_NListtree_Format,",,",
						End,
					End,
				End,
			Child, BalanceObject,End,
			Child, contents_page = PageGroup,
				MUIA_Group_ActivePage, PAGE_TEXT,
				Child, VGroup,
					Child, NListviewObject,
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
					struct mail *text_mail;

					mail_read_contents(folder,data->mail);

					data->text_list = text_list;
					data->mime_tree = mime_tree;
					data->contents_page = contents_page;
					data->datatype_datatypes = datatype_datatypes;
					data->html_simplehtml = html_simplehtml;
					data->file_req = MUI_AllocAslRequestTags(ASL_FileRequest, ASLFR_DoSaveMode, TRUE, TAG_DONE);
					data->num = num;
					read_open[num] = 1;

					SetAttrs(data->html_simplehtml,
							MUIA_SimpleHTML_HorizScrollbar,html_horiz_scrollbar,
							MUIA_SimpleHTML_VertScrollbar,html_vert_scrollbar,
							TAG_DONE);

					insert_headers(header_list,data->mail);
					insert_mime(mime_tree,data->mail,MUIV_NListtree_Insert_ListNode_Root);

					text_mail = mail_find_content_type(data->mail, "text", "plain");

					DoMethod(mime_tree, MUIM_Notify, MUIA_NListtree_Active, MUIV_EveryTime, App, 4, MUIM_CallHook, &hook_standard, mime_tree_active, data);
					DoMethod(mime_tree, MUIM_Notify, MUIA_NList_ButtonClick, MUIV_EveryTime, App, 4, MUIM_CallHook, &hook_standard, mime_tree_button, data);
					DoMethod(save_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, save_button_pressed, data);
					DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, read_window_close, data);
					DoMethod(App,OM_ADDMEMBER,wnd);

					show_mail(data,text_mail?text_mail:data->mail);
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
