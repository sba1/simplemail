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
static Object *tree_folder, *tree_mail;

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

	nnset(tree_mail, MUIA_NList_TitleMark, sortmode2titlemark(folder_get_primary_sort(folder),folder_get_type(folder)));
	nnset(tree_mail, MUIA_NList_TitleMark2, sortmode2titlemark(folder_get_secondary_sort(folder),folder_get_type(folder)));
}

/******************************************************************
 A mail has been dropped onto a folder
*******************************************************************/
static void foldertreelist_maildrop(void)
{
	struct folder *folder = (struct folder*)xget(tree_folder, MUIA_FolderTreelist_MailDrop);
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
	LONG title_mark = xget(tree_mail, MUIA_NList_TitleMark);
	LONG title_click = xget(tree_mail, MUIA_NList_TitleClick);
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

	set(tree_mail, MUIA_NList_TitleMark, title_mark);

	if (folder)
	{
		folder_set_primary_sort(folder, titlemark2sortmode(title_mark));
		main_set_folder_mails(folder);
	}
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
        
		WindowContents, VGroup,
			Child, HGroupV,
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
					End,
				End,
			Child, HGroup,
				Child, NListviewObject,
					MUIA_HorizWeight, 33,
					MUIA_CycleChain, 1,
					MUIA_NListview_NList, tree_folder = FolderTreelistObject,
						MUIA_NList_Exports, MUIV_NList_Exports_ColWidth|MUIV_NList_Exports_ColOrder,
						MUIA_NList_Imports, MUIV_NList_Imports_ColWidth|MUIV_NList_Imports_ColOrder,
						MUIA_ObjectID, MAKE_ID('M','W','F','T'),
						End,
					End,
				Child, BalanceObject, End,
				Child, NListviewObject,
					MUIA_CycleChain,1,
					MUIA_NListview_NList, tree_mail = MailTreelistObject,
						MUIA_NList_TitleMark, MUIV_NList_TitleMark_Down | 4,
						MUIA_NList_Exports, MUIV_NList_Exports_ColWidth|MUIV_NList_Exports_ColOrder,
						MUIA_NList_Imports, MUIV_NList_Imports_ColWidth|MUIV_NList_Imports_ColOrder,
						MUIA_ObjectID, MAKE_ID('M','W','M','T'),
						End,
					End,
			End,
			Child, TextObject,
				TextFrame,
				End,
			End,
		End;
	
	if(win_main != NULL)
	{
		set(tree_folder,MUIA_UserData,tree_mail); /* for the drag'n'drop support */
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
		DoMethod(tree_mail, MUIM_Notify, MUIA_NListtree_DoubleClick, MUIV_EveryTime, MUIV_Notify_Application, 3,  MUIM_CallHook, &hook_standard, callback_read_mail);
		DoMethod(tree_mail, MUIM_Notify, MUIA_NList_TitleClick, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, mailtreelist_title_click);
		DoMethod(tree_folder, MUIM_Notify, MUIA_NListtree_Active, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_folder_active);
		DoMethod(tree_folder, MUIM_Notify, MUIA_FolderTreelist_MailDrop, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, foldertreelist_maildrop);

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
 Refreshs the folder list
*******************************************************************/
void main_refresh_folders(void)
{
	struct folder *f = folder_first();

	int act = xget(tree_folder, MUIA_NList_Active);

	set(tree_folder,MUIA_NListtree_Quiet,TRUE);
	DoMethod(tree_folder,MUIM_NList_Clear);
	while (f)
	{
		DoMethod(tree_folder,MUIM_NListtree_Insert,"" /*name*/, f, /*udata */
						 MUIV_NListtree_Insert_ListNode_Root,MUIV_NListtree_Insert_PrevNode_Tail,0/*flags*/);
		f = folder_next(f);
	}
	nnset(tree_folder,MUIA_NList_Active,act);
	set(tree_folder,MUIA_NListtree_Quiet,FALSE);
}

/******************************************************************
 Refreshs a single folder entry
*******************************************************************/
void main_refresh_folder(struct folder *folder)
{
	struct MUI_NListtree_TreeNode *tree_node = FindListtreeUserData(tree_folder, folder);
	if (tree_node)
	{
		DoMethod(tree_folder,MUIM_NListtree_Redraw,tree_node,0);
	}
}

/******************************************************************
 Inserts a new mail into the listview
*******************************************************************/
void main_insert_mail(struct mail *mail)
{
	DoMethod(tree_mail,MUIM_NListtree_Insert,"" /*name*/, mail, /*udata */
					 MUIV_NListtree_Insert_ListNode_Root,MUIV_NListtree_Insert_PrevNode_Tail,0/*flags*/);
}

/******************************************************************
 Remove a given mail from the listview
*******************************************************************/
void main_remove_mail(struct mail *mail)
{
	struct MUI_NListtree_TreeNode *treenode = FindListtreeUserData(tree_mail, mail);
	if (treenode)
	{
		DoMethod(tree_mail, MUIM_NListtree_Remove, NULL, treenode,0);
	}
}

/******************************************************************
 Replaces a mail with a new mail
*******************************************************************/
void main_replace_mail(struct mail *oldmail, struct mail *newmail)
{
	struct MUI_NListtree_TreeNode *treenode = FindListtreeUserData(tree_mail, oldmail);
	if (treenode)
	{
/*		DoMethod(tree_mail, MUIM_NListtree_Rename, treenode, newmail, MUIV_NListtree_Rename_Flag_User);*/
		set(tree_mail, MUIA_NListtree_Quiet, TRUE);
		DoMethod(tree_mail, MUIM_NListtree_Remove, NULL, treenode,0);
		main_insert_mail(newmail);
		set(tree_mail, MUIA_NListtree_Quiet, FALSE);
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

	set(tree_mail, MUIA_NListtree_Quiet, TRUE);
	DoMethod(tree_mail, MUIM_NListtree_Remove, MUIV_NListtree_Remove_ListNode_Root, MUIV_NListtree_Remove_TreeNode_All, 0);

	set(tree_mail, MUIA_MailTreelist_FolderType, folder_get_type(folder));

	if (primary_sort == FOLDER_SORT_FROMTO || primary_sort == FOLDER_SORT_SUBJECT)
	{
		struct mail *lm = NULL; /* last mail */
		APTR treenode = NULL;

		SetAttrs(tree_mail,
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
					treenode = (APTR)DoMethod(tree_mail, MUIM_NListtree_Insert, (folder_get_type(folder) == FOLDER_TYPE_SEND)?m->to:m->from, MUIV_MailTreelist_UserData_Name, /* special hint */
							 MUIV_NListtree_Insert_ListNode_Root,MUIV_NListtree_Insert_PrevNode_Tail,TNF_LIST|TNF_OPEN);
				}
			} else
			{
				if (!lm || mystricmp(m->subject,lm->subject))
				{
					treenode = (APTR)DoMethod(tree_mail, MUIM_NListtree_Insert, m->subject, MUIV_MailTreelist_UserData_Name, /* special hint */
							 MUIV_NListtree_Insert_ListNode_Root,MUIV_NListtree_Insert_PrevNode_Tail,TNF_LIST|TNF_OPEN);
				}
			}

			if (!treenode) break;

			DoMethod(tree_mail, MUIM_NListtree_Insert,"" /*name*/, m, /*udata */
						 treenode,MUIV_NListtree_Insert_PrevNode_Tail,0/*flags*/);

			lm = m;
		}
	} else
	{
		SetAttrs(tree_mail,
				MUIA_NListtree_TreeColumn, 0,
				MUIA_NListtree_ShowTree, FALSE,
				TAG_DONE);

		while ((m = folder_next_mail(folder,&handle)))
		{
			DoMethod(tree_mail,MUIM_NListtree_Insert,"" /*name*/, m, /*udata */
						 MUIV_NListtree_Insert_ListNode_Root,MUIV_NListtree_Insert_PrevNode_Tail,0/*flags*/);
		}
	}

	SetAttrs(tree_mail,
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
	tree_node = (struct MUI_NListtree_TreeNode *)xget(tree_folder,MUIA_NListtree_Active);

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
	tree_node = (struct MUI_NListtree_TreeNode *)xget(tree_mail,MUIA_NListtree_Active);

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
	DoMethod(tree_mail, MUIM_NListtree_NextSelected, &treenode);
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
	DoMethod(tree_mail, MUIM_NListtree_NextSelected, &treenode);
	if (treenode == (struct MUI_NListtree_TreeNode *)MUIV_NListtree_NextSelected_End) return NULL;
	*((struct MUI_NListtree_TreeNode **)handle) = treenode;
	return (struct mail*)treenode->tn_User;
}

/******************************************************************
 Remove all selected mails,
*******************************************************************/
void main_remove_mails_selected(void)
{
	DoMethod(tree_mail, MUIM_NListtree_Remove,
			MUIV_NListtree_Remove_ListNode_Root, MUIV_NListtree_Remove_TreeNode_Selected, 0);
/*
	struct MUI_NListtree_TreeNode *treenode;
	int j = 0, i = 0;
	struct MUI_NListtree_TreeNode **array;

	treenode = (struct MUI_NListtree_TreeNode *)MUIV_NListtree_PrevSelected_Start;

	for (;;)
	{
		DoMethod(tree_mail, MUIM_NListtree_PrevSelected, &treenode);
		if (treenode==(struct MUI_NListtree_TreeNode *)MUIV_NListtree_PrevSelected_End)
			break;
		i++;
	}

	if ((array = (struct MUI_NListtree_TreeNode **)AllocVec(sizeof(struct MUI_NListtree_TreeNode *)*i,0)))
	{
		treenode = (struct MUI_NListtree_TreeNode *)MUIV_NListtree_PrevSelected_Start;

		for (;;)
		{
			DoMethod(tree_mail, MUIM_NListtree_PrevSelected, &treenode);
			if (treenode==(struct MUI_NListtree_TreeNode *)MUIV_NListtree_PrevSelected_End)
				break;
			array[j++] = treenode;
		}

		for (i=0;i<j;i++)
		{
			DoMethod(tree_mail, MUIM_NListtree_Remove, MUIV_NListtree_Remove_ListNode_Root,array[i],0);
		}		

		FreeVec(array);
	}*/
}
