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
#include <mui/betterstring_mcc.h> /* there also exists new newer version of this class */
#include <mui/texteditor_mcc.h>
#include <clib/alib_protos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "account.h"
#include "addressbook.h"
#include "codecs.h"
#include "codesets.h"
#include "configuration.h"
#include "taglines.h"
#include "folder.h"
#include "mail.h"
#include "parse.h"
#include "signature.h"
#include "simplemail.h"
#include "smintl.h"
#include "support_indep.h"
#include "debug.h"

#include "accountpopclass.h"
#include "addressstringclass.h"
#include "amigasupport.h"
#include "attachmentlistclass.h"
#include "composeeditorclass.h"
#include "compiler.h"
#include "composewnd.h"
#include "datatypesclass.h"
#include "mainwnd.h" /* main_refresh_mail() */
#include "muistuff.h"
#include "picturebuttonclass.h"
#include "support.h"
#include "utf8stringclass.h"

struct MUI_NListtree_TreeNode *FindListtreeUserData(Object *tree, APTR udata); /* in mainwnd.c */

#define MAX_COMPOSE_OPEN 10
static struct Compose_Data *compose_open[MAX_COMPOSE_OPEN];

struct Compose_Data /* should be a customclass */
{
	Object *wnd;
	Object *headers_group;
	Object *from_label;
	Object *from_group;
	Object *from_accountpop;
	Object *reply_label;
	Object *reply_button;
	Object *reply_string;
	Object *to_label;
	Object *to_group;
	Object *to_string;
	Object *cc_label;
	Object *cc_group;
	Object *cc_string;
	Object *subject_label;
	Object *subject_string;
	Object *copy_button;
	Object *cut_button;
	Object *paste_button;
	Object *undo_button;
	Object *redo_button;
	Object *x_text;
	Object *y_text;
	Object *text_texteditor;
	Object *quick_attach_tree;
	Object *attach_tree;
	Object *attach_desc_string;
	Object *contents_page;
	Object *datatype_datatypes;
	Object *encrypt_button;
	Object *sign_button;

	int reply_stuff_attached;

	char *filename; /* the emails filename if changed */
	char *folder; /* the emails folder if changed */
	char *reply_id; /* the emails reply-id if changed */
	int compose_action;
	struct mail *ref_mail; /* the mail which status should be changed after editing */

	struct FileRequester *file_req;

	struct attachment *last_attachment;

	int num; /* the number of the window */
	/* more to add */

	char **sign_array; /* The array which contains the signature names */
	int sign_array_utf8count; /* The count of the array to free the memory */

	int attachment_unique_id;
};

/******************************************************************
 Convert the contents of the from popup to the text field
*******************************************************************/
STATIC ASM SAVEDS VOID from_objstr(REG(a0,struct Hook *h),REG(a2,Object *list), REG(a1,Object *str))
{
	struct account *ac;
	char buf[256];

	DoMethod(list,MUIM_NList_GetEntry,MUIV_NList_GetEntry_Active,&ac);
	if (!ac) return;

	if (ac->name)
	{
		if (needs_quotation(ac->name))
			sprintf(buf, "\"%s\"",ac->name);
		else strcpy(buf,ac->name);
	}

	sprintf(buf+strlen(buf)," <%s> (%s)",ac->email, ac->smtp->name);

	set(str,MUIA_Text_Contents,buf);
	set(str,MUIA_UserData,ac);
}

STATIC ASM SAVEDS struct account *from_construct(REG(a0,struct Hook *h),REG(a2,APTR pool), REG(a1, struct account *ent))
{
	return account_duplicate(ent);
}

STATIC ASM SAVEDS VOID from_destruct(REG(a0,struct Hook *h),REG(a2,APTR pool), REG(a1,struct account *ent))
{
	account_free(ent);
}

STATIC ASM SAVEDS VOID from_display(REG(a0,struct Hook *h), REG(a2, char **array), REG(a1,struct account *account))
{
	if (account)
	{
			*array++ = account->email;
			*array++ = account->smtp->name;
	}
}

/******************************************************************
 This close and disposed the window (note: this must not be called
 within a normal callback hook (because the object is disposed in
 this function))!
*******************************************************************/
static void compose_window_dispose(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	set(data->wnd,MUIA_Window_Open,FALSE);
	DoMethod(App,OM_REMMEMBER,data->wnd);
	set(data->datatype_datatypes, MUIA_DataTypes_FileName, NULL);
	if (data->reply_stuff_attached == 0)
	{
		MUI_DisposeObject(data->reply_label);
		MUI_DisposeObject(data->reply_string);
	}
	MUI_DisposeObject(data->wnd);
	if (data->file_req) MUI_FreeAslRequest(data->file_req);
	if (data->filename) free(data->filename);
	if (data->folder) free(data->folder);
	if (data->reply_id) free(data->reply_id);
	if (data->num < MAX_COMPOSE_OPEN) compose_open[data->num] = 0;
	if (data->sign_array)
	{
		int i;
		for (i=0;i<data->sign_array_utf8count;i++)
		{
			free(data->sign_array[i]);
		}
		free(data->sign_array);
	}
	free(data);
}

/******************************************************************
 Expand the to string. Returns 1 for a success else 0
*******************************************************************/
static int compose_expand_to(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	char *to_contents = (char*)xget(data->to_string, MUIA_UTF8String_Contents);

	if (to_contents && *to_contents)
	{
		char *str;
		if ((str = addressbook_get_expand_str(to_contents)))
		{
			set(data->to_string, MUIA_UTF8String_Contents, str);
			free(str);
			return 1;
		}
		DisplayBeep(NULL);
		set(data->wnd, MUIA_Window_ActiveObject,data->to_string);
		return 0;
	}
	return 1;
}

/******************************************************************
 Expand the to string. Returns 1 for a success else 0
*******************************************************************/
static int compose_expand_cc(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	char *cc_contents = (char*)xget(data->cc_string, MUIA_UTF8String_Contents);

	if (cc_contents && *cc_contents)
	{
		char *str;
		if ((str = addressbook_get_expand_str(cc_contents)))
		{
			set(data->cc_string, MUIA_UTF8String_Contents, str);
			free(str);
			return 1;
		}
		DisplayBeep(NULL);
		set(data->wnd, MUIA_Window_ActiveObject,data->cc_string);
		return 1;
	}
	return 1;
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
	int act;

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
		if ((treenode = (struct MUI_NListtree_TreeNode *)xget(data->attach_tree, MUIA_NListtree_Active)))
		{
			/* we have another entry inside but no list (multipart), so insert also a multipart node */
			struct attachment multipart;

			memset(&multipart, 0, sizeof(multipart));
			multipart.content_type = "multipart/mixed";
			multipart.unique_id = data->attachment_unique_id++;

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

	act = !xget(data->attach_tree, MUIA_NListtree_Active);

	treenode = (struct MUI_NListtree_TreeNode *)DoMethod(data->attach_tree, MUIM_NListtree_Insert, "" /*name*/, attach, /* udata */
					 insertlist,MUIV_NListtree_Insert_PrevNode_Tail, (list?(TNF_OPEN|TNF_LIST):(act?MUIV_NListtree_Insert_Flag_Active:0)));

	/* for the quick attachments list */
	if (!list && treenode)
	{
		DoMethod(data->quick_attach_tree, MUIM_NListtree_Insert, "", attach, /* udata */
					MUIV_NListtree_Insert_ListNode_Root, MUIV_NListtree_Insert_PrevNode_Tail, (act?MUIV_NListtree_Insert_Flag_Active:0));
	}
					

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
	attach.unique_id = data->attachment_unique_id++;

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
	attach.unique_id = data->attachment_unique_id++;

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
						attach.unique_id = data->attachment_unique_id++;

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
 Add files to the list
*******************************************************************/
static void compose_remove_file(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	struct MUI_NListtree_TreeNode *treenode = (struct MUI_NListtree_TreeNode*)DoMethod(data->attach_tree, MUIM_NListtree_GetEntry, MUIV_NListtree_GetEntry_ListNode_Active, MUIV_NListtree_GetEntry_Position_Active,0);
	int rem, id;

  if (!treenode) return;

  id = ((struct attachment*)treenode->tn_User)->unique_id;

	rem = DoMethod(data->attach_tree, MUIM_NListtree_GetNr, treenode, MUIV_NListtree_GetNr_Flag_CountLevel) == 2;

	treenode = (struct MUI_NListtree_TreeNode*)DoMethod(data->attach_tree, MUIM_NListtree_GetEntry, treenode, MUIV_NListtree_GetEntry_Position_Parent,0);

	if (treenode && rem) set(data->attach_tree, MUIA_NListtree_Quiet,TRUE);
	DoMethod(data->attach_tree, MUIM_NListtree_Remove, MUIV_NListtree_Remove_ListNode_Active, MUIV_NListtree_Remove_TreeNode_Active, 0);

	if (treenode && rem)
	{
		DoMethod(data->attach_tree, MUIM_NListtree_Move, treenode, MUIV_NListtree_Move_OldTreeNode_Head, MUIV_NListtree_Move_NewListNode_Root, MUIV_NListtree_Move_NewTreeNode_Head, 0); 
		DoMethod(data->attach_tree, MUIM_NListtree_Remove, MUIV_NListtree_Remove_ListNode_Root, treenode, 0); 
		set(data->attach_tree, MUIA_NListtree_Active, MUIV_NListtree_Active_First); 
		set(data->attach_tree, MUIA_NListtree_Quiet,FALSE);
	}

  treenode = (struct MUI_NListtree_TreeNode*)DoMethod(data->quick_attach_tree, MUIM_AttachmentList_FindUniqueID, id);
  if (treenode)
  {
		DoMethod(data->quick_attach_tree, MUIM_NListtree_Remove, MUIV_NListtree_Remove_ListNode_Root, treenode, 0);
  }
  
}

/******************************************************************
 A new attachment has been clicked
*******************************************************************/
static void compose_quick_attach_active(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	struct MUI_NListtree_TreeNode *activenode = (struct MUI_NListtree_TreeNode *)xget(data->quick_attach_tree, MUIA_NListtree_Active);

	if (activenode && activenode->tn_User)
	{
		if ((activenode = (struct MUI_NListtree_TreeNode*)DoMethod(data->attach_tree, MUIM_AttachmentList_FindUniqueID, ((struct attachment *)activenode->tn_User)->unique_id)))
		{
			SetAttrs(data->attach_tree,
					MUIA_NListtree_Active, activenode,
					TAG_DONE);
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
				set(data->text_texteditor, MUIA_TextEditor_ImportHook, MUIV_TextEditor_ImportHook_EMail);
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
			set(data->attach_desc_string, MUIA_String_Contents, attach->description);
		}

		if ((activenode = (struct MUI_NListtree_TreeNode*)DoMethod(data->quick_attach_tree, MUIM_AttachmentList_FindUniqueID, attach->unique_id)))
		{
			SetAttrs(data->quick_attach_tree,
					MUIA_NoNotify, TRUE,
					MUIA_NListtree_Active,activenode,
					TAG_DONE);
		}
	} else
	{
/*		set(data->contents_page, MUIA_Disabled, TRUE);*/
	}
	data->last_attachment = attach;
}

/******************************************************************
 Attach the mail given in the treenode to the current mail
*******************************************************************/
static void compose_attach_desc(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	struct MUI_NListtree_TreeNode *activenode = (struct MUI_NListtree_TreeNode *)xget(data->attach_tree, MUIA_NListtree_Active);
	struct attachment *attach = NULL;

	if (activenode)
	{
		if ((attach = (struct attachment *)activenode->tn_User))
		{
			free(attach->description);
			attach->description = mystrdup((char*)xget(data->attach_desc_string,MUIA_String_Contents));
			DoMethod(data->attach_tree, MUIM_NListtree_Redraw, MUIV_NListtree_Redraw_Active, 0);
		}
	}
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

	cmail->content_type = mystrdup(attach->content_type);
	cmail->content_description = utf8create(attach->description,user.config.default_codeset?user.config.default_codeset->name:NULL);

	if (treenode->tn_Flags & TNF_LIST)
	{
		struct MUI_NListtree_TreeNode *tn = (struct MUI_NListtree_TreeNode *)DoMethod(data->attach_tree,
				MUIM_NListtree_GetEntry, treenode, MUIV_NListtree_GetEntry_Position_Head, 0);


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
		cmail->text = (attach->contents)?utf8create(attach->contents,user.config.default_codeset?user.config.default_codeset->name:NULL):NULL;
		cmail->content_filename = (attach->filename)?utf8create(sm_file_part(attach->filename),user.config.default_codeset?user.config.default_codeset->name:NULL):NULL;
		cmail->filename = mystrdup(attach->filename);
		cmail->temporary_filename = mystrdup(attach->temporary_filename);
	}
}

/******************************************************************
 Compose a mail and close the window
*******************************************************************/
static void compose_mail(struct Compose_Data *data, int hold)
{
	if (compose_expand_to(&data) && compose_expand_cc(&data))
	{
		struct account *account;
		char from[200];
		char *to = (char*)xget(data->to_string, MUIA_UTF8String_Contents);
		char *cc = (char*)xget(data->cc_string, MUIA_UTF8String_Contents);
		char *replyto = (char*)xget(data->reply_string, MUIA_UTF8String_Contents);
		char *subject = (char*)xget(data->subject_string, MUIA_UTF8String_Contents);
		struct composed_mail new_mail;

		from[0] = 0;
		account = (struct account*)xget(data->from_accountpop,MUIA_AccountPop_Account);
		if (account)
		{
			char *fmt;
			if (account->email)
			{
				if (account->name && account->name[0])
				{
					if (needs_quotation(account->name)) fmt = "\"%s\" <%s>";
					else fmt = "%s <%s>";
					sm_snprintf(from,sizeof(from),fmt,account->name,account->email);
				} else
				{
					mystrlcpy(from,account->email,sizeof(from));
				}
			}
		}

		/* update the current attachment */
		compose_attach_active(&data);

		/* Initialize the structure with default values */
		composed_mail_init(&new_mail);

		/* Attach the mails recursivly */
		compose_window_attach_mail(data, NULL /*root*/, &new_mail);

		/* TODO: free this stuff!! */

		new_mail.from = from[0]?from:NULL;
		new_mail.replyto = replyto;
		new_mail.to = to;
		new_mail.cc = cc;
		new_mail.subject = subject;
		new_mail.mail_filename = data->filename;
		new_mail.mail_folder = data->folder;
		new_mail.reply_message_id = data->reply_id;
		new_mail.encrypt = xget(data->encrypt_button,MUIA_Selected);
		new_mail.sign = xget(data->sign_button,MUIA_Selected);

		/* Move this out */
		if ((mail_compose_new(&new_mail,hold)))
		{
			/* Change the status of a mail if it was replied or forwarded */
			if (data->ref_mail && mail_get_status_type(data->ref_mail) != MAIL_STATUS_SENT
												 && mail_get_status_type(data->ref_mail) != MAIL_STATUS_WAITSEND)
			{
				if (data->compose_action == COMPOSE_ACTION_REPLY)
				{
					struct folder *f = folder_find_by_mail(data->ref_mail);
					if (f)
					{
						folder_set_mail_status(f, data->ref_mail, MAIL_STATUS_REPLIED|(data->ref_mail->status & MAIL_STATUS_FLAG_MARKED));
						main_refresh_mail(data->ref_mail);
					}
				} else
				{
					if (data->compose_action == COMPOSE_ACTION_FORWARD)
					{
						struct folder *f = folder_find_by_mail(data->ref_mail);
						folder_set_mail_status(f, data->ref_mail, MAIL_STATUS_FORWARD|(data->ref_mail->status & MAIL_STATUS_FLAG_MARKED));
						main_refresh_mail(data->ref_mail);
					}
				}
			}
			/* Close (and dispose) the compose window (data) */
			DoMethod(App, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, compose_window_dispose, data);
		}
	}
}

/******************************************************************
 The mail should be send immediatly
*******************************************************************/
static void compose_window_send_now(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	compose_mail(data,2);
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
	attach.unique_id = data->attachment_unique_id++;

	if (mail->content_description && *mail->content_description)
	{
		int len = strlen(mail->content_description)+1;
		if ((attach.description = malloc(len)))
		{
			utf8tostr(mail->content_description, attach.description, len, user.config.default_codeset);
		}
	}

	if (!num_multiparts)
	{
		void *cont;
		int cont_len;

		/* decode the mail */
		mail_decode(mail);

		mail_decoded_data(mail,&cont,&cont_len);

		/* if the content type is a text it can be edited */
		if (!mystricmp(buf,"text/plain"))
		{
			char *isobuf = NULL;
			char *cont_dup = mystrndup((char*)cont,cont_len); /* we duplicate this only because we need a null byte */

			if (cont_dup)
			{
				if ((isobuf = (char*)malloc(cont_len+1)))
					utf8tostr(cont_dup, isobuf, cont_len+1, user.config.default_codeset);
				free(cont_dup);
			}

			attach.contents = isobuf;
			attach.editable = 1;
			attach.lastxcursor = 0;
			attach.lastycursor = 0;
		} else
		{
			BPTR fh;

			tmpnam(tmpname);

			if ((fh = Open(tmpname,MODE_NEWFILE)))
			{
				Write(fh,cont,cont_len);
				Close(fh);
			}
			attach.filename = mail->content_name?mail->content_name:tmpname;
			attach.temporary_filename = tmpname;
		}
	}

	if (!num_multiparts)
	{
		DoMethod(data->quick_attach_tree,MUIM_NListtree_Insert,"",&attach,
						 MUIV_NListtree_Insert_ListNode_Root,
						 MUIV_NListtree_Insert_PrevNode_Tail,0);
	}

	treenode = (struct MUI_NListtree_TreeNode *)DoMethod(data->attach_tree,MUIM_NListtree_Insert,"",&attach,listnode,MUIV_NListtree_Insert_PrevNode_Tail,num_multiparts?(TNF_LIST|TNF_OPEN):0);

	free(attach.contents);
	free(attach.description);

	if (!treenode) return;

	for (i=0;i<num_multiparts; i++)
	{
		compose_add_mail(data,mail->multipart_array[i],treenode);
	}
}

/******************************************************************
 Add a signature if neccesary
*******************************************************************/
static void compose_add_signature(struct Compose_Data *data)
{
	struct signature *sign = (struct signature*)list_first(&user.config.signature_list);
	if (user.config.signatures_use && sign && sign->signature)
	{
		char *text = (char*)DoMethod(data->text_texteditor, MUIM_TextEditor_ExportText);
		int add_sign = 0;

		if (text)
		{
			add_sign = strstr(text,"\n-- \n")?(0):(!!strncmp("-- \n",text,4));
		}
		if (add_sign)
		{
			char *sign_iso = utf8tostrcreate(sign->signature,user.config.default_codeset);
			if (sign_iso)
			{
				char *new_text = (char*)malloc(strlen(text) + strlen(sign_iso) + 50);
				if (new_text)
				{
					strcpy(new_text,text);
					strcat(new_text,"\n-- \n");
					strcat(new_text,sign_iso);

					new_text = taglines_add_tagline(new_text);
				
/*
					DoMethod(data->text_texteditor,MUIM_TextEditor_InsertText,"\n-- \n", MUIV_TextEditor_InsertText_Bottom);
					DoMethod(data->text_texteditor,MUIM_TextEditor_InsertText,sign->signature, MUIV_TextEditor_InsertText_Bottom);
*/
					SetAttrs(data->text_texteditor,
							MUIA_TextEditor_CursorX,0,
							MUIA_TextEditor_CursorY,0,
							MUIA_TextEditor_Contents,new_text,
							TAG_DONE);

					free(new_text);
				}
				free(sign_iso);
			}
		}

		if (text) FreeVec(text);
	}
}

/******************************************************************
 Set a signature
*******************************************************************/
static void compose_set_signature(void **msg)
{
	struct Compose_Data *data = (struct Compose_Data*)msg[0];
	int val = (int)msg[1];
	struct signature *sign = (struct signature*)list_find(&user.config.signature_list,val);
	char *text;
	int x = xget(data->text_texteditor,MUIA_TextEditor_CursorX);
	int y = xget(data->text_texteditor,MUIA_TextEditor_CursorY);

	if (!sign)
	{
		if ((text = (char*)DoMethod(data->text_texteditor, MUIM_TextEditor_ExportText)))
		{
			char *sign_text = strstr(text,"\n-- \n");
			if (sign_text)
			{
				*sign_text = 0;

				SetAttrs(data->text_texteditor,
						MUIA_TextEditor_Contents,text,
						MUIA_TextEditor_CursorX,x,
						MUIA_TextEditor_CursorY,y,
						TAG_DONE);
			}
			FreeVec(text);
		}
		return;
	}
	if (!sign->signature) return;

	if ((text = (char*)DoMethod(data->text_texteditor, MUIM_TextEditor_ExportText)))
	{
		char *sign_text = strstr(text,"\n-- \n");
		char *sign_iso = utf8tostrcreate(sign->signature,user.config.default_codeset);
		char *new_text;

		if (sign_text) *sign_text = 0;

		if (sign_iso)
		{
			if ((new_text = (char*)malloc(strlen(text)+strlen(sign_iso)+10)))
			{
				strcpy(new_text,text);
				strcat(new_text,"\n-- \n");
				strcat(new_text,sign_iso);

				new_text = taglines_add_tagline(new_text);

				SetAttrs(data->text_texteditor,
						MUIA_TextEditor_Contents,new_text,
						MUIA_TextEditor_CursorX,x,
						MUIA_TextEditor_CursorY,y,
						TAG_DONE);

				free(new_text);
			}
			free(sign_iso);
		}
		FreeVec(text);
	}
}

/******************************************************************
 Set the reply to the current selected account
*******************************************************************/
static void compose_set_replyto(void **msg)
{
	struct Compose_Data *data = (struct Compose_Data*)msg[0];
	struct account *ac = (struct account*)xget(data->from_accountpop, MUIA_AccountPop_Account);
	if (ac)
	{
		set(data->reply_string, MUIA_UTF8String_Contents, ac->reply);
		if (ac->reply && *ac->reply) set(data->reply_button,MUIA_Selected,TRUE);
	}
}

/******************************************************************
 Called when the reply button is clicked
*******************************************************************/
static void compose_reply_button(void **msg)
{
	struct Compose_Data *data = (struct Compose_Data*)msg[0];
	int reply_selected;
	Object *group = data->headers_group;
	if (!group) return;
	reply_selected = xget(data->reply_button,MUIA_Selected);

	DoMethod(group,MUIM_Group_InitChange);
	if (data->reply_stuff_attached && !reply_selected)
	{
		DoMethod(group,OM_REMMEMBER,data->reply_label);
		DoMethod(group,OM_REMMEMBER,data->reply_string);
		data->reply_stuff_attached = 0;
	} else
	{
		if (!data->reply_stuff_attached && reply_selected)
		{
			DoMethod(group,OM_ADDMEMBER,data->reply_label);
			DoMethod(group,OM_ADDMEMBER,data->reply_string);
			DoMethod(group,MUIM_Group_Sort,data->from_label,data->from_group,
																		 data->reply_label,data->reply_string,
																		 data->to_label,data->to_group,
																		 data->cc_label,data->cc_group,
																		 data->subject_label,data->subject_string,NULL);
			data->reply_stuff_attached = 1;
		}
	}
	DoMethod(group,MUIM_Group_ExitChange);
}

/******************************************************************
 New Gadget should be activated
*******************************************************************/
static void compose_new_active(void **msg)
{
	struct Compose_Data *data = (struct Compose_Data *)msg[0];
	Object *obj = (Object*)msg[1];

	if ((ULONG)obj == (ULONG)MUIV_Window_ActiveObject_Next &&
		  (ULONG)xget(data->wnd, MUIA_Window_ActiveObject) == (ULONG)data->copy_button)
	{
		set(data->wnd, MUIA_Window_ActiveObject, data->text_texteditor);
	}
}

/******************************************************************
 Opens a compose window
*******************************************************************/
int compose_window_open(struct compose_args *args)
{
	Object *wnd, *send_later_button, *hold_button, *cancel_button, *send_now_button, *headers_group;
	Object *from_label, *from_group, *from_accountpop, *reply_button, *reply_label, *reply_string, *to_label, *to_group, *to_string, *cc_label, *cc_group, *cc_string, *subject_label, *subject_string;
	Object *copy_button, *cut_button, *paste_button,*undo_button,*redo_button;
	Object *text_texteditor, *xcursor_text, *ycursor_text, *slider;
	Object *datatype_datatypes;
	Object *expand_to_button, *expand_cc_button;
	Object *quick_attach_tree;
	Object *attach_tree, *attach_desc_string, *add_text_button, *add_multipart_button, *add_files_button, *remove_button;
	Object *contents_page;
	Object *signatures_group;
	Object *signatures_cycle;
	Object *add_attach_button;
	Object *encrypt_button;
	Object *sign_button;

	struct signature *sign;
	char **sign_array = NULL;
	int sign_array_utf8count = 0;
	int num;
	int i;

	static char *register_titles[3];
	static int register_titles_are_translated;

	if (!register_titles_are_translated)
	{
		register_titles[0] = _("Mail");
		register_titles[1] = _("Attachments");
		register_titles_are_translated = 1;
	}

	/* Find out if window is already open */
	if (args->to_change && args->to_change->filename)
	{
		for (num=0; num < MAX_COMPOSE_OPEN; num++)
		{
			if (compose_open[num])
			{
				if (!mystricmp(args->to_change->filename, compose_open[num]->filename))
				{
					DoMethod(compose_open[num]->wnd, MUIM_Window_ToFront);
					set(compose_open[num]->wnd, MUIA_Window_Activate, TRUE);
					return num;
				}
			}
		}
	}

	for (num=0; num < MAX_COMPOSE_OPEN; num++)
		if (!compose_open[num]) break;

	if (num == MAX_COMPOSE_OPEN) return -1;

	i = list_length(&user.config.signature_list);

	if (user.config.signatures_use && i)
	{
		if ((sign_array = (char**)malloc((i+2)*sizeof(char*))))
		{
			int j=0;
			sign = (struct signature*)list_first(&user.config.signature_list);
			while (sign)
			{
				sign_array[j]=utf8tostrcreate(sign->name, user.config.default_codeset);
				sign = (struct signature*)node_next(&sign->node);
				j++;
			}
			sign_array_utf8count=j;
			sign_array[j] = _("No Signature");
			sign_array[j+1] = NULL;
		}

		signatures_group = HGroup,
			MUIA_Weight, 33,
			Child, MakeLabel(_("Use signature")),
			Child, signatures_cycle = MakeCycle(_("Use signature"),sign_array),
			End;
	} else
	{
		signatures_group = NULL;
		signatures_cycle = NULL;
	}

	slider = ScrollbarObject, End;

	wnd = WindowObject,
		MUIA_HelpNode, "WR_W",
		(num < MAX_COMPOSE_OPEN)?MUIA_Window_ID:TAG_IGNORE, MAKE_ID('C','O','M',num),
		MUIA_Window_Title, _("SimpleMail - Compose Message"),
		  
		WindowContents, VGroup,
			Child, RegisterGroup(register_titles),
				/* First register */
				Child, VGroup,
					Child, HGroup,
						Child, headers_group = ColGroup(2),
							Child, from_label = MakeLabel(_("_From")),
							Child, from_group = HGroup,
								MUIA_Group_Spacing,0,
								Child, from_accountpop = AccountPopObject, End,
								Child, reply_button = TextObject, ButtonFrame, MUIA_Selected,1, MUIA_InputMode, MUIV_InputMode_Toggle,MUIA_Text_PreParse, "\033c",MUIA_Text_Contents,_("R"), MUIA_Text_SetMax, TRUE, End,
								End,

							Child, reply_label = MakeLabel(_("_Replies To")),
							Child, reply_string = AddressStringObject,
								StringFrame,
								MUIA_CycleChain,1,
								MUIA_ControlChar, GetControlChar(_("_Replies To")),
								MUIA_String_AdvanceOnCR, TRUE,
								End,

							Child, to_label = MakeLabel(_("_To")),
							Child, to_group = HGroup,
								MUIA_Group_Spacing,0,
								Child, to_string = AddressStringObject,
									StringFrame,
									MUIA_CycleChain, 1,
									MUIA_ControlChar, GetControlChar(_("_To")),
									MUIA_String_AdvanceOnCR, TRUE,
									End,
								Child, expand_to_button = PopButton(MUII_ArrowLeft),
								End,

							Child, cc_label = MakeLabel(_("C_opies To")),
							Child, cc_group = HGroup,
								MUIA_Group_Spacing,0,
								Child, cc_string = AddressStringObject,
									StringFrame,
									MUIA_CycleChain, 1,
									MUIA_ControlChar, GetControlChar(_("C_opies To")),
									MUIA_String_AdvanceOnCR, TRUE,
									End,
								Child, expand_cc_button = PopButton(MUII_ArrowLeft),
								End,

							Child, subject_label = MakeLabel(_("S_ubject")),
							Child, subject_string = UTF8StringObject,
								StringFrame,
								MUIA_CycleChain, 1,
								MUIA_ControlChar, GetControlChar(_("S_ubject")),
								End,
							End,
						Child, BalanceObject, End,
						Child, NListviewObject,
							MUIA_Weight, 50,
							MUIA_CycleChain, 1,
							MUIA_NListview_NList, quick_attach_tree = AttachmentListObject,
								MUIA_AttachmentList_Quick, TRUE,
								End,
							End,
						End,
					Child, contents_page = PageGroup,
						MUIA_Group_ActivePage, 0,
						Child, VGroup,
							Child, HGroup,
								MUIA_VertWeight,0,
								Child, HGroup,
									MUIA_Group_Spacing,0,
									Child, copy_button = MakePictureButton(_("Copy"),"PROGDIR:Images/Copy"),
									Child, cut_button = MakePictureButton(_("Cut"),"PROGDIR:Images/Cut"),
									Child, paste_button = MakePictureButton(_("Paste"),"PROGDIR:Images/Paste"),
									End,
								Child, HGroup,
									MUIA_Weight, 66,
									MUIA_Group_Spacing,0,
									Child, undo_button = MakePictureButton(_("Undo"),"PROGDIR:Images/Undo"),
									Child, redo_button = MakePictureButton(_("Redo"),"PROGDIR:Images/Redo"),
									End,
								Child, HGroup,
									MUIA_Weight, 33,
									MUIA_Group_Spacing,0,
									Child, add_attach_button = MakePictureButton(_("_Attach"),"PROGDIR:Images/AddAttachment"),
									End,
								Child, HGroup,
									MUIA_Weight, 33,
									MUIA_Group_Spacing,0,
									Child, encrypt_button = MakePictureButton(_("_Encrypt"),"PROGDIR:Images/Encrypt"),
									Child, sign_button = MakePictureButton(_("Si_gn"),"PROGDIR:Images/Sign"),
									End,
								Child, RectangleObject,
									MUIA_FixHeight,1,
									MUIA_HorizWeight,signatures_group?33:100,
									End,
								signatures_group?Child:TAG_IGNORE,signatures_group,
								signatures_group?Child:TAG_IGNORE,RectangleObject,
									MUIA_FixHeight,1,
									MUIA_HorizWeight,signatures_group?33:100,
									End,
								Child, VGroup,
									TextFrame,
									MUIA_Background, MUII_TextBack,
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
									MUIA_TextEditor_WrapBorder, user.config.write_wrap_type == 1 ? user.config.write_wrap : 0,
									End,
								Child, slider,
								End,
							End,
						Child, VGroup,
							Child, datatype_datatypes = DataTypesObject, TextFrame, End,
							End,
						End,
					End,

				/* New register page */
				Child, VGroup,
					Child, NListviewObject,
						MUIA_CycleChain, 1,
						MUIA_NListview_NList, attach_tree = AttachmentListObject,
							End,
						End,
					Child, HGroup,
						Child, MakeLabel(_("Description")),
						Child, attach_desc_string = BetterStringObject,
							StringFrame,
							MUIA_ControlChar, GetControlChar(_("Description")),
							End,
						End,
					Child, HGroup,
						Child, add_text_button = MakeButton(_("Add text")),
						Child, add_multipart_button = MakeButton(_("Add multipart")),
						Child, add_files_button = MakeButton(_("Add file(s)")),
						Child, remove_button = MakeButton(_("Remove")),
						End,
					End,
				End,
			Child, HGroup,
				Child, send_now_button = MakeButton(_("_Send now")),
				Child, send_later_button = MakeButton(_("Send _later")),
				Child, hold_button = MakeButton(_("_Hold")),
				Child, cancel_button = MakeButton(_("_Cancel")),
				End,
			End,
		End;
	
	if (wnd)
	{
		struct Compose_Data *data = (struct Compose_Data*)malloc(sizeof(struct Compose_Data));
		if (data)
		{
			set(sign_button,MUIA_ShowMe, FALSE); /* temporary not shown because not implemented */

			memset(data,0,sizeof(struct Compose_Data));
			data->wnd = wnd;
			data->num = num;
			data->headers_group = headers_group;
			data->from_label = from_label;
			data->from_group = from_group;
			data->from_accountpop = from_accountpop;
			data->to_label = to_label;
			data->to_group = to_group;
			data->to_string = to_string;
			data->cc_label = cc_label;
			data->cc_group = cc_group;
			data->cc_string = cc_string;
			data->reply_string = reply_string;
			data->reply_label = reply_label;
			data->reply_button = reply_button;
			data->subject_label = subject_label;
			data->subject_string = subject_string;
			data->text_texteditor = text_texteditor;
			data->x_text = xcursor_text;
			data->y_text = ycursor_text;
			data->attach_tree = attach_tree;
			data->attach_desc_string = attach_desc_string;
			data->quick_attach_tree = quick_attach_tree;
			data->contents_page = contents_page;
			data->datatype_datatypes = datatype_datatypes;
			data->encrypt_button = encrypt_button;
			data->sign_button = sign_button;
			data->copy_button = copy_button;
			data->cut_button = cut_button;
			data->paste_button = paste_button;
			data->undo_button = undo_button;
			data->redo_button = redo_button;

			set(encrypt_button, MUIA_InputMode, MUIV_InputMode_Toggle);
			set(sign_button, MUIA_InputMode, MUIV_InputMode_Toggle);

			data->file_req = MUI_AllocAslRequestTags(ASL_FileRequest, TAG_DONE);

			/* mark the window as opened */
			compose_open[num] = data;

			DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, compose_window_hold, data);
			DoMethod(expand_to_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, compose_expand_to, data);
			DoMethod(expand_cc_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, compose_expand_cc, data);
			DoMethod(to_string, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime, App, 4, MUIM_CallHook, &hook_standard, compose_expand_to, data);
			DoMethod(cc_string, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime, App, 4, MUIM_CallHook, &hook_standard, compose_expand_cc, data);
			DoMethod(reply_button, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, App, 4, MUIM_CallHook, &hook_standard, compose_reply_button, data);
			DoMethod(text_texteditor, MUIM_Notify, MUIA_TextEditor_CursorX, MUIV_EveryTime, xcursor_text, 4, MUIM_SetAsString, MUIA_Text_Contents, "%04ld", MUIV_TriggerValue);
			DoMethod(text_texteditor, MUIM_Notify, MUIA_TextEditor_CursorY, MUIV_EveryTime, ycursor_text, 4, MUIM_SetAsString, MUIA_Text_Contents, "%04ld", MUIV_TriggerValue);
			DoMethod(add_text_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, compose_add_text, data);
			DoMethod(add_multipart_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, compose_add_multipart, data);
			DoMethod(add_files_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, compose_add_files, data);
			DoMethod(add_attach_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, compose_add_files, data);
			DoMethod(remove_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, compose_remove_file, data);
			DoMethod(attach_tree, MUIM_Notify, MUIA_NListtree_Active, MUIV_EveryTime, attach_tree, 4, MUIM_CallHook, &hook_standard, compose_attach_active, data);
			DoMethod(attach_desc_string, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, App, 4, MUIM_CallHook, &hook_standard, compose_attach_desc, data);
			DoMethod(quick_attach_tree, MUIM_Notify, MUIA_NListtree_Active, MUIV_EveryTime, quick_attach_tree, 4, MUIM_CallHook, &hook_standard, compose_quick_attach_active, data);
			DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, compose_window_dispose, data);
			DoMethod(hold_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, compose_window_hold, data);
			DoMethod(send_now_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, compose_window_send_now, data);
			DoMethod(send_later_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, compose_window_send_later, data);
			DoMethod(copy_button,MUIM_Notify, MUIA_Pressed, FALSE, text_texteditor, 2, MUIM_TextEditor_ARexxCmd,"Copy");
			DoMethod(cut_button,MUIM_Notify, MUIA_Pressed, FALSE, text_texteditor, 2, MUIM_TextEditor_ARexxCmd,"Cut");
			DoMethod(paste_button,MUIM_Notify, MUIA_Pressed, FALSE, text_texteditor, 2, MUIM_TextEditor_ARexxCmd,"Paste");
			DoMethod(undo_button,MUIM_Notify, MUIA_Pressed, FALSE, text_texteditor, 2, MUIM_TextEditor_ARexxCmd,"Undo");
			DoMethod(redo_button,MUIM_Notify, MUIA_Pressed, FALSE, text_texteditor, 2 ,MUIM_TextEditor_ARexxCmd,"Redo");
			DoMethod(subject_string,MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime, wnd, 3, MUIM_Set, MUIA_Window_ActiveObject, text_texteditor);
			DoMethod(wnd, MUIM_Notify, MUIA_Window_ActiveObject, MUIV_EveryTime, App, 5, MUIM_CallHook, &hook_standard, compose_new_active, data, MUIV_TriggerValue);
			DoMethod(signatures_cycle, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, signatures_cycle, 5, MUIM_CallHook, &hook_standard, compose_set_signature, data, MUIV_TriggerValue);
			DoMethod(from_accountpop, MUIM_Notify, MUIA_AccountPop_Account, MUIV_EveryTime, from_accountpop, 4, MUIM_CallHook, &hook_standard, compose_set_replyto, data);

			data->reply_stuff_attached = 1;
			set(data->reply_button,MUIA_Selected,FALSE);

			DoMethod(App,OM_ADDMEMBER,wnd);

			DoMethod(from_accountpop, MUIM_AccountPop_Refresh);

			if (args->to_change)
			{
				/* A mail should be changed */
				int entries;
				char *from, *to, *cc, *reply;

				/* Find and set the correct account */
				if ((from = mail_find_header_contents(args->to_change, "from")))
				{
					struct account *ac = account_find_by_from(from);
					set(from_accountpop, MUIA_AccountPop_Account, ac);
				}

				compose_add_mail(data,args->to_change,NULL);

				entries = xget(attach_tree,MUIA_NList_Entries);
				if (entries==0)
				{
					compose_add_text(&data);
				} else
				{
					/* Active the first entry if there is only one entry */
					if (entries==1) set(attach_tree,MUIA_NList_Active,0);
					else set(attach_tree,MUIA_NList_Active,1);
				}

				if ((to = mail_find_header_contents(args->to_change,"to")))
				{
					/* set the To string */
					char *decoded_to;
					parse_text_string(to,&decoded_to);
					set(to_string,MUIA_UTF8String_Contents,decoded_to);
					free(decoded_to);
				}

				if ((cc = mail_find_header_contents(args->to_change,"cc")))
				{
					/* set the CC string */
					char *decoded_cc;
					parse_text_string(cc,&decoded_cc);
					set(cc_string,MUIA_UTF8String_Contents,decoded_cc);
					free(decoded_cc);
				}

				if ((reply = mail_find_header_contents(args->to_change,"replyto")))
				{
					/* set the ReplyTo string */
					char *decoded_reply;
					parse_text_string(reply,&decoded_reply);
					set(reply_string,MUIA_UTF8String_Contents,decoded_reply);
					free(decoded_reply);
					set(data->reply_button,MUIA_Selected,TRUE);
				}

				set(subject_string,MUIA_UTF8String_Contents,args->to_change->subject);

				if (args->action == COMPOSE_ACTION_REPLY)
					set(wnd,MUIA_Window_ActiveObject, data->text_texteditor);
				else
				{
					if (mystrlen((char*)xget(to_string,MUIA_String_Contents)))
					{
						set(wnd,MUIA_Window_ActiveObject, data->subject_string);
					}
					else set(wnd,MUIA_Window_ActiveObject, data->to_string);
				}

				data->filename = mystrdup(args->to_change->filename);
				data->folder = mystrdup("Outgoing");
				data->reply_id = mystrdup(args->to_change->message_reply_id);
			} else
			{
				compose_add_text(&data);
				/* activate the "To" field */
				set(wnd,MUIA_Window_ActiveObject,data->to_string);
			}

			compose_add_signature(data);
			data->sign_array = sign_array;
			data->sign_array_utf8count = sign_array_utf8count;

			data->compose_action = args->action;
			data->ref_mail = args->ref_mail;

			set(wnd,MUIA_Window_Open,TRUE);

			return num;
		}
		MUI_DisposeObject(wnd);
	}
	return -1;
}

/******************************************************************
 Activate a read window
*******************************************************************/
void compose_window_activate(int num)
{
	if (num < 0 || num >= MAX_COMPOSE_OPEN) return;
	if (compose_open[num] && compose_open[num]->wnd) set(compose_open[num]->wnd,MUIA_Window_Open,TRUE);
}

/******************************************************************
 Closes a read window
*******************************************************************/
void compose_window_close(int num, int action)
{
	if (num < 0 || num >= MAX_COMPOSE_OPEN) return;
	if (compose_open[num] && compose_open[num]->wnd)
	{
		switch (action)
		{
			case COMPOSE_CLOSE_CANCEL: compose_window_dispose(&compose_open[num]); break;
			case COMPOSE_CLOSE_SEND:  compose_mail(compose_open[num],2);
			case COMPOSE_CLOSE_LATER: compose_mail(compose_open[num],0);
			case COMPOSE_CLOSE_HOLD: compose_mail(compose_open[num],1);
		}
	}
}

