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

/**
 * @file composewnd.c
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <libraries/asl.h>
#include <libraries/iffparse.h> /* MAKE_ID */
#include <libraries/mui.h>
#include <mui/NListview_mcc.h>
#include <mui/NListtree_mcc.h>
#include <mui/BetterString_mcc.h> /* there also exists new newer version of this class */
#include <mui/TextEditor_mcc.h>
#include <clib/alib_protos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "account.h"
#include "addressbook.h"
#include "addresslist.h"
#include "codecs.h"
#include "codesets.h"
#include "configuration.h"
#include "debug.h"
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
#include "signaturecycleclass.h"
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
#include "smtoolbarclass.h"
#include "utf8stringclass.h"

enum
{
	SM_COMPOSEWND_BUTTON_COPY = 0,
	SM_COMPOSEWND_BUTTON_CUT,
	SM_COMPOSEWND_BUTTON_PASTE,
	SM_COMPOSEWND_BUTTON_UNDO,
	SM_COMPOSEWND_BUTTON_REDO,
	SM_COMPOSEWND_BUTTON_ATTACH,
	SM_COMPOSEWND_BUTTON_ENCRYPT,
	SM_COMPOSEWND_BUTTON_SIGN
};

static const struct MUIS_SMToolbar_Button sm_composewnd_buttons[] =
{
	{PIC(0,4), SM_COMPOSEWND_BUTTON_COPY,  0, N_("Copy"),  NULL, "Copy"},
	{PIC(0,5), SM_COMPOSEWND_BUTTON_CUT,   0, N_("Cut"),   NULL, "Cut"},
	{PIC(2,8), SM_COMPOSEWND_BUTTON_PASTE, 0, N_("Paste"), NULL, "Paste"},
	{(ULONG)MUIV_SMToolbar_Space},
	{PIC(3,3), SM_COMPOSEWND_BUTTON_UNDO, 0, N_("Undo"), NULL, "Undo"},
	{PIC(3,0), SM_COMPOSEWND_BUTTON_REDO, 0, N_("Redo"), NULL, "Redo"},
	{(ULONG)MUIV_SMToolbar_Space},
	{PIC(0,0), SM_COMPOSEWND_BUTTON_ATTACH, 0, N_("_Attach"), NULL, "AddAttachment"},
	{(ULONG)MUIV_SMToolbar_Space},
	{PIC(0,8), SM_COMPOSEWND_BUTTON_ENCRYPT, MUIV_SMToolbar_ButtonFlag_Toggle, N_("Encrypt"), NULL, "Encrypt"},
	/* signbutton temporary not created because not implemented */
	/* {PIC(3,2), SM_COMPOSEWND_BUTTON_SIGN,    MUIV_SMToolbar_ButtonFlag_Toggle, N_("Si_gn"),   NULL, "Sign"}, */
	{(ULONG)MUIV_SMToolbar_End},
};

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
	Object *bcc_button;
	Object *bcc_label;
	Object *bcc_group;
	Object *bcc_string;
	Object *subject_label;
	Object *subject_string;
	Object *toolbar;
	Object *x_text;
	Object *y_text;
	Object *text_texteditor;
	Object *quick_attach_tree;
	Object *attach_tree;
	Object *attach_desc_string;
	Object *contents_page;
	Object *datatype_datatypes;
	Object *signatures_group;
	Object *signatures_cycle;
	Object *importance_cycle;

	int reply_stuff_attached;
	int bcc_stuff_attached;

	char *filename; /* the emails filename if changed */
	char *folder; /* the emails folder if changed */
	char *reply_id; /* the emails reply-id if changed */
	int compose_action;
	struct mail_info *ref_mail; /* the mail which status should be changed after editing */

	struct FileRequester *file_req;

	struct attachment *last_attachment;

	int num; /* the number of the window */
	/* more to add */

	int attachment_unique_id;
};

/**
 * This close and disposed the window (note: this must not be called
 * within a normal callback hook (because the object is disposed in
 * this function))!
 *
 * @param pdata
 */
static void compose_window_dispose(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	set(data->wnd,MUIA_Window_Open,FALSE);
	DoMethod(App, OM_REMMEMBER, (ULONG)data->wnd);
	set(data->datatype_datatypes, MUIA_DataTypes_FileName, NULL);
	if (data->reply_stuff_attached == 0)
	{
		MUI_DisposeObject(data->reply_label);
		MUI_DisposeObject(data->reply_string);
	}
	if (data->bcc_stuff_attached == 0)
	{
		MUI_DisposeObject(data->bcc_label);
		MUI_DisposeObject(data->bcc_group);
	}
	MUI_DisposeObject(data->wnd);
	if (data->file_req) MUI_FreeAslRequest(data->file_req);
	if (data->filename) free(data->filename);
	if (data->folder) free(data->folder);
	if (data->reply_id) free(data->reply_id);
	if (data->num < MAX_COMPOSE_OPEN) compose_open[data->num] = 0;
	free(data);
}

/**
 * Expand the to string. Returns 1 for a success else 0.
 *
 * @param pdata
 * @return
 */
static int compose_expand_to(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	char *to_contents = (char*)xget(data->to_string, MUIA_UTF8String_Contents);

	if (to_contents && *to_contents)
	{
		char *str;
		if ((str = addressbook_get_expanded(to_contents)))
		{
			/* We create now a list of addresses and recreate a string afterwards,
			 * which may include puny code or not (depending on the charset) */
			struct address_list *list = address_list_create(str);
			if (list)
			{
				utf8 *puny = address_list_to_utf8_codeset_safe(list, user.config.default_codeset);
				if (puny)
				{
					set(data->to_string, MUIA_UTF8String_Contents, puny);
					free(puny);
					address_list_free(list);
					free(str);
					return 1;
				}
				address_list_free(list);
			}
			free(str);
		}
		DisplayBeep(NULL);
		set(data->wnd, MUIA_Window_ActiveObject,data->to_string);
		return 0;
	}
	return 1;
}

/**
 * Expand the CC string. Returns 1 for a success else 0.
 *
 * @param pdata
 * @return
 */
static int compose_expand_cc(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	char *cc_contents = (char*)xget(data->cc_string, MUIA_UTF8String_Contents);

	if (cc_contents && *cc_contents)
	{
		char *str;
		if ((str = addressbook_get_expanded(cc_contents)))
		{
			/* We create now a list of addresses and recreate a string afterwards,
			 * which may include puny code or not (depending on the charset) */
			struct address_list *list = address_list_create(str);
			if (list)
			{
				utf8 *puny = address_list_to_utf8_codeset_safe(list, user.config.default_codeset);
				if (puny)
				{
					set(data->cc_string, MUIA_UTF8String_Contents, puny);
					free(puny);
					address_list_free(list);
					free(str);
					return 1;
				}
				address_list_free(list);
			}
			free(str);
		}
		DisplayBeep(NULL);
		set(data->wnd, MUIA_Window_ActiveObject,data->cc_string);
		return 0;
	}
	return 1;
}

/**
 * Expand the BCC string. Returns 1 for a success else 0.
 *
 * @param pdata
 * @return
 */
static int compose_expand_bcc(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	char *bcc_contents = (char*)xget(data->bcc_string, MUIA_UTF8String_Contents);

	if (bcc_contents && *bcc_contents)
	{
		char *str;
		if ((str = addressbook_get_expanded(bcc_contents)))
		{
			/* We create now a list of addresses and recreate a string afterwards,
			 * which may include puny code or not (depending on the charset) */
			struct address_list *list = address_list_create(str);
			if (list)
			{
				utf8 *puny = address_list_to_utf8_codeset_safe(list, user.config.default_codeset);
				if (puny)
				{
					set(data->bcc_string, MUIA_UTF8String_Contents, puny);
					free(puny);
					address_list_free(list);
					free(str);
					return 1;
				}
				address_list_free(list);
			}
			free(str);
		}
		DisplayBeep(NULL);
		set(data->bcc_button,MUIA_Selected,TRUE);
		set(data->wnd, MUIA_Window_ActiveObject,data->bcc_string);
		return 0;
	}
	return 1;
}

/**
 * Add an attachment to the treelist
 *
 * @param data
 * @param attach
 * @param list
 */
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

			insertlist = (struct MUI_NListtree_TreeNode *)DoMethod(data->attach_tree, MUIM_NListtree_Insert, (ULONG)"", (ULONG)&multipart,
					MUIV_NListtree_Insert_ListNode_ActiveFallback, MUIV_NListtree_Insert_PrevNode_Tail,TNF_OPEN|TNF_LIST);

			if (insertlist)
			{
				DoMethod(data->attach_tree, MUIM_NListtree_Move, MUIV_NListtree_Move_OldListNode_Active, MUIV_NListtree_Move_OldTreeNode_Active,
								(ULONG)insertlist, MUIV_NListtree_Move_NewTreeNode_Tail);
			} else
			{
				set(data->attach_tree, MUIA_NListtree_Quiet, FALSE);
				return;
			}
		}
	}

	act = !xget(data->attach_tree, MUIA_NListtree_Active);

	treenode = (struct MUI_NListtree_TreeNode *)DoMethod(data->attach_tree, MUIM_NListtree_Insert, (ULONG)"" /*name*/, (ULONG)attach, /* udata */
					 (ULONG)insertlist,MUIV_NListtree_Insert_PrevNode_Tail, (list?(TNF_OPEN|TNF_LIST):(act?MUIV_NListtree_Insert_Flag_Active:0)));

	/* for the quick attachments list */
	if (!list && treenode)
	{
		DoMethod(data->quick_attach_tree, MUIM_NListtree_Insert, (ULONG)"", (ULONG)attach, /* udata */
					MUIV_NListtree_Insert_ListNode_Root, MUIV_NListtree_Insert_PrevNode_Tail, (act?MUIV_NListtree_Insert_Flag_Active:0));
	}


	if (quiet)
	{
		set(data->attach_tree, MUIA_NListtree_Quiet, FALSE);
	}
}


/**
 * Add a multipart node to the list.
 *
 * @param pdata
 */
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

/**
 * Add a multipart node to the list.
 *
 * @param pdata
 */
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

/**
 * Add the given file as an attachment.
 *
 * @param data
 * @param filename
 */
static void compose_add_file_as_an_attachment(struct Compose_Data *data, char *filename)
{
	struct attachment attach;
	memset(&attach, 0, sizeof(attach));

	attach.content_type = (char *)identify_file(filename);
	attach.editable = 0;
	attach.filename = filename;
	attach.unique_id = data->attachment_unique_id++;

	compose_add_attachment(data,&attach,0);
}

/**
 * Add files to the list.
 *
 * @param pdata
 */
static void compose_add_files(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;

	if (data->file_req)
	{
		struct Window *iwnd;

		iwnd = (struct Window*)xget(data->wnd, MUIA_Window);

		set(App, MUIA_Application_Sleep, TRUE);

		if (MUI_AslRequestTags(data->file_req,
				ASLFR_DoMultiSelect, TRUE,
				iwnd?ASLFR_Window:TAG_IGNORE, iwnd,
				TAG_DONE))
		{
			int i;

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

						compose_add_file_as_an_attachment(data,buf);
						FreeVec(buf);
					}
					FreeVec(drawer);
				}
			}
		}

		set(App, MUIA_Application_Sleep, FALSE);
	}
}

/**
 * Add files to the list.
 *
 * @param pdata
 */
static void compose_remove_file(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	struct MUI_NListtree_TreeNode *treenode = (struct MUI_NListtree_TreeNode*)DoMethod(data->attach_tree, MUIM_NListtree_GetEntry, MUIV_NListtree_GetEntry_ListNode_Active, MUIV_NListtree_GetEntry_Position_Active,0);
	int rem, id;

  if (!treenode) return;

  id = ((struct attachment*)treenode->tn_User)->unique_id;

	rem = DoMethod(data->attach_tree, MUIM_NListtree_GetNr, (ULONG)treenode, MUIV_NListtree_GetNr_Flag_CountLevel) == 2;

	treenode = (struct MUI_NListtree_TreeNode*)DoMethod(data->attach_tree, MUIM_NListtree_GetEntry, (ULONG)treenode, MUIV_NListtree_GetEntry_Position_Parent,0);

	if (treenode && rem) set(data->attach_tree, MUIA_NListtree_Quiet,TRUE);
	DoMethod(data->attach_tree, MUIM_NListtree_Remove, MUIV_NListtree_Remove_ListNode_Active, MUIV_NListtree_Remove_TreeNode_Active, 0);

	if (treenode && rem)
	{
		DoMethod(data->attach_tree, MUIM_NListtree_Move, (ULONG)treenode, MUIV_NListtree_Move_OldTreeNode_Head, MUIV_NListtree_Move_NewListNode_Root, MUIV_NListtree_Move_NewTreeNode_Head, 0);
		DoMethod(data->attach_tree, MUIM_NListtree_Remove, MUIV_NListtree_Remove_ListNode_Root, (ULONG)treenode, 0);
		set(data->attach_tree, MUIA_NListtree_Active, MUIV_NListtree_Active_First);
		set(data->attach_tree, MUIA_NListtree_Quiet,FALSE);
	}

  treenode = (struct MUI_NListtree_TreeNode*)DoMethod(data->quick_attach_tree, MUIM_AttachmentList_FindUniqueID, id);
  if (treenode)
  {
		DoMethod(data->quick_attach_tree, MUIM_NListtree_Remove, MUIV_NListtree_Remove_ListNode_Root, (ULONG)treenode, 0);
  }

}

/**
 * A new attachment has been clicked.
 *
 * @param pdata
 */
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

/**
 * A new attachment has been clicked.
 *
 * @param pdata
 */
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

				DoMethod(data->x_text, MUIM_SetAsString, MUIA_Text_Contents, (ULONG)"%04ld", xget(data->text_texteditor,MUIA_TextEditor_CursorX));
				DoMethod(data->y_text, MUIM_SetAsString, MUIA_Text_Contents, (ULONG)"%04ld", xget(data->text_texteditor,MUIA_TextEditor_CursorY));

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

/**
 * Attach the mail given in the treenode to the current mail.
 *
 * @param pdata
 */
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

/**
 * Attach the mail given in the treenode to the current mail
 * (recursive)
 *
 * @param data
 * @param treenode
 * @param cmail
 */
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
				MUIM_NListtree_GetEntry, (ULONG)treenode, MUIV_NListtree_GetEntry_Position_Head, 0);


		while (tn)
		{
			struct composed_mail *newcmail = (struct composed_mail *)malloc(sizeof(struct composed_mail));
			if (newcmail)
			{
				composed_mail_init(newcmail);
				compose_window_attach_mail(data,tn,newcmail);
				composed_mail_add(cmail,newcmail);
			}
			tn = (struct MUI_NListtree_TreeNode*)DoMethod(data->attach_tree, MUIM_NListtree_GetEntry, (ULONG)tn, MUIV_NListtree_GetEntry_Position_Next,0);
		}
	} else
	{
		cmail->text = (attach->contents)?utf8create(attach->contents,user.config.default_codeset?user.config.default_codeset->name:NULL):NULL;
		cmail->content_filename = (attach->filename)?utf8create(sm_file_part(attach->filename),user.config.default_codeset?user.config.default_codeset->name:NULL):NULL;
		cmail->filename = mystrdup(attach->filename);
		cmail->temporary_filename = mystrdup(attach->temporary_filename);
	}
}

/**
 * Compose a mail and close the window.
 *
 * @param data
 * @param hold
 */
static void compose_mail(struct Compose_Data *data, int hold)
{
	if (compose_expand_to(&data) && compose_expand_cc(&data) && compose_expand_bcc(&data))
	{
		struct account *account;
		char from[200];
		char *to = (char*)xget(data->to_string, MUIA_UTF8String_Contents);
		char *cc = (char*)xget(data->cc_string, MUIA_UTF8String_Contents);
		char *bcc = (char*)xget(data->bcc_string, MUIA_UTF8String_Contents);
		char *replyto = (char*)xget(data->reply_string, MUIA_UTF8String_Contents);
		char *subject = (char*)xget(data->subject_string, MUIA_UTF8String_Contents);
		struct composed_mail new_mail;

		from[0] = 0;
		account = (struct account*)xget(data->from_accountpop,MUIA_AccountPop_Account);
		if (account)
		{
			if (account->email)
			{
				if (account->name && account->name[0])
				{
					const char *fmt;
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
		new_mail.bcc = bcc;
		new_mail.subject = subject;
		new_mail.mail_filename = data->filename;
		new_mail.mail_folder = data->folder;
		new_mail.reply_message_id = data->reply_id;
		DoMethod(data->toolbar, MUIM_SMToolbar_GetAttr, SM_COMPOSEWND_BUTTON_ENCRYPT, MUIA_SMToolbar_Attr_Selected, (ULONG)&new_mail.encrypt);
		DoMethod(data->toolbar, MUIM_SMToolbar_GetAttr, SM_COMPOSEWND_BUTTON_SIGN, MUIA_SMToolbar_Attr_Selected, (ULONG)&new_mail.sign);
		new_mail.importance = xget(data->importance_cycle, MUIA_Cycle_Active);

		/* Move this out */
		if ((mail_compose_new(&new_mail,hold)))
		{
			/* Change the status of a mail if it was replied or forwarded */
			if (data->ref_mail && mail_get_status_type(data->ref_mail) != MAIL_STATUS_SENT
												 && mail_get_status_type(data->ref_mail) != MAIL_STATUS_WAITSEND)
			{
				int marked = data->ref_mail->status & MAIL_STATUS_FLAG_MARKED;
				if (data->compose_action == COMPOSE_ACTION_REPLY)
				{
					struct folder *f = folder_find_by_mail(data->ref_mail);
					if (f)
					{
						folder_set_mail_status(f, data->ref_mail, (mail_status_t)(MAIL_STATUS_REPLIED|marked));
						main_refresh_mail(data->ref_mail);
						main_refresh_folder(f);
					}
				} else
				{
					if (data->compose_action == COMPOSE_ACTION_FORWARD)
					{
						struct folder *f = folder_find_by_mail(data->ref_mail);
						folder_set_mail_status(f, data->ref_mail, (mail_status_t)(MAIL_STATUS_FORWARD|marked));
						main_refresh_mail(data->ref_mail);
					}
				}
			}
			/* Close (and dispose) the compose window (data) */
			DoMethod(App, MUIM_Application_PushMethod, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)compose_window_dispose, (ULONG)data);
		} else
		{
			if (!new_mail.subject || !new_mail.subject[0])
				set(data->wnd,MUIA_Window_ActiveObject,data->subject_string);
		}
	}
}

/**
 * The mail should be send immediately.
 *
 * @param pdata
 */
static void compose_window_send_now(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	compose_mail(data,2);
}

/**
 * A mail should be send later.
 *
 * @param pdata
 */
static void compose_window_send_later(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	compose_mail(data,0);
}

/**
 * A mail should be hold.
 *
 * @param pdata
 */
static void compose_window_hold(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	compose_mail(data,1);
}

/**
 * Inserts a mail into the listtree (uses recursion)
 *
 * @param data
 * @param mail
 * @param listnode
 */
static void compose_add_mail(struct Compose_Data *data, struct mail_complete *mail, struct MUI_NListtree_TreeNode *listnode)
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
		if ((attach.description = (char *)malloc(len)))
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
		DoMethod(data->quick_attach_tree,MUIM_NListtree_Insert, (ULONG)"", (ULONG)&attach,
						 MUIV_NListtree_Insert_ListNode_Root,
						 MUIV_NListtree_Insert_PrevNode_Tail, 0);
	}

	treenode = (struct MUI_NListtree_TreeNode *)DoMethod(data->attach_tree, MUIM_NListtree_Insert, (ULONG)"", (ULONG)&attach, (ULONG)listnode, MUIV_NListtree_Insert_PrevNode_Tail, num_multiparts?(TNF_LIST|TNF_OPEN):0);

	free(attach.contents);
	free(attach.description);

	if (!treenode) return;

	for (i=0;i<num_multiparts; i++)
	{
		compose_add_mail(data,mail->multipart_array[i],treenode);
	}
}

/**
 * Set default signature.
 *
 * @param pdata
 */
static void compose_set_def_signature(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	struct account *ac = (struct account *)xget(data->from_accountpop, MUIA_AccountPop_Account);

	if (data->signatures_cycle && ac)
	{
		if (mystrcmp(ac->def_signature, MUIV_SignatureCycle_Default) == 0)
		{
			/* If the account is set to <Default> we fallback to the first entry */
			set(data->signatures_cycle, MUIA_SignatureCycle_Active, 0);
		} else
		{
			/* if the signature is not available "NoSignature" will be used. */
			set(data->signatures_cycle, MUIA_SignatureCycle_SignatureName, ac->def_signature);
		}
	}
}

/**
 * Set a signature.
 *
 * @param pdata
 */
static void compose_set_signature(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	int val = (int)xget(data->signatures_cycle, MUIA_SignatureCycle_Active);
	char *sign_name = (char *)xget(data->signatures_cycle, MUIA_SignatureCycle_SignatureName);
	struct signature *sign;
	char *text;
	int x = xget(data->text_texteditor,MUIA_TextEditor_CursorX);
	int y = xget(data->text_texteditor,MUIA_TextEditor_CursorY);

	if (mystrcmp(sign_name, MUIV_SignatureCycle_NoSignature) == 0)
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
	} else
	{
		sign = (struct signature*)list_find(&user.config.signature_list,val);
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

/**
 * Set the correct signature when edit a previous written mail.
 *
 * @param pdata
 */
static void compose_set_old_signature(struct Compose_Data **pdata)
{
	struct Compose_Data *data = *pdata;
	char *text;

	if ((text = (char*)DoMethod(data->text_texteditor, MUIM_TextEditor_ExportText)))
	{
		char *sign_text = strstr(text,"\n-- \n");
		if (sign_text)
		{
			struct signature *sign;
			char *sign_iso;

			sign_text += 5;
			sign = (struct signature*)list_first(&user.config.signature_list);
			while (sign)
			{
				sign_iso = utf8tostrcreate(sign->signature,user.config.default_codeset);
				if (mystrcmp(sign_text, sign_iso) == 0)
				{
					set(data->signatures_cycle, MUIA_SignatureCycle_SignatureName, sign->name);
					sign = NULL;
				} else
				{
					sign = (struct signature*)node_next(&sign->node);
				}
				free(sign_iso);
			}
		}
		FreeVec(text);
	}
}

/*****************************************************************************/

void compose_refresh_signature_cycle()
{
	int num;
	struct Compose_Data *data;

	for (num=0; num < MAX_COMPOSE_OPEN; num++)
	{
		if (compose_open[num])
		{
			data = compose_open[num];
			if (data->signatures_cycle)
			{
				DoMethod(data->signatures_group, MUIM_Group_InitChange);
				DoMethod(data->signatures_cycle, MUIM_SignatureCycle_Refresh, (ULONG)&user.config.signature_list);
				DoMethod(data->signatures_group, MUIM_Group_ExitChange);
			}
		}
	}
}

/**
 * Set the reply to the current selected account
 *
 * @param msg
 */
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

/**
 * Called when the reply button is clicked.
 *
 * @param msg
 */
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
		DoMethod(group, OM_REMMEMBER, (ULONG)data->reply_label);
		DoMethod(group, OM_REMMEMBER, (ULONG)data->reply_string);
		data->reply_stuff_attached = 0;
	} else
	{
		if (!data->reply_stuff_attached && reply_selected)
		{
			DoMethod(group, OM_ADDMEMBER, (ULONG)data->reply_label);
			DoMethod(group, OM_ADDMEMBER, (ULONG)data->reply_string);
			if (data->bcc_stuff_attached)
			{
				DoMethod(group,MUIM_Group_Sort, (ULONG)data->from_label, (ULONG)data->from_group,
				                                (ULONG)data->reply_label, (ULONG)data->reply_string,
				                                (ULONG)data->to_label, (ULONG)data->to_group,
				                                (ULONG)data->cc_label, (ULONG)data->cc_group,
				                                (ULONG)data->bcc_label, (ULONG)data->bcc_group,
				                                (ULONG)data->subject_label, (ULONG)data->subject_string, NULL);
			} else
			{
				DoMethod(group,MUIM_Group_Sort, (ULONG)data->from_label, (ULONG)data->from_group,
				                                (ULONG)data->reply_label, (ULONG)data->reply_string,
				                                (ULONG)data->to_label, (ULONG)data->to_group,
				                                (ULONG)data->cc_label, (ULONG)data->cc_group,
				                                (ULONG)data->subject_label, (ULONG)data->subject_string,NULL);
			}
			data->reply_stuff_attached = 1;
		}
	}
	DoMethod(group,MUIM_Group_ExitChange);
}

/**
 * Called when the bcc button is clicked.
 *
 * @param msg the message
 */
static void compose_bcc_button(void **msg)
{
	struct Compose_Data *data = (struct Compose_Data*)msg[0];
	int bcc_selected;
	Object *group = data->headers_group;
	if (!group) return;
	bcc_selected = xget(data->bcc_button,MUIA_Selected);

	DoMethod(group,MUIM_Group_InitChange);
	if (data->bcc_stuff_attached && !bcc_selected)
	{
		DoMethod(group, OM_REMMEMBER, (ULONG)data->bcc_label);
		DoMethod(group, OM_REMMEMBER, (ULONG)data->bcc_group);
		data->bcc_stuff_attached = 0;
	} else
	{
		if (!data->bcc_stuff_attached && bcc_selected)
		{
			DoMethod(group, OM_ADDMEMBER, (ULONG)data->bcc_label);
			DoMethod(group, OM_ADDMEMBER, (ULONG)data->bcc_group);
			if (data->reply_stuff_attached)
			{
				DoMethod(group,MUIM_Group_Sort, (ULONG)data->from_label, (ULONG)data->from_group,
				                                (ULONG)data->reply_label, (ULONG)data->reply_string,
				                                (ULONG)data->to_label, (ULONG)data->to_group,
				                                (ULONG)data->cc_label, (ULONG)data->cc_group,
				                                (ULONG)data->bcc_label, (ULONG)data->bcc_group,
				                                (ULONG)data->subject_label, (ULONG)data->subject_string, NULL);
			} else
			{
				DoMethod(group,MUIM_Group_Sort, (ULONG)data->from_label, (ULONG)data->from_group,
				                                (ULONG)data->to_label, (ULONG)data->to_group,
				                                (ULONG)data->cc_label, (ULONG)data->cc_group,
				                                (ULONG)data->bcc_label, (ULONG)data->bcc_group,
				                                (ULONG)data->subject_label, (ULONG)data->subject_string, NULL);
			}
			data->bcc_stuff_attached = 1;
		}
	}
	DoMethod(group,MUIM_Group_ExitChange);
}

/**
 * New gadget should be activated
 *
 * @param msg
 */
static void compose_new_active(void **msg)
{
	struct Compose_Data *data = (struct Compose_Data *)msg[0];
	Object *obj = (Object*)msg[1];

	if ((ULONG)obj == (ULONG)MUIV_Window_ActiveObject_Next &&
		  (ULONG)xget(data->wnd, MUIA_Window_ActiveObject) == (ULONG)data->toolbar)
	{
		set(data->wnd, MUIA_Window_ActiveObject, data->text_texteditor);
	}
}

/*****************************************************************************/

int compose_window_open(struct compose_args *args)
{
	Object *wnd, *send_later_button, *hold_button, *cancel_button, *send_now_button, *headers_group;
	Object *from_label, *from_group, *from_accountpop, *reply_button, *reply_label, *reply_string, *to_label, *to_group, *to_string, *cc_label, *cc_group, *cc_string, *subject_label, *subject_string;
	Object *bcc_button, *bcc_label, *bcc_group, *bcc_string;
	Object *text_texteditor, *xcursor_text, *ycursor_text, *slider;
	Object *datatype_datatypes;
	Object *expand_to_button, *expand_cc_button, *expand_bcc_button;
	Object *quick_attach_tree;
	Object *attach_tree, *attach_desc_string, *add_text_button, *add_multipart_button, *add_files_button, *remove_button;
	Object *contents_page;
	Object *signatures_group;
	Object *signatures_cycle, *importance_cycle;
	Object *toolbar;

	int num;

	static char *register_titles[3];
	static int register_titles_are_translated = 0;
	static char *importance_labels[4];
	static int importance_labels_are_translated = 0;

	if (!register_titles_are_translated)
	{
		register_titles[0] = _("Mail");
		register_titles[1] = _("Attachments");
		register_titles[2] = NULL;
		register_titles_are_translated = 1;
	}

	if (!importance_labels_are_translated)
	{
		importance_labels[0] = _("Low");
		importance_labels[1] = _("Normal");
		importance_labels[2] = _("High");
		importance_labels[3] = NULL;
		importance_labels_are_translated = 1;
	}

	/* Find out if window is already open */
	if (args->to_change && args->to_change->info->filename)
	{
		for (num=0; num < MAX_COMPOSE_OPEN; num++)
		{
			if (compose_open[num])
			{
				if (!mystricmp(args->to_change->info->filename, compose_open[num]->filename))
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

	if (user.config.signatures_use)
	{
		/* create SignatureCycle and point to "NoSignatures", so we get notified when the default
		   signature is set */
		signatures_cycle = SignatureCycleObject,
			MUIA_SignatureCycle_HasDefaultEntry, FALSE,
			MUIA_SignatureCycle_SignatureName, MUIV_SignatureCycle_NoSignature,
			End;
		if (signatures_cycle)
		{
			signatures_group = ColGroup(2),
				MUIA_Weight, 33,
				Child, MakeLabel(_("Importance")),
				Child, importance_cycle = MakeCycle(_("Importance"), importance_labels),
				Child, MakeLabel(_("Signature")),
				Child, signatures_cycle,
			End;
		} else
		{
			/* there are no signatures */
			signatures_group = NULL;
		}
	} else
	{
		signatures_group = NULL;
		signatures_cycle = NULL;
	}
	if (!signatures_group)
	{
		signatures_group = HGroup,
			MUIA_Weight, 33,
			Child, MakeLabel(_("Importance")),
			Child, importance_cycle = MakeCycle(_("Importance"), importance_labels),
			End;
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
								Child, bcc_button = TextObject, ButtonFrame, MUIA_Selected,1, MUIA_InputMode, MUIV_InputMode_Toggle,MUIA_Text_PreParse, "\033c",MUIA_Text_Contents,_("B"), MUIA_Text_SetMax, TRUE, End,
								End,

							Child, bcc_label = MakeLabel(_("_Blind Copies To")),
							Child, bcc_group = HGroup,
								MUIA_Group_Spacing,0,
								Child, bcc_string = AddressStringObject,
									StringFrame,
									MUIA_CycleChain, 1,
									MUIA_ControlChar, GetControlChar(_("_Blind Copies To")),
									MUIA_String_AdvanceOnCR, TRUE,
									End,
								Child, expand_bcc_button = PopButton(MUII_ArrowLeft),
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
								Child, toolbar = SMToolbarObject,
									MUIA_Group_Horiz, TRUE,
									MUIA_SMToolbar_Buttons, sm_composewnd_buttons,
									End,
								Child, RectangleObject,
									MUIA_FixHeight,1,
									MUIA_HorizWeight,signatures_group?15:100,
									End,
								signatures_group?Child:TAG_IGNORE,signatures_group,
								signatures_group?Child:TAG_IGNORE,RectangleObject,
									MUIA_FixHeight,1,
									MUIA_HorizWeight,signatures_group?15:100,
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
			data->bcc_button = bcc_button;
			data->bcc_label = bcc_label;
			data->bcc_group = bcc_group;
			data->bcc_string = bcc_string;
			data->reply_string = reply_string;
			data->reply_label = reply_label;
			data->reply_button = reply_button;
			data->subject_label = subject_label;
			data->subject_string = subject_string;
			data->text_texteditor = text_texteditor;
			data->toolbar = toolbar;
			data->x_text = xcursor_text;
			data->y_text = ycursor_text;
			data->attach_tree = attach_tree;
			data->attach_desc_string = attach_desc_string;
			data->quick_attach_tree = quick_attach_tree;
			data->contents_page = contents_page;
			data->datatype_datatypes = datatype_datatypes;
			data->signatures_group = signatures_group;
			data->signatures_cycle = signatures_cycle;
			data->importance_cycle = importance_cycle;

			data->file_req = (struct FileRequester *)MUI_AllocAslRequestTags(ASL_FileRequest, TAG_DONE);

			/* mark the window as opened */
			compose_open[num] = data;

			DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)App, 7, MUIM_Application_PushMethod, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)compose_window_hold, (ULONG)data);
			DoMethod(expand_to_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)compose_expand_to, (ULONG)data);
			DoMethod(expand_cc_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)compose_expand_cc, (ULONG)data);
			DoMethod(expand_bcc_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)compose_expand_bcc, (ULONG)data);
			DoMethod(to_string, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)compose_expand_to, (ULONG)data);
			DoMethod(cc_string, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)compose_expand_cc, (ULONG)data);
			DoMethod(bcc_string, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)compose_expand_bcc, (ULONG)data);
			DoMethod(bcc_button, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)compose_bcc_button, (ULONG)data);
			DoMethod(reply_button, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)compose_reply_button, (ULONG)data);
			DoMethod(text_texteditor, MUIM_Notify, MUIA_TextEditor_CursorX, MUIV_EveryTime, (ULONG)xcursor_text, 4, MUIM_SetAsString, MUIA_Text_Contents, (ULONG)"%04ld", MUIV_TriggerValue);
			DoMethod(text_texteditor, MUIM_Notify, MUIA_TextEditor_CursorY, MUIV_EveryTime, (ULONG)ycursor_text, 4, MUIM_SetAsString, MUIA_Text_Contents, (ULONG)"%04ld", MUIV_TriggerValue);
			DoMethod(text_texteditor, MUIM_Notify, MUIA_TextEditor_UndoAvailable, MUIV_EveryTime, (ULONG)toolbar, 4, MUIM_SMToolbar_SetAttr, SM_COMPOSEWND_BUTTON_UNDO, MUIA_SMToolbar_Attr_Disabled, MUIV_NotTriggerValue);
			DoMethod(text_texteditor, MUIM_Notify, MUIA_TextEditor_RedoAvailable, MUIV_EveryTime, (ULONG)toolbar, 4, MUIM_SMToolbar_SetAttr, SM_COMPOSEWND_BUTTON_REDO, MUIA_SMToolbar_Attr_Disabled, MUIV_NotTriggerValue);
			DoMethod(add_text_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)compose_add_text, (ULONG)data);
			DoMethod(add_multipart_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)compose_add_multipart, (ULONG)data);
			DoMethod(add_files_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)compose_add_files, (ULONG)data);
			DoMethod(remove_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)compose_remove_file, (ULONG)data);
			DoMethod(attach_tree, MUIM_Notify, MUIA_NListtree_Active, MUIV_EveryTime, (ULONG)attach_tree, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)compose_attach_active, (ULONG)data);
			DoMethod(attach_desc_string, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)compose_attach_desc, (ULONG)data);
			DoMethod(quick_attach_tree, MUIM_Notify, MUIA_NListtree_Active, MUIV_EveryTime, (ULONG)quick_attach_tree, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)compose_quick_attach_active, (ULONG)data);
			DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 7, MUIM_Application_PushMethod, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)compose_window_dispose, (ULONG)data);
			DoMethod(hold_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 7, MUIM_Application_PushMethod, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)compose_window_hold, (ULONG)data);
			DoMethod(send_now_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 7, MUIM_Application_PushMethod, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)compose_window_send_now, (ULONG)data);
			DoMethod(send_later_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)compose_window_send_later, (ULONG)data);
			DoMethod(subject_string,MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime, (ULONG)wnd, 3, MUIM_Set, MUIA_Window_ActiveObject, (ULONG)text_texteditor);
			DoMethod(wnd, MUIM_Notify, MUIA_Window_ActiveObject, MUIV_EveryTime, (ULONG)App, 5, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)compose_new_active, (ULONG)data, MUIV_TriggerValue);
			DoMethod(from_accountpop, MUIM_Notify, MUIA_AccountPop_Account, MUIV_EveryTime, (ULONG)from_accountpop, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)compose_set_replyto, (ULONG)data);

			/* create notifies for toolbar buttons */
			DoMethod(toolbar, MUIM_SMToolbar_DoMethod, SM_COMPOSEWND_BUTTON_COPY,   7, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)text_texteditor, 2, MUIM_TextEditor_ARexxCmd, (ULONG)"Copy");
			DoMethod(toolbar, MUIM_SMToolbar_DoMethod, SM_COMPOSEWND_BUTTON_CUT,    7, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)text_texteditor, 2, MUIM_TextEditor_ARexxCmd, (ULONG)"Cut");
			DoMethod(toolbar, MUIM_SMToolbar_DoMethod, SM_COMPOSEWND_BUTTON_PASTE,  7, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)text_texteditor, 2, MUIM_TextEditor_ARexxCmd, (ULONG)"Paste");
			DoMethod(toolbar, MUIM_SMToolbar_DoMethod, SM_COMPOSEWND_BUTTON_UNDO,   7, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)text_texteditor, 2, MUIM_TextEditor_ARexxCmd, (ULONG)"Undo");
			DoMethod(toolbar, MUIM_SMToolbar_DoMethod, SM_COMPOSEWND_BUTTON_REDO,   7, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)text_texteditor, 2, MUIM_TextEditor_ARexxCmd, (ULONG)"Redo");
			DoMethod(toolbar, MUIM_SMToolbar_DoMethod, SM_COMPOSEWND_BUTTON_ATTACH, 9, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)compose_add_files, (ULONG)data);

			data->reply_stuff_attached = 1;
			set(data->reply_button,MUIA_Selected,FALSE);
			data->bcc_stuff_attached = 1;
			set(data->bcc_button,MUIA_Selected,FALSE);
			set(data->importance_cycle, MUIA_Cycle_Active, 1);

			DoMethod(App,OM_ADDMEMBER, (ULONG)wnd);

			DoMethod(from_accountpop, MUIM_AccountPop_Refresh);

			if (args->to_change)
			{
				/* A mail should be changed */
				int entries;
				char *from, *importance, *bcc;

				/* Find and set the correct account */
				if ((from = mail_find_header_contents(args->to_change, "from")))
				{
					struct account *ac = account_find_by_from(from);
					set(from_accountpop, MUIA_AccountPop_Account, ac);
				}

				/* Find and set the correct importance (normal is default) */
				if ((importance = mail_find_header_contents(args->to_change, "importance")))
				{
					if (!mystricmp(importance, "high")) set(data->importance_cycle, MUIA_Cycle_Active, 2);
					else if (!mystricmp(importance, "low")) set(data->importance_cycle, MUIA_Cycle_Active, 0);
				}

				/* Find and set the correct BCC */
				if ((bcc = mail_find_header_contents(args->to_change, "bcc")))
				{
					struct address_list *bcc_list = address_list_create(bcc);
					if (bcc_list)
					{
						utf8 *bcc_str = address_list_to_utf8_codeset_safe(bcc_list,user.config.default_codeset);
						if (bcc_str)
						{
							set(bcc_string,MUIA_UTF8String_Contents,bcc_str);
							free(bcc_str);
							set(data->bcc_button,MUIA_Selected,TRUE);
						}
						address_list_free(bcc_list);
					}
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

				if (args->to_change->info->to_list)
				{
					utf8 *to_str = address_list_to_utf8_codeset_safe(args->to_change->info->to_list,user.config.default_codeset);
					if (to_str)
					{
						set(to_string,MUIA_UTF8String_Contents,to_str);
						free(to_str);
					}
				}

				if (args->to_change->info->cc_list)
				{
					utf8 *cc_str = address_list_to_utf8_codeset_safe(args->to_change->info->cc_list,user.config.default_codeset);
					if (cc_str)
					{
						set(cc_string,MUIA_UTF8String_Contents,cc_str);
						free(cc_str);
					}
				}

				if (args->to_change->info->reply_addr)
				{
					char *reply_addr = args->to_change->info->reply_addr;
					if (!codesets_unconvertable_chars(user.config.default_codeset,reply_addr,strlen(reply_addr)))
						set(reply_string,MUIA_UTF8String_Contents,reply_addr);
					else
					{
						char *puny = encode_address_puny(reply_addr);
						set(reply_string,MUIA_UTF8String_Contents,puny);
						free(puny);
					}
					set(data->reply_button,MUIA_Selected,TRUE);
				}

				set(subject_string,MUIA_UTF8String_Contents,args->to_change->info->subject);

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

				data->filename = mystrdup(args->to_change->info->filename);
				data->folder = mystrdup("Outgoing");
				data->reply_id = mystrdup(args->to_change->info->message_reply_id);
			} else
			{
				compose_add_text(&data);
				/* activate the "To" field */
				set(wnd,MUIA_Window_ActiveObject,data->to_string);
			}

			if (signatures_cycle)
			{
				struct folder *f = main_get_folder();

				DoMethod(signatures_cycle, MUIM_Notify, MUIA_SignatureCycle_Active, MUIV_EveryTime, (ULONG)signatures_cycle, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)compose_set_signature, (ULONG)data);
				DoMethod(signatures_cycle, MUIM_Notify, MUIA_SignatureCycle_SignatureName, MUIV_EveryTime, (ULONG)signatures_cycle, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)compose_set_signature, (ULONG)data);
				DoMethod(from_accountpop, MUIM_Notify, MUIA_AccountPop_Account, MUIV_EveryTime, (ULONG)from_accountpop, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)compose_set_def_signature, (ULONG)data);
				if (args->action == COMPOSE_ACTION_EDIT)
				{
					compose_set_old_signature(&data);
				} else
				{
					if (f && (mystrcmp(f->def_signature, MUIV_SignatureCycle_Default) != 0))
					{
						set(data->signatures_cycle, MUIA_SignatureCycle_SignatureName, f->def_signature);
					} else
					{
						compose_set_def_signature(&data);
					}
				}
			}

			data->compose_action = args->action;
			data->ref_mail = args->ref_mail;

			set(wnd,MUIA_Window_Open,TRUE);

			return num;
		}
		MUI_DisposeObject(wnd);
	}
	return -1;
}

/*****************************************************************************/

void compose_window_activate(int num)
{
	if (num < 0 || num >= MAX_COMPOSE_OPEN) return;
	if (compose_open[num] && compose_open[num]->wnd) set(compose_open[num]->wnd,MUIA_Window_Open,TRUE);
}

/*****************************************************************************/

void compose_window_attach(int num, char **filenames)
{
	int i;
	char *filename;

	if (num < 0 || num >= MAX_COMPOSE_OPEN) return;
	if (!compose_open[num]) return;

	for (i=0;(filename = filenames[i]);i++)
		compose_add_file_as_an_attachment(compose_open[num],filename);
}

/*****************************************************************************/

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

