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

#include <libraries/asl.h>
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
#include "codecs.h"
#include "mail.h"
#include "parse.h"
#include "simplemail.h"
#include "support.h"

#include "addressstringclass.h"
#include "amigasupport.h"
#include "attachmentlistclass.h"
#include "composeeditorclass.h"
#include "compiler.h"
#include "composewnd.h"
#include "datatypesclass.h"
#include "muistuff.h"
#include "picturebuttonclass.h"

struct MUI_NListtree_TreeNode *FindListtreeUserData(Object *tree, APTR udata); /* in mainwnd.c */

#define MAX_COMPOSE_OPEN 10
static int compose_open[MAX_COMPOSE_OPEN];

struct Compose_Data /* should be a customclass */
{
	Object *wnd;
	Object *to_string;
	Object *subject_string;
	Object *copy_button;
	Object *cut_button;
	Object *paste_button;
	Object *undo_button;
	Object *redo_button;
	Object *x_text;
	Object *y_text;
	Object *text_texteditor;
	Object *attach_tree;
	Object *contents_page;
	Object *datatype_datatypes;
	Object *attach_group;
	Object *vertical_balance;
	Object *main_group;
	Object *switch_button;

	char *filename; /* the emails filename if changed */
	char *folder; /* the emails folder if changed */
	char *reply_id; /* the emails reply-id if changed */

	struct FileRequester *file_req;

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
	set(data->datatype_datatypes, MUIA_DataTypes_FileName, NULL);
	MUI_DisposeObject(data->wnd);
	if (data->file_req) MUI_FreeAslRequest(data->file_req);
	if (data->filename) free(data->filename);
	if (data->folder) free(data->folder);
	if (data->reply_id) free(data->reply_id);
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
			multipart.content_type = "multipart/mixed";

			quiet = 1;
			set(data->attach_tree, MUIA_NListtree_Quiet, TRUE);

			insertlist = (struct MUI_NListtree_TreeNode *)DoMethod(data->attach_tree, MUIM_NListtree_Insert, "", &multipart,
					MUIV_NListtree_Insert_ListNode_ActiveFallback, MUIV_NListtree_Insert_PrevNode_Tail,TNF_OPEN|TNF_LIST);

			if (insertlist)
			{
				DoMethod(data->attach_tree, MUIM_NListtree_Move, MUIV_NListtree_Move_OldListNode_Active, MUIV_NListtree_Move_OldTreeNode_Active,
								insertlist, MUIV_NListtree_Move_NewTreeNode_Tail);
			} else
			{
				set(data->attach_tree, MUIA_NListtree_Quiet, FALSE);
				return;
			}
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
	attach.content_type = "text/plain";
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
	attach.content_type = "multipart/mixed";
	attach.editable = 0;

	compose_add_attachment(data,&attach,1);
}

/******************************************************************
 Add files to the list
*******************************************************************/
static void compose_add_files(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	struct attachment attach;

	if (data->file_req)
	{
		if (MUI_AslRequestTags(data->file_req,ASLFR_DoMultiSelect, TRUE, TAG_DONE))
		{
			int i;
			memset(&attach, 0, sizeof(attach));

			for (i=0; i<data->file_req->fr_NumArgs;i++)
			{
				STRPTR drawer = NameOfLock(data->file_req->fr_ArgList[i].wa_Lock);
				if (drawer)
				{
					int len = strlen(drawer)+strlen(data->file_req->fr_ArgList[i].wa_Name)+4;
					STRPTR buf = (STRPTR)AllocVec(len,MEMF_PUBLIC);
					if (buf)
					{
						strcpy(buf,drawer);
						AddPart(buf,data->file_req->fr_ArgList[i].wa_Name,len);
						attach.content_type = identify_file(buf);
						attach.editable = 0;
						attach.filename = buf;

						compose_add_attachment(data,&attach,0);
						FreeVec(buf);
					}
					FreeVec(drawer);
				}
			}
		}
	}
}

/******************************************************************
 A new attachment has been clicked
*******************************************************************/
static void compose_attach_active(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	struct MUI_NListtree_TreeNode *activenode = (struct MUI_NListtree_TreeNode *)xget(data->attach_tree, MUIA_NListtree_Active);
	struct attachment *attach = NULL;

	if (data->last_attachment && data->last_attachment->editable)
	{
		/* Try if the attachment is still in the list (could be not the case if removed) */
		struct MUI_NListtree_TreeNode *tn = FindListtreeUserData(data->attach_tree, data->last_attachment);
		if (tn)
		{
			STRPTR text_buf;

			set(data->text_texteditor, MUIA_TextEditor_ExportHook, MUIV_TextEditor_ExportHook_EMail);

			if ((text_buf = (STRPTR)DoMethod(data->text_texteditor, MUIM_TextEditor_ExportText)))
			{
				/* free the memory of the last contents */
				if (data->last_attachment->contents) free(data->last_attachment->contents);
				data->last_attachment->contents = mystrdup(text_buf);
				data->last_attachment->lastxcursor = xget(data->text_texteditor, MUIA_TextEditor_CursorX);
				data->last_attachment->lastycursor = xget(data->text_texteditor, MUIA_TextEditor_CursorY);
				FreeVec(text_buf);
			}
			set(data->text_texteditor, MUIA_TextEditor_ExportHook, MUIV_TextEditor_ExportHook_Plain);
		}
	}

	if (activenode)
	{
		attach = (struct attachment *)activenode->tn_User;
	}

	if (attach)
	{
		if (attach != data->last_attachment)
		{
			if (attach->editable)
			{
				set(data->text_texteditor, MUIA_TextEditor_ImportHook, MUIV_TextEditor_ImportHook_MIME);
				SetAttrs(data->text_texteditor,
						MUIA_TextEditor_Contents, attach->contents?attach->contents:"",
						MUIA_TextEditor_CursorX,attach->lastxcursor,
						MUIA_TextEditor_CursorY,attach->lastycursor,
						MUIA_NoNotify, TRUE,
						TAG_DONE);
				set(data->text_texteditor, MUIA_TextEditor_ImportHook, MUIV_TextEditor_ImportHook_Plain);

				DoMethod(data->x_text, MUIM_SetAsString, MUIA_Text_Contents, "%04ld", xget(data->text_texteditor,MUIA_TextEditor_CursorX));
				DoMethod(data->y_text, MUIM_SetAsString, MUIA_Text_Contents, "%04ld", xget(data->text_texteditor,MUIA_TextEditor_CursorY));

				set(data->wnd, MUIA_Window_ActiveObject, data->text_texteditor);
			}

			SetAttrs(data->contents_page,
					MUIA_Disabled, FALSE,
					MUIA_Group_ActivePage, attach->editable?0:1,
					TAG_DONE);

			set(data->datatype_datatypes, MUIA_DataTypes_FileName, attach->temporary_filename?attach->temporary_filename:attach->filename);
		}
	} else
	{
/*		set(data->contents_page, MUIA_Disabled, TRUE);*/
	}
	data->last_attachment = attach;
}

/******************************************************************
 Attach the mail given in the treenode to the current mail
 (recursive)
*******************************************************************/
static void compose_window_attach_mail(struct Compose_Data *data, struct MUI_NListtree_TreeNode *treenode, struct composed_mail *cmail)
{
	struct attachment *attach;

	if (!treenode) treenode = (struct MUI_NListtree_TreeNode *)DoMethod(data->attach_tree,
				MUIM_NListtree_GetEntry, MUIV_NListtree_GetEntry_ListNode_Root, MUIV_NListtree_GetEntry_Position_Head, 0);

	if (!treenode) return;
	if (!(attach = (struct attachment *)treenode->tn_User)) return;

	if (treenode->tn_Flags & TNF_LIST)
	{
		struct MUI_NListtree_TreeNode *tn = (struct MUI_NListtree_TreeNode *)DoMethod(data->attach_tree,
				MUIM_NListtree_GetEntry, treenode, MUIV_NListtree_GetEntry_Position_Head, 0);

		cmail->content_type = mystrdup(attach->content_type);

		while (tn)
		{
			struct composed_mail *newcmail = (struct composed_mail *)malloc(sizeof(struct composed_mail));
			if (newcmail)
			{
				composed_mail_init(newcmail);
				compose_window_attach_mail(data,tn,newcmail);
				list_insert_tail(&cmail->list,&newcmail->node);
			}
			tn = (struct MUI_NListtree_TreeNode*)DoMethod(data->attach_tree, MUIM_NListtree_GetEntry, tn, MUIV_NListtree_GetEntry_Position_Next,0);
		}
	} else
	{
		cmail->content_type = mystrdup(attach->content_type);
		cmail->text = mystrdup(attach->contents);
		cmail->filename = mystrdup(attach->filename);
		cmail->temporary_filename = mystrdup(attach->temporary_filename);
	}
}

/******************************************************************
 Compose a mail and close the window
*******************************************************************/
static void compose_mail(struct Compose_Data *data, int hold)
{
	if (compose_expand_to(&data))
	{
		char *to = (char*)xget(data->to_string, MUIA_String_Contents);
		char *subject = (char*)xget(data->subject_string, MUIA_String_Contents);
		struct composed_mail new_mail;

		/* update the current attachment */
		compose_attach_active(&data);

		/* Initialize the structure with default values */
		composed_mail_init(&new_mail);

		/* Attach the mails recursivly */
		compose_window_attach_mail(data, NULL /*root*/, &new_mail);

		new_mail.to = to;
		new_mail.subject = subject;
		new_mail.mail_filename = data->filename;
		new_mail.mail_folder = data->folder;
		new_mail.reply_message_id = data->reply_id;

		if ((mail_compose_new(&new_mail,hold)))
		{
			/* Close (and dispose) the compose window (data) */
			DoMethod(App, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, compose_window_close, data);	
		}
	}
}

/******************************************************************
 A mail should be send later
*******************************************************************/
static void compose_window_send_later(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	compose_mail(data,0);
}

/******************************************************************
 A mail should be hold
*******************************************************************/
static void compose_window_hold(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	compose_mail(data,1);
}

/******************************************************************
 The switch button's selected state has changed
*******************************************************************/
static void compose_switch_view(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;

	DoMethod(data->main_group, MUIM_Group_InitChange);
	if (xget(data->switch_button, MUIA_Selected))
	{
		set(data->attach_group, MUIA_ShowMe, TRUE);
		set(data->vertical_balance, MUIA_ShowMe, TRUE);
	} else
	{
		set(data->attach_group, MUIA_ShowMe, FALSE);
		set(data->vertical_balance, MUIA_ShowMe, FALSE);
	}
	DoMethod(data->main_group, MUIM_Group_ExitChange);
}

/******************************************************************
 inserts a mail into the listtree (uses recursion)
*******************************************************************/
static void compose_add_mail(struct Compose_Data *data, struct mail *mail, struct MUI_NListtree_TreeNode *listnode)
{
	/* Note, the following three datas are static although the function is recursive
	 * It minimalizes the possible stack overflow
   */
	static char buf[128];
	static char tmpname[L_tmpnam+1];
	static struct attachment attach;
	struct MUI_NListtree_TreeNode *treenode;
	int i,num_multiparts = mail->num_multiparts;

	memset(&attach,0,sizeof(attach));

	if (mail->content_type)
	{
		/* If the mail has a content type */
		sprintf(buf,"%s/%s",mail->content_type,mail->content_subtype);
	} else
	{
		/* Use text/plain as the default content type */
		strcpy(buf,"text/plain");
	}

	attach.content_type = buf;

	if (!num_multiparts)
	{
		/* decode the mail */
		mail_decode(mail);

		/* if the content type is a text it can be edited */
		if (!mystricmp(buf,"text/plain"))
		{
			if (mail->decoded_data)
			{
				attach.contents = mystrndup(mail->decoded_data,mail->decoded_len);
			} else
			{
				attach.contents = mystrndup(mail->text + mail->text_begin,mail->text_len);
			}
			attach.editable = 1;
			attach.lastxcursor = 0;
			attach.lastycursor = 0;
		} else
		{
			BPTR fh;

			tmpnam(tmpname);

			if ((fh = Open(tmpname,MODE_NEWFILE)))
			{
				Write(fh,mail->decoded_data,mail->decoded_len);
				Close(fh);
			}
			attach.filename = mail->filename?mail->filename:tmpname;
			attach.temporary_filename = tmpname;
		}
	}

	treenode = (struct MUI_NListtree_TreeNode *)DoMethod(data->attach_tree,MUIM_NListtree_Insert,"",&attach,listnode,MUIV_NListtree_Insert_PrevNode_Tail,num_multiparts?(TNF_LIST|TNF_OPEN):0);
	if (!treenode) return;

	if (attach.contents) free(attach.contents);

	for (i=0;i<num_multiparts; i++)
	{
		compose_add_mail(data,mail->multipart_array[i],treenode);
	}
}

/******************************************************************
 Opens a compose window
*******************************************************************/
void compose_window_open(char *to_str, struct mail *tochange)
{
	Object *wnd, *send_later_button, *hold_button, *cancel_button;
	Object *to_string, *subject_string;
	Object *copy_button, *cut_button, *paste_button,*undo_button,*redo_button;
	Object *text_texteditor, *xcursor_text, *ycursor_text, *slider;
	Object *datatype_datatypes;
	Object *expand_to_button;
	Object *attach_tree, *add_text_button, *add_multipart_button, *add_files_button, *remove_button;
	Object *contents_page;
	Object *switch_button;
	Object *main_group, *attach_group, *vertical_balance;

	int num;

	for (num=0; num < MAX_COMPOSE_OPEN; num++)
		if (!compose_open[num]) break;

	slider = ScrollbarObject, End;

	wnd = WindowObject,
		(num < MAX_COMPOSE_OPEN)?MUIA_Window_ID:TAG_IGNORE, MAKE_ID('C','O','M',num),
    MUIA_Window_Title, "SimpleMail - Compose Message",
        
		WindowContents, main_group = VGroup,
			Child, ColGroup(2),
				Child, MakeLabel("_To"),
				Child, HGroup,
					MUIA_Group_Spacing,0,
					Child, to_string = AddressStringObject,
						StringFrame,
						MUIA_CycleChain, 1,
						MUIA_ControlChar, 't',
						MUIA_String_AdvanceOnCR, TRUE,
						End,
					Child, expand_to_button = PopButton(MUII_ArrowLeft),
					End,
				Child, MakeLabel("_Subject"),
				Child, subject_string = BetterStringObject,
					StringFrame,
					MUIA_CycleChain, 1,
					MUIA_ControlChar, 's',
					MUIA_String_AdvanceOnCR, TRUE,
					End,
				End,
			Child, contents_page = PageGroup,
				MUIA_Group_ActivePage, 0,
				Child, VGroup,
					Child, HGroup,
						Child, HGroup,
							MUIA_Group_Spacing,0,
							Child, copy_button = MakePictureButton("_Copy","PROGDIR:Images/Copy"),
							Child, cut_button = MakePictureButton("_Cut","PROGDIR:Images/Cut"),
							Child, paste_button = MakePictureButton("_Paste","PROGDIR:Images/Paste"),
							End,
						Child, HGroup,
							MUIA_Weight, 66,
							MUIA_Group_Spacing,0,
							Child, undo_button = MakePictureButton("_Undo","PROGDIR:Images/Undo"),
							Child, redo_button = MakePictureButton("_Redo","PROGDIR:Images/Redo"),
							End,
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
					Child, HGroup,
						MUIA_Group_Spacing, 0,
						Child, text_texteditor = (Object*)ComposeEditorObject,
							InputListFrame,
							MUIA_CycleChain, 1,
							MUIA_TextEditor_Slider, slider,
							MUIA_TextEditor_FixedFont, TRUE,
							End,
						Child, slider,
						End,
					End,
				Child, VGroup,
/*					Child, HVSpace,*/
					Child, datatype_datatypes = DataTypesObject, TextFrame, End,
/*					Child, MakeButton("Test"),
					Child, HVSpace,*/
					End,
				End,
			Child, vertical_balance = BalanceObject, MUIA_ShowMe, FALSE,End,
			Child, attach_group = VGroup,
				MUIA_Weight, 33,
				MUIA_ShowMe, FALSE,
				Child, NListviewObject,
					MUIA_CycleChain, 1,
					MUIA_NListview_NList, attach_tree = AttachmentListObject,
						End,
					End,
				Child, HGroup,
					Child, add_text_button = MakeButton("Add text"),
					Child, add_multipart_button = MakeButton("Add multipart"),
					Child, add_files_button = MakeButton("Add file(s)"),
					Child, MakeButton("Pack & add"),
					Child, remove_button = MakeButton("Remove"),
					End,
				End,
			Child, switch_button = RectangleObject,
				ButtonFrame,
				MUIA_FixHeight,1,
				MUIA_InputMode, MUIV_InputMode_Toggle,
				End,
			Child, HGroup,
				Child, send_later_button = MakeButton("Send later"),
				Child, hold_button = MakeButton("Hold"),
				Child, cancel_button = MakeButton("Cancel"),
				End,
			End,
		End;
	
	if (wnd)
	{
		struct Compose_Data *data = (struct Compose_Data*)malloc(sizeof(struct Compose_Data));
		if (data)
		{
			memset(data,0,sizeof(struct Compose_Data));
			data->wnd = wnd;
			data->num = num;
			data->to_string = to_string;
			data->subject_string = subject_string;
			data->text_texteditor = text_texteditor;
			data->x_text = xcursor_text;
			data->y_text = ycursor_text;
			data->attach_tree = attach_tree;
			data->contents_page = contents_page;
			data->datatype_datatypes = datatype_datatypes;
			data->attach_group = attach_group;
			data->vertical_balance = vertical_balance;
			data->main_group = main_group;
			data->switch_button = switch_button;
			data->copy_button = copy_button;
			data->cut_button = cut_button;
			data->paste_button = paste_button;
			data->undo_button = undo_button;
			data->redo_button = redo_button;

			data->file_req = MUI_AllocAslRequestTags(ASL_FileRequest, TAG_DONE);

			/* mark the window as opened */
			compose_open[num] = 1;

			DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, compose_window_close, data);
			DoMethod(expand_to_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, compose_expand_to, data);
			DoMethod(text_texteditor, MUIM_Notify, MUIA_TextEditor_CursorX, MUIV_EveryTime, xcursor_text, 4, MUIM_SetAsString, MUIA_Text_Contents, "%04ld", MUIV_TriggerValue);
			DoMethod(text_texteditor, MUIM_Notify, MUIA_TextEditor_CursorY, MUIV_EveryTime, ycursor_text, 4, MUIM_SetAsString, MUIA_Text_Contents, "%04ld", MUIV_TriggerValue);
			DoMethod(add_text_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, compose_add_text, data);
			DoMethod(add_multipart_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, compose_add_multipart, data);
			DoMethod(add_files_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, compose_add_files, data);
			DoMethod(remove_button, MUIM_Notify, MUIA_Pressed, FALSE, attach_tree, 4, MUIM_NListtree_Remove, MUIV_NListtree_Remove_ListNode_Active, MUIV_NListtree_Remove_TreeNode_Active, 0);
			DoMethod(attach_tree, MUIM_Notify, MUIA_NListtree_Active, MUIV_EveryTime, attach_tree, 4, MUIM_CallHook, &hook_standard, compose_attach_active, data);
			DoMethod(switch_button, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, switch_button, 4, MUIM_CallHook, &hook_standard, compose_switch_view, data);
			DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, compose_window_close, data);
			DoMethod(hold_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, compose_window_hold, data);
			DoMethod(send_later_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, compose_window_send_later, data);
			DoMethod(copy_button,MUIM_Notify, MUIA_Pressed, FALSE, text_texteditor, 2, MUIM_TextEditor_ARexxCmd,"Copy");
			DoMethod(cut_button,MUIM_Notify, MUIA_Pressed, FALSE, text_texteditor, 2, MUIM_TextEditor_ARexxCmd,"Cut");
			DoMethod(paste_button,MUIM_Notify, MUIA_Pressed, FALSE, text_texteditor, 2, MUIM_TextEditor_ARexxCmd,"Paste");
			DoMethod(undo_button,MUIM_Notify, MUIA_Pressed, FALSE, text_texteditor, 2, MUIM_TextEditor_ARexxCmd,"Undo");
			DoMethod(redo_button,MUIM_Notify, MUIA_Pressed, FALSE, text_texteditor, 2 ,MUIM_TextEditor_ARexxCmd,"Redo");
			DoMethod(App,OM_ADDMEMBER,wnd);

			if (!tochange)
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

			if (tochange)
			{
				/* A mail should be changed */
				int entries;
				char *to;
				char *decoded_to = NULL;

				compose_add_mail(data,tochange,NULL);

				entries = xget(attach_tree,MUIA_NList_Entries);

				if (entries==0)
				{
					compose_add_text(&data);
				} else
				{
					/* Active the first entry if there is only one entry */
					if (entries==1) set(attach_tree,MUIA_NList_Active,0);
					else
					{
						set(attach_tree,MUIA_NList_Active,1);
					}
				}

				if ((to = mail_find_header_contents(tochange,"To")))
				{
					/* set the To string */
					parse_text_string(to,&decoded_to);
				}

				set(subject_string,MUIA_String_Contents,tochange->subject);
				set(to_string,MUIA_String_Contents,decoded_to);

				set(wnd,MUIA_Window_ActiveObject, data->text_texteditor);
				if (decoded_to) free(decoded_to);

				if (tochange->filename) data->filename = strdup(tochange->filename);
				data->folder = strdup("Outgoing");
				data->reply_id = mystrdup(tochange->message_reply_id);
			}

			set(wnd,MUIA_Window_Open,TRUE);

			return;
		}
		MUI_DisposeObject(wnd);
	}
}
