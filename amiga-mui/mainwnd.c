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
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#define MUIV_NListtree_Remove_Flag_NoActive (1<<13) /* internal */

#include "SimpleMail_rev.h"

#include "account.h"
#include "arexx.h"
#include "configuration.h"
#include "folder.h"
#include "mail.h"
#include "simplemail.h"
#include "smintl.h"
#include "support_indep.h"

#include "addresstreelistclass.h"
#include "amigasupport.h"
#include "compiler.h"
#include "foldertreelistclass.h"
#include "mainwnd.h"
#include "mailtreelistclass.h"
#include "muistuff.h"
#include "picturebuttonclass.h"
#include "popupmenuclass.h"

static const char *image_files[] = {
		"PROGDIR:Images/MailRead",
		"PROGDIR:Images/MailModify",
		"PROGDIR:Images/MailDelete",
		"PROGDIR:Images/MailGetAddress",
		"PROGDIR:Images/MailNew",
		"PROGDIR:Images/MailReply",
		"PROGDIR:Images/MailForward",
		"PROGDIR:Images/MailsFetch",
		"PROGDIR:Images/MailsSend",
		"PROGDIR:Images/Filter",
		"PROGDIR:Images/FilterEdit",
		"PROGDIR:Images/Addressbook",
		"PROGDIR:Images/Config",
		NULL
};
/*struct MyBrush *brushes[sizeof(image_files)/sizeof(char*)];*/

static Object *win_main;
static Object *main_menu;
static Object *main_settings_folder_menuitem;
static Object *main_settings_addressbook_menuitem;
static Object *main_scripts_menu;
static Object *main_scripts_execute_menuitem;
static Object *main_group;
static Object *button_fetch;
static Object *button_send;
static Object *button_read;
static Object *button_getadd;
static Object *button_delete;
static Object *button_change;
static Object *button_new;
static Object *button_reply;
static Object *button_forward;
static Object *button_filter;
static Object *button_efilter;
static Object *button_abook;
static Object *button_config;
static Object *switch1_button; /* switch button for the two views */
static Object *switch2_button; /* switch button for the two views */
static Object *mail_tree;
static Object *mail_tree_group;
static Object *mail_listview;
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
static Object *address_tree;
static Object *left_listview_group;
static Object *left_listview_balance;
static Object *folder_checksingleaccount_menuitem;

static int folders_in_popup;

static void main_refresh_folders_text(void);
static void settings_show_changed(void);

struct MUI_NListtree_TreeNode *FindListtreeUserData(Object *tree, APTR udata)
{
/*
	DoMethod(tree, MUIA_NListtree_FindUserDataHook, MUIV_NListtree_FindUserDataHook_PointerCompare);
	return (struct MUI_NListtree_TreeNode *)DoMethod(tree, MUIM_NListtree_FindUserData,  MUIV_NListtree_FindUserData_ListNode_Root, udata, 0);
*/

	int i;
	int count = DoMethod(tree,MUIM_NListtree_GetNr, MUIV_NListtree_GetNr_TreeNode_Active,MUIV_NListtree_GetNr_Flag_CountAll);

	for (i=0;i<count;i++)
	{
		struct MUI_NListtree_TreeNode *tn = (struct MUI_NListtree_TreeNode*)
			DoMethod(tree,MUIM_NListtree_GetEntry,MUIV_NListtree_GetEntry_ListNode_Root,i,0);
		if (tn->tn_User == udata) return tn;
	}
	return NULL;
}

/******************************************************************
 Display the about Requester
*******************************************************************/
void display_about(void)
{
	MUI_Request(App, NULL, 0, 
		_("SimpleMail - About"),
		_("*Ok"),
		"SimpleMail version %ld.%ld\n\n%s 2000-2001\nHynek Schlawack %s Sebastian Bauer\n%s.",
		VERSION,REVISION,_("Copyright ©"),_("and"),_("Released under the terms of the GNU Public License"));
}

/******************************************************************
 Converts a given sort mode to a nlist title mark
*******************************************************************/
static ULONG sortmode2titlemark(int sortmode, int type)
{
	int col = 0;
	if (sortmode == FOLDER_SORT_THREAD) return 0;
	col = sortmode & FOLDER_SORT_MODEMASK;
	return (col | ((sortmode & FOLDER_SORT_REVERSE)?MUIV_NList_TitleMark_Up:MUIV_NList_TitleMark_Down));
}

/******************************************************************
 Converts a given nlist title mark to a sort mode
*******************************************************************/
static int titlemark2sortmode(int titlemark)
{
	int col = titlemark & MUIV_NList_TitleMark_ColMask;
	if (titlemark == 0) return FOLDER_SORT_THREAD;
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
		struct MUI_NListtree_TreeNode *tn = (struct MUI_NListtree_TreeNode*)
			DoMethod(folder_tree,MUIM_NListtree_GetEntry,MUIV_NListtree_GetEntry_ListNode_Root,i,0);
		struct MUI_NListtree_TreeNode *parent = (struct MUI_NListtree_TreeNode*)
			DoMethod(folder_tree,MUIM_NListtree_GetEntry,tn,MUIV_NListtree_GetEntry_Position_Parent,0);
		struct folder *f = (struct folder*)tn->tn_User;

		folder_add_to_tree(f,parent?(struct folder*)parent->tn_User:NULL);
	}
}

/******************************************************************
 An entry has been doubleclicked
*******************************************************************/
static void foldertreelist_doubleclick(void)
{
	struct folder *f = main_get_folder();
	if (f->special != FOLDER_SPECIAL_GROUP)
		callback_edit_folder();
}

/******************************************************************
 An addressbook entry has been doubleclicked
*******************************************************************/
static void addresstreelist_doubleclick(void)
{
	struct MUI_NListtree_TreeNode *treenode = (struct MUI_NListtree_TreeNode *)xget(address_tree, MUIA_NListtree_Active);
	if (treenode && treenode->tn_User)
	{
		callback_write_mail_to((struct addressbook_entry *)treenode->tn_User);
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
					ASLFR_InitialFile, initial_file,
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
	int folder, addressbook;

	DoMethod(main_group, MUIM_Group_InitChange);
	DoMethod(mail_tree_group, MUIM_Group_InitChange);

	folder = xget(main_settings_folder_menuitem,MUIA_Menuitem_Checked);
	addressbook = xget(main_settings_addressbook_menuitem,MUIA_Menuitem_Checked);

	set(folder_group, MUIA_ShowMe, !folder);
	set(folder_listview_group, MUIA_ShowMe, folder);
	set(address_listview_group,MUIA_ShowMe, addressbook);
	set(folder_balance, MUIA_ShowMe, folder || addressbook);
	set(left_listview_group, MUIA_ShowMe, folder || addressbook);
	set(left_listview_balance, MUIA_ShowMe, folder && addressbook);

	DoMethod(mail_tree_group, MUIM_Group_ExitChange);
	DoMethod(main_group, MUIM_Group_ExitChange);
}

/******************************************************************
 Frees the brushes
*******************************************************************/
/*
static void main_free_brushes(void)
{
	int i;
	for (i=0;i<sizeof(image_files)/sizeof(char*);i++)
	  if (brushes[i]) FreeBrush(brushes[i]);
}
*/

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
		MENU_PROJECT_QUIT,
		MENU_FOLDER,
		MENU_FOLDER_NEWGROUP,
		MENU_FOLDER_NEWFOLDER,
		MENU_FOLDER_DELETE,
		MENU_FOLDER_OPTIONS,
		MENU_FOLDER_ORDER,
		MENU_FOLDER_ORDER_SAVE,
		MENU_FOLDER_ORDER_RESET,
		MENU_FOLDER_SEND,
		MENU_FOLDER_FETCH,
		MENU_FOLDER_CHECKSINGLEACCOUNT,
		MENU_MESSAGE_READ,
		MENU_MESSAGE_EDIT,
		MENU_MESSAGE_MOVE,
		MENU_MESSAGE_COPY,
		MENU_MESSAGE_DELETE,
		MENU_SETTINGS,
		MENU_SETTINGS_SHOW_FOLDERS,
		MENU_SETTINGS_SHOW_ADDRESSBOOK,
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
		{NM_ITEM, N_("Q:Quit"), NULL, 0, 0, (APTR)MENU_PROJECT_QUIT},
		{NM_TITLE, N_("Folder"), NULL, 0, 0, NULL},
		{NM_ITEM, N_("New Group..."), NULL, 0, 0, (APTR)MENU_FOLDER_NEWGROUP},
		{NM_ITEM, N_("New Folder..."), NULL, 0, 0, (APTR)MENU_FOLDER_NEWFOLDER},
		{NM_ITEM, N_("Delete"), NULL, 0, 0, (APTR)MENU_FOLDER_DELETE},
		{NM_ITEM, N_("Settings..."), NULL, 0, 0, (APTR)MENU_FOLDER_OPTIONS},
		{NM_ITEM, N_("Order"), NULL, 0, 0, NULL},
		{NM_SUB, N_("Save"), NULL, 0, 0, (APTR)MENU_FOLDER_ORDER_SAVE},
		{NM_SUB, N_("Reset"), NULL, 0, 0, (APTR)MENU_FOLDER_ORDER_RESET},
		{NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
		{NM_ITEM, N_("S:Send queued mails..."), NULL, 0, 0, (APTR)MENU_FOLDER_SEND},
		{NM_ITEM, N_("F:Fetch mails..."), NULL, 0, 0, (APTR)MENU_FOLDER_FETCH},
		{NM_ITEM, N_("Check single account"), NULL, 0, 0, (APTR)MENU_FOLDER_CHECKSINGLEACCOUNT},

		{NM_TITLE, N_("Message"), NULL, 0, 0, NULL},
		{NM_ITEM, N_("D:Read"), NULL, 0, 0, (APTR)MENU_MESSAGE_READ},
		{NM_ITEM, N_("E:Edit"), NULL, 0, 0, (APTR)MENU_MESSAGE_EDIT},
		{NM_ITEM, N_("M:Move..."), NULL, NM_ITEMDISABLED, 0L, (APTR)MENU_MESSAGE_MOVE},
		{NM_ITEM, N_("Copy..."), NULL, NM_ITEMDISABLED, 0L, (APTR)MENU_MESSAGE_COPY},
		{NM_ITEM, N_("Delete..."), "Del", NM_COMMANDSTRING, 0L, (APTR)MENU_MESSAGE_DELETE},
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
						MUIA_Weight, 200,
						Child, button_read = MakePictureButton(_("Rea_d"),"PROGDIR:Images/MailRead"),
						Child, button_change = MakePictureButton(_("_Edit"),"PROGDIR:Images/MailModify"),
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
						Child, button_fetch = MakePictureButton(_("Fetc_h"),"PROGDIR:Images/MailsFetch"),
						Child, button_send = MakePictureButton(_("_Send"),"PROGDIR:Images/MailsSend"),
						End,
					Child, HGroup,
						MUIA_Group_Spacing, 0,
						Child, button_filter = MakePictureButton(_("F_ilter"),"PROGDIR:Images/Filter"),	
						Child, button_efilter = MakePictureButton(_("_Filters"),"PROGDIR:Images/FilterEdit"),
						End,
					Child, HGroup,
						MUIA_Group_Spacing, 0,
						Child, button_abook = MakePictureButton(_("_Abook"),"PROGDIR:Images/Addressbook"),
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
							MUIA_NListview_NList, address_tree = AddressTreelistObject,
								MUIA_AddressTreelist_InAddressbook, 0,
								MUIA_ObjectID, MAKE_ID('M','W','A','L'),
								End,
							End,
						End,
					End,
				Child, folder_balance = BalanceObject, End,
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
		main_settings_folder_menuitem = (Object*)DoMethod(main_menu,MUIM_FindUData,MENU_SETTINGS_SHOW_FOLDERS);
		main_settings_addressbook_menuitem = (Object*)DoMethod(main_menu,MUIM_FindUData, MENU_SETTINGS_SHOW_ADDRESSBOOK);
		main_scripts_menu = (Object*)DoMethod(main_menu,MUIM_FindUData,MENU_SCRIPTS);
		main_scripts_execute_menuitem = (Object*)DoMethod(main_menu,MUIM_FindUData,MENU_SCRIPTS_EXECUTESCRIPT);

		set(main_settings_folder_menuitem,MUIA_ObjectID,MAKE_ID('M','N','S','F'));
		set(main_settings_addressbook_menuitem,MUIA_ObjectID,MAKE_ID('M','N','S','A'));

		settings_show_changed();

		if (xget(folder_tree, MUIA_Version) < 1 || (xget(folder_tree, MUIA_Version) >= 1 && xget(folder_tree, MUIA_Revision)<8))
		{
	 	printf(_("SimpleMail needs at least version 1.8 of the NListtree mui subclass!\nIt's available at %s"),"http://www.aphaso.de\n");
	 	return 0;
		}

		folder_checksingleaccount_menuitem = (Object*)DoMethod(main_menu, MUIM_FindUData, MENU_FOLDER_CHECKSINGLEACCOUNT);

		DoMethod(App, OM_ADDMEMBER, win_main);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_CloseRequest, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

		/* Menu notifies */
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_PROJECT_ABOUT, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, display_about);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_PROJECT_ABOUTMUI, App, 2, MUIM_Application_AboutMUI, 0);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_PROJECT_QUIT, App, 2, MUIM_Application_ReturnID,  MUIV_Application_ReturnID_Quit);

		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_NEWGROUP, App, 3, MUIM_CallHook, &hook_standard, callback_new_group);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_NEWFOLDER, App, 3, MUIM_CallHook, &hook_standard, callback_new_folder);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_DELETE, App, 3, MUIM_CallHook, &hook_standard, callback_remove_folder);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_OPTIONS, App, 3, MUIM_CallHook, &hook_standard, callback_edit_folder);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_ORDER_SAVE, App, 3, MUIM_CallHook, &hook_standard, folder_save_order);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_ORDER_RESET, App, 3, MUIM_CallHook, &hook_standard, callback_reload_folder_order);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_SEND, App, 3, MUIM_CallHook, &hook_standard, callback_send_mails);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_FOLDER_FETCH, App, 3, MUIM_CallHook, &hook_standard, callback_fetch_mails);

		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_MESSAGE_READ, App, 3, MUIM_CallHook, &hook_standard, callback_read_mail);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_MESSAGE_EDIT, App, 3, MUIM_CallHook, &hook_standard, callback_change_mail);
/*
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_MESSAGE_MOVE, App, 2, MUIM_Application_ReturnID,  MUIV_Application_ReturnID_Quit);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_MESSAGE_COPY, App, 2, MUIM_Application_ReturnID,  MUIV_Application_ReturnID_Quit);
*/
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_MESSAGE_DELETE, App, 3, MUIM_CallHook, &hook_standard, callback_delete_mails);

		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_SETTINGS_MUI, App, 2, MUIM_Application_OpenConfigWindow, 0);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_SETTINGS_CONFIGURATION,App, 3, MUIM_CallHook, &hook_standard, callback_config);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_SETTINGS_FILTER, App, 3, MUIM_CallHook, &hook_standard, callback_edit_filter);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_SETTINGS_SHOW_FOLDERS, App, 3, MUIM_CallHook, &hook_standard, settings_show_changed);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_SETTINGS_SHOW_ADDRESSBOOK, App, 3, MUIM_CallHook, &hook_standard, settings_show_changed);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_SETTINGS_SAVEPREFS, App, 2, MUIM_Application_Save, MUIV_Application_Save_ENV);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_SETTINGS_SAVEPREFS, App, 2, MUIM_Application_Save, MUIV_Application_Save_ENVARC);

		DoMethod(win_main, MUIM_Notify, MUIA_Window_MenuAction, MENU_SCRIPTS_EXECUTESCRIPT, App, 4, MUIM_CallHook, &hook_standard, menu_execute_script, -1);

//	DoMethod(entry, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, App, 4, MUIM_CallHook, &hook_standard, menu_execute_script, -1);


		/* Key notifies */
		DoMethod(win_main, MUIM_Notify, MUIA_Window_InputEvent, "delete", App, 3, MUIM_CallHook, &hook_standard, callback_delete_mails);

		/* Gadget notifies */
		DoMethod(button_read, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_read_mail);
		DoMethod(button_getadd, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_get_address);
		DoMethod(button_delete, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_delete_mails);
		DoMethod(button_fetch, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_fetch_mails);
		DoMethod(button_send, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_send_mails);
		DoMethod(button_new, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_new_mail);
		DoMethod(button_reply, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_reply_mail);
		DoMethod(button_forward, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_forward_mail);
		DoMethod(button_change, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_change_mail);
		DoMethod(button_filter, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_filter);
		DoMethod(button_efilter, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_edit_filter);
		DoMethod(button_abook, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_addressbook);
		DoMethod(button_config, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_config);

		DoMethod(switch1_button, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, switch_folder_view);
		DoMethod(switch2_button, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, switch_folder_view);
		DoMethod(mail_tree, MUIM_Notify, MUIA_NListtree_DoubleClick, MUIV_EveryTime, MUIV_Notify_Application, 3,  MUIM_CallHook, &hook_standard, callback_read_mail);
		DoMethod(mail_tree, MUIM_Notify, MUIA_NList_TitleClick, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, mailtreelist_title_click);
		DoMethod(folder_tree, MUIM_Notify, MUIA_NListtree_Active, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_folder_active);
		DoMethod(folder_tree, MUIM_Notify, MUIA_NListtree_Active, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, main_refresh_folders_text);
		DoMethod(folder_tree, MUIM_Notify, MUIA_FolderTreelist_MailDrop, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, foldertreelist_maildrop);
		DoMethod(folder_tree, MUIM_Notify, MUIA_FolderTreelist_OrderChanged, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, foldertreelist_orderchanged);
		DoMethod(folder_tree, MUIM_Notify, MUIA_NListtree_DoubleClick, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard,  foldertreelist_doubleclick);
		DoMethod(folder_popupmenu, MUIM_Notify, MUIA_Popupmenu_Selected, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, popup_selected);
		set(folder_tree,MUIA_UserData,mail_tree); /* for the drag'n'drop support */
		DoMethod(address_tree, MUIM_Notify, MUIA_NListtree_DoubleClick, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, addresstreelist_doubleclick);

		main_build_accounts();
		main_build_scripts();
		main_build_addressbook();

		rc = TRUE;
	}
	
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
}

/******************************************************************
 Inserts a new mail into the listview at the end
*******************************************************************/
void main_insert_mail(struct mail *mail)
{
	DoMethod(mail_tree,MUIM_NListtree_Insert,"" /*name*/, mail, /*udata */
					 MUIV_NListtree_Insert_ListNode_Root,MUIV_NListtree_Insert_PrevNode_Tail,0/*flags*/);
}

/******************************************************************
 Inserts a new mail into the listview after a given position
*******************************************************************/
void main_insert_mail_pos(struct mail *mail, int after)
{
	struct MUI_NListtree_TreeNode *tn = NULL;
	int i=0;

	while (after >= 0)
	{
		tn = (struct MUI_NListtree_TreeNode*)DoMethod(mail_tree,MUIM_NListtree_GetEntry,MUIV_NListtree_GetEntry_ListNode_Root,i,0);
		if (tn->tn_User != (APTR)MUIV_MailTreelist_UserData_Name) after--;
		i++;
	}

	DoMethod(mail_tree,MUIM_NListtree_Insert,"" /*name*/, mail, /*udata */
					 MUIV_NListtree_Insert_ListNode_Root,tn?tn:MUIV_NListtree_Insert_PrevNode_Head,0/*flags*/);
}



/******************************************************************
 Remove a given mail from the listview
*******************************************************************/
void main_remove_mail(struct mail *mail)
{
	struct MUI_NListtree_TreeNode *treenode = FindListtreeUserData(mail_tree, mail);
	if (treenode)
	{
		DoMethod(mail_tree, MUIM_NListtree_Remove, MUIV_NListtree_Remove_ListNode_Root, treenode,0);
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
		DoMethod(folder_tree, MUIM_NListtree_Redraw, MUIV_NListtree_Redraw_Active, 0);
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
 Clears the folder list
*******************************************************************/
void main_clear_folder_mails(void)
{
	DoMethod(mail_tree, MUIM_NListtree_Clear, NULL, 0);
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

	main_clear_folder_mails();

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

				if (lm)
				{
					if (folder_get_type(folder) == FOLDER_TYPE_SEND) res = mystricmp(m->to, lm->to);
					else res = mystricmp(m->from, lm->from);
				}

				if (!lm || res)
				{
					treenode = (APTR)DoMethod(mail_tree, MUIM_NListtree_Insert, (folder_get_type(folder) == FOLDER_TYPE_SEND)?(m->to?m->to:""):(m->from?m->from:""), MUIV_MailTreelist_UserData_Name, /* special hint */
							 MUIV_NListtree_Insert_ListNode_Root,MUIV_NListtree_Insert_PrevNode_Tail,TNF_LIST/*|TNF_OPEN*/);
				}
			} else
			{
				if (!lm || mystricmp(m->subject,lm->subject))
				{
					treenode = (APTR)DoMethod(mail_tree, MUIM_NListtree_Insert, m->subject, MUIV_MailTreelist_UserData_Name, /* special hint */
							 MUIV_NListtree_Insert_ListNode_Root,MUIV_NListtree_Insert_PrevNode_Tail,TNF_LIST/*|TNF_OPEN*/);
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

	if ((m = folder_find_best_mail_to_select(folder)))
	{
		set(mail_tree, MUIA_NListtree_Active, FindListtreeUserData(mail_tree, m));
	}

	SetAttrs(mail_tree,
				MUIA_NListtree_Quiet, FALSE,
				TAG_DONE);

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
  struct MUI_NListtree_TreeNode *tn = FindListtreeUserData(mail_tree,m);
  set(mail_tree,MUIA_NListtree_Active,tn);
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
	if ((ULONG)treenode->tn_User == MUIV_MailTreelist_UserData_Name) return main_get_mail_next_selected(handle);
	if (treenode) return (struct mail*)treenode->tn_User;
	return NULL;
}

/******************************************************************
 Returns the next selected mail. NULL if no more mails are
 selected
*******************************************************************/
struct mail *main_get_mail_next_selected(void *handle)
{
	struct MUI_NListtree_TreeNode *treenode;
	do
	{
		treenode = *((struct MUI_NListtree_TreeNode **)handle);
		DoMethod(mail_tree, MUIM_NListtree_NextSelected, &treenode);
		if (treenode == (struct MUI_NListtree_TreeNode *)MUIV_NListtree_NextSelected_End) return NULL;
		*((struct MUI_NListtree_TreeNode **)handle) = treenode;
	} while((ULONG)treenode->tn_User == MUIV_MailTreelist_UserData_Name);
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

	treenode = (struct MUI_NListtree_TreeNode *)MUIV_NListtree_PrevSelected_Start;

	for (;;)
	{
		DoMethod(mail_tree, MUIM_NListtree_PrevSelected, &treenode);
		if (treenode==(struct MUI_NListtree_TreeNode *)MUIV_NListtree_PrevSelected_End)
			break;
		if (!treenode) break;
		i++;
	}

	if (!i) return; /* no emails selected */
	set(mail_tree, MUIA_NListtree_Quiet, TRUE);

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
			if ((ULONG)array[i]->tn_User == MUIV_MailTreelist_UserData_Name) continue;

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

/******************************************************************
 Build the check singe account menu
*******************************************************************/
void main_build_accounts(void)
{
	struct account *account = (struct account*)list_first(&user.config.account_list);
	int i=0;

	DisposeAllFamilyChilds(folder_checksingleaccount_menuitem);

	while (account)
	{
		if (account->pop && account->pop->name)
		{
			Object *entry = MenuitemObject,
				MUIA_Menuitem_Title, account->pop->name,
				End;

			DoMethod(folder_checksingleaccount_menuitem, OM_ADDMEMBER, entry);
			DoMethod(entry, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, App, 4, MUIM_CallHook, &hook_standard, menu_check_single_account,i);
		}
		account = (struct account*)node_next(&account->node);
		i++;
	}

	if (!i)
	{
		Object *entry = MenuitemObject, MUIA_Menuitem_Title, _("No POP3 Server specified"),	End;
		DoMethod(folder_checksingleaccount_menuitem, OM_ADDMEMBER, entry);
	}
}

/******************************************************************
 Build the scripts menu
*******************************************************************/
void main_build_scripts(void)
{
//	Object *entry;

	DoMethod(main_scripts_menu, OM_REMMEMBER, main_scripts_execute_menuitem);
	DisposeAllFamilyChilds(main_scripts_menu);
	DoMethod(main_scripts_menu, OM_ADDMEMBER, main_scripts_execute_menuitem);

//	DoMethod(entry, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, App, 4, MUIM_CallHook, &hook_standard, menu_execute_script, -1);
}

/******************************************************************
 Build the addressbook which is displayed in this window
*******************************************************************/
void main_build_addressbook(void)
{
	DoMethod(address_tree, MUIM_AddressTreelist_Refresh,NULL);
}

/******************************************************************
 Freeze the mail list
*******************************************************************/
void main_freeze_mail_list(void)
{
	set(mail_tree,MUIA_NListtree_Quiet,TRUE);
}

/******************************************************************
 Thaws the mail list
*******************************************************************/
void main_thaw_mail_list(void)
{
	set(mail_tree,MUIA_NListtree_Quiet,FALSE);
}

/******************************************************************
 Select a mail, specified by number
*******************************************************************/
void main_select_mail(int mail)
{
	set(mail_tree,MUIA_NList_Active, mail);
}


/******************************************************************
 A amiga private function!!! Returns the screen of the main window
*******************************************************************/
struct Screen *main_get_screen(void)
{
	return (struct Screen*)xget(win_main,MUIA_Window_Screen);
}
