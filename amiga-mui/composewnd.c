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
** composewnd.c 
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <libraries/iffparse.h> /* MAKE_ID */
#include <libraries/mui.h>
#include <mui/nlistview_mcc.h>
#include <mui/nlisttree_mcc.h>
#include <mui/betterstring_mcc.h>
#include <mui/texteditor_mcc.h>
#include <clib/alib_protos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "addressbook.h"
#include "mail.h"
#include "simplemail.h"
#include "support.h"

#include "addressstringclass.h"
#include "compiler.h"
#include "muistuff.h"
#include "composewnd.h"

struct MUI_NListtree_TreeNode *FindListtreeUserData(Object *tree, APTR udata); /* in mainwnd.c */

struct attachment
{
	char *filename;
	char *description;
	char *mime;

	int size; /* size of the file */
	int editable; /* 1 text is editable */

	char *contents; /* text contents */
};

static struct Hook attach_construct_hook;
static struct Hook attach_destruct_hook;
static struct Hook attach_display_hook;

/* the address window functions */
STATIC ASM SAVEDS struct attachment *attach_construct(register __a1 struct MUIP_NListtree_ConstructMessage *msg)
{
	struct attachment *attach = (struct attachment *)msg->UserData;
	struct attachment *new_attach = (struct attachment *)malloc(sizeof(struct attachment));
	if (new_attach)
	{
		*new_attach = *attach;
		new_attach->filename = mystrdup(attach->filename);
		new_attach->description = mystrdup(attach->description);
		new_attach->mime = mystrdup(attach->mime);
		new_attach->contents = mystrdup(attach->contents);
	}
	return new_attach;
}

STATIC ASM SAVEDS VOID attach_destruct(register __a1 struct MUIP_NListtree_DestructMessage *msg)
{
	struct attachment *attach = (struct attachment *)msg->UserData;
	if (attach)
	{
		if (attach->filename) free(attach->filename);
		if (attach->description) free(attach->description);
		if (attach->mime) free(attach->mime);
		if (attach->contents) free(attach->contents);
		free(attach);
	}
}

STATIC ASM SAVEDS VOID attach_display(register __a1 struct MUIP_NListtree_DisplayMessage *msg)
{
	if (msg->TreeNode)
	{
		struct attachment *attach = (struct attachment *)msg->TreeNode->tn_User;
		*msg->Array++ = attach->filename;
		*msg->Array++ = NULL;
		*msg->Array++ = attach->mime;
		*msg->Array++ = attach->description;
	} else
	{
		*msg->Array++ = "File name";
		*msg->Array++ = "Size";
		*msg->Array++ = "Contents";
		*msg->Array = "Description";
	}
}


#define MAX_COMPOSE_OPEN 10
static int compose_open[MAX_COMPOSE_OPEN];

struct Compose_Data /* should be a customclass */
{
	Object *wnd;
	Object *to_string;
	Object *subject_string;
	Object *text_texteditor;
	Object *attach_tree;
	Object *contents_page;

	struct attachment *last_attachment;

	int num; /* the number of the window */
	/* more to add */
};

/******************************************************************
 This close and disposed the window (note: this must not be called
 within a normal callback hook (because the object is disposed in
 this function)!
*******************************************************************/
static void compose_window_close(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	set(data->wnd,MUIA_Window_Open,FALSE);
	DoMethod(App,OM_REMMEMBER,data->wnd);
	MUI_DisposeObject(data->wnd);
	if (data->num < MAX_COMPOSE_OPEN) compose_open[data->num] = 0;
	free(data);
}

/******************************************************************
 Expand the to string. Returns 1 for a success else 0
*******************************************************************/
static int compose_expand_to(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	char *str = addressbook_get_expand_str((char*)xget(data->to_string, MUIA_String_Contents));
	if (str)
	{
		set(data->to_string, MUIA_String_Contents, str);
		free(str);
		return 1;
	}
	DisplayBeep(NULL);
	set(data->wnd, MUIA_Window_ActiveObject,data->to_string);
	return 0;
}

/******************************************************************
 Add a attchment to the treelist
*******************************************************************/
static void compose_add_attachment(struct Compose_Data *data, struct attachment *attach, int list)
{
	struct MUI_NListtree_TreeNode *treenode = (struct MUI_NListtree_TreeNode *)xget(data->attach_tree, MUIA_NListtree_ActiveList);
	struct MUI_NListtree_TreeNode *activenode = (struct MUI_NListtree_TreeNode *)xget(data->attach_tree, MUIA_NListtree_Active);
	struct MUI_NListtree_TreeNode *insertlist = (struct MUI_NListtree_TreeNode *)MUIV_NListtree_Insert_ListNode_ActiveFallback;
	int quiet = 0;

	if (activenode)
	{
		if (activenode->tn_Flags & TNF_LIST)
		{
			/* if the active node is a list add the new node to this list */
			insertlist = activenode;
			treenode = insertlist;
		}
	}

	if (!treenode)
	{
		/* no list */
		treenode = (struct MUI_NListtree_TreeNode *)xget(data->attach_tree, MUIA_NListtree_Active);
		if (treenode)
		{
			/* we have another entry inside but no list (multipart), so insert also a multipart node */
			struct attachment multipart;

			memset(&multipart, 0, sizeof(multipart));
			multipart.mime = "multipart/mixed";

			quiet = 1;
			set(data->attach_tree, MUIA_NListtree_Quiet, TRUE);

			insertlist = (struct MUI_NListtree_TreeNode *)DoMethod(data->attach_tree, MUIM_NListtree_Insert, "", &multipart,
					MUIV_NListtree_Insert_ListNode_ActiveFallback, MUIV_NListtree_Insert_PrevNode_Tail,TNF_OPEN|TNF_LIST);

			if (insertlist)
			{
				DoMethod(data->attach_tree, MUIM_NListtree_Move, MUIV_NListtree_Move_OldListNode_Active, MUIV_NListtree_Move_OldTreeNode_Active,
								insertlist, MUIV_NListtree_Move_NewTreeNode_Tail);
			} else return;
		}
	}

	DoMethod(data->attach_tree, MUIM_NListtree_Insert, "" /*name*/, attach, /* udata */
					 insertlist,MUIV_NListtree_Insert_PrevNode_Tail, (list?TNF_OPEN|TNF_LIST:0)|MUIV_NListtree_Insert_Flag_Active);

	if (quiet)
	{
		set(data->attach_tree, MUIA_NListtree_Quiet, FALSE);
	}
}


/******************************************************************
 Add a multipart node to the list
*******************************************************************/
static void compose_add_text(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	struct attachment attach;

	memset(&attach, 0, sizeof(attach));
	attach.mime = "text/plain";
	attach.editable = 1;

	compose_add_attachment(data,&attach,0);
}

/******************************************************************
 Add a multipart node to the list
*******************************************************************/
static void compose_add_multipart(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	struct attachment attach;

	memset(&attach, 0, sizeof(attach));
	attach.mime = "multipart/mixed";
	attach.editable = 0;

	compose_add_attachment(data,&attach,1);
}

/******************************************************************
 A new attachment has been clicked
*******************************************************************/
static void compose_attach_active(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	struct MUI_NListtree_TreeNode *activenode = (struct MUI_NListtree_TreeNode *)xget(data->attach_tree, MUIA_NListtree_Active);
	struct attachment *attach = NULL;

	if (data->last_attachment)
	{
		/* Try if the attachment is still in the list (could be not the case if removed) */
		struct MUI_NListtree_TreeNode *tn = FindListtreeUserData(data->attach_tree, data->last_attachment);
		if (tn)
		{
			STRPTR text_buf = (STRPTR)DoMethod(data->text_texteditor, MUIM_TextEditor_ExportText);
			if (text_buf)
			{
				/* free the memory of the last contents */
				if (data->last_attachment->contents) free(data->last_attachment->contents);
				data->last_attachment->contents = mystrdup(text_buf);
				FreeVec(text_buf);
			}
		}
	}

	if (activenode)
	{
		attach = (struct attachment *)activenode->tn_User;
	}

	if (attach)
	{
		if (attach->editable)
		{
			set(data->text_texteditor, MUIA_TextEditor_Contents, attach->contents?attach->contents:"");
		}

		SetAttrs(data->contents_page,
				MUIA_Disabled, FALSE,
				MUIA_Group_ActivePage, attach->editable?0:1,
				TAG_DONE);
	} else
	{
/*		set(data->contents_page, MUIA_Disabled, TRUE); */
	}
	data->last_attachment = attach;
}

/******************************************************************
 A mail should be send later
*******************************************************************/
static void compose_window_send_later(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	if (compose_expand_to(pdata))
	{
		char *to = (char*)xget(data->to_string, MUIA_String_Contents);
		char *subject = (char*)xget(data->subject_string, MUIA_String_Contents);

		STRPTR text_buf = (STRPTR)DoMethod(data->text_texteditor, MUIM_TextEditor_ExportText);
		if (text_buf)
		{
			struct composed_mail new_mail;
			new_mail.to = to;
			new_mail.subject = subject;
			new_mail.text = text_buf;
			mail_compose_new(&new_mail);
			FreeVec(text_buf);
		}

		/* Close (and dispose) the compose window (data) */
		DoMethod(App, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, compose_window_close, data);	
	}
}

/******************************************************************
 Opens a compose window
*******************************************************************/
void compose_window_open(char *to_str)
{
	Object *wnd, *send_later_button, *cancel_button;
	Object *to_string, *subject_string;
	Object *text_texteditor, *xcursor_text, *ycursor_text;
	Object *expand_to_button;
	Object *attach_tree, *add_text_button, *add_multipart_button, *remove_button;
	Object *contents_page;

	int num;

	attach_construct_hook.h_Entry = (HOOKFUNC)attach_construct;
	attach_destruct_hook.h_Entry = (HOOKFUNC)attach_destruct;
	attach_display_hook.h_Entry = (HOOKFUNC)attach_display;

	for (num=0; num < MAX_COMPOSE_OPEN; num++)
		if (!compose_open[num]) break;

	wnd = WindowObject,
		(num < MAX_COMPOSE_OPEN)?MUIA_Window_ID:TAG_IGNORE, MAKE_ID('C','O','M',num),
    MUIA_Window_Title, "SimpleMail - Compose Message",
        
		WindowContents, VGroup,
			Child, ColGroup(2),
				Child, MakeLabel("_To"),
				Child, HGroup,
					MUIA_Group_Spacing,0,
					Child, to_string = AddressStringObject,
						MUIA_CycleChain, 1,
						StringFrame,
						End,
					Child, expand_to_button = PopButton(MUII_ArrowLeft),
					End,
				Child, MakeLabel("_Subject"),
				Child, subject_string = BetterStringObject,
					MUIA_CycleChain, 1,
					StringFrame,
					End,
				End,
			Child, contents_page = PageGroup,
				MUIA_Group_ActivePage, 0,
				Child, VGroup,
					Child, HGroup,
						Child, HVSpace,
						Child, VGroup,
							TextFrame,
							MUIA_Group_Spacing, 0,
							Child, xcursor_text = TextObject,
								MUIA_Font, MUIV_Font_Fixed,
								MUIA_Text_Contents, "0000",
								MUIA_Text_SetMax, TRUE,
								MUIA_Text_SetMin, TRUE,
								End,
							Child, ycursor_text = TextObject,
								MUIA_Font, MUIV_Font_Fixed,
								MUIA_Text_Contents, "0000",
								MUIA_Text_SetMax, TRUE,
								MUIA_Text_SetMin, TRUE,
								End,
							End,
						End,
					Child, text_texteditor = (Object*)TextEditorObject,
						MUIA_CycleChain, 1,
						End,
					End,
				Child, VGroup,
					Child, HVSpace,
					Child, MakeButton("Test"),
					Child, HVSpace,
					End,
				End,
			Child, BalanceObject, End,
			Child, VGroup,
				MUIA_Weight, 33,
				Child, NListviewObject,
					MUIA_CycleChain, 1,
					MUIA_NListview_NList, attach_tree = NListtreeObject,
						MUIA_NListtree_ConstructHook, &attach_construct_hook,
						MUIA_NListtree_DestructHook, &attach_construct_hook,
						MUIA_NListtree_DisplayHook, &attach_display_hook,
						MUIA_NListtree_Title, TRUE,
						MUIA_NListtree_Format, ",,,",
						MUIA_NListtree_TreeColumn, 2,
						End,
					End,
				Child, HGroup,
					Child, add_text_button = MakeButton("Add text"),
					Child, add_multipart_button = MakeButton("Add multipart"),
					Child, MakeButton("Add file(s)"),
					Child, MakeButton("Pack & add"),
					Child, remove_button = MakeButton("Remove"),
					End,
				End,
			Child, HorizLineObject,
			Child, HGroup,
				Child, send_later_button = MakeButton("Send later"),
				Child, cancel_button = MakeButton("Cancel"),
				End,
			End,
		End;
	
	if (wnd)
	{
		struct Compose_Data *data = (struct Compose_Data*)malloc(sizeof(struct Compose_Data));
		if (data)
		{
			data->wnd = wnd;
			data->num = num;
			data->to_string = to_string;
			data->subject_string = subject_string;
			data->text_texteditor = text_texteditor;
			data->attach_tree = attach_tree;
			data->contents_page = contents_page;

			/* mark the window as opened */
			compose_open[num] = 1;

			DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, compose_window_close, data);
			DoMethod(expand_to_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, compose_expand_to, data);
			DoMethod(text_texteditor, MUIM_Notify, MUIA_TextEditor_CursorX, MUIV_EveryTime, xcursor_text, 4, MUIM_SetAsString, MUIA_Text_Contents, "%04ld", MUIV_TriggerValue);
			DoMethod(text_texteditor, MUIM_Notify, MUIA_TextEditor_CursorY, MUIV_EveryTime, ycursor_text, 4, MUIM_SetAsString, MUIA_Text_Contents, "%04ld", MUIV_TriggerValue);
			DoMethod(add_text_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, compose_add_text, data);
			DoMethod(add_multipart_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, compose_add_multipart, data);
			DoMethod(remove_button, MUIM_Notify, MUIA_Pressed, FALSE, attach_tree, 4, MUIM_NListtree_Remove, MUIV_NListtree_Remove_ListNode_Active, MUIV_NListtree_Remove_TreeNode_Active, 0);
			DoMethod(attach_tree, MUIM_Notify, MUIA_NListtree_Active, MUIV_EveryTime, attach_tree, 4, MUIM_CallHook, &hook_standard, compose_attach_active, data);
			DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, compose_window_close, data);
			DoMethod(send_later_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, compose_window_send_later, data);
			DoMethod(App,OM_ADDMEMBER,wnd);

			compose_add_text(&data);

			if (to_str)
			{
				set(to_string,MUIA_String_Contents,to_str);
				/* activate the "Subject" field */
				set(wnd,MUIA_Window_ActiveObject,data->subject_string);
			} else
			{
				/* activate the "To" field */
				set(wnd,MUIA_Window_ActiveObject,data->to_string);
			}

			set(wnd,MUIA_Window_Open,TRUE);

			return;
		}
		MUI_DisposeObject(wnd);
	}
}
