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
** addressbookwnd.c
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
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "addressbook.h"
#include "simplemail.h"

#include "addressbookwnd.h"
#include "compiler.h"
#include "muistuff.h"
#include "picturebuttonclass.h"
#include "support_indep.h"

struct MUI_NListtree_TreeNode *FindListtreeUserData(Object *tree, APTR udata); /* in mainwnd.c */

/* some local functions and data */
static void addressbook_refresh(void);

/* for the address window */
static Object *address_wnd;
static Object *address_tree;
static struct Hook address_construct_hook;
static struct Hook address_destruct_hook;
static struct Hook address_display_hook;


/* the Person Window */
#define MAX_PERSON_OPEN 10
static int person_open[MAX_PERSON_OPEN];

struct Person_Data /* should be a customclass */
{
	Object *wnd;
	Object *rem_button;
	Object *email_list;
	Object *email_string;
	Object *alias_string;
	Object *realname_string;
	Object *homepage_string;
	Object *description_string;
	Object *street_string;
	Object *city_string;
	Object *country_string;
	Object *phone1_string;
	Object *phone2_string;

	struct addressbook_entry *person; /* NULL if new person */

	int num; /* the number of the window */
	/* more to add */
};

/******************************************************************
 This close and disposed the window (note: this must not be called
 within a normal callback hook, because the object is disposed in
 this function)!
*******************************************************************/
static void person_window_close(struct Person_Data **pdata)
{
	struct Person_Data *data = *pdata;
	set(data->wnd,MUIA_Window_Open,FALSE);
	DoMethod(App,OM_REMMEMBER,data->wnd);
	MUI_DisposeObject(data->wnd);
	if (data->num < MAX_PERSON_OPEN) person_open[data->num] = 0;
	free(data);
}

/******************************************************************
 This close and disposed the window (note: this must not be called
 within a normal callback hook, because the object is disposed in
 this function)!
 The person is added to the list (currently only at the end)
*******************************************************************/
static void person_window_ok(struct Person_Data **pdata)
{
	struct Person_Data *data = *pdata;
	struct addressbook_entry *new_entry;
	set(data->wnd,MUIA_Window_Open,FALSE);

	if ((new_entry = addressbook_create_person((char*)xget(data->realname_string,MUIA_String_Contents), NULL)))
	{
		int i;
		char *alias;
		struct MUI_NListtree_TreeNode *treenode = NULL;

		alias = (char*)xget(data->alias_string,MUIA_String_Contents);
		if (alias && *alias) addressbook_set_alias(new_entry, alias);
		addressbook_set_description(new_entry, (char *)xget(data->description_string,MUIA_String_Contents));

		if (new_entry->u.person.phone1) free(new_entry->u.person.phone1);
		new_entry->u.person.phone1 = mystrdup((char *)xget(data->phone1_string,MUIA_String_Contents));
		if (new_entry->u.person.phone2) free(new_entry->u.person.phone2);
		new_entry->u.person.phone2 = mystrdup((char *)xget(data->phone2_string,MUIA_String_Contents));

		if (new_entry->u.person.street) free(new_entry->u.person.street);
		new_entry->u.person.street = mystrdup((char*)xget(data->street_string,MUIA_String_Contents));
		if (new_entry->u.person.city) free(new_entry->u.person.city);
		new_entry->u.person.city = mystrdup((char*)xget(data->city_string,MUIA_String_Contents));
		if (new_entry->u.person.country) free(new_entry->u.person.country);
		new_entry->u.person.country = mystrdup((char*)xget(data->country_string,MUIA_String_Contents));
		if (new_entry->u.person.homepage) free(new_entry->u.person.homepage);
		new_entry->u.person.homepage = mystrdup((char*)xget(data->homepage_string,MUIA_String_Contents));

		for (i=0;i<xget(data->email_list,MUIA_NList_Entries);i++)
		{
			char *email;
			DoMethod(data->email_list, MUIM_NList_GetEntry, i, &email);
			addressbook_person_add_email(new_entry,email);
		}

		if (data->person)
		{
			if ((treenode = FindListtreeUserData(address_tree, data->person)))
			{
				DoMethod(address_tree, MUIM_NListtree_Rename, treenode, new_entry, MUIV_NListtree_Rename_Flag_User);
			}
		}

		if (!treenode)
		{
			/* Now add it to the listview (in the active list) */
			DoMethod(address_tree, MUIM_NListtree_Insert, "" /*name*/, new_entry, /*udata */
							   MUIV_NListtree_Insert_ListNode_ActiveFallback,MUIV_NListtree_Insert_PrevNode_Tail,0);
		}

		addressbook_free_entry(new_entry);
	}

	person_window_close(pdata);
}

/******************************************************************
 Add a new e-mail address
*******************************************************************/
static void person_add_email(struct Person_Data **pdata)
{
	struct Person_Data *data = *pdata;
	DoMethod(data->email_list, MUIM_NList_InsertSingle, "»New e-mail address«", MUIV_NList_Insert_Bottom);
	set(data->email_list, MUIA_NList_Active, MUIV_NList_Active_Bottom);
}

/******************************************************************
 The email list view has a new active element
*******************************************************************/
static void person_email_list_active(struct Person_Data **pdata)
{
	struct Person_Data *data = *pdata;
	char *email;
	DoMethod(data->email_list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &email);
	if (email)
	{
		set(data->rem_button, MUIA_Disabled, FALSE);

		SetAttrs(data->email_string,
			MUIA_Disabled, FALSE,
			MUIA_String_Contents, email,
			MUIA_NoNotify, TRUE,
			TAG_DONE);

		set(data->wnd, MUIA_Window_ActiveObject, data->email_string);
	} else
	{
		set(data->rem_button, MUIA_Disabled, TRUE);
		SetAttrs(data->email_string,
			MUIA_Disabled, TRUE,
			MUIA_String_Contents, "",
			MUIA_NoNotify, TRUE,
			TAG_DONE);
	}
}

/******************************************************************
 Contents of the email string has been changed
*******************************************************************/
static void person_email_string_contents(struct Person_Data **pdata)
{
	struct Person_Data *data = *pdata;
	LONG active = xget(data->email_list, MUIA_NList_Active);
	if (active >= 0)
	{
		DoMethod(data->email_list,MUIM_NList_ReplaceSingle, xget(data->email_string, MUIA_String_Contents), active, NOWRAP, ALIGN_LEFT);
	}
}

/******************************************************************
 Opens a person window
*******************************************************************/
void person_window_open(struct addressbook_entry *entry)
{
	Object *wnd, *add_button, *rem_button, *email_list, *email_string;
	Object *alias_string, *realname_string, *ok_button, *cancel_button;
	Object *homepage_string, *street_string, *city_string, *country_string, *phone1_string, *phone2_string;
	Object *description_string;
	int num;

	for (num=0; num < MAX_PERSON_OPEN; num++)
		if (!person_open[num]) break;

	wnd = WindowObject,
		(num < MAX_PERSON_OPEN)?MUIA_Window_ID:TAG_IGNORE, MAKE_ID('P','E','R',num),
    MUIA_Window_Title, "SimpleMail - Edit Person",
        
		WindowContents, VGroup,
			Child, VGroup,
				Child, HorizLineTextObject("Electronic mail"),
				Child, HGroup,
					Child, VGroup,
						Child, ColGroup(2),
							Child, MakeLabel("Alias"),
							Child, alias_string = BetterStringObject,
								StringFrame,
								MUIA_CycleChain, 1,
								MUIA_String_AdvanceOnCR, TRUE,
								End,
							Child, MakeLabel("Real Name"),
							Child, realname_string = BetterStringObject,
								StringFrame,
								MUIA_CycleChain, 1,
								MUIA_String_AdvanceOnCR, TRUE,
								End,
							Child, MakeLabel("PGP Key-ID"),
							Child, BetterStringObject,
								StringFrame,
								MUIA_CycleChain, 1,
								MUIA_String_AdvanceOnCR, TRUE,
								End,
							Child, MakeLabel("Homepage"),
							Child, homepage_string = BetterStringObject,
								StringFrame,
								MUIA_CycleChain, 1,
								MUIA_String_AdvanceOnCR, TRUE,
								End,
							End,
						Child, HVSpace,
						End,
					Child, VGroup,
						Child, NListviewObject,
							MUIA_CycleChain, 1,
							MUIA_NListview_NList, email_list = NListObject,
								MUIA_NList_DragSortable, TRUE,
								MUIA_NList_ConstructHook, MUIV_NList_ConstructHook_String,
								MUIA_NList_DestructHook, MUIV_NList_DestructHook_String,
								End,
							End,
						Child, HGroup,
							Child, email_string = BetterStringObject,
								StringFrame,
								MUIA_CycleChain, 1,
								End,
							Child, HGroup,
								MUIA_Group_Spacing, 0,
								Child, add_button = MakeButton("Add"),
								Child, rem_button = MakeButton("Rem"),
								End,
							End,
						End,
					End,
				End,

			Child, HGroup,
				Child, ColGroup(2),
					Child, HorizLineTextObject("Snail mail"),
					Child, HorizLineTextObject("Miscellanous"),
/*					Child, HorizLineTextObject("Portrait"),*/
	
					Child, ColGroup(2),
						Child, MakeLabel("Street"),
						Child, street_string = BetterStringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							End,
						Child, MakeLabel("City/ZIP"),
						Child, city_string = BetterStringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							End,
						Child, MakeLabel("State/Country"),
						Child, country_string = BetterStringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							End,
						Child, MakeLabel("Phone numbers"),
						Child, phone1_string = BetterStringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							End,
						Child, HSpace(0),
						Child, phone2_string = BetterStringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							End,
						End,

					Child, VGroup,
						Child, ColGroup(2),
							Child, MakeLabel("Description"),
							Child, description_string = BetterStringObject,
								StringFrame,
								MUIA_CycleChain, 1,
								MUIA_String_AdvanceOnCR, TRUE,
								End,
							Child, MakeLabel("Date of birth"),
							Child, BetterStringObject,
								StringFrame,
								MUIA_CycleChain, 1,
								MUIA_String_AdvanceOnCR, TRUE,
								End,
							End,
						Child, HGroup,
							Child, VGroup,
								Child, MakeLabel("Notepad"),
								Child, VSpace(0),
								End,
							Child, TextEditorObject,
								InputListFrame,
								MUIA_CycleChain,1,
								End,
							End,
						End,
					End,
				End,

			Child, HorizLineObject,
			Child, HGroup,
				Child, ok_button = MakeButton("_Ok"),
				Child, cancel_button = MakeButton("_Cancel"),
				End,
			End,
		End;
	
	if (wnd)
	{
		struct Person_Data *data = (struct Person_Data*)malloc(sizeof(struct Person_Data));
		if (data)
		{
			data->wnd = wnd;
			data->rem_button = rem_button;
			data->alias_string = alias_string;
			data->email_list = email_list;
			data->email_string = email_string;
			data->realname_string = realname_string;
			data->description_string = description_string;
			data->homepage_string = homepage_string;
			data->street_string = street_string;
			data->city_string = city_string;
			data->country_string = country_string;
			data->phone1_string = phone1_string;
			data->phone2_string = phone2_string;
			data->person = entry;
			data->num = num;

			/* mark the window as opened */
			person_open[num] = 1;

			set(email_string, MUIA_String_AttachedList, email_list);
			set(add_button, MUIA_Weight,0);
			set(rem_button, MUIA_Weight,0);

			DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, person_window_close, data);
			DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, person_window_close, data);
			DoMethod(ok_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, person_window_ok, data);
			DoMethod(add_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, person_add_email, data);
			DoMethod(rem_button, MUIM_Notify, MUIA_Pressed, FALSE, email_list, 2, MUIM_NList_Remove, MUIV_NList_Remove_Active);
			DoMethod(email_list, MUIM_Notify, MUIA_NList_Active, MUIV_EveryTime, App, 4, MUIM_CallHook, &hook_standard, person_email_list_active, data);
			DoMethod(email_string, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, App, 4, MUIM_CallHook, &hook_standard, person_email_string_contents, data);
			DoMethod(App,OM_ADDMEMBER,wnd);

			/* A person must be changed */
			if (entry && entry->type == ADDRESSBOOK_ENTRY_PERSON)
			{
				int i;
				for (i=0;i<entry->u.person.num_emails;i++)
				{
					DoMethod(email_list, MUIM_NList_InsertSingle, entry->u.person.emails[i], MUIV_NList_Insert_Bottom);
				}
				set(realname_string, MUIA_String_Contents, entry->u.person.realname);
				set(description_string, MUIA_String_Contents, entry->u.person.description);
				set(alias_string, MUIA_String_Contents, entry->u.person.alias);
				set(phone1_string, MUIA_String_Contents, entry->u.person.phone1);
				set(phone2_string, MUIA_String_Contents, entry->u.person.phone2);
				set(street_string, MUIA_String_Contents, entry->u.person.street);
				set(city_string, MUIA_String_Contents, entry->u.person.city);
				set(country_string, MUIA_String_Contents, entry->u.person.country);
				set(homepage_string, MUIA_String_Contents, entry->u.person.homepage);
			}

			person_email_list_active(&data);
			set(wnd,MUIA_Window_ActiveObject,alias_string);
			set(wnd,MUIA_Window_Open,TRUE);

			return;
		}
		MUI_DisposeObject(wnd);
	}
}





/* the Group Window */
#define MAX_GROUP_OPEN 10
static int group_open[MAX_GROUP_OPEN];

struct Group_Data /* should be a customclass */
{
	Object *wnd;
	Object *alias_string;
	Object *description_string;

	struct addressbook_entry *group; /* NULL if new group */

	int num; /* the number of the window */
	/* more to add */
};

/******************************************************************
 This close and disposed the window (note: this must not be called
 within a normal callback hook, because the object is disposed in
 this function)!
*******************************************************************/
static void group_window_close(struct Group_Data **pdata)
{
	struct Group_Data *data = *pdata;
	set(data->wnd,MUIA_Window_Open,FALSE);
	DoMethod(App,OM_REMMEMBER,data->wnd);
	MUI_DisposeObject(data->wnd);
	if (data->num < MAX_GROUP_OPEN) group_open[data->num] = 0;
	free(data);
}

/******************************************************************
 This close and disposed the window (note: this must not be called
 within a normal callback hook, because the object is disposed in
 this function)!
 The group is added to the list (currently only at the end)
*******************************************************************/
static void group_window_ok(struct Group_Data **pdata)
{
	struct Group_Data *data = *pdata;
	struct addressbook_entry *new_entry;
	set(data->wnd,MUIA_Window_Open,FALSE);

	if ((new_entry = addressbook_new_group(NULL)))
	{
		struct MUI_NListtree_TreeNode *treenode = NULL;
		char *alias = (char*)xget(data->alias_string,MUIA_String_Contents);
		if (alias && *alias) addressbook_set_alias(new_entry, alias);
		addressbook_set_description(new_entry, (char *)xget(data->description_string,MUIA_String_Contents));

		if (data->group)
		{
			if ((treenode = FindListtreeUserData(address_tree, data->group)))
			{
				DoMethod(address_tree, MUIM_NListtree_Rename, treenode, new_entry, MUIV_NListtree_Rename_Flag_User);
			}
		}

		if (!treenode)
		{
			/* Now add it to the listview (in the active list) */
			DoMethod(address_tree, MUIM_NListtree_Insert, "" /*name*/, new_entry, /*udata */
						   MUIV_NListtree_Insert_ListNode_ActiveFallback,MUIV_NListtree_Insert_PrevNode_Tail,TNF_LIST|TNF_OPEN);
		}

		addressbook_free_entry(new_entry);
	}

	group_window_close(pdata);
}

/******************************************************************
 Opens a group window
*******************************************************************/
static void group_window_open(struct addressbook_entry *entry)
{
	Object *wnd;
	Object *alias_string, *ok_button, *cancel_button;
	Object *description_string;
	int num;

	for (num=0; num < MAX_GROUP_OPEN; num++)
		if (!group_open[num]) break;

	wnd = WindowObject,
		(num < MAX_GROUP_OPEN)?MUIA_Window_ID:TAG_IGNORE, MAKE_ID('G','R','P',num),
    MUIA_Window_Title, "SimpleMail - Edit Group",
        
		WindowContents, VGroup,
			Child, VGroup,
				Child, ColGroup(2),
					Child, MakeLabel("Alias"),
					Child, alias_string = BetterStringObject,
						StringFrame,
						MUIA_CycleChain, 1,
						End,

					Child, MakeLabel("Description"),
					Child, description_string = BetterStringObject,
						StringFrame,
						MUIA_CycleChain, 1,
						End,
					End,
				End,

			Child, HorizLineObject,
			Child, HGroup,
				Child, ok_button = MakeButton("Ok"),
				Child, cancel_button = MakeButton("Cancel"),
				End,
			End,
		End;
	
	if (wnd)
	{
		struct Group_Data *data = (struct Group_Data*)malloc(sizeof(struct Group_Data));
		if (data)
		{
			data->wnd = wnd;
			data->alias_string = alias_string;
			data->description_string = description_string;
			data->group = entry;
			data->num = num;

			/* mark the window as opened */
			group_open[num] = 1;

			DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, group_window_close, data);
			DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, group_window_close, data);
			DoMethod(ok_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, group_window_ok, data);
			DoMethod(App,OM_ADDMEMBER,wnd);

			/* A group must be changed */
			if (entry && entry->type == ADDRESSBOOK_ENTRY_GROUP)
			{
				set(alias_string, MUIA_String_Contents, entry->u.group.alias);
				set(description_string, MUIA_String_Contents, entry->u.group.description);
			}

			set(wnd,MUIA_Window_ActiveObject,alias_string);
			set(wnd,MUIA_Window_Open,TRUE);
			return;
		}
		MUI_DisposeObject(wnd);
	}
}


/* the address window functions */
STATIC ASM struct addressbook_entry *address_construct(register __a1 struct MUIP_NListtree_ConstructMessage *msg)
{
	struct addressbook_entry *entry = (struct addressbook_entry *)msg->UserData;
	return addressbook_duplicate_entry(entry);
}

STATIC ASM VOID address_destruct(register __a1 struct MUIP_NListtree_DestructMessage *msg)
{
	struct addressbook_entry *entry = (struct addressbook_entry *)msg->UserData;
	addressbook_free_entry(entry);
}

STATIC ASM VOID address_display(register __a1 struct MUIP_NListtree_DisplayMessage *msg)
{
	if (msg->TreeNode)
	{
		struct addressbook_entry *entry = (struct addressbook_entry *)msg->TreeNode->tn_User;

		switch (entry->type)
		{
			case	ADDRESSBOOK_ENTRY_GROUP:
						*msg->Array++ = entry->u.group.alias;
						*msg->Array++ = NULL;
						*msg->Array++ = entry->u.group.description;
						*msg->Array = NULL;
						break;

			case	ADDRESSBOOK_ENTRY_PERSON:
						*msg->Array++ = entry->u.person.alias;
						*msg->Array++ = entry->u.person.realname;
						*msg->Array++ = entry->u.person.description;
						if (entry->u.person.num_emails)
							*msg->Array = entry->u.person.emails[0];
						break;

			case	ADDRESSBOOK_ENTRY_LIST:
						*msg->Array++ = entry->u.list.alias;
						*msg->Array++ = entry->u.list.nameofml;
						*msg->Array++ = entry->u.list.description;
						*msg->Array = NULL;
						break;
		}
	} else
	{
		*msg->Array++ = "Alias";
		*msg->Array++ = "Name";
		*msg->Array++ = "Description";
		*msg->Array = "Address";
	}
}

/******************************************************************
 Updates the internal address book
*******************************************************************/
static void addressbook_update(struct MUI_NListtree_TreeNode *treenode, struct addressbook_entry *group)
{
	struct addressbook_entry *entry;
/*	cleanup_addressbook();*/
	if (!treenode) treenode = (struct MUI_NListtree_TreeNode *)DoMethod(address_tree,
				MUIM_NListtree_GetEntry, MUIV_NListtree_GetEntry_ListNode_Root, MUIV_NListtree_GetEntry_Position_Head, 0);

	if (!treenode) return;

	while (treenode)
	{
		if ((entry = (struct addressbook_entry *)treenode->tn_User))
		{
			if (treenode->tn_Flags & TNF_LIST)
			{
				struct MUI_NListtree_TreeNode *tn = (struct MUI_NListtree_TreeNode *)DoMethod(address_tree,
						MUIM_NListtree_GetEntry, treenode, MUIV_NListtree_GetEntry_Position_Head, 0);
				struct addressbook_entry *new_group = addressbook_duplicate_entry(entry);
				
				if (new_group)
				{
					addressbook_insert_tail(group,new_group);
					if (tn) addressbook_update(tn,new_group);
				}
			} else
			{
				if (entry->type == ADDRESSBOOK_ENTRY_PERSON)
				{
					struct addressbook_entry *new_person = addressbook_duplicate_entry(entry);

					if (new_person)
						addressbook_insert_tail(group,new_person);
				}
			}
		}
		treenode = (struct MUI_NListtree_TreeNode*)DoMethod(address_tree, MUIM_NListtree_GetEntry, treenode, MUIV_NListtree_GetEntry_Position_Next,0);
	}
}

/******************************************************************
 Save the addressbook to disk
*******************************************************************/
static void addressbook_save_pressed(void)
{
	cleanup_addressbook();
	addressbook_update(NULL,NULL);
	addressbook_save();
}

/******************************************************************
 Adds a new person to the window
*******************************************************************/
static void addressbook_add_person(void)
{
	person_window_open(NULL);
}

/******************************************************************
 Adds a new group to the window
*******************************************************************/
static void addressbook_add_group(void)
{
	group_window_open(NULL);
}

/******************************************************************
 Change the current selected entry
*******************************************************************/
static void addressbook_change(void)
{
	struct MUI_NListtree_TreeNode *treenode = (struct MUI_NListtree_TreeNode *)
				DoMethod(address_tree, MUIM_NListtree_GetEntry, MUIV_NListtree_GetEntry_ListNode_Active, MUIV_NListtree_GetEntry_Position_Active, 0);

	if (treenode)
	{
		struct addressbook_entry *entry = (struct addressbook_entry *)treenode->tn_User;
		if (entry->type == ADDRESSBOOK_ENTRY_PERSON)
		{
			person_window_open(entry);
		} else if (entry->type == ADDRESSBOOK_ENTRY_GROUP)
		{
			group_window_open(entry);
		}
	}
}

/******************************************************************
 The to button has been clicked
*******************************************************************/
static void addressbook_to(void)
{
	struct MUI_NListtree_TreeNode *treenode = (struct MUI_NListtree_TreeNode *)
				DoMethod(address_tree, MUIM_NListtree_GetEntry, MUIV_NListtree_GetEntry_ListNode_Active, MUIV_NListtree_GetEntry_Position_Active, 0);

	if (treenode)
	{
		struct addressbook_entry *entry = (struct addressbook_entry *)treenode->tn_User;
		callback_write_mail_to(entry);
	}
}

/******************************************************************
 Removes the currently selected entry
*******************************************************************/
static void addressbook_delete(void)
{
	DoMethod(address_tree, MUIM_NListtree_Remove, MUIV_NListtree_Remove_ListNode_Active, MUIV_NListtree_Remove_TreeNode_Active, 0);
}

/******************************************************************
 Adds an entry to the addressbook (recursivly)
*******************************************************************/
static void addressbook_add(struct addressbook_entry *entry, APTR listnode)
{
	entry = addressbook_first(entry);

	while (entry)
	{
		struct MUI_NListtree_TreeNode *treenode;
		LONG flags = 0;

		if (entry->type == ADDRESSBOOK_ENTRY_GROUP) flags = TNF_LIST|TNF_OPEN;

		treenode = (struct MUI_NListtree_TreeNode*)DoMethod(address_tree, MUIM_NListtree_Insert, "" /*name*/, entry, /*udata */
						   listnode,MUIV_NListtree_Insert_PrevNode_Tail,flags);

		if (entry && (flags & TNF_LIST))
		{
			addressbook_add(entry,treenode);
		}
		entry = addressbook_next(entry);
	}
}

/******************************************************************
 Refreshes the addressbook to match the internal addressbook list
*******************************************************************/
static void addressbook_refresh(void)
{
	if (!address_wnd) return;

	set(address_tree, MUIA_NListtree_Quiet, TRUE);
	DoMethod(address_tree, MUIM_NListtree_Remove, MUIV_NListtree_Remove_ListNode_Root, MUIV_NListtree_Remove_TreeNode_All, 0);

	addressbook_add(NULL, MUIV_NListtree_Insert_ListNode_Root);

	set(address_tree, MUIA_NListtree_Quiet, FALSE);
}

/******************************************************************
 Initialzes the addressbook window
*******************************************************************/
static void addressbook_init(void)
{
	Object *new_person_button, *change_button, *new_group_button, *save_button, *delete_button;
	Object *to_button;

	init_hook(&address_construct_hook, (HOOKFUNC)address_construct);
	init_hook(&address_destruct_hook, (HOOKFUNC)address_destruct);
	init_hook(&address_display_hook, (HOOKFUNC)address_display);

	address_wnd = WindowObject,
		MUIA_Window_ID, MAKE_ID('A','D','B','K'),
    MUIA_Window_Title, "SimpleMail - Addressbook",
        
		WindowContents, VGroup,
			Child, HGroup,
				Child, VGroup,
					MUIA_Weight,0,
					Child, to_button = MakeButton("_To:"),
					Child, MakeButton("_CC:"),
					Child, MakeButton("_BCC:"),
					End,
				Child, HGroup,
					Child, save_button = MakePictureButton("_Save","PROGDIR:Images/Save"),
/*					Child, MakeButton("Search"),*/
					Child, new_person_button = MakePictureButton("_Person","PROGDIR:Images/User"),
/*					Child, MakeButton("New List"),*/
					Child, new_group_button = MakePictureButton("_Group","PROGDIR:Images/Group"),
					Child, change_button = MakePictureButton("_Edit","PROGDIR:Images/Edit"),
					Child, delete_button = MakePictureButton("_Delete","PROGDIR:Images/Delete"),
					End,
				End,
			Child, HGroup,
				Child, NListviewObject,
					MUIA_CycleChain, 1,
					MUIA_NListview_NList, address_tree = NListtreeObject,
						MUIA_NListtree_ConstructHook, &address_construct_hook,
						MUIA_NListtree_DestructHook, &address_destruct_hook,
						MUIA_NListtree_DisplayHook, &address_display_hook,
						MUIA_NListtree_Title, TRUE,
						MUIA_NListtree_Format,",,,",
						MUIA_NListtree_DoubleClick, MUIV_NListtree_DoubleClick_Tree,
						End,
					End,
				End,
			End,
		End;

	if (!address_wnd) return;
	DoMethod(App,OM_ADDMEMBER,address_wnd);
	DoMethod(address_wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, address_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
	DoMethod(save_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 3, MUIM_CallHook, &hook_standard, addressbook_save_pressed);
	DoMethod(new_person_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 3, MUIM_CallHook, &hook_standard, addressbook_add_person);
	DoMethod(new_group_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 3, MUIM_CallHook, &hook_standard, addressbook_add_group);
	DoMethod(change_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 3, MUIM_CallHook, &hook_standard, addressbook_change);
	DoMethod(delete_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 3, MUIM_CallHook, &hook_standard, addressbook_delete);
	DoMethod(to_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 3, MUIM_CallHook, &hook_standard, addressbook_to);
	DoMethod(address_tree, MUIM_Notify, MUIA_NListtree_DoubleClick, MUIV_EveryTime, App, 3, MUIM_CallHook, &hook_standard, addressbook_change);

	set(address_tree, MUIA_UserData, 1); /* used in addresstringclass.c */

}

/******************************************************************
 Opens a addressbook
*******************************************************************/
void addressbook_open(void)
{
	if (!address_wnd) addressbook_init();
	if (address_wnd)
	{
		if (!xget(address_wnd, MUIA_Window_Open)) addressbook_refresh();
		set(address_wnd, MUIA_Window_Open, TRUE);
	}
}

/******************************************************************
 Opens the addressbook with a new person taken from the mail
*******************************************************************/
void addressbook_open_with_new_address(struct mail *m)
{
	struct addressbook_entry *entry = addressbook_get_entry_from_mail(m);
	if (entry)
	{
		addressbook_open();
		person_window_open(entry);
		addressbook_free_entry(entry);
	}
}
