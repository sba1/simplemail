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
#include <stdlib.h>

#include <libraries/gadtools.h>
#include <libraries/iffparse.h> /* MAKE_ID */
#include <libraries/mui.h>
#include <libraries/asl.h>
#include <mui/nlistview_mcc.h>
#include <mui/nlisttree_mcc.h>
#include <clib/alib_protos.h>
#include <proto/asl.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "SimpleMail_rev.h"

#include "account.h"
#include "addressbook.h"
#include "arexx.h"
#include "configuration.h"
#include "debug.h"
#include "folder.h"
#include "mail.h"
#include "readwnd.h"
#include "simplemail.h"
#include "smintl.h"
#include "support.h"
#include "support_indep.h"

#include "addressentrylistclass.h"
#include "amigasupport.h"
#include "compiler.h"
#include "foldertreelistclass.h"
#include "mainwnd.h"
#include "mailtreelistclass.h"
#include "messageviewclass.h"
#include "muistuff.h"
#include "picturebuttonclass.h"
#include "popupmenuclass.h"

/*****************************************************/
/* Minimum requirements of some external MUI classes */

#define NLISTTREE_MIN_VERSION   18
#define NLISTTREE_MIN_REVISION  22

#define NLIST_MIN_VERSION   20
#define NLIST_MIN_REVISION  115

/*****************************************************/


static Object *win_main;
static Object *main_menu;
static Object *main_settings_folder_menuitem;
static Object *main_settings_addressbook_menuitem;
static Object *main_settings_messageview_menuitem;
static Object *main_scripts_menu;
static Object *main_scripts_execute_menuitem;
static Object *main_group;
static Object *button_fetch;
static Object *button_send;
static Object *button_read;
static Object *button_getadd;
static Object *button_delete;
static Object *button_change;
static Object *button_move;
static Object *button_new;
static Object *button_reply;
static Object *button_forward;
static Object *button_search;
static Object *button_filter;
static Object *button_efilter;
static Object *button_spamcheck;
static Object *button_isolate;
static Object *button_abook;
static Object *button_config;
static Object *switch1_button; /* switch button for the two views */
static Object *switch2_button; /* switch button for the two views */
static Object *mail_tree;
static Object *mail_tree_group;
static Object *mail_listview;
static Object *mail_messageview;
static Object *folder_listview_group;
static Object *folder_listview;
static Object *folder_tree;
static Object *folder2_text; /* the text field below the folder list tree */
static Object *folder_balance;
static Object *folder_group;
static Object *folder_text;
static Object *folder_popupmenu;
static Object *address_listview_group;
static Object *address_listview;
static Object *address_list;
static Object *left_listview_group;
static Object *left_listview_balance;
static Object *right_group;
static Object *right_balance;
static Object *project_checksingleaccount_menuitem;
static Object *status_text;

/* For the Balance Snapshot */
static Object *balance_text;
static LONG Weights[6] = {33, 100, 100, 100, 100, 100};

static int folders_in_popup;

static void main_refresh_folders_text(void);
static void settings_show_changed(void);

struct MUI_NListtree_TreeNode *FindListtreeUserData(Object *tree, APTR udata)
{
	set(tree, MUIA_NListtree_FindUserDataHook, MUIV_NListtree_FindUserDataHook_PointerCompare);
	return (struct MUI_NListtree_TreeNode *)DoMethod(tree, MUIM_NListtree_FindUserData,  MUIV_NListtree_FindUserData_ListNode_Root, udata, 0);
/*

	int i;
	int count = DoMethod(tree,MUIM_NListtree_GetNr, MUIV_NListtree_GetNr_TreeNode_Active,MUIV_NListtree_GetNr_Flag_CountAll);

	for (i=0;i<count;i++)
	{
		struct MUI_NListtree_TreeNode *tn = (struct MUI_NListtree_TreeNode*)
			DoMethod(tree,MUIM_NListtree_GetEntry,MUIV_NListtree_GetEntry_ListNode_Root,i,0);
		if (tn->tn_User == udata) return tn;
	}
	return NULL;*/
}

/******************************************************************
 Display the about Requester
*******************************************************************/
void display_about(void)
{
	MUI_Request(App, NULL, 0, 
		_("SimpleMail - About"),
		_("*Ok"),
		"SimpleMail version %ld.%ld\n\n%s 2000-2005\nHynek Schlawack %s Sebastian Bauer\n%s.",
		VERSION,REVISION,_("Copyright (c)"),_("and"),_("Released under the terms of the GNU Public License"));
}

/******************************************************************
 Converts a given sort mode to a nlist title mark
*******************************************************************/
static ULONG sortmode2titlemark(int sortmode, int type)
{
	int col = 0;
/*	if (sortmode == FOLDER_SORT_THREAD) return 0;*/
	col = sortmode & FOLDER_SORT_MODEMASK;
	return (col | ((sortmode & FOLDER_SORT_REVERSE)?MUIV_NList_TitleMark_Up:MUIV_NList_TitleMark_Down));
}

/******************************************************************
 Converts a given nlist title mark to a sort mode
*******************************************************************/
static int titlemark2sortmode(int titlemark)
{
	int col = titlemark & MUIV_NList_TitleMark_ColMask;
/*	if (titlemark == 0) return FOLDER_SORT_THREAD;*/
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
 The order of the folders has been changed.
*******************************************************************/
static void foldertreelist_orderchanged(void)
{
	int i;
	int count = DoMethod(folder_tree,MUIM_NListtree_GetNr, MUIV_NListtree_GetNr_TreeNode_Active,MUIV_NListtree_GetNr_Flag_CountAll);

	folder_unlink_all();

	for (i=0;i<count;i++)
	{
		struct MUI_NListtree_TreeNode *tn;
		struct MUI_NListtree_TreeNode *parent;
		struct folder *f;

		tn = (struct MUI_NListtree_TreeNode*)DoMethod(folder_tree,MUIM_NListtree_GetEntry,MUIV_NListtree_GetEntry_ListNode_Root,i,0);
		if (!tn) return;

		parent = (struct MUI_NListtree_TreeNode*)DoMethod(folder_tree,MUIM_NListtree_GetEntry,tn,MUIV_NListtree_GetEntry_Position_Parent,0);
		if (!parent) return;

		f = (struct folder*)tn->tn_User;

		folder_add_to_tree(f,parent?(struct folder*)parent->tn_User:NULL);
	}
}

/******************************************************************
 An entry has been doubleclicked
*******************************************************************/
static void foldertreelist_doubleclick(void)
{
	callback_edit_folder();
}

/******************************************************************
 An addressbook entry has been doubleclicked
*******************************************************************/
static void addressentrylist_doubleclick(void)
{
	struct addressbook_entry_new *entry;

	DoMethod(address_list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &entry);
	if (entry && entry->email_array)
		callback_write_mail_to_str(entry->email_array[0],NULL);
}

/******************************************************************
 The Mailtree's title has been clicked, so change the primary sort
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
		folder_config_save(folder);
		main_set_folder_mails(folder);
		read_refresh_prevnext_button(folder);
	}
}


/******************************************************************
 The Mailtree's title has been shift-clicked, so change the secondary
 sort  mode if necessary
*******************************************************************/
static void mailtreelist_title_click2(void)
{
	LONG title_mark = xget(mail_tree, MUIA_NList_TitleMark2);
	LONG title_click = xget(mail_tree, MUIA_NList_TitleClick2);
	struct folder *folder = main_get_folder();

	if ((title_mark & MUIV_NList_TitleMark2_ColMask) == title_click)
	{
		switch (title_mark & MUIV_NList_TitleMark2_TypeMask)
		{
			case	MUIV_NList_TitleMark2_Down:
						title_mark = title_click | MUIV_NList_TitleMark2_Up;
						break;

			case	MUIV_NList_TitleMark2_Up:
						title_mark = title_click | MUIV_NList_TitleMark2_Down;
						break;
		}
	} else
	{
		title_mark = title_click | MUIV_NList_TitleMark2_Down;
	}

	set(mail_tree, MUIA_NList_TitleMark2, title_mark);

	if (folder)
	{
		folder_set_secondary_sort(folder, titlemark2sortmode(title_mark));
		folder_config_save(folder);
		main_set_folder_mails(folder);
		read_refresh_prevnext_button(folder);
	}
}

/******************************************************************
 Switch the view of the folders
*******************************************************************/
static void menu_check_single_account(int *val)
{
	callback_check_single_account(*val);
}

/******************************************************************
 Switch the view of the folders
*******************************************************************/
static void menu_execute_script(int *val_ptr)
{
	int val = *val_ptr;
	if (val == -1)
	{
		struct FileRequester *file_req = AllocAslRequest(ASL_FileRequest, NULL);
		if (file_req)
		{
			static char *initial_drawer;
			static char *initial_file;
			static char *initial_pattern;
			static int initial_call;

			if (!initial_call)
			{
				initial_pattern = mystrdup("#?.smrx");
				initial_drawer = mystrdup("PROGDIR:ARexx");
				initial_call = 1;
			}

			if (AslRequestTags(file_req,
					ASLFR_Screen, xget(win_main,MUIA_Window_Screen),
					ASLFR_InitialDrawer, initial_drawer,
					initial_file?ASLFR_InitialFile:TAG_IGNORE, initial_file,
					ASLFR_InitialPattern, initial_pattern,
					ASLFR_DoPatterns, TRUE,
					ASLFR_RejectIcons, TRUE,
					TAG_DONE))
			{
				char *full_path;

				free(initial_drawer);
				free(initial_file);
				free(initial_pattern);

				initial_drawer = mystrdup(file_req->fr_Drawer);
				initial_file = mystrdup(file_req->fr_File);
				initial_pattern = mystrdup(file_req->fr_Pattern);

				if ((full_path = mycombinepath(initial_drawer, initial_file)))
				{
					arexx_execute_script(full_path);
					free(full_path);
				}
			}
			FreeAslRequest(file_req);
		}
	}
}

/******************************************************************
 Switch the view of the folders
*******************************************************************/
static void switch_folder_view(void)
{
	set(main_settings_folder_menuitem, MUIA_Menuitem_Checked, !xget(main_settings_folder_menuitem,MUIA_Menuitem_Checked));
	settings_show_changed();
}

/******************************************************************
 A new entry in the popupmenu has been selected
*******************************************************************/
static void popup_selected(void)
{
	LONG newact = xget(folder_popupmenu,MUIA_Popupmenu_SelectedData);
	if (newact != -1)
	{
		struct folder *f = folder_find(newact);
		if (f)
		{
			APTR treenode = FindListtreeUserData(folder_tree, f);
			if (treenode)
			{
				set(folder_tree, MUIA_NListtree_Active, treenode);
			}
		}
	}
}

/******************************************************************
 Some show settings has been changed
*******************************************************************/
static void settings_show_changed(void)
{
	int folder, addressbook, messageview;

	DoMethod(main_group, MUIM_Group_InitChange);
	DoMethod(mail_tree_group, MUIM_Group_InitChange);
	DoMethod(right_group, MUIM_Group_InitChange);

	folder = xget(main_settings_folder_menuitem,MUIA_Menuitem_Checked);
	addressbook = xget(main_settings_addressbook_menuitem,MUIA_Menuitem_Checked);
	messageview = xget(main_settings_messageview_menuitem,MUIA_Menuitem_Checked);

	set(folder_group, MUIA_ShowMe, !folder);
	set(folder_listview_group, MUIA_ShowMe, folder);
	set(address_listview_group,MUIA_ShowMe, addressbook);
	set(folder_balance, MUIA_ShowMe, folder || addressbook);
	set(left_listview_group, MUIA_ShowMe, folder || addressbook);
	set(left_listview_balance, MUIA_ShowMe, folder && addressbook);
	set(right_balance, MUIA_ShowMe, messageview);
	set(mail_messageview, MUIA_ShowMe, messageview);

	DoMethod(right_group, MUIM_Group_ExitChange);
	DoMethod(mail_tree_group, MUIM_Group_ExitChange);
	DoMethod(main_group, MUIM_Group_ExitChange);
}

/******************************************************************
 Loads environment and sets the balance groups
*******************************************************************/
void main_load_environment(void)
{
	char *ls;
	int i=0;
	LONG count, weight;

	DoMethod(App, MUIM_Application_Load, MUIV_Application_Load_ENV);
	if (!*(ls = (STRPTR)xget(balance_text, MUIA_String_Contents))) ls = "33 100 100 100 100 100";
	/* I'm not allowed to use sscanf() */
	count = StrToLong(ls, &weight);
	while (count != -1)
	{
		if (i<6) Weights[i++] = weight;
		ls += count;
		count = StrToLong(ls, &weight);
	}
	set(left_listview_group, MUIA_HorizWeight, Weights[0]);
	set(right_group, MUIA_HorizWeight, Weights[1]);
	set(folder_listview_group, MUIA_VertWeight, Weights[2]);
	set(address_listview_group, MUIA_VertWeight, Weights[3]);
	set(mail_listview, MUIA_VertWeight, Weights[4]);
	set(mail_messageview, MUIA_VertWeight, Weights[5]);
}

/******************************************************************
 Saves environment and snapshots the balance groups
*******************************************************************/
void main_save_environment(void)
{
	char buf[80];

	Weights[0] = xget(left_listview_group, MUIA_HorizWeight);
	Weights[1] = xget(right_group, MUIA_HorizWeight);
	Weights[2] = xget(folder_listview_group, MUIA_VertWeight);
	Weights[3] = xget(address_listview_group, MUIA_VertWeight);
	Weights[4] = xget(mail_listview, MUIA_VertWeight);
	Weights[5] = xget(mail_messageview, MUIA_VertWeight);

	sprintf(buf, "%ld %ld %ld %ld %ld %ld", Weights[0], Weights[1], Weights[2], Weights[3], Weights[4], Weights[5]);
	setstring(balance_text, buf);

	DoMethod(App, MUIM_Application_Save, MUIV_Application_Save_ENV);
	DoMethod(App, MUIM_Application_Save, MUIV_Application_Save_ENVARC);
}

/******************************************************************
 Initialize the main window
*******************************************************************/
int main_window_init(void)
{
	int rc;
	enum {
		MENU_PROJECT,
		MENU_PROJECT_ABOUT = 1,
		MENU_PROJECT_ABOUTMUI,
		MENU_PROJECT_IMPORTMBOX,
		MENU_PROJECT_IMPORTDBX,
		MENU_PROJECT_FETCH,
		MENU_PROJECT_CHECKSINGLEACCOUNT,
		MENU_PROJECT_SEND,
		MENU_PROJECT_QUIT,

		MENU_FOLDER,
		MENU_FOLDER_NEWGROUP,
		MENU_FOLDER_NEWFOLDER,
		MENU_FOLDER_RESCAN,
		MENU_FOLDER_DELETE,
		MENU_FOLDER_OPTIONS,
		MENU_FOLDER_ORDER,
		MENU_FOLDER_ORDER_SAVE,
		MENU_FOLDER_ORDER_RESET,
		MENU_FOLDER_DELALLINDEX,
		MENU_FOLDER_SAVEALLINDEX,
		MENU_FOLDER_IMPORTMBOX,
		MENU_FOLDER_EXPORT,
		MENU_FOLDER_SPAMCHECK,
		MENU_FOLDER_MOVESPAM,
		MENU_FOLDER_HAM,

		MENU_MESSAGE_NEW,
		MENU_MESSAGE_REPLY,
		MENU_MESSAGE_FORWARD,
		MENU_MESSAGE_READ,
		MENU_MESSAGE_EDIT,
		MENU_MESSAGE_MOVE,
		MENU_MESSAGE_COPY,
		MENU_MESSAGE_DELETE,

		MENU_SETTINGS,
		MENU_SETTINGS_SHOW_FOLDERS,
		MENU_SETTINGS_SHOW_ADDRESSBOOK,
		MENU_SETTINGS_SHOW_SELECTED_MESSAGE,
		MENU_SETTINGS_CONFIGURATION,
		MENU_SETTINGS_FILTER,
		MENU_SETTINGS_MUI,
		MENU_SETTINGS_SAVEPREFS,

		MENU_SCRIPTS,
		MENU_SCRIPTS_EXECUTESCRIPT
	};

	static const struct NewMenu nm_untranslated[] =
	{
		{NM_TITLE, N_("Project"), NULL, 0, 0, NULL},
		{NM_ITEM, N_("?:About..."), NULL, 0, 0, (APTR)MENU_PROJECT_ABOUT},
		{NM_ITEM, N_("About MUI..."), NULL, 0, 0, (APTR)MENU_PROJECT_ABOUTMUI},
		{NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
		{NM_ITEM, N_("Delete all indexfiles"), NULL, 0, 0, (APTR)MENU_FOLDER_DELALLINDEX},
		{NM_ITEM, N_("Save all indexfiles"), NULL, 0, 0, (APTR)MENU_FOLDER_SAVEALLINDEX},
		{NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
		{NM_ITEM, N_("Import mbox file..."), NULL, 0, 0, (APTR)MENU_PROJECT_IMPORTMBOX},
		{NM_ITEM, N_("Import dbx file..."), NULL, 0, 0, (APTR)MENU_PROJECT_IMPORTDBX},
		{NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
		{NM_ITEM, N_("S:Send queued mails..."), NULL, 0, 0, (APTR)MENU_PROJECT_SEND},
		{NM_ITEM, N_("F:Check all active accounts..."), NULL, 0, 0, (APTR)MENU_PROJECT_FETCH},
		{NM_ITEM, N_("Check single account"), NULL, 0, 0, (APTR)MENU_PROJECT_CHECKSINGLEACCOUNT},
		{NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
		{NM_ITEM, N_("Q:Quit"), NULL, 0, 0, (APTR)MENU_PROJECT_QUIT},
		{NM_TITLE, N_("Folder"), NULL, 0, 0, NULL},
		{NM_ITEM, N_("New..."), NULL, 0, 0, (APTR)MENU_FOLDER_NEWFOLDER},
		{NM_ITEM, N_("New group..."), NULL, 0, 0, (APTR)MENU_FOLDER_NEWGROUP},
		{NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
		{NM_ITEM, N_("Delete..."), NULL, 0, 0, (APTR)MENU_FOLDER_DELETE},
		{NM_ITEM, N_("Import mbox file..."), NULL, 0, 0, (APTR)MENU_FOLDER_IMPORTMBOX},
		{NM_ITEM, N_("Export as mbox..."), NULL, 0, 0, (APTR)MENU_FOLDER_EXPORT},
		{NM_ITEM, N_("Rescan"), NULL, 0, 0, (APTR)MENU_FOLDER_RESCAN},
		{NM_ITEM, N_("Options..."), NULL, 0, 0, (APTR)MENU_FOLDER_OPTIONS},
		{NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
		{NM_ITEM, N_("P:Run spam mail check"), NULL, 0, 0, (APTR)MENU_FOLDER_SPAMCHECK},
		{NM_ITEM, N_("O:Isolate spam mails"), NULL, 0, 0, (APTR)MENU_FOLDER_MOVESPAM},
		{NM_ITEM, N_("H:Classify all mails as ham"), NULL, 0, 0, (APTR)MENU_FOLDER_HAM},
		{NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
		{NM_ITEM, N_("Order"), NULL, 0, 0, NULL},
		{NM_SUB, N_("Save"), NULL, 0, 0, (APTR)MENU_FOLDER_ORDER_SAVE},
		{NM_SUB, N_("Reset"), NULL, 0, 0, (APTR)MENU_FOLDER_ORDER_RESET},
		{NM_TITLE, N_("Message"), NULL, 0, 0, NULL},
		{NM_ITEM, N_("N:New..."), NULL, 0, 0, (APTR)MENU_MESSAGE_NEW},
		{NM_ITEM, N_("D:Read..."), NULL, 0, 0, (APTR)MENU_MESSAGE_READ},
		{NM_ITEM, N_("E:Edit..."), NULL, 0, 0, (APTR)MENU_MESSAGE_EDIT},
		{NM_ITEM, N_("R:Reply..."), NULL, 0, 0, (APTR)MENU_MESSAGE_REPLY},
		{NM_ITEM, N_("W:Forward..."), NULL, 0, 0, (APTR)MENU_MESSAGE_FORWARD},
		{NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
		{NM_ITEM, N_("M:Move..."), NULL, 0, 0L, (APTR)MENU_MESSAGE_MOVE},
		{NM_ITEM, N_("Copy..."), NULL, NM_ITEMDISABLED, 0L, (APTR)MENU_MESSAGE_COPY},
		{NM_ITEM, N_("Delete"), "Del", NM_COMMANDSTRING, 0L, (APTR)MENU_MESSAGE_DELETE},
/*
		{NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
		{NM_ITEM, "Print...", NULL, NM_ITEMDISABLED, 0L, NULL},
		{NM_ITEM, "Save", NULL, NM_ITEMDISABLED, 0L, NULL},
		{NM_SUB, "Complete Message...", NULL, 0, 0, NULL},
		{NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
		{NM_SUB, "Remove Attachments", NULL, 0, 0, NULL},
*/
		{NM_TITLE, N_("Settings"), NULL, 0, 0, (APTR)MENU_SETTINGS},
		{NM_ITEM, N_("Show folders?"), NULL, CHECKED|CHECKIT|MENUTOGGLE, 0, (APTR)MENU_SETTINGS_SHOW_FOLDERS},
		{NM_ITEM, N_("Show addressbook?"), NULL, CHECKED|CHECKIT|MENUTOGGLE, 0, (APTR)MENU_SETTINGS_SHOW_ADDRESSBOOK},
		{NM_ITEM, N_("Show selected message?"), NULL, CHECKED|CHECKIT|MENUTOGGLE, 0, (APTR)MENU_SETTINGS_SHOW_SELECTED_MESSAGE},
		{NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
		{NM_ITEM, N_("Configuration..."), NULL, 0, 0, (APTR)MENU_SETTINGS_CONFIGURATION},
		{NM_ITEM, N_("Filters..."), NULL, 0, 0, (APTR)MENU_SETTINGS_FILTER},
		{NM_ITEM, N_("MUI..."), NULL, 0, 0, (APTR)MENU_SETTINGS_MUI},
		{NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
		{NM_ITEM, N_("Save Settings"), NULL, 0, 0, (APTR)MENU_SETTINGS_SAVEPREFS},
		{NM_TITLE, N_("Scripts"), NULL, 0, 0, (APTR)MENU_SCRIPTS},
		{NM_ITEM, N_(".:Execute ARexx Script..."), NULL, 0, 0, (APTR)MENU_SCRIPTS_EXECUTESCRIPT},
		{NM_END, NULL, NULL, 0, 0, NULL}
	};

	struct NewMenu *nm;
	int i;

	SM_ENTER;

	/* translate the menu entries */
	if (!(nm = malloc(sizeof(nm_untranslated)))) return 0;
	memcpy(nm,nm_untranslated,sizeof(nm_untranslated));

	for (i=0;i<ARRAY_LEN(nm_untranslated)-1;i++)
	{
		if (nm[i].nm_Label && nm[i].nm_Label != NM_BARLABEL)
		{
			nm[i].nm_Label = mystrdup(_(nm[i].nm_Label));
			if (nm[i].nm_Label[1] == ':') nm[i].nm_Label[1] = 0;
		}
	}

	rc = FALSE;

	main_menu = MUI_MakeObject(MUIO_MenustripNM, nm, MUIO_MenustripNM_CommandKeyCheck);

	win_main = WindowObject,
		MUIA_HelpNode, "MA_W",
		MUIA_Window_ID, MAKE_ID('M','A','I','N'),
		MUIA_Window_Title, VERS,

		MUIA_Window_Menustrip,main_menu,

		WindowContents, main_group = VGroup,
			Child, HGroupV,
				Child, HGroup,
					MUIA_VertWeight,0,
					Child, HGroup,
						MUIA_Group_Spacing, 0,
						MUIA_Weight, 250,
						Child, button_read = MakePictureButton(_("Rea_d"),"PROGDIR:Images/MailRead"),
						Child, button_change = MakePictureButton(_("_Edit"),"PROGDIR:Images/MailModify"),
						Child, button_move = MakePictureButton(_("_Move"),"PROGDIR:Images/MailMove"),
						Child, button_delete = MakePictureButton(_("De_lete"),"PROGDIR:Images/MailDelete"),
						Child, button_getadd = MakePictureButton(_("Ge_tAdd"),"PROGDIR:Images/MailGetAddress"),
						End,
					Child, HGroup,
						MUIA_Group_Spacing, 0,
						MUIA_Weight, 150,
						Child, button_new = MakePictureButton(_("_New"),"PROGDIR:Images/MailNew"),
						Child, button_reply = MakePictureButton(_("_Reply"),"PROGDIR:Images/MailReply"),
						Child, button_forward = MakePictureButton(_("For_ward"),"PROGDIR:Images/MailForward"),
						End,
					Child, HGroup,
						MUIA_Group_Spacing, 0,
						Child, button_fetch = MakePictureButton(_("_Fetch"),"PROGDIR:Images/MailsFetch"),
						Child, button_send = MakePictureButton(_("_Send"),"PROGDIR:Images/MailsSend"),
						End,
					Child, HGroup,
						MUIA_Group_Spacing, 0,
						MUIA_Weight, 200,
						Child, button_search = MakePictureButton(_("Searc_h"),"PROGDIR:Images/Search"),
						Child, button_filter = MakePictureButton(_("F_ilter"),"PROGDIR:Images/Filter"),
						Child, button_spamcheck = MakePictureButton(_("S_pam"),"PROGDIR:Images/SpamCheck"),
						Child, button_isolate = MakePictureButton(_("Is_olate"),"PROGDIR:Images/SpamIsolate"),
						End,
					Child, HGroup,
						MUIA_Group_Spacing, 0,
						MUIA_Weight, 150,
						Child, button_abook = MakePictureButton(_("_Abook"),"PROGDIR:Images/Addressbook"),
						Child, button_efilter = MakePictureButton(_("Fi_lters"),"PROGDIR:Images/FilterEdit"),
						Child, button_config = MakePictureButton(_("_Config"),"PROGDIR:Images/Config"),
						End,
					End,
				End,

			Child, folder_group = HGroup,
				MUIA_Group_Spacing,0,
				Child, folder_text = TextObject, TextFrame, MUIA_Text_PreParse, MUIX_C, MUIA_Background, MUII_TextBack, End,
				Child, folder_popupmenu = PopupmenuObject,
					ImageButtonFrame,
					MUIA_CycleChain,1,
					MUIA_Image_Spec, MUII_PopUp,
					MUIA_Image_FreeVert, TRUE,
					End,
				Child, switch1_button = PopButton(MUII_ArrowLeft),
				End,

			Child, mail_tree_group = HGroup,
				MUIA_Group_Spacing, 1,
				Child, left_listview_group = VGroup,
					MUIA_HorizWeight, 33,
					Child, folder_listview_group = VGroup,
						Child, folder_listview = NListviewObject,
							MUIA_CycleChain, 1,
							MUIA_NListview_NList, folder_tree = FolderTreelistObject,
								MUIA_NList_Exports, MUIV_NList_Exports_ColWidth|MUIV_NList_Exports_ColOrder,
								MUIA_NList_Imports, MUIV_NList_Imports_ColWidth|MUIV_NList_Imports_ColOrder,
								MUIA_ObjectID, MAKE_ID('M','W','F','T'),
								End,
							End,
						Child, HGroup,
							MUIA_Group_Spacing, 0,
							Child, folder2_text = TextObject, TextFrame,MUIA_Text_SetMin,FALSE,MUIA_Background, MUII_TextBack, End,
							Child, switch2_button = PopButton(MUII_ArrowRight),
							End,
						End,
					Child, left_listview_balance = BalanceObject, End,
					Child, address_listview_group = VGroup,
						Child, address_listview = NListviewObject,
							MUIA_CycleChain, 1,
							MUIA_NListview_NList, address_list = AddressEntryListObject,
								MUIA_AddressEntryList_Type, MUIV_AddressEntryList_Type_Main,
								MUIA_ObjectID, MAKE_ID('M','W','A','L'),
								End,
							End,
						End,
					End,
				Child, folder_balance = BalanceObject, End,
				Child, right_group = VGroup,
					Child, mail_listview = NListviewObject,
						MUIA_CycleChain,1,
						MUIA_NListview_NList, mail_tree = MailTreelistObject,
							MUIA_NList_TitleMark, MUIV_NList_TitleMark_Down | 4,
							MUIA_NList_Exports, MUIV_NList_Exports_ColWidth|MUIV_NList_Exports_ColOrder,
							MUIA_NList_Imports, MUIV_NList_Imports_ColWidth|MUIV_NList_Imports_ColOrder,
							MUIA_ObjectID, MAKE_ID('M','W','M','T'),
							End,
						End,
					Child, right_balance = BalanceObject, End,
					Child, mail_messageview = MessageViewObject, MUIA_CycleChain, 1, End,
					End,
				End,
			Child, status_text = TextObject,
				TextFrame,
				End,

			Child, balance_text = StringObject,
				MUIA_ObjectID, MAKE_ID('M','W','B','S'),
				MUIA_String_MaxLen, 80,
				MUIA_ShowMe, FALSE,
				End,
			End,
		End;

	if (win_main)
	{
		Object *testnlist;

		SM_DEBUGF(15,("Created Main Window at %p\n",win_main));

		if (xget(folder_tree, MUIA_Version) < NLISTTREE_MIN_VERSION ||
		   (xget(folder_tree, MUIA_Version) == NLISTTREE_MIN_VERSION && xget(folder_tree, MUIA_Revision) < NLISTTREE_MIN_REVISION))
		{
			struct EasyStruct es;
			es.es_StructSize = sizeof(struct EasyStruct);
			es.es_Flags = 0;
			es.es_Title =  "SimpleMail";
			es.es_TextFormat = _("SimpleMail needs at least version %ld.%ld of the NListtree.mcc MUI subclass!\nIt's available from %s");
			es.es_GadgetFormat = _("Ok");

			EasyRequest(NULL,&es,NULL,NLISTTREE_MIN_VERSION,NLISTTREE_MIN_REVISION,"http://www.sourceforge.net/projects/nlist-classes");
			MUI_DisposeObject(win_main);
			win_main = NULL;
			return 0;
		}

		testnlist = NListObject, End;
		if (!testnlist)
		{
			MUI_DisposeObject(win_main);
			win_main = NULL;
			return 0;
		}

		if (xget(testnlist, MUIA_Version) < NLIST_MIN_VERSION ||
		   (xget(testnlist, MUIA_Version) == NLIST_MIN_VERSION && xget(testnlist, MUIA_Revision) < NLIST_MIN_REVISION))
		{
			struct EasyStruct es;
			es.es_StructSize = sizeof(struct EasyStruct);
			es.es_Flags = 0;
			es.es_Title =  "SimpleMail";
			es.es_TextFormat = _("SimpleMail needs at least version %ld.%ld of the NList.mcc MUI subclass!\nIt's available from %s");
			es.es_GadgetFormat = _("Ok");

			EasyRequest(NULL,&es,NULL,NLIST_MIN_VERSION,NLIST_MIN_REVISION,"http://www.sourceforge.net/projects/nlist-classes");
			MUI_DisposeObject(testnlist);
			MUI_DisposeObject(win_main);
			win_main = NULL;
			return 0;
		}
		MUI_DisposeObject(testnlist);

		/* Short Help */
		set(button_search, MUIA_ShortHelp, _("Opens a window where you can search through your mail folder."));
		set(button_filter, MUIA_ShortHelp, _("Process every mail within the current selected folder via the filters."));
		set(button_efilter, MUIA_ShortHelp, _("Opens a window where the filters can be edited."));
		set(button_spamcheck, MUIA_ShortHelp, _("Checks the current selected folder for spam.\nIf a potential spam mail has been found it will be marked."));
		set(button_isolate, MUIA_ShortHelp, _("Isolates all spam marked mails within the current selected folder."));

		main_settings_folder_menuitem = (Object*)DoMethod(main_menu,MUIM_FindUData,MENU_SETTINGS_SHOW_FOLDERS);
		main_settings_addressbook_menuitem = (Object*)DoMethod(main_menu,MUIM_FindUData, MENU_SETTINGS_SHOW_ADDRESSBOOK);
		main_settings_messageview_menuitem = (Object*)DoMethod(main_menu,MUIM_FindUData, MENU_SETTINGS_SHOW_SELECTED_MESSAGE);
		main_scripts_menu = (Object*)DoMethod(main_menu,MUIM_FindUData,MENU_SCRIPTS);
		main_scripts_execute_menuitem = (Object*)DoMethod(main_menu,MUIM_FindUData,MENU_SCRIPTS_EXECUTESCRIPT);

		set(main_settings_folder_menuitem,MUIA_ObjectID,MAKE_ID('M','N','S','F'));
		set(main_settings_addressbook_menuitem,MUIA_ObjectID,MAKE_ID('M','N','S','A'));
		set(main_settings_messageview_menuitem,MUIA_ObjectID,MAKE_ID('M','N','M','V'));

		settings_show_changed();

		project_checksingleaccount_menuitem = (Object*)DoMethod(main_menu, MUIM_FindUData, MENU_PROJECT_CHECKSINGLEACCOUNT);

		DoMethod(App, OM_ADDMEMBER, win_main);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_CloseRequest, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

		/* Menu notifies */
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_PROJECT_ABOUT, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, display_about);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_PROJECT_ABOUTMUI, App, 2, MUIM_Application_AboutMUI, 0);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_PROJECT_IMPORTMBOX, App, 4, MUIM_CallHook, &hook_standard, callback_import_mbox, 0);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_PROJECT_IMPORTDBX, App, 4, MUIM_CallHook, &hook_standard, callback_import_dbx, 0);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_PROJECT_FETCH, App, 3, MUIM_CallHook, &hook_standard, callback_fetch_mails);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_PROJECT_SEND, App, 3, MUIM_CallHook, &hook_standard, callback_send_mails);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_PROJECT_QUIT, App, 2, MUIM_Application_ReturnID,  MUIV_Application_ReturnID_Quit);

		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_NEWGROUP, App, 3, MUIM_CallHook, &hook_standard, callback_new_group);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_NEWFOLDER, App, 3, MUIM_CallHook, &hook_standard, callback_new_folder);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_DELETE, App, 3, MUIM_CallHook, &hook_standard, callback_remove_folder);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_OPTIONS, App, 3, MUIM_CallHook, &hook_standard, callback_edit_folder);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_ORDER_SAVE, App, 3, MUIM_CallHook, &hook_standard, folder_save_order);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_ORDER_RESET, App, 3, MUIM_CallHook, &hook_standard, callback_reload_folder_order);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_RESCAN, App, 3, MUIM_CallHook, &hook_standard, callback_rescan_folder);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_DELALLINDEX, App, 3, MUIM_CallHook, &hook_standard, callback_delete_all_indexfiles);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_SAVEALLINDEX, App, 3, MUIM_CallHook, &hook_standard, callback_save_all_indexfiles);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_IMPORTMBOX, App, 4, MUIM_CallHook, &hook_standard, callback_import_mbox, 1);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_EXPORT, App, 3, MUIM_CallHook, &hook_standard, callback_export);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_SPAMCHECK, App, 3, MUIM_CallHook, &hook_standard, callback_check_selected_folder_for_spam);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_MOVESPAM, App, 3, MUIM_CallHook, &hook_standard, callback_move_spam_marked_mails);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_HAM, App, 3, MUIM_CallHook, &hook_standard, callback_classify_selected_folder_as_ham);

		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_MESSAGE_NEW, App, 3, MUIM_CallHook, &hook_standard, callback_new_mail);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_MESSAGE_REPLY, App, 3, MUIM_CallHook, &hook_standard, callback_reply_selected_mails);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_MESSAGE_FORWARD, App, 3, MUIM_CallHook, &hook_standard, callback_forward_selected_mails);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_MESSAGE_READ, App, 3, MUIM_CallHook, &hook_standard, callback_read_active_mail);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_MESSAGE_EDIT, App, 3, MUIM_CallHook, &hook_standard, callback_change_mail);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_MESSAGE_MOVE, App, 3, MUIM_CallHook, &hook_standard, callback_move_selected_mails);
/*
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_MESSAGE_COPY, App, 2, MUIM_Application_ReturnID,  MUIV_Application_ReturnID_Quit);
*/
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_MESSAGE_DELETE, App, 3, MUIM_CallHook, &hook_standard, callback_delete_mails);

		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_SETTINGS_MUI, App, 2, MUIM_Application_OpenConfigWindow, 0);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_SETTINGS_CONFIGURATION,App, 3, MUIM_CallHook, &hook_standard, callback_config);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_SETTINGS_FILTER, App, 3, MUIM_CallHook, &hook_standard, callback_edit_filter);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_SETTINGS_SHOW_FOLDERS, App, 3, MUIM_CallHook, &hook_standard, settings_show_changed);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_SETTINGS_SHOW_ADDRESSBOOK, App, 3, MUIM_CallHook, &hook_standard, settings_show_changed);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_SETTINGS_SHOW_SELECTED_MESSAGE, App, 3, MUIM_CallHook, &hook_standard, settings_show_changed);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_SETTINGS_SAVEPREFS, App, 3, MUIM_CallHook, &hook_standard, main_save_environment);

		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_SCRIPTS_EXECUTESCRIPT, App, 4, MUIM_CallHook, &hook_standard, menu_execute_script, -1);


		/* Key notifies */
		DoMethod(win_main, MUIM_Notify, MUIA_Window_InputEvent, "delete", App, 3, MUIM_CallHook, &hook_standard, callback_delete_mails);

		/* Gadget notifies */
		DoMethod(button_read, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_read_active_mail);
		DoMethod(button_getadd, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_get_address);
		DoMethod(button_delete, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_delete_mails);
		DoMethod(button_move, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_move_selected_mails);
		DoMethod(button_fetch, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_fetch_mails);
		DoMethod(button_send, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_send_mails);
		DoMethod(button_new, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_new_mail);
		DoMethod(button_reply, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_reply_selected_mails);
		DoMethod(button_forward, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_forward_selected_mails);
		DoMethod(button_change, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_change_mail);
		DoMethod(button_search, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_search);
		DoMethod(button_filter, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_filter);
		DoMethod(button_efilter, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_edit_filter);
		DoMethod(button_spamcheck, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_check_selected_folder_for_spam);
		DoMethod(button_isolate, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_move_spam_marked_mails);

		DoMethod(button_abook, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_addressbook);
		DoMethod(button_config, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_config);

		DoMethod(switch1_button, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, switch_folder_view);
		DoMethod(switch2_button, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, switch_folder_view);
		DoMethod(mail_tree, MUIM_Notify, MUIA_MailTreelist_Active, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_mail_within_main_selected);
		DoMethod(mail_tree, MUIM_Notify, MUIA_MailTreelist_DoubleClick, MUIV_EveryTime, MUIV_Notify_Application, 3,  MUIM_CallHook, &hook_standard, callback_read_active_mail);
		DoMethod(mail_tree, MUIM_Notify, MUIA_NList_TitleClick, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, mailtreelist_title_click);
		DoMethod(mail_tree, MUIM_Notify, MUIA_NList_TitleClick2, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, mailtreelist_title_click2);
		DoMethod(folder_tree, MUIM_Notify, MUIA_NListtree_Active, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_folder_active);
		DoMethod(folder_tree, MUIM_Notify, MUIA_NListtree_Active, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, main_refresh_folders_text);
		DoMethod(folder_tree, MUIM_Notify, MUIA_FolderTreelist_MailDrop, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, foldertreelist_maildrop);
		DoMethod(folder_tree, MUIM_Notify, MUIA_FolderTreelist_OrderChanged, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, foldertreelist_orderchanged);
		DoMethod(folder_tree, MUIM_Notify, MUIA_NListtree_DoubleClick, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard,  foldertreelist_doubleclick);
		DoMethod(folder_popupmenu, MUIM_Notify, MUIA_Popupmenu_Selected, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, popup_selected);
		set(folder_tree,MUIA_UserData,mail_tree); /* for the drag'n'drop support */
		DoMethod(address_list, MUIM_Notify, MUIA_NList_DoubleClick, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, addressentrylist_doubleclick);

		main_build_accounts();
		main_build_scripts();
		main_build_addressbook();

		SM_DEBUGF(15,("Going to load environment\n"));
		main_load_environment();
		SM_DEBUGF(15,("Environment loaded\n"));

		rc = TRUE;
	}

	SM_LEAVE;
	return(rc);
}

/******************************************************************
 Open the main window
*******************************************************************/
int main_window_open(void)
{
	if (win_main)
	{
		/* The settings could have been loaded */
		settings_show_changed();
		set(win_main, MUIA_Window_Open, TRUE);
		return 1;
	}
	return 0;
}

/******************************************************************
 Refreshs the folder text line
*******************************************************************/
static void main_refresh_folders_text(void)
{
	char buf[256];

	if (folder_text)
	{
		struct folder *f = main_get_folder();
		if (f)
		{
			sprintf(buf, MUIX_B "%s"  MUIX_N " %s  " MUIX_B "%s"  MUIX_N " %ld  " MUIX_B "%s"  MUIX_N " %ld  " MUIX_B "%s"  MUIX_N " %ld  ",
			        _("Folder:"),f->name,_("Total:"),f->num_mails,_("New:"),f->new_mails,_("Unread:"),f->unread_mails);
			set(folder_text, MUIA_Text_Contents,buf);
		}
	}

	if (folder2_text)
	{
		int total_msg,total_unread,total_new;
		folder_get_stats(&total_msg,&total_unread,&total_new);
		sprintf(buf, _("Total:%d New:%d Unread:%d"),total_msg,total_new,total_unread);
		set(folder2_text,MUIA_Text_Contents,buf);
	}
}

/******************************************************************
 Refreshs the folder list
*******************************************************************/
void main_refresh_folders(void)
{
	int act = xget(folder_tree, MUIA_NList_Active);

	if (folder_popupmenu) DoMethod(folder_popupmenu,MUIM_Popupmenu_Clear);

	if (folder_popupmenu)
	{
		struct folder *f;
		char buf[256];
		int i=0;

		for (f=folder_first();f;f=folder_next(f),i++)
		{
			sprintf(buf,_("%s (T:%ld N:%ld U:%ld)"),f->name,f->num_mails,f->new_mails,f->unread_mails);
			DoMethod(folder_popupmenu,MUIM_Popupmenu_AddEntry, buf,i);
		}
	}
	set(folder_tree,MUIA_NListtree_Quiet,TRUE);
	DoMethod(folder_tree,MUIM_FolderTreelist_Refresh,NULL);
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

	if (folder->parent_folder) main_refresh_folder(folder->parent_folder);
}

/******************************************************************
 Inserts a new mail into the listview at the end
*******************************************************************/
void main_insert_mail(struct mail *mail)
{
	DoMethod(mail_tree, MUIM_MailTree_InsertMail, mail, -2);
}

/******************************************************************
 Inserts a new mail into the listview after a given position
*******************************************************************/
void main_insert_mail_pos(struct mail *mail, int after)
{
	DoMethod(mail_tree, MUIM_MailTree_InsertMail, mail, after);
}

/******************************************************************
 Remove a given mail from the listview
*******************************************************************/
void main_remove_mail(struct mail *mail)
{
	DoMethod(mail_tree, MUIM_MailTree_RemoveMail, mail);
}

/******************************************************************
 Replaces a mail with a new mail.
 This also activates the new mail, however it would be better if
 this done by an extra call
*******************************************************************/
void main_replace_mail(struct mail *oldmail, struct mail *newmail)
{
	DoMethod(mail_tree, MUIM_MailTree_ReplaceMail, oldmail, newmail);
}

/******************************************************************
 Refresh a mail (if it status has changed fx)
*******************************************************************/
void main_refresh_mail(struct mail *m)
{
	DoMethod(mail_tree, MUIM_MailTree_RefreshMail, m);
	DoMethod(folder_tree, MUIM_NListtree_Redraw, MUIV_NListtree_Redraw_Active, 0);
	main_refresh_folders_text();
}

/******************************************************************
 Clears the folder list
*******************************************************************/
void main_clear_folder_mails(void)
{
	DoMethod(mail_tree, MUIM_MailTree_Clear);
}

/******************************************************************
 Updates the mail trees with the mails in the given folder
*******************************************************************/
void main_set_folder_mails(struct folder *folder)
{ 
	DoMethod(mail_tree, MUIM_MailTree_SetFolderMails, folder);
	mailtreelist_update_title_markers();
}

/******************************************************************
 Activates a different folder
*******************************************************************/
void main_set_folder_active(struct folder *folder)
{
	struct MUI_NListtree_TreeNode *tn = FindListtreeUserData(folder_tree, folder);
	set(folder_tree,MUIA_NListtree_Active,tn);
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
 Sets the active mail
*******************************************************************/
void main_set_active_mail(struct mail *m)
{
	set(mail_tree, MUIA_MailTree_Active, m);
}

/******************************************************************
 Returns the active mail. NULL if no one is active
*******************************************************************/
struct mail *main_get_active_mail(void)
{
	return (struct mail*)xget(mail_tree, MUIA_MailTreelist_Active);
}

/******************************************************************
 Returns the filename of the active mail, NULL if no thing is
 selected
*******************************************************************/
char *main_get_mail_filename(void)
{
	struct mail *m = main_get_active_mail();
	if (m) return m->info->filename;
	return NULL;
}

/******************************************************************
 Returns the first selected mail. NULL if no mail is selected
*******************************************************************/
struct mail *main_get_mail_first_selected(void *handle)
{
	return (struct mail*)DoMethod(mail_tree, MUIM_MailTree_GetFirstSelected, handle);
}

/******************************************************************
 Returns the next selected mail. NULL if no more mails are
 selected
*******************************************************************/
struct mail *main_get_mail_next_selected(void *handle)
{
	return (struct mail*)DoMethod(mail_tree, MUIM_MailTree_GetNextSelected, handle);
}

/******************************************************************
 Remove all selected mails,
*******************************************************************/
void main_remove_mails_selected(void)
{
	DoMethod(mail_tree, MUIM_MailTree_RemoveSelected);
}

/******************************************************************
 Refresh all selected mails
*******************************************************************/
void main_refresh_mails_selected(void)
{
	DoMethod(mail_tree, MUIM_MailTree_RefreshSelected);
}

/******************************************************************
 Build the check singe account menu
*******************************************************************/
void main_build_accounts(void)
{
	struct account *account = (struct account*)list_first(&user.config.account_list);
	int i=0;

	DisposeAllFamilyChilds(project_checksingleaccount_menuitem);

	while (account)
	{
		if (account->pop && account->pop->name)
		{
			char buf[200];
			Object *entry;

			if (account->account_name)
			{
				utf8tostr(account->account_name, buf, sizeof(buf), user.config.default_codeset);
				strcat(buf,"...");
			} else if (account->pop->login)
			{
				sm_snprintf(buf,sizeof(buf),"%s@%s...",account->pop->login,account->pop->name);
			} else strcpy(buf,account->pop->name);

			entry = MenuitemObject,
				MUIA_Menuitem_Title, mystrdup(buf), /* leakes */
				End;

			DoMethod(project_checksingleaccount_menuitem, OM_ADDMEMBER, entry);
			DoMethod(entry, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, App, 4, MUIM_CallHook, &hook_standard, menu_check_single_account,i);
		}
		account = (struct account*)node_next(&account->node);
		i++;
	}

	if (!i)
	{
		Object *entry = MenuitemObject, MUIA_Menuitem_Title, _("No fetchable account specified"),	End;
		DoMethod(project_checksingleaccount_menuitem, OM_ADDMEMBER, entry);
	}
}

/******************************************************************
 Build the scripts menu
*******************************************************************/
void main_build_scripts(void)
{
	DoMethod(main_scripts_menu, OM_REMMEMBER, main_scripts_execute_menuitem);
	DisposeAllFamilyChilds(main_scripts_menu);
	DoMethod(main_scripts_menu, OM_ADDMEMBER, main_scripts_execute_menuitem);
}

/******************************************************************
 Build the addressbook which is displayed in this window
*******************************************************************/
void main_build_addressbook(void)
{
	DoMethod(address_list, MUIM_AddressEntryList_Refresh,NULL);
}

/******************************************************************
 Freeze the mail list
*******************************************************************/
void main_freeze_mail_list(void)
{
	DoMethod(mail_tree,MUIM_MailTree_Freeze);
}

/******************************************************************
 Thaws the mail list
*******************************************************************/
void main_thaw_mail_list(void)
{
	DoMethod(mail_tree,MUIM_MailTree_Thaw);
}

/******************************************************************
 Select a mail, specified by number
*******************************************************************/
void main_select_mail(int mail)
{
	set(mail_tree,MUIA_NList_Active, mail);
}

/******************************************************************
 Return the iconified status of the Application
*******************************************************************/
int main_is_iconified(void)
{
	return xget(App, MUIA_Application_Iconified);
}

/******************************************************************
 A amiga private function!!! Returns the screen of the main window
*******************************************************************/
struct Screen *main_get_screen(void)
{
	return (struct Screen*)xget(win_main,MUIA_Window_Screen);
}

/******************************************************************
 Sets an UTF formated status text
*******************************************************************/
void main_set_status_text(char *txt)
{
	/* TODO: Convert to host charset */
	set(status_text, MUIA_Text_Contents, txt);
}

/******************************************************************
 Displays the given mail within the message view
*******************************************************************/
void main_display_active_mail(void)
{
	struct mail *m = main_get_active_mail();
	char *f = main_get_folder_drawer();

	DoMethod(mail_messageview, MUIM_MessageView_DisplayMail, m, f);
}
