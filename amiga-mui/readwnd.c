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
#include <clib/alib_protos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "mail.h"
#include "simplemail.h"

#include "compiler.h"
#include "muistuff.h"
#include "readwnd.h"

static struct Hook header_display_hook;
static struct Hook mime_display_hook;

#define MAX_READ_OPEN 10
static int read_open[MAX_READ_OPEN];

struct Read_Data /* should be a customclass */
{
	Object *wnd;
	Object *mime_tree;
	Object *text_list;
	Object *contents_page;
	struct FileRequester *file_req;
	int num; /* the number of the window */
	struct mail *mail; /* the mail which is displayed */
	/* more to add */
};

STATIC ASM SAVEDS VOID header_display(register __a2 char **array, register __a1 struct header *header)
{
	if (header)
	{
		*array++ = header->name;
		*array = header->contents;
	}
}


STATIC ASM SAVEDS VOID mime_display(register __a1 struct MUIP_NListtree_DisplayMessage *msg)
{
	if (msg->TreeNode)
	{
		struct mail *mail = (struct mail*)msg->TreeNode->tn_User;
		static char buf[256];
		sprintf(buf,"%s/%s",mail->content_type,mail->content_subtype);
		*msg->Array++ = buf;
		*msg->Array = mail->content_transfer_encoding;
	} else
	{
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

	set(data->text_list, MUIA_NList_Quiet, TRUE);
	DoMethod(data->text_list,MUIM_NList_Clear);

	if (mail->decoded_data)
	{
		if (stricmp(mail->content_type,"text"))
		{
			char buf[256];
			set(data->contents_page, MUIA_Group_ActivePage, 1);
			sprintf(buf,"Base64 encoded data (size %ld)",mail->decoded_len);
			DoMethod(data->text_list,MUIM_NList_InsertSingle,buf,MUIV_NList_Insert_Bottom);
			set(data->text_list, MUIA_NList_Quiet, FALSE);
			return;
		}
		buf = mail->decoded_data;
		buf_end = buf + mail->decoded_len;
	} else
	{
		buf = mail->text + mail->text_begin;
		buf_end = buf + mail->text_len;
	}

	while (buf < buf_end)
	{
		DoMethod(data->text_list,MUIM_NList_InsertSingle, buf, MUIV_NList_Insert_Bottom);
		if ((buf = strchr(buf,10))) buf++;
		else break;
	}

	set(data->text_list, MUIA_NList_Quiet, FALSE);

	set(data->contents_page, MUIA_Group_ActivePage, 0);
}

/******************************************************************
 inserts the mime informations (uses ugly recursion)
*******************************************************************/
static void insert_mime(Object *mime_tree, struct mail *mail, struct MUI_NListtree_TreeNode *listnode)
{
	struct MUI_NListtree_TreeNode *treenode;
	int i;

	treenode = (struct MUI_NListtree_TreeNode *)DoMethod(mime_tree,MUIM_NListtree_Insert,"",mail,listnode,MUIV_NListtree_Insert_PrevNode_Tail,(mail->num_multiparts==0)?0:(TNF_LIST|TNF_OPEN));
	if (!treenode) return;

	for (i=0;i<mail->num_multiparts;i++)
	{
		insert_mime(mime_tree,mail->multipart_array[i],treenode);
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
			mail_decode((struct mail*)treenode->tn_User);
			insert_text(data,(struct mail*)treenode->tn_User);
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
		if (!(treenode->tn_Flags & TNF_LIST))
		{
			if (MUI_AslRequest(data->file_req, NULL))
			{
				BPTR dlock;
				STRPTR drawer = data->file_req->fr_Drawer;
				struct mail *mail = (struct mail*)treenode->tn_User;

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
}

/******************************************************************
 Opens a read window
*******************************************************************/
void read_window_open(char *folder, char *filename)
{
	Object *wnd,*header_list,*text_list, *mime_tree, *contents_page, *save_button;
	int num;

	for (num=0; num < MAX_READ_OPEN; num++)
		if (!read_open[num]) break;

	header_display_hook.h_Entry = (HOOKFUNC)header_display;
	mime_display_hook.h_Entry = (HOOKFUNC)mime_display;

	wnd = WindowObject,
		(num < MAX_READ_OPEN)?MUIA_Window_ID:TAG_IGNORE, MAKE_ID('R','E','A',num),
    MUIA_Window_Title, "SimpleMail - Read Message",
        
		WindowContents, VGroup,
			Child, HGroup,
				MUIA_VertWeight,33,
				Child, NListviewObject,
					MUIA_HorizWeight,300,
					MUIA_NListview_NList, header_list = NListObject,
						MUIA_NList_DisplayHook, &header_display_hook,
						MUIA_NList_Format, "P=" MUIX_R MUIX_PH ",",
						End,
					End,
				Child, BalanceObject, End,
				Child, NListviewObject,
					MUIA_NListview_NList, mime_tree = NListtreeObject,
						MUIA_NListtree_DisplayHook, &mime_display_hook,
						MUIA_NListtree_DragDropSort, FALSE,
						MUIA_NListtree_Format,",",
						End,
					End,
				End,
			Child, BalanceObject,End,
			Child, contents_page = PageGroup,
				MUIA_Group_ActivePage, 0,
				Child, VGroup,
					Child, NListviewObject,
						MUIA_NListview_NList, text_list = NListObject,
							MUIA_NList_ConstructHook, MUIV_NList_ConstructHook_String,
							MUIA_NList_DestructHook, MUIV_NList_DestructHook_String,
							End,
						End,
					End,
				Child, VGroup,
					Child, HVSpace,
					Child, save_button = MakeButton("Save"),
					Child, HVSpace,
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
					mail_read_contents(folder,data->mail);

					data->text_list = text_list;
					data->mime_tree = mime_tree;
					data->contents_page = contents_page;
					data->file_req = MUI_AllocAslRequestTags(ASL_FileRequest, ASLFR_DoSaveMode, TRUE, TAG_DONE);
					data->num = num;
					read_open[num] = 1;

					insert_headers(header_list,data->mail);
					insert_mime(mime_tree,data->mail,MUIV_NListtree_Insert_ListNode_Root);
					DoMethod(mime_tree, MUIM_Notify, MUIA_NListtree_Active, MUIV_EveryTime, App, 4, MUIM_CallHook, &hook_standard, mime_tree_active, data);
					DoMethod(save_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, save_button_pressed, data);
					DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, read_window_close, data);
					DoMethod(App,OM_ADDMEMBER,wnd);
					set(mime_tree,MUIA_NListtree_Active,MUIV_NListtree_Active_First);
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
