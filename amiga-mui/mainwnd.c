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
** mainwnd.c
*/

#include <string.h>
#include <stdio.h>

#include <libraries/iffparse.h> /* MAKE_ID */
#include <libraries/mui.h>
#include <mui/nlistview_mcc.h>
#include <mui/nlisttree_mcc.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "SimpleMail_rev.h"

#include "folder.h"
#include "mail.h"
#include "simplemail.h"
#include "support.h"

#include "amigasupport.h"
#include "compiler.h"
#include "foldertreelistclass.h"
#include "mainwnd.h"
#include "mailtreelistclass.h"
#include "muistuff.h"
#include "picturebuttonclass.h"

static Object *win_main;
static Object *main_group;
static Object *buttons_group;
static Object *button_fetch;
static Object *button_send;
static Object *button_read;
static Object *button_getadd;
static Object *button_delete;
static Object *button_change;
static Object *button_new;
static Object *button_reply;
static Object *button_abook;
static Object *button_config;
static Object *button_switch; /* switch button for the two views */
static Object *mail_tree;
static Object *mail_tree_group;
static Object *mail_listview;
static Object *folder_tree;
static Object *folder_text;
static Object *folder_listview;
static Object *folder_balance;
static Object *folder_popobject;

static int folders_in_popup;

static void main_refresh_folders_text(void);
static int init_folder_placement(void);

struct MUI_NListtree_TreeNode *FindListtreeUserData(Object *tree, APTR udata)
{
	struct MUI_NListtree_TreeNode *listnode = (struct MUI_NListtree_TreeNode*)MUIV_NListtree_GetEntry_ListNode_Root;

	while (1)
	{
		struct MUI_NListtree_TreeNode *treenode = (struct MUI_NListtree_TreeNode*)DoMethod(tree,
				MUIM_NListtree_GetEntry, listnode, MUIV_NListtree_GetEntry_Position_Head, 0);

		while (treenode)
		{
			struct MUI_NListtree_TreeNode *newtreenode;
			if (treenode->tn_User == udata) return treenode;
			if (treenode->tn_Flags & TNF_LIST)
			{
				listnode = treenode;
				break;
			}

			if (!(newtreenode = (struct MUI_NListtree_TreeNode*)DoMethod(tree, MUIM_NListtree_GetEntry, treenode, MUIV_NListtree_GetEntry_Position_Next,0)))
			{
				do
				{
					newtreenode = (struct MUI_NListtree_TreeNode *)DoMethod(tree, MUIM_NListtree_GetEntry, treenode,MUIV_NListtree_GetEntry_Position_Parent,0);
					if (!newtreenode) break;
					treenode = newtreenode;
					newtreenode = (struct MUI_NListtree_TreeNode*)DoMethod(tree, MUIM_NListtree_GetEntry, treenode, MUIV_NListtree_GetEntry_Position_Next,0);
				} while (!newtreenode);
			}
			treenode = newtreenode;
		}

		if (!treenode) break;
	}
	return NULL;
}

/******************************************************************
 Converts a given sort mode to a nlist title mark
*******************************************************************/
static ULONG sortmode2titlemark(int sortmode, int type)
{
	int col = 0;
	if (sortmode == FOLDER_SORT_THREAD) return 0;
	col = sortmode & FOLDER_SORT_MODEMASK;
	if (col == 1) /* from/to */
	{
		if (type == FOLDER_TYPE_SEND) col = 2;
	} else if (col > 1) col++;
	return (col | ((sortmode & FOLDER_SORT_REVERSE)?MUIV_NList_TitleMark_Up:MUIV_NList_TitleMark_Down));
}

/******************************************************************
 Converts a given nlist title mark to a sort mode
*******************************************************************/
static int titlemark2sortmode(int titlemark)
{
	int col = titlemark & MUIV_NList_TitleMark_ColMask;
	if (titlemark == 0) return FOLDER_SORT_THREAD;
	if (col == 2) col = 1; /* from/to column */
	else if (col > 2) col--;
	return col|((titlemark & MUIV_NList_TitleMark_Up)?FOLDER_SORT_REVERSE:0);
}

/******************************************************************
 Update the mailtree's titlemarkers depending upon the folder's
 sort mode
*******************************************************************/
static void mailtreelist_update_title_markers(void)
{
	struct folder *folder = main_get_folder();
	if (!folder) return;

	nnset(mail_tree, MUIA_NList_TitleMark, sortmode2titlemark(folder_get_primary_sort(folder),folder_get_type(folder)));
	nnset(mail_tree, MUIA_NList_TitleMark2, sortmode2titlemark(folder_get_secondary_sort(folder),folder_get_type(folder)));
}

/******************************************************************
 A mail has been dropped onto a folder
*******************************************************************/
static void foldertreelist_maildrop(void)
{
	struct folder *folder = (struct folder*)xget(folder_tree, MUIA_FolderTreelist_MailDrop);
	if (folder)
	{
		callback_maildrop(folder);
	}
}

/******************************************************************
 The Mailtree's title has been clicked, so change the sort
 mode if necessary
*******************************************************************/
static void mailtreelist_title_click(void)
{
	LONG title_mark = xget(mail_tree, MUIA_NList_TitleMark);
	LONG title_click = xget(mail_tree, MUIA_NList_TitleClick);
	struct folder *folder = main_get_folder();

	if ((title_mark & MUIV_NList_TitleMark_ColMask) == title_click)
	{
		switch (title_mark & MUIV_NList_TitleMark_TypeMask)
		{
			case	MUIV_NList_TitleMark_Down:
						title_mark = title_click | MUIV_NList_TitleMark_Up;
						break;

			case	MUIV_NList_TitleMark_Up:
						title_mark = title_click | MUIV_NList_TitleMark_Down;
						break;
		}
	} else
	{
		title_mark = title_click | MUIV_NList_TitleMark_Down;
	}

	set(mail_tree, MUIA_NList_TitleMark, title_mark);

	if (folder)
	{
		folder_set_primary_sort(folder, titlemark2sortmode(title_mark));
		main_set_folder_mails(folder);
	}
}

/******************************************************************
 Switch the view of the folders
*******************************************************************/
static void switch_folder_view(void)
{
	folders_in_popup = !folders_in_popup;
	init_folder_placement();
}

/******************************************************************
 This hook is called after the popup has succesfully closed
*******************************************************************/
static void folder_popup_objstrfunc(void)
{
	DoMethod(App, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, callback_folder_active);
	DoMethod(App, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, main_refresh_folders_text);
}

/******************************************************************
 Initialize the main window
*******************************************************************/
static int init_folder_placement(void)
{
	void *act;
	if (folder_tree) act = (void*)xget(folder_tree,MUIA_NListtree_Active);
	else act = NULL;

	{
		DoMethod(main_group, MUIM_Group_InitChange);
		DoMethod(mail_tree_group, MUIM_Group_InitChange);

		if (folder_popobject)
		{
			DoMethod(main_group, OM_REMMEMBER, folder_popobject);

			MUI_DisposeObject(folder_popobject);
			folder_popobject = NULL;
			folder_text = NULL;
			folder_listview = NULL;
			folder_tree = NULL;
		}
		else
		{
			if (folder_listview)
			{
				DoMethod(mail_tree_group, OM_REMMEMBER, folder_listview);

				MUI_DisposeObject(folder_listview);
				folder_listview = NULL;
				folder_tree = NULL;
			}
			if (folder_balance)
			{
				DoMethod(mail_tree_group, OM_REMMEMBER, folder_balance);
				MUI_DisposeObject(folder_balance);
				folder_balance = NULL;
			}
		}

		if (folders_in_popup)
		{
			static struct Hook folder_popup_objstrhook;

			folder_popobject = PopobjectObject,
				MUIA_Popstring_Button, PopButton(MUII_PopUp),
				MUIA_Popstring_String, folder_text = TextObject, TextFrame, MUIA_Text_PreParse, MUIX_C, End,
				MUIA_Popobject_ObjStrHook, &folder_popup_objstrhook,
				MUIA_Popobject_Object, folder_listview = NListviewObject,
					MUIA_CycleChain, 1,
					MUIA_NListview_NList, folder_tree = FolderTreelistObject,
						MUIA_NList_Exports, MUIV_NList_Exports_ColWidth|MUIV_NList_Exports_ColOrder,
						MUIA_NList_Imports, MUIV_NList_Imports_ColWidth|MUIV_NList_Imports_ColOrder,
						MUIA_ObjectID, MAKE_ID('M','W','F','T'),
						End,
					End,
				End;

			init_hook(&folder_popup_objstrhook,(HOOKFUNC)folder_popup_objstrfunc);

			DoMethod(main_group, OM_ADDMEMBER, folder_popobject);
			DoMethod(main_group, MUIM_Group_Sort, buttons_group, folder_popobject, mail_tree_group,0);

			DoMethod(folder_tree, MUIM_Notify, MUIA_NListtree_DoubleClick, MUIV_EveryTime, folder_popobject, 2, MUIM_Popstring_Close, 1);
		} else
		{
			folder_listview = folder_listview = NListviewObject,
					MUIA_CycleChain, 1,
					MUIA_HorizWeight, 33,
					MUIA_NListview_NList, folder_tree = FolderTreelistObject,
						MUIA_NList_Exports, MUIV_NList_Exports_ColWidth|MUIV_NList_Exports_ColOrder,
						MUIA_NList_Imports, MUIV_NList_Imports_ColWidth|MUIV_NList_Imports_ColOrder,
						MUIA_ObjectID, MAKE_ID('M','W','F','T'),
						End,
					End,

			folder_balance = BalanceObject, End;

			DoMethod(mail_tree_group, OM_ADDMEMBER, folder_listview);
			DoMethod(mail_tree_group, OM_ADDMEMBER, folder_balance);
			DoMethod(mail_tree_group, MUIM_Group_Sort, folder_listview, folder_balance, mail_listview, 0);

			DoMethod(folder_tree, MUIM_Notify, MUIA_NListtree_Active, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_folder_active);
			DoMethod(folder_tree, MUIM_Notify, MUIA_FolderTreelist_MailDrop, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, foldertreelist_maildrop);
			DoMethod(folder_tree, MUIM_Notify, MUIA_NListtree_DoubleClick, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_edit_folder);
		}

		main_refresh_folders();
		nnset(folder_tree,MUIA_NListtree_Active,act);
		main_refresh_folders_text();

		DoMethod(mail_tree_group, MUIM_Group_ExitChange);
		DoMethod(main_group, MUIM_Group_ExitChange);
	}

	set(folder_tree,MUIA_UserData,mail_tree); /* for the drag'n'drop support */
	return 1;
}

/******************************************************************
 Initialize the main window
*******************************************************************/
int main_window_init(void)
{
	int rc;
	rc = FALSE;

	win_main = WindowObject,
		MUIA_Window_ID, MAKE_ID('M','A','I','N'),
    MUIA_Window_Title, VERS,
        
		WindowContents, main_group = VGroup,
			Child, buttons_group = HGroupV,
				Child, HGroup,
					MUIA_Group_Spacing, 0,
					MUIA_Weight, 200,
					Child, button_read = MakePictureButton("R_ead","PROGDIR:Images/MailRead"),
					Child, button_change = MakePictureButton("_Modify","PROGDIR:Images/MailModify"),
					Child, button_delete = MakePictureButton("_Delete","PROGDIR:Images/MailDelete"),
					Child, button_getadd = MakePictureButton("_GetAdd","PROGDIR:Images/MailGetAddress"),
					End,
				Child, HGroup,
					MUIA_Group_Spacing, 0,
					Child, button_new = MakePictureButton("_New","PROGDIR:Images/MailNew"),
					Child, button_reply = MakePictureButton("_Reply","PROGDIR:Images/MailReply"),
					End,
				Child, HGroup,
					MUIA_Group_Spacing, 0,
					Child, button_fetch = MakePictureButton("_Fetch","PROGDIR:Images/MailsFetch"),
					Child, button_send = MakePictureButton("_Send","PROGDIR:Images/MailsSend"),
					End,
				Child, HGroup,
					MUIA_Group_Spacing, 0,
					Child, button_abook = MakePictureButton("_Abook","PROGDIR:Images/Addressbook"),
					Child, button_config = MakePictureButton("_Config","PROGDIR:Images/Config"),
					Child, button_switch = MakePictureButton("S_witch", "PROGDIR:Images/Switch"),
					End,
				End,

			Child, mail_tree_group = HGroup,
				Child, mail_listview = NListviewObject,
					MUIA_CycleChain,1,
					MUIA_NListview_NList, mail_tree = MailTreelistObject,
						MUIA_NList_TitleMark, MUIV_NList_TitleMark_Down | 4,
						MUIA_NList_Exports, MUIV_NList_Exports_ColWidth|MUIV_NList_Exports_ColOrder,
						MUIA_NList_Imports, MUIV_NList_Imports_ColWidth|MUIV_NList_Imports_ColOrder,
						MUIA_ObjectID, MAKE_ID('M','W','M','T'),
						End,
					End,
				End,
			End,
		End;
	
	if(win_main != NULL)
	{
		if (!init_folder_placement()) return 0;

		DoMethod(App, OM_ADDMEMBER, win_main);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_CloseRequest, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
		DoMethod(button_read, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_read_mail);
		DoMethod(button_getadd, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_get_address);
		DoMethod(button_delete, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_delete_mails);
		DoMethod(button_fetch, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_fetch_mails);
		DoMethod(button_send, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_send_mails);
		DoMethod(button_new, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_new_mail);
		DoMethod(button_reply, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_reply_mail);
		DoMethod(button_change, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_change_mail);
		DoMethod(button_abook, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_addressbook);
		DoMethod(button_config, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_config);
		DoMethod(button_switch, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, switch_folder_view);
		DoMethod(mail_tree, MUIM_Notify, MUIA_NListtree_DoubleClick, MUIV_EveryTime, MUIV_Notify_Application, 3,  MUIM_CallHook, &hook_standard, callback_read_mail);
		DoMethod(mail_tree, MUIM_Notify, MUIA_NList_TitleClick, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, mailtreelist_title_click);

		rc = TRUE;
		
	}
	else
	{
		tell("Can\'t create window!");
	}
	
	return(rc);
}

/******************************************************************
 Open the main window
*******************************************************************/
int main_window_open(void)
{
	int rc;
	rc = FALSE;
	
	if(win_main != NULL)
	{
		set(win_main, MUIA_Window_Open, TRUE);
		rc = TRUE;
	}	
	
	return(rc);
}

/******************************************************************
 Refreshs the folder text line
*******************************************************************/
static void main_refresh_folders_text(void)
{
	if (folder_text)
	{
		char buf[256];
		struct folder *f = main_get_folder();
		if (f)
		{
			sprintf(buf, MUIX_PH "Folder:"  MUIX_PT "%s " MUIX_PH "Messages:"  MUIX_PT "%ld " MUIX_PH "New:"  MUIX_PT "%ld: " MUIX_PH "Unread:"  MUIX_PT "%ld",f->name,f->num_mails,f->new_mails,f->unread_mails);
			set(folder_text, MUIA_Text_Contents,buf);
		}
	}
}

/******************************************************************
 Refreshs the folder list
*******************************************************************/
void main_refresh_folders(void)
{
	struct folder *f = folder_first();

	int act = xget(folder_tree, MUIA_NList_Active);

	set(folder_tree,MUIA_NListtree_Quiet,TRUE);
	DoMethod(folder_tree,MUIM_NList_Clear);
	while (f)
	{
		DoMethod(folder_tree,MUIM_NListtree_Insert,"" /*name*/, f, /*udata */
						 MUIV_NListtree_Insert_ListNode_Root,MUIV_NListtree_Insert_PrevNode_Tail,0/*flags*/);
		f = folder_next(f);
	}
	nnset(folder_tree,MUIA_NList_Active,act);
	set(folder_tree,MUIA_NListtree_Quiet,FALSE);
	main_refresh_folders_text();
}

/******************************************************************
 Refreshs a single folder entry
*******************************************************************/
void main_refresh_folder(struct folder *folder)
{
	struct MUI_NListtree_TreeNode *tree_node = FindListtreeUserData(folder_tree, folder);
	if (tree_node)
	{
		DoMethod(folder_tree,MUIM_NListtree_Redraw,tree_node,0);
	}
	main_refresh_folders_text();
}

/******************************************************************
 Inserts a new mail into the listview
*******************************************************************/
void main_insert_mail(struct mail *mail)
{
	DoMethod(mail_tree,MUIM_NListtree_Insert,"" /*name*/, mail, /*udata */
					 MUIV_NListtree_Insert_ListNode_Root,MUIV_NListtree_Insert_PrevNode_Tail,0/*flags*/);
}

/******************************************************************
 Remove a given mail from the listview
*******************************************************************/
void main_remove_mail(struct mail *mail)
{
	struct MUI_NListtree_TreeNode *treenode = FindListtreeUserData(mail_tree, mail);
	if (treenode)
	{
		DoMethod(mail_tree, MUIM_NListtree_Remove, NULL, treenode,0);
	}
}

/******************************************************************
 Replaces a mail with a new mail
*******************************************************************/
void main_replace_mail(struct mail *oldmail, struct mail *newmail)
{
	struct MUI_NListtree_TreeNode *treenode = FindListtreeUserData(mail_tree, oldmail);
	if (treenode)
	{
/*		DoMethod(mail_tree, MUIM_NListtree_Rename, treenode, newmail, MUIV_NListtree_Rename_Flag_User);*/
		set(mail_tree, MUIA_NListtree_Quiet, TRUE);
		DoMethod(mail_tree, MUIM_NListtree_Remove, NULL, treenode,0);
		main_insert_mail(newmail);
		set(mail_tree, MUIA_NListtree_Quiet, FALSE);
	}
}

/******************************************************************
 Refresh a mail (if it status has changed fx)
*******************************************************************/
void main_refresh_mail(struct mail *m)
{
	struct MUI_NListtree_TreeNode *treenode = FindListtreeUserData(mail_tree, m);
	if (treenode)
	{
		DoMethod(mail_tree, MUIM_NListtree_Redraw, treenode, 0);
		main_refresh_folders_text();
	}
}

/******************************************************************
 Updates the mail trees with the mails in the given folder
*******************************************************************/
static void main_insert_mail_threaded(struct folder *folder, struct mail *mail, void *parentnode)
{
	int mail_flags = 0;
	struct mail *submail;
	APTR newnode;

	if ((submail = mail->sub_thread_mail))
	{
		mail_flags = TNF_LIST|TNF_OPEN;
	}

	newnode = (APTR)DoMethod(mail_tree,MUIM_NListtree_Insert,"" /*name*/, mail, /*udata */
				 parentnode,MUIV_NListtree_Insert_PrevNode_Tail,mail_flags);

	while (submail)
	{
		main_insert_mail_threaded(folder,submail,newnode);
		submail = submail->next_thread_mail;
	}
}

/******************************************************************
 Updates the mail trees with the mails in the given folder
*******************************************************************/
void main_set_folder_mails(struct folder *folder)
{ 
	void *handle = NULL;
	struct mail *m;
	int primary_sort = folder_get_primary_sort(folder)&FOLDER_SORT_MODEMASK;
	int threaded = folder->type == FOLDER_TYPE_MAILINGLIST;

	set(mail_tree, MUIA_NListtree_Quiet, TRUE);

/*	DoMethod(mail_tree, MUIM_NListtree_Remove, MUIV_NListtree_Remove_ListNode_Root, MUIV_NListtree_Remove_TreeNode_All, 0);*/
	DoMethod(mail_tree, MUIM_NList_Clear);

	set(mail_tree, MUIA_MailTreelist_FolderType, folder_get_type(folder));

	if ((primary_sort == FOLDER_SORT_FROMTO || primary_sort == FOLDER_SORT_SUBJECT) && !threaded)
	{
		struct mail *lm = NULL; /* last mail */
		APTR treenode = NULL;

		SetAttrs(mail_tree,
				MUIA_NListtree_TreeColumn, (primary_sort==2)?3:(folder_get_type(folder)==FOLDER_TYPE_SEND?2:1),
				MUIA_NListtree_ShowTree, TRUE,
				TAG_DONE);

		while ((m = folder_next_mail(folder,&handle)))
		{
			if (primary_sort == FOLDER_SORT_FROMTO)
			{
				int res;

				if (folder_get_type(folder) == FOLDER_TYPE_SEND) res = mystricmp(m->to, lm->to);
				else res = mystricmp(m->from, lm->from);

				if (!lm || res)
				{
					treenode = (APTR)DoMethod(mail_tree, MUIM_NListtree_Insert, (folder_get_type(folder) == FOLDER_TYPE_SEND)?m->to:m->from, MUIV_MailTreelist_UserData_Name, /* special hint */
							 MUIV_NListtree_Insert_ListNode_Root,MUIV_NListtree_Insert_PrevNode_Tail,TNF_LIST|TNF_OPEN);
				}
			} else
			{
				if (!lm || mystricmp(m->subject,lm->subject))
				{
					treenode = (APTR)DoMethod(mail_tree, MUIM_NListtree_Insert, m->subject, MUIV_MailTreelist_UserData_Name, /* special hint */
							 MUIV_NListtree_Insert_ListNode_Root,MUIV_NListtree_Insert_PrevNode_Tail,TNF_LIST|TNF_OPEN);
				}
			}

			if (!treenode) break;

			DoMethod(mail_tree, MUIM_NListtree_Insert,"" /*name*/, m, /*udata */
						 treenode,MUIV_NListtree_Insert_PrevNode_Tail,0/*flags*/);

			lm = m;
		}
	} else
	{
		if (!threaded)
		{
			SetAttrs(mail_tree,
					MUIA_NListtree_TreeColumn, 0,
					MUIA_NListtree_ShowTree, FALSE,
					TAG_DONE);

			while ((m = folder_next_mail(folder,&handle)))
			{
				DoMethod(mail_tree,MUIM_NListtree_Insert,"" /*name*/, m, /*udata */
							 MUIV_NListtree_Insert_ListNode_Root,MUIV_NListtree_Insert_PrevNode_Tail,0/*flags*/);
			}
		} else
		{
			SetAttrs(mail_tree,
					MUIA_NListtree_TreeColumn, 0,
					MUIA_NListtree_ShowTree, TRUE,
					TAG_DONE);

			while ((m = folder_next_mail(folder,&handle)))
			{
				if (!m->child_mail)
					main_insert_mail_threaded(folder,m,(void*)MUIV_NListtree_Insert_ListNode_Root);
			}
		}
	}

	SetAttrs(mail_tree,
				MUIA_NListtree_Quiet, FALSE,
				TAG_DONE);

	mailtreelist_update_title_markers();
}

/******************************************************************
 Returns the current selected folder, NULL if no real folder
 has been selected. It returns the true folder like it is
 in the folder list
*******************************************************************/
struct folder *main_get_folder(void)
{
	struct MUI_NListtree_TreeNode *tree_node;
	tree_node = (struct MUI_NListtree_TreeNode *)xget(folder_tree,MUIA_NListtree_Active);

	if (tree_node)
	{
		if (tree_node->tn_User)
		{
			return (struct folder*)tree_node->tn_User;
		}
	}
	return NULL;
	
}

/******************************************************************
 Returns the current selected folder drawer, NULL if no folder
 has been selected
*******************************************************************/
char *main_get_folder_drawer(void)
{
	struct folder *folder = main_get_folder();
	if (folder) return folder->path;
	return NULL;
}

/******************************************************************
 Returns the active mail. NULL if no one is active
*******************************************************************/
struct mail *main_get_active_mail(void)
{
	struct MUI_NListtree_TreeNode *tree_node;
	tree_node = (struct MUI_NListtree_TreeNode *)xget(mail_tree,MUIA_NListtree_Active);

	if (tree_node)
	{
		if (tree_node->tn_User && tree_node->tn_User != (void*)MUIV_MailTreelist_UserData_Name)
		{
			return (struct mail*)tree_node->tn_User;
		}
	}
	return NULL;
}

/******************************************************************
 Returns the filename of the active mail, NULL if no thing is
 selected
*******************************************************************/
char *main_get_mail_filename(void)
{
	struct mail *m = main_get_active_mail();
	if (m) return m->filename;
	return NULL;
}

/******************************************************************
 Returns the first selected mail. NULL if no mail is selected
*******************************************************************/
struct mail *main_get_mail_first_selected(void *handle)
{
	struct MUI_NListtree_TreeNode *treenode = (struct MUI_NListtree_TreeNode *)MUIV_NListtree_NextSelected_Start;
	DoMethod(mail_tree, MUIM_NListtree_NextSelected, &treenode);
	if (treenode == (struct MUI_NListtree_TreeNode *)MUIV_NListtree_NextSelected_End) return NULL;
	*((struct MUI_NListtree_TreeNode **)handle) = treenode;
	return (struct mail*)treenode->tn_User;
}

/******************************************************************
 Returns the next selected mail. NULL if no more mails are
 selected
*******************************************************************/
struct mail *main_get_mail_next_selected(void *handle)
{
	struct MUI_NListtree_TreeNode *treenode = *((struct MUI_NListtree_TreeNode **)handle);
	DoMethod(mail_tree, MUIM_NListtree_NextSelected, &treenode);
	if (treenode == (struct MUI_NListtree_TreeNode *)MUIV_NListtree_NextSelected_End) return NULL;
	*((struct MUI_NListtree_TreeNode **)handle) = treenode;
	return (struct mail*)treenode->tn_User;
}

/******************************************************************
 Remove all selected mails,
*******************************************************************/
void main_remove_mails_selected(void)
{
/*
	DoMethod(mail_tree, MUIM_NListtree_Remove,
			MUIV_NListtree_Remove_ListNode_Root, MUIV_NListtree_Remove_TreeNode_Selected, 0);
*/

	struct MUI_NListtree_TreeNode *treenode;
	int j = 0, i = 0;
	struct MUI_NListtree_TreeNode **array;

	set(mail_tree, MUIA_NListtree_Quiet, TRUE);

	treenode = (struct MUI_NListtree_TreeNode *)MUIV_NListtree_PrevSelected_Start;

	for (;;)
	{
		DoMethod(mail_tree, MUIM_NListtree_PrevSelected, &treenode);
		if (treenode==(struct MUI_NListtree_TreeNode *)MUIV_NListtree_PrevSelected_End)
			break;
		i++;
	}

	if ((array = (struct MUI_NListtree_TreeNode **)AllocVec(sizeof(struct MUI_NListtree_TreeNode *)*i,0)))
	{
		treenode = (struct MUI_NListtree_TreeNode *)MUIV_NListtree_PrevSelected_Start;

		for (;;)
		{
			DoMethod(mail_tree, MUIM_NListtree_PrevSelected, &treenode);
			if (treenode==(struct MUI_NListtree_TreeNode *)MUIV_NListtree_PrevSelected_End)
				break;
			array[j++] = treenode;
		}

		for (i=0;i<j;i++)
		{
			if (array[i]->tn_Flags & TNF_LIST)
			{
				struct MUI_NListtree_TreeNode *node = (struct MUI_NListtree_TreeNode *)
					DoMethod(mail_tree, MUIM_NListtree_GetEntry, array[i], MUIV_NListtree_GetEntry_Position_Head, 0);

				while (node)
				{
					struct MUI_NListtree_TreeNode *nextnode = (struct MUI_NListtree_TreeNode *)
						DoMethod(mail_tree, MUIM_NListtree_GetEntry, node, MUIV_NListtree_GetEntry_Position_Next, 0);

					DoMethod(mail_tree, MUIM_NListtree_Move, array[i], node, MUIV_NListtree_Move_NewListNode_Root, MUIV_NListtree_Move_NewTreeNode_Tail);
					node = nextnode;
				}
			}

			DoMethod(mail_tree, MUIM_NListtree_Remove, MUIV_NListtree_Remove_ListNode_Root,array[i],0);
		}		

		FreeVec(array);
	}

	set(mail_tree, MUIA_NListtree_Quiet, FALSE);
}
