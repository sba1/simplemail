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
#include <mui/NListview_mcc.h>
#include <mui/NListtree_mcc.h>

#include <clib/alib_protos.h>
#include <proto/asl.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "version.h"

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
#ifdef __AROS__ /*use mailtext instead of simplemhtml*/
#include "mailtextview.h"
#else
#include "messageviewclass.h"
#endif
#include "muistuff.h"
#include "picturebuttonclass.h"
#include "popupmenuclass.h"
#include "smtoolbarclass.h"
#include "utf8stringclass.h"

/*****************************************************/
/* Minimum requirements of some external MUI classes */
/* AROS doesn't have the latest versions yet        */
#define NLISTTREE_MIN_VERSION   18
#ifdef __AROS__
#define NLISTTREE_MIN_REVISION  19
#else
#define NLISTTREE_MIN_REVISION  22
#endif
#define NLIST_MIN_VERSION   20
#ifdef __AROS__
#define NLIST_MIN_REVISION  112
#else
#define NLIST_MIN_REVISION  115
#endif
/*****************************************************/

enum
{
	SM_MAINWND_BUTTON_READ = 0,
	SM_MAINWND_BUTTON_EDIT,
	SM_MAINWND_BUTTON_MOVE,
	SM_MAINWND_BUTTON_DELETE,
	SM_MAINWND_BUTTON_GETADDRESS,
	SM_MAINWND_BUTTON_NEW,
	SM_MAINWND_BUTTON_REPLY,
	SM_MAINWND_BUTTON_FORWARD,
	SM_MAINWND_BUTTON_FETCH,
	SM_MAINWND_BUTTON_SEND,
	SM_MAINWND_BUTTON_SEARCH,
	SM_MAINWND_BUTTON_FILTER,
	SM_MAINWND_BUTTON_SPAM,
	SM_MAINWND_BUTTON_ISOLATE,
	SM_MAINWND_BUTTON_ADDRESSBOOK,
	SM_MAINWND_BUTTON_EDITFILTER,
	SM_MAINWND_BUTTON_CONFIG
};

static const struct MUIS_SMToolbar_Button sm_mainwnd_buttons[] =
{
    {PIC(2,2), SM_MAINWND_BUTTON_READ,       0, N_("Rea_d"), NULL, "MailRead"},
    {PIC(1,7), SM_MAINWND_BUTTON_EDIT,       0, N_("_Edit"), NULL, "MailModify"},
    {PIC(1,8), SM_MAINWND_BUTTON_MOVE,       0, N_("_Move"), NULL, "MailMove"},
    {PIC(1,4), SM_MAINWND_BUTTON_DELETE,     0, N_("De_lete"), NULL,"MailDelete"},
    {PIC(1,6), SM_MAINWND_BUTTON_GETADDRESS, 0, N_("Ge_tAdd"), NULL,"MailGetAddress"},
    {MUIV_SMToolbar_Space},
    {PIC(1,9), SM_MAINWND_BUTTON_NEW,     0, N_("_New"), NULL,"MailNew"},
    {PIC(1,5), SM_MAINWND_BUTTON_REPLY,   0, N_("_Reply"),NULL,"MailReply"},
    {PIC(2,3), SM_MAINWND_BUTTON_FORWARD, 0, N_("For_ward"),NULL,"MailForward"},
    {MUIV_SMToolbar_Space},
    {PIC(2,5), SM_MAINWND_BUTTON_FETCH, 0, N_("_Fetch"),NULL,"MailsFetch"},
    {PIC(2,7), SM_MAINWND_BUTTON_SEND,  0, N_("_Send"),NULL,"MailsSend"},
    {MUIV_SMToolbar_Space},
    {PIC(3,5), SM_MAINWND_BUTTON_SEARCH,  0, N_("Searc_h"),N_("Opens a window where you can search through your mail folder."),"Search"},
    {PIC(0,9), SM_MAINWND_BUTTON_FILTER,  0, N_("F_ilter"),N_("Process every mail within the current selected folder via the filters."),"Filter"},
    {PIC(3,6), SM_MAINWND_BUTTON_SPAM,    0, N_("S_pam"),N_("Checks the current selected folder for spam.\nIf a potential spam mail has been found it will be marked."),"SpamCheck"},
    {PIC(3,7), SM_MAINWND_BUTTON_ISOLATE, 0, N_("Is_olate"),N_("Isolates all spam marked mails within the current selected folder."),"SpamIsolate"},
    {MUIV_SMToolbar_Space},
    {PIC(0,1), SM_MAINWND_BUTTON_ADDRESSBOOK, 0, N_("_Abook"),NULL,"Addressbook"},
    {PIC(1,0), SM_MAINWND_BUTTON_EDITFILTER,  0, N_("Filters"),N_("Opens a window where the filters can be edited."),"FilterEdit"},
    {PIC(0,3), SM_MAINWND_BUTTON_CONFIG,      0, N_("_Config"),NULL,"Config"},
    {MUIV_SMToolbar_End},
};

/*****************************************************/

static Object *win_main;
static Object *main_menu;
static Object *main_settings_folder_menuitem;
static Object *main_settings_addressbook_menuitem;
static Object *main_settings_messageview_menuitem;
static Object *main_settings_filter_menuitem;
static Object *main_scripts_menu;
static Object *main_scripts_execute_menuitem;
static Object *main_group;
static Object *main_toolbar;
static Object *switch1_button; /* switch button for the two views */
static Object *switch2_button; /* switch button for the two views */
static Object *mail_tree;
static Object *mail_tree_group;
static Object *mail_listview;
static Object *mail_messageview;
static Object *filter_group;
static Object *filter_string;
static Object *filter_clear_button;
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

/** @brief menu to be displayed in the main window */
static struct NewMenu *main_newmenu;

static void main_refresh_folders_text(void);
static void settings_show_changed(void);
static LONG mailtreelist_2_sort2marker(int sort);

struct MUI_NListtree_TreeNode *FindListtreeUserData(Object *tree, APTR udata)
{
	set(tree, MUIA_NListtree_FindUserDataHook, MUIV_NListtree_FindUserDataHook_PointerCompare);
	return (struct MUI_NListtree_TreeNode *)DoMethod(tree, MUIM_NListtree_FindUserData, MUIV_NListtree_FindUserData_ListNode_Root, (ULONG)udata, 0);
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
		"SimpleMail version %ld.%ld (%s)\n\n%s 2000-2009\nHynek Schlawack %s Sebastian Bauer\n%s.",
		VERSION,REVISION,SIMPLEMAIL_DATE,_("Copyright (c)"),_("and"),_("Released under the terms of the GNU Public License"));
}

/******************************************************************
 Open an arbitrary message
*******************************************************************/
static void open_message(void)
{
	callback_open_message(NULL,-1);
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

	nnset(mail_tree, MUIA_MailTreelist_TitleMark, mailtreelist_2_sort2marker(folder_get_primary_sort(folder)));
	nnset(mail_tree, MUIA_MailTreelist_TitleMark2, mailtreelist_2_sort2marker(folder_get_secondary_sort(folder)));
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

		parent = (struct MUI_NListtree_TreeNode*)DoMethod(folder_tree, MUIM_NListtree_GetEntry, (ULONG)tn, MUIV_NListtree_GetEntry_Position_Parent, 0);
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
 A mail has been doubleclicked
*******************************************************************/
static void mailtreelist_doubleclick(void)
{
	if (main_get_folder() == folder_outgoing())
		callback_change_mail();
	else
		callback_read_active_mail();
}

/******************************************************************
 An addressbook entry has been doubleclicked
*******************************************************************/
static void addressentrylist_doubleclick(void)
{
	struct addressbook_entry_new *entry;

	DoMethod(address_list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, (ULONG)&entry);
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

static LONG mailtreelist_2_sort2marker(int sort)
{
	LONG marker = 0;
	switch (sort & ~(FOLDER_SORT_REVERSE))
	{
		case	FOLDER_SORT_FROMTO: marker = COLUMN_TYPE_FROMTO; break;
		case	FOLDER_SORT_SUBJECT: marker = COLUMN_TYPE_SUBJECT; break;
		case	FOLDER_SORT_STATUS: marker = COLUMN_TYPE_STATUS; break;
		case	FOLDER_SORT_REPLY: marker = COLUMN_TYPE_REPLYTO; break;
		case	FOLDER_SORT_DATE: marker = COLUMN_TYPE_DATE; break;
		case	FOLDER_SORT_SIZE: marker = COLUMN_TYPE_SIZE; break;
		case	FOLDER_SORT_FILENAME: marker = COLUMN_TYPE_FILENAME; break;
		case	FOLDER_SORT_POP3: marker = COLUMN_TYPE_POP3; break;
		case	FOLDER_SORT_RECV: marker = COLUMN_TYPE_RECEIVED; break;
	}

	if (!(sort & FOLDER_SORT_REVERSE))
		marker |= MUIV_MailTreelist_TitleMark_Decreasing;

	return marker;
}

static void mailtreelist_2_title_click(int *which_one_ptr)
{
	int which_one = *which_one_ptr;
	struct folder *folder = main_get_folder();
	int title_click;
	int old_sort;
	int new_sort;

	if (!folder) return;

	if (which_one == 1)
	{
		title_click = xget(mail_tree, MUIA_MailTreelist_TitleClick);
		old_sort = folder_get_primary_sort(folder);
	} else
	{
		title_click = xget(mail_tree, MUIA_MailTreelist_TitleClick2);
		old_sort = folder_get_secondary_sort(folder);
	}

	switch (title_click & (~MUIV_MailTreelist_TitleMark_Decreasing))
	{
		case	COLUMN_TYPE_FROMTO: new_sort = FOLDER_SORT_FROMTO; break;
		case	COLUMN_TYPE_SUBJECT: new_sort = FOLDER_SORT_SUBJECT; break;
		case	COLUMN_TYPE_STATUS: new_sort = FOLDER_SORT_STATUS; break;
		case	COLUMN_TYPE_REPLYTO: new_sort = FOLDER_SORT_REPLY; break;
		case	COLUMN_TYPE_DATE: new_sort = FOLDER_SORT_DATE; break;
		case	COLUMN_TYPE_SIZE: new_sort = FOLDER_SORT_SIZE; break;
		case	COLUMN_TYPE_FILENAME: new_sort = FOLDER_SORT_FILENAME; break;
		case	COLUMN_TYPE_POP3: new_sort = FOLDER_SORT_POP3; break;
		case	COLUMN_TYPE_RECEIVED: new_sort = FOLDER_SORT_RECV; break;
		default:
			return;
	}

	if (new_sort == (old_sort & ~(FOLDER_SORT_REVERSE)))
	{
		if (old_sort & FOLDER_SORT_REVERSE)
		{
			new_sort &= ~FOLDER_SORT_REVERSE;
			title_click |= MUIV_MailTreelist_TitleMark_Decreasing;
		}	else
		{
			new_sort |= FOLDER_SORT_REVERSE;
		}
	} else
	{
		title_click |= MUIV_MailTreelist_TitleMark_Decreasing;
	}

	if (which_one == 1)
	{
		set(mail_tree, MUIA_MailTreelist_TitleMark, title_click);
		folder_set_primary_sort(folder, new_sort);
	} else
	{
		set(mail_tree, MUIA_MailTreelist_TitleMark2, title_click);
		folder_set_secondary_sort(folder, new_sort);
	}

	folder_config_save(folder);
	main_set_folder_mails(folder);
	read_refresh_prevnext_button(folder);
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
 Called if the "delete"-key has been pressed.
*******************************************************************/
static void main_window_delete_pressed(void)
{
	if ((void*)xget(win_main,MUIA_Window_ActiveObject) != (void*)filter_string)
		callback_delete_mails();
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
	int folder, addressbook, messageview, filter;

	DoMethod(main_group, MUIM_Group_InitChange);
	DoMethod(mail_tree_group, MUIM_Group_InitChange);
	DoMethod(right_group, MUIM_Group_InitChange);

	folder = xget(main_settings_folder_menuitem,MUIA_Menuitem_Checked);
	addressbook = xget(main_settings_addressbook_menuitem,MUIA_Menuitem_Checked);
	messageview = xget(main_settings_messageview_menuitem,MUIA_Menuitem_Checked);
	filter = xget(main_settings_filter_menuitem,MUIA_Menuitem_Checked);

	set(folder_group, MUIA_ShowMe, !folder);
	set(folder_listview_group, MUIA_ShowMe, folder);
	set(address_listview_group,MUIA_ShowMe, addressbook);
	set(folder_balance, MUIA_ShowMe, folder || addressbook);
	set(left_listview_group, MUIA_ShowMe, folder || addressbook);
	set(left_listview_balance, MUIA_ShowMe, folder && addressbook);
	set(right_balance, MUIA_ShowMe, messageview);
	set(mail_messageview, MUIA_ShowMe, messageview);
	set(filter_group, MUIA_ShowMe, filter);

	DoMethod(right_group, MUIM_Group_ExitChange);
	DoMethod(mail_tree_group, MUIM_Group_ExitChange);
	DoMethod(main_group, MUIM_Group_ExitChange);

	/* FIXME: This is necessary only because the mail is not displayed
	 * if the object is not in the Show state. Must be fixed within
	 * MessageView */
	if (messageview)
	{
		struct mail_info *m = main_get_active_mail();
		char *f = main_get_folder_drawer();
		DoMethod(mail_messageview, MUIM_MessageView_DisplayMail, (ULONG)m, (ULONG)f);
	}
}

/******************************************************************
 Quick filter show settings changed.
*******************************************************************/
static void settings_quick_filter_changed(void)
{
	int filter;

	filter = xget(main_settings_filter_menuitem,MUIA_Menuitem_Checked);

	set(filter_group, MUIA_ShowMe, filter);

	callback_quick_filter_changed();
}

/******************************************************************
 Loads environment and sets the balance groups
*******************************************************************/
void main_load_environment(void)
{
	char *ls;
	int i=0;
	LONG count, weight;

	DoMethod(App, MUIM_Application_Load, (ULONG)MUIV_Application_Load_ENV);
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

	DoMethod(App, MUIM_Application_Save, (ULONG)MUIV_Application_Save_ENV);
	DoMethod(App, MUIM_Application_Save, (ULONG)MUIV_Application_Save_ENVARC);
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
		MENU_PROJECT_OPEN,
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
		MENU_MESSAGE_RAW,
		MENU_MESSAGE_MOVE,
		MENU_MESSAGE_COPY,
		MENU_MESSAGE_DELETE,
		MENU_MESSAGE_SAVE,
		MENU_MESSAGE_SELECT_ALL,
		MENU_MESSAGE_CLEAR_SELECTION,

		MENU_SETTINGS,
		MENU_SETTINGS_SHOW_FOLDERS,
		MENU_SETTINGS_SHOW_ADDRESSBOOK,
		MENU_SETTINGS_SHOW_SELECTED_MESSAGE,
		MENU_SETTINGS_SHOW_QUICK_FILTER,
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
		{NM_ITEM, N_("O:Open Message..."), NULL, 0, 0, (APTR)MENU_PROJECT_OPEN},
		{NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
		{NM_ITEM, N_("Delete All Indexfiles"), NULL, 0, 0, (APTR)MENU_FOLDER_DELALLINDEX},
		{NM_ITEM, N_("Save All Indexfiles"), NULL, 0, 0, (APTR)MENU_FOLDER_SAVEALLINDEX},
		{NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
		{NM_ITEM, N_("Import mbox File..."), NULL, 0, 0, (APTR)MENU_PROJECT_IMPORTMBOX},
		{NM_ITEM, N_("Import dbx File..."), NULL, 0, 0, (APTR)MENU_PROJECT_IMPORTDBX},
		{NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
		{NM_ITEM, N_("S:Send Queued Mails..."), NULL, 0, 0, (APTR)MENU_PROJECT_SEND},
		{NM_ITEM, N_("F:Check All Active Accounts..."), NULL, 0, 0, (APTR)MENU_PROJECT_FETCH},
		{NM_ITEM, N_("Check Single Account"), NULL, 0, 0, (APTR)MENU_PROJECT_CHECKSINGLEACCOUNT},
		{NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
		{NM_ITEM, N_("Q:Quit"), NULL, 0, 0, (APTR)MENU_PROJECT_QUIT},
		{NM_TITLE, N_("Folder"), NULL, 0, 0, NULL},
		{NM_ITEM, N_("New..."), NULL, 0, 0, (APTR)MENU_FOLDER_NEWFOLDER},
		{NM_ITEM, N_("New Group..."), NULL, 0, 0, (APTR)MENU_FOLDER_NEWGROUP},
		{NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
		{NM_ITEM, N_("Delete..."), NULL, 0, 0, (APTR)MENU_FOLDER_DELETE},
		{NM_ITEM, N_("Import mbox File..."), NULL, 0, 0, (APTR)MENU_FOLDER_IMPORTMBOX},
		{NM_ITEM, N_("Export As mbox..."), NULL, 0, 0, (APTR)MENU_FOLDER_EXPORT},
		{NM_ITEM, N_("Rescan"), NULL, 0, 0, (APTR)MENU_FOLDER_RESCAN},
		{NM_ITEM, N_("Options..."), NULL, 0, 0, (APTR)MENU_FOLDER_OPTIONS},
		{NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
		{NM_ITEM, N_("P:Run Spam Mail Check"), NULL, 0, 0, (APTR)MENU_FOLDER_SPAMCHECK},
		{NM_ITEM, N_("I:Isolate Spam Mails"), NULL, 0, 0, (APTR)MENU_FOLDER_MOVESPAM},
		{NM_ITEM, N_("H:Classify All Mails As Ham"), NULL, 0, 0, (APTR)MENU_FOLDER_HAM},
		{NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
		{NM_ITEM, N_("Order"), NULL, 0, 0, NULL},
		{NM_SUB, N_("Save"), NULL, 0, 0, (APTR)MENU_FOLDER_ORDER_SAVE},
		{NM_SUB, N_("Reset"), NULL, 0, 0, (APTR)MENU_FOLDER_ORDER_RESET},
		{NM_TITLE, N_("Message"), NULL, 0, 0, NULL},
		{NM_ITEM, N_("N:New..."), NULL, 0, 0, (APTR)MENU_MESSAGE_NEW},
		{NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
		{NM_ITEM, N_("D:Read..."), NULL, 0, 0, (APTR)MENU_MESSAGE_READ},
		{NM_ITEM, N_("E:Edit..."), NULL, 0, 0, (APTR)MENU_MESSAGE_EDIT},
		{NM_ITEM, N_("Save As..."), NULL, 0, 0L, (APTR)MENU_MESSAGE_SAVE},
		{NM_ITEM, N_("R:Reply..."), NULL, 0, 0, (APTR)MENU_MESSAGE_REPLY},
		{NM_ITEM, N_("W:Forward..."), NULL, 0, 0, (APTR)MENU_MESSAGE_FORWARD},
		{NM_ITEM, N_("Show Raw..."), NULL, 0, 0, (APTR)MENU_MESSAGE_RAW},
		{NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
		{NM_ITEM, N_("M:Move..."), NULL, 0, 0L, (APTR)MENU_MESSAGE_MOVE},
		{NM_ITEM, N_("Copy..."), NULL, NM_ITEMDISABLED, 0L, (APTR)MENU_MESSAGE_COPY},
		{NM_ITEM, N_("Delete"), "Del", NM_COMMANDSTRING, 0L, (APTR)MENU_MESSAGE_DELETE},
		{NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
		{NM_ITEM, N_("A:Select All Messages"), NULL, 0, 0L, (APTR)MENU_MESSAGE_SELECT_ALL},
		{NM_ITEM, N_("Z:Clear Selection"), NULL, 0, 0L, (APTR)MENU_MESSAGE_CLEAR_SELECTION},


/*
		{NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
		{NM_ITEM, "Print...", NULL, NM_ITEMDISABLED, 0L, NULL},
		{NM_ITEM, "Save", NULL, NM_ITEMDISABLED, 0L, NULL},
		{NM_SUB, "Complete Message...", NULL, 0, 0, NULL},
		{NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
		{NM_SUB, "Remove Attachments", NULL, 0, 0, NULL},
*/
		{NM_TITLE, N_("Settings"), NULL, 0, 0, (APTR)MENU_SETTINGS},
		{NM_ITEM, N_("Show Folders?"), NULL, CHECKED|CHECKIT|MENUTOGGLE, 0, (APTR)MENU_SETTINGS_SHOW_FOLDERS},
		{NM_ITEM, N_("Show Addressbook?"), NULL, CHECKED|CHECKIT|MENUTOGGLE, 0, (APTR)MENU_SETTINGS_SHOW_ADDRESSBOOK},
		{NM_ITEM, N_("Show Selected message?"), NULL, CHECKED|CHECKIT|MENUTOGGLE, 0, (APTR)MENU_SETTINGS_SHOW_SELECTED_MESSAGE},
		{NM_ITEM, N_("Show Quick Filter?"), NULL, CHECKED|CHECKIT|MENUTOGGLE, 0, (APTR)MENU_SETTINGS_SHOW_QUICK_FILTER},
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

	int i;

	SM_ENTER;

	/* translate the menu entries */
	if (!(main_newmenu = AllocVec(sizeof(nm_untranslated),MEMF_SHARED))) return 0;
	memcpy(main_newmenu,nm_untranslated,sizeof(nm_untranslated));

	for (i=0;i<ARRAY_LEN(nm_untranslated)-1;i++)
	{
		if (main_newmenu[i].nm_Label && main_newmenu[i].nm_Label != NM_BARLABEL)
		{
			STRPTR tmpstring = StrCopy(_(main_newmenu[i].nm_Label));
			if (tmpstring[1] == ':') tmpstring[1] = 0;
			main_newmenu[i].nm_Label = tmpstring;
		}
	}

	rc = FALSE;

	main_menu = MUI_MakeObject(MUIO_MenustripNM, main_newmenu, MUIO_MenustripNM_CommandKeyCheck);
	mail_listview = MakeMailTreelist(MAKE_ID('M','W','M','T'),&mail_tree);

	win_main = WindowObject,
		MUIA_HelpNode, "MA_W",
		MUIA_Window_ID, MAKE_ID('M','A','I','N'),
		MUIA_Window_Title, VERS,

		MUIA_Window_Menustrip,main_menu,

		WindowContents, main_group = VGroup,
			Child, HGroup,
				InnerSpacing(0,0),
				Child, main_toolbar = SMToolbarObject,
					MUIA_Group_Horiz, TRUE,
					MUIA_SMToolbar_InVGroup, TRUE,
					MUIA_SMToolbar_Buttons, sm_mainwnd_buttons,
					End,
#ifndef __AROS__  /* will be fixed*/
				Child, filter_group = VGroup,
					MUIA_Weight, 20,
					InnerSpacing(0,0),
					GroupSpacing(0),
					Child, HVSpace,
					Child, HGroup,
						InnerSpacing(0,0),
						GroupSpacing(0),
						Child, filter_string = UTF8StringObject,
							MUIA_CycleChain,1,
							MUIA_ShortHelp, _("Allows you to filter for messages containing the given texts within their From or Subject fields."),
							StringFrame,
							End,
						Child, filter_clear_button = MakeButton("X"),
						End,
					Child, HVSpace,
					End,
#endif
				End,

			Child, folder_group = HGroup,
				MUIA_Group_Spacing, 0,
				Child, folder_text = TextObject, TextFrame, MUIA_Text_PreParse, MUIX_C, MUIA_Background, MUII_TextBack, End,
				Child, folder_popupmenu = PopupmenuObject,
					ImageButtonFrame,
					MUIA_CycleChain, 1,
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
					Child, mail_listview,
					Child, right_balance = BalanceObject, End,
					Child, mail_messageview = MessageViewObject,
						MUIA_CycleChain, 1,
						MUIA_ObjectID, MAKE_ID('M','W','M','V'),
						End,
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

		SetAttrs(mail_tree,
				MUIA_MailTreelist_UseCustomBackground, !user.config.dont_draw_alternating_rows,
				MUIA_MailTreelist_RowBackgroundRGB, user.config.row_background,
				MUIA_MailTreelist_AltRowBackgroundRGB, user.config.alt_row_background,
				TAG_DONE);

		set(filter_clear_button,MUIA_Weight,0);

		main_settings_folder_menuitem = (Object*)DoMethod(main_menu,MUIM_FindUData,MENU_SETTINGS_SHOW_FOLDERS);
		main_settings_addressbook_menuitem = (Object*)DoMethod(main_menu,MUIM_FindUData, MENU_SETTINGS_SHOW_ADDRESSBOOK);
		main_settings_messageview_menuitem = (Object*)DoMethod(main_menu,MUIM_FindUData, MENU_SETTINGS_SHOW_SELECTED_MESSAGE);
		main_settings_filter_menuitem = (Object*)DoMethod(main_menu,MUIM_FindUData, MENU_SETTINGS_SHOW_QUICK_FILTER);
		main_scripts_menu = (Object*)DoMethod(main_menu,MUIM_FindUData,MENU_SCRIPTS);
		main_scripts_execute_menuitem = (Object*)DoMethod(main_menu,MUIM_FindUData,MENU_SCRIPTS_EXECUTESCRIPT);

		set(win_main, MUIA_Window_DefaultObject, mail_tree);
		set(main_settings_folder_menuitem,MUIA_ObjectID,MAKE_ID('M','N','S','F'));
		set(main_settings_addressbook_menuitem,MUIA_ObjectID,MAKE_ID('M','N','S','A'));
		set(main_settings_messageview_menuitem,MUIA_ObjectID,MAKE_ID('M','N','M','V'));
		set(main_settings_filter_menuitem,MUIA_ObjectID,MAKE_ID('M','N','F','I'));

		settings_show_changed();

		project_checksingleaccount_menuitem = (Object*)DoMethod(main_menu, MUIM_FindUData, MENU_PROJECT_CHECKSINGLEACCOUNT);

		DoMethod(App, OM_ADDMEMBER, (ULONG)win_main);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_CloseRequest, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

		/* Menu notifies */
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_PROJECT_ABOUT, (ULONG)App, 6, MUIM_Application_PushMethod, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)display_about);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_PROJECT_ABOUTMUI, (ULONG)App, 2, MUIM_Application_AboutMUI, 0);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_PROJECT_OPEN, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)open_message);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_PROJECT_IMPORTMBOX, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_import_mbox, 0);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_PROJECT_IMPORTDBX, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_import_dbx, 0);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_PROJECT_FETCH, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_fetch_mails);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_PROJECT_SEND, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_send_mails);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_PROJECT_QUIT, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_NEWGROUP, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_new_group);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_NEWFOLDER, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_new_folder);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_DELETE, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_remove_folder);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_OPTIONS, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_edit_folder);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_ORDER_SAVE, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)folder_save_order);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_ORDER_RESET, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_reload_folder_order);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_RESCAN, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_rescan_folder);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_DELALLINDEX, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_delete_all_indexfiles);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_SAVEALLINDEX, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_save_all_indexfiles);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_IMPORTMBOX, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_import_mbox, 1);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_EXPORT, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_export);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_SPAMCHECK, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_check_selected_folder_for_spam);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_MOVESPAM, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_move_spam_marked_mails);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_HAM, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_classify_selected_folder_as_ham);

		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_MESSAGE_NEW, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_new_mail);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_MESSAGE_REPLY, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_reply_selected_mails);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_MESSAGE_SAVE, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_save_active_mail);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_MESSAGE_FORWARD, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_forward_selected_mails);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_MESSAGE_READ, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_read_active_mail);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_MESSAGE_EDIT, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_change_mail);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_MESSAGE_RAW, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_show_raw);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_MESSAGE_MOVE, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_move_selected_mails);
/*
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_MESSAGE_COPY, App, 2, MUIM_Application_ReturnID,  MUIV_Application_ReturnID_Quit);
*/
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_MESSAGE_SELECT_ALL, (ULONG)mail_listview, 1, MUIM_MailTreelist_SelectAll);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_MESSAGE_CLEAR_SELECTION, (ULONG)mail_listview, 1, MUIM_MailTreelist_ClearSelection);

		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_SETTINGS_MUI, (ULONG)App, 2, MUIM_Application_OpenConfigWindow, 0);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_SETTINGS_CONFIGURATION, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_config);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_SETTINGS_FILTER, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_edit_filter);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_SETTINGS_SHOW_FOLDERS, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)settings_show_changed);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_SETTINGS_SHOW_ADDRESSBOOK, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)settings_show_changed);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_SETTINGS_SHOW_SELECTED_MESSAGE, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)settings_show_changed);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_SETTINGS_SHOW_QUICK_FILTER, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)settings_quick_filter_changed);

		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_SETTINGS_SAVEPREFS, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)main_save_environment);

		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_SCRIPTS_EXECUTESCRIPT, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)menu_execute_script, -1);

		/* Toolbar notifies */
		DoMethod(main_toolbar, MUIM_SMToolbar_DoMethod, SM_MAINWND_BUTTON_READ,        8, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_read_active_mail);
		DoMethod(main_toolbar, MUIM_SMToolbar_DoMethod, SM_MAINWND_BUTTON_EDIT,        8, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_change_mail);
		DoMethod(main_toolbar, MUIM_SMToolbar_DoMethod, SM_MAINWND_BUTTON_MOVE,        8, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_move_selected_mails);
		DoMethod(main_toolbar, MUIM_SMToolbar_DoMethod, SM_MAINWND_BUTTON_DELETE,      8, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_delete_mails);
		DoMethod(main_toolbar, MUIM_SMToolbar_DoMethod, SM_MAINWND_BUTTON_GETADDRESS,  8, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_get_address);
		DoMethod(main_toolbar, MUIM_SMToolbar_DoMethod, SM_MAINWND_BUTTON_NEW,         8, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_new_mail);
		DoMethod(main_toolbar, MUIM_SMToolbar_DoMethod, SM_MAINWND_BUTTON_REPLY,       8, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_reply_selected_mails);
		DoMethod(main_toolbar, MUIM_SMToolbar_DoMethod, SM_MAINWND_BUTTON_FORWARD,     8, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_forward_selected_mails);
		DoMethod(main_toolbar, MUIM_SMToolbar_DoMethod, SM_MAINWND_BUTTON_FETCH,       8, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_fetch_mails);
		DoMethod(main_toolbar, MUIM_SMToolbar_DoMethod, SM_MAINWND_BUTTON_SEND,        8, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_send_mails);
		DoMethod(main_toolbar, MUIM_SMToolbar_DoMethod, SM_MAINWND_BUTTON_SEARCH,      8, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_search);
		DoMethod(main_toolbar, MUIM_SMToolbar_DoMethod, SM_MAINWND_BUTTON_FILTER,      8, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_filter);
		DoMethod(main_toolbar, MUIM_SMToolbar_DoMethod, SM_MAINWND_BUTTON_SPAM,        8, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_check_selected_folder_for_spam);
		DoMethod(main_toolbar, MUIM_SMToolbar_DoMethod, SM_MAINWND_BUTTON_ISOLATE,     8, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_move_spam_marked_mails);
		DoMethod(main_toolbar, MUIM_SMToolbar_DoMethod, SM_MAINWND_BUTTON_ADDRESSBOOK, 8, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_addressbook);
		DoMethod(main_toolbar, MUIM_SMToolbar_DoMethod, SM_MAINWND_BUTTON_EDITFILTER,  8, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_edit_filter);
		DoMethod(main_toolbar, MUIM_SMToolbar_DoMethod, SM_MAINWND_BUTTON_CONFIG,      8, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_config);

		/* Key notifies */
		DoMethod(win_main, MUIM_Notify, MUIA_Window_InputEvent, (ULONG)"delete", (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)main_window_delete_pressed);

		DoMethod(switch1_button, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)switch_folder_view);
		DoMethod(switch2_button, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)switch_folder_view);
		DoMethod(mail_tree, MUIM_Notify, MUIA_MailTreelist_Active, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_mail_within_main_selected);
		DoMethod(mail_tree, MUIM_Notify, MUIA_MailTreelist_DoubleClick, MUIV_EveryTime, MUIV_Notify_Application, 3,  MUIM_CallHook, (ULONG)&hook_standard, (ULONG)mailtreelist_doubleclick);
		DoMethod(mail_tree, MUIM_Notify, MUIA_NList_TitleClick, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)mailtreelist_title_click);
		DoMethod(mail_tree, MUIM_Notify, MUIA_NList_TitleClick2, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)mailtreelist_title_click2);
		DoMethod(mail_tree, MUIM_Notify, MUIA_MailTreelist_TitleClick, MUIV_EveryTime, MUIV_Notify_Application, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)mailtreelist_2_title_click, 1);
		DoMethod(mail_tree, MUIM_Notify, MUIA_MailTreelist_TitleClick2, MUIV_EveryTime, MUIV_Notify_Application, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)mailtreelist_2_title_click, 2);
		DoMethod(folder_tree, MUIM_Notify, MUIA_NListtree_Active, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_folder_active);
		DoMethod(folder_tree, MUIM_Notify, MUIA_NListtree_Active, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)main_refresh_folders_text);
		DoMethod(folder_tree, MUIM_Notify, MUIA_FolderTreelist_MailDrop, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)foldertreelist_maildrop);
		DoMethod(folder_tree, MUIM_Notify, MUIA_FolderTreelist_OrderChanged, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)foldertreelist_orderchanged);
		DoMethod(folder_tree, MUIM_Notify, MUIA_NListtree_DoubleClick, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)foldertreelist_doubleclick);
		DoMethod(folder_popupmenu, MUIM_Notify, MUIA_Popupmenu_Selected, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)popup_selected);
		set(folder_tree,MUIA_UserData,mail_tree); /* for the drag'n'drop support */
		DoMethod(address_list, MUIM_Notify, MUIA_NList_DoubleClick, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)addressentrylist_doubleclick);
		DoMethod(filter_string, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)callback_quick_filter_changed);
		DoMethod(filter_clear_button, MUIM_Notify, MUIA_Pressed, FALSE, filter_string, 3, MUIM_Set, MUIA_UTF8String_Contents, "");
		DoMethod(filter_clear_button, MUIM_Notify, MUIA_Pressed, FALSE, filter_string, 3, MUIM_Set, MUIA_String_Acknowledge, TRUE);

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
		DoMethod(win_main, MUIM_Window_ScreenToFront);

		return 1;
	}
	return 0;
}


/**
 * Frees entries associated with the accounts menu.
 */
static void main_free_accounts(void)
{
	Object *o = project_checksingleaccount_menuitem;
	struct List *child_list = (struct List*)xget(o,MUIA_Family_List);
	Object *cstate = (Object *)child_list->lh_Head;
	Object *child;

	while ((child = (Object*)NextObject(&cstate)))
	{
		char *title;

		DoMethod(o,OM_REMMEMBER, (ULONG)child);

		if ((title = (char*)xget(child,MUIA_Menuitem_Title)))
			FreeVec(title);
		MUI_DisposeObject(child);
	}
}

/**
 * Deinitializes the main window.
 */
void main_window_deinit(void)
{
	int i;

	if (win_main)
	{
		set(win_main, MUIA_Window_Open, FALSE);
		main_free_accounts();
	}

	if (main_newmenu)
	{
		/* Free labels and new menu structure */
		for (i=0;main_newmenu[i].nm_Type != NM_END;i++)
		{
			if (main_newmenu[i].nm_Label != NM_BARLABEL)
				FreeVec(main_newmenu[i].nm_Label);
		}
		FreeVec(main_newmenu);
	}
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
			sm_snprintf(buf, sizeof(buf), MUIX_B "%s"  MUIX_N " %s  " MUIX_B "%s"  MUIX_N " %d  " MUIX_B "%s"  MUIX_N " %d  " MUIX_B "%s"  MUIX_N " %d  ",
			        _("Folder:"),f->name,_("Total:"),f->num_mails,_("New:"),f->new_mails,_("Unread:"),f->unread_mails);
			set(folder_text, MUIA_Text_Contents,buf);
		}
	}

	if (folder2_text)
	{
		int total_msg,total_unread,total_new;
		folder_get_stats(&total_msg,&total_unread,&total_new);
		sm_snprintf(buf, sizeof(buf), _("Total:%d New:%d Unread:%d"),total_msg,total_new,total_unread);
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
			sprintf(buf,_("%s (T:%d N:%d U:%d)"), f->name, f->num_mails, f->new_mails, f->unread_mails);
			DoMethod(folder_popupmenu, MUIM_Popupmenu_AddEntry, (ULONG)buf, i);
		}
	}
	set(folder_tree, MUIA_NListtree_Quiet, TRUE);
	DoMethod(folder_tree, MUIM_FolderTreelist_Refresh, NULL);
	nnset(folder_tree, MUIA_NList_Active, act);
	set(folder_tree, MUIA_NListtree_Quiet, FALSE);
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
		DoMethod(folder_tree, MUIM_NListtree_Redraw, (ULONG)tree_node, 0);
	}
	main_refresh_folders_text();

	if (folder->parent_folder) main_refresh_folder(folder->parent_folder);
}

/******************************************************************
 Inserts a new mail into the listview at the end
*******************************************************************/
void main_insert_mail(struct mail_info *mail)
{
	DoMethod(mail_tree, MUIM_MailTree_InsertMail, (ULONG)mail, -2);
}

/******************************************************************
 Inserts a new mail into the listview after a given position
*******************************************************************/
void main_insert_mail_pos(struct mail_info *mail, int after)
{
	DoMethod(mail_tree, MUIM_MailTree_InsertMail, (ULONG)mail, after);
}

/******************************************************************
 Remove a given mail from the listview
*******************************************************************/
void main_remove_mail(struct mail_info *mail)
{
	DoMethod(mail_tree, MUIM_MailTree_RemoveMail, (ULONG)mail);
}

/******************************************************************
 Replaces a mail with a new mail.
 This also activates the new mail, however it would be better if
 this done by an extra call
*******************************************************************/
void main_replace_mail(struct mail_info *oldmail, struct mail_info *newmail)
{
	DoMethod(mail_tree, MUIM_MailTree_ReplaceMail, (ULONG)oldmail, (ULONG)newmail);
}

/******************************************************************
 Refresh a mail (if it status has changed fx)
*******************************************************************/
void main_refresh_mail(struct mail_info *m)
{
	DoMethod(mail_tree, MUIM_MailTree_RefreshMail, (ULONG)m);
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
	struct folder *created_folder = NULL;
	utf8 *filter = main_get_quick_filter_contents();

	if (filter && *filter)
	{
		if ((created_folder = folder_create_live_filter(folder,filter)))
			folder = created_folder;
	}

	DoMethod(mail_tree, MUIM_MailTree_SetFolderMails, (ULONG)folder);

	if (created_folder)
		folder_delete_live_folder(created_folder);

	mailtreelist_update_title_markers();
}

/******************************************************************
 Activates a different folder
*******************************************************************/
void main_set_folder_active(struct folder *folder)
{
	struct MUI_NListtree_TreeNode *tn;

	SM_ENTER;
	SM_DEBUGF(20,("Set active folder to \"%s\"\n",folder->name));

	tn = FindListtreeUserData(folder_tree, folder);
	set(folder_tree,MUIA_NListtree_Active,tn);

	SM_LEAVE;
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
void main_set_active_mail(struct mail_info *m)
{
	set(mail_tree, MUIA_MailTree_Active, m);
}

/******************************************************************
 Returns the active mail. NULL if no one is active
*******************************************************************/
struct mail_info *main_get_active_mail(void)
{
	return (struct mail_info*)xget(mail_tree, MUIA_MailTreelist_Active);
}

/******************************************************************
 Returns the filename of the active mail, NULL if no thing is
 selected
*******************************************************************/
char *main_get_mail_filename(void)
{
	struct mail_info *m = main_get_active_mail();
	if (m) return m->filename;
	return NULL;
}

/******************************************************************
 Returns the contents of the quick filter (or NULL).
*******************************************************************/
utf8 *main_get_quick_filter_contents(void)
{
	if (xget(main_settings_filter_menuitem,MUIA_Menuitem_Checked))
		return (utf8*)getutf8string(filter_string);
	return NULL;
}

/******************************************************************
 Returns the first selected mail. NULL if no mail is selected
*******************************************************************/
struct mail_info *main_get_mail_first_selected(void *handle)
{
	return (struct mail_info*)DoMethod(mail_tree, MUIM_MailTree_GetFirstSelected, (ULONG)handle);
}

/******************************************************************
 Returns the next selected mail. NULL if no more mails are
 selected
*******************************************************************/
struct mail_info *main_get_mail_next_selected(void *handle)
{
	return (struct mail_info*)DoMethod(mail_tree, MUIM_MailTree_GetNextSelected, (ULONG)handle);
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

	main_free_accounts();

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
				MUIA_Menuitem_Title, StrCopy(buf),
				End;

			DoMethod(project_checksingleaccount_menuitem, OM_ADDMEMBER, (ULONG)entry);
			DoMethod(entry, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)menu_check_single_account, i);
		}
		account = (struct account*)node_next(&account->node);
		i++;
	}

	if (!i)
	{
		Object *entry = MenuitemObject, MUIA_Menuitem_Title, StrCopy(_("No fetchable account specified")),	End;
		DoMethod(project_checksingleaccount_menuitem, OM_ADDMEMBER, (ULONG)entry);
	}
}

/******************************************************************
 Build the scripts menu
*******************************************************************/
void main_build_scripts(void)
{
	DoMethod(main_scripts_menu, OM_REMMEMBER, (ULONG)main_scripts_execute_menuitem);
	DisposeAllFamilyChilds(main_scripts_menu);
	DoMethod(main_scripts_menu, OM_ADDMEMBER, (ULONG)main_scripts_execute_menuitem);
}

/******************************************************************
 Build the addressbook which is displayed in this window
*******************************************************************/
void main_build_addressbook(void)
{
	DoMethod(address_list, MUIM_AddressEntryList_Refresh, NULL);
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
 Return wheter the message view is displayed.
*******************************************************************/
int main_is_message_view_displayed(void)
{
	return !!xget(main_settings_messageview_menuitem,MUIA_Menuitem_Checked);
}

/******************************************************************
 A amiga private function!!! Returns the screen of the main window
*******************************************************************/
struct Screen *main_get_screen(void)
{
	return (struct Screen*)xget(win_main, MUIA_Window_Screen);
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
	struct mail_info *m = main_get_active_mail();
	char *f = main_get_folder_drawer();

	DoMethod(mail_messageview, MUIM_MessageView_DisplayMail, (ULONG)m, (ULONG)f);
}

/******************************************************************
 Refresh the title of the main window
*******************************************************************/
void main_refresh_window_title(unsigned int autocheck_seconds_start)
{
	static char win_main_title[128];

	if (win_main)
	{
		if (user.config.receive_autocheck)
			sprintf(win_main_title, _("%s (next autocheck at %s)"), VERS, sm_get_time_str(autocheck_seconds_start + user.config.receive_autocheck * 60));
		else
			sprintf(win_main_title, "%s", VERS);

		set(win_main, MUIA_Window_Title, win_main_title);
	}
}
