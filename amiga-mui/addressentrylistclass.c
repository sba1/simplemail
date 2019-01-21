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
 * @file addressentrylistclass.c
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <libraries/iffparse.h>
#include <libraries/mui.h>
#include <mui/NListview_mcc.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>

#include "addressbook.h"
#include "codesets.h"
#include "configuration.h"
#include "debug.h"
#include "lists.h"
#include "simplemail.h"
#include "smintl.h"
#include "support.h"
#include "support_indep.h"

#include "addressentrylistclass.h"
#include "compiler.h"
#include "mailtreelistclass.h"
#include "muistuff.h"

struct entry_node
{
	struct node node;
	struct addressbook_entry_new *entry;
};

struct AddressEntryList_Data
{
	struct Hook construct_hook;
	struct Hook compare_hook;
	struct Hook destruct_hook;
	struct Hook display_hook;

	ULONG type;

	char *visible_group; /* the group which is visible or NULL */
	struct list unvisible_list; /* list containing elements which are not visible */

	char alias_buf[64];
	char realname_buf[64];
	char description_buf[128];
	char email_buf[128];
	char group_buf[128];

	/* Menu */
	Object *title_menu;
	Object *show_realname_item;
	Object *show_nickname_item;
	Object *show_description_item;
	Object *show_email_item;
	Object *show_groups_item;
};

/**
 * Constructor for address entries.
 */
STATIC ASM SAVEDS struct addressbook_entry_new *addressentry_construct(REG(a0, struct Hook *h), REG(a2, Object *obj),REG(a1,struct NList_ConstructMessage *msg))
{
	struct addressbook_entry_new *entry = (struct addressbook_entry_new *)msg->entry;
	return addressbook_duplicate_entry_new(entry);
}

/**
 * Destructor for address entries.
 */
STATIC ASM SAVEDS VOID addressentry_destruct(REG(a0, struct Hook *h), REG(a2, Object *obj),REG(a1,struct NList_DestructMessage *msg))
{
	struct addressbook_entry_new *entry = (struct addressbook_entry_new *)msg->entry;
	addressbook_free_entry_new(entry);
}

/**
 * Display function function for address entries
 */
STATIC ASM SAVEDS VOID addressentry_display(REG(a0,struct Hook *h),REG(a2,Object *obj), REG(a1,struct NList_DisplayMessage *msg))
{
	char **array = msg->strings;
	struct addressbook_entry_new *entry = (struct addressbook_entry_new*)msg->entry;
	struct AddressEntryList_Data *data = (struct AddressEntryList_Data*)h->h_Data;

	unsigned int i, count;

	if (entry)
	{
		utf8tostr(entry->alias, data->alias_buf, sizeof(data->alias_buf), user.config.default_codeset);
		utf8tostr(entry->realname, data->realname_buf, sizeof(data->realname_buf), user.config.default_codeset);
		utf8tostr(entry->description, data->description_buf, sizeof(data->description_buf), user.config.default_codeset);

		*array++ = data->realname_buf;
		*array++ = data->alias_buf;
		*array++ = data->description_buf;

		if (entry->email_array) *array = entry->email_array[0];
		else *array = NULL;

		array++;

		/* group */
		count = 0;
		data->group_buf[0] = 0;
		for (i=0;i<array_length(entry->group_array);i++)
		{
			if (i)
			{
				if (count + 2 >= sizeof(data->group_buf)) break;
				data->group_buf[count] = ',',
				data->group_buf[++count] = ' ';
				data->group_buf[++count] = 0;
			}
			count += utf8tostr(entry->group_array[i], &data->group_buf[count], sizeof(data->group_buf) - count, user.config.default_codeset);
		}
		*array++ = data->group_buf;


	} else
	{
		*array++ = (char*)Q_("?people:Name");
		*array++ = (char*)_("Alias");
		*array++ = (char*)_("Description");
		*array++ = (char*)_("Address");
		*array = (char*)_("Groups");
	}
}

/**
 * Compare function for address entries.
 */
STATIC ASM SAVEDS LONG addressentry_compare(REG(a0, struct Hook *h), REG(a2, Object *obj), REG(a1,struct NList_CompareMessage *msg))
{
	struct addressbook_entry_new *entry1 = (struct addressbook_entry_new *)msg->entry1;
	struct addressbook_entry_new *entry2 = (struct addressbook_entry_new *)msg->entry2;

	return utf8stricmp(entry1->realname,entry2->realname);
}

/**
 * Update the lists format.
 *
 * @param cl the class
 * @param obj the object
 */
STATIC VOID AddressEntryList_UpdateFormat(struct IClass *cl,Object *obj)
{
	struct AddressEntryList_Data *data = (struct AddressEntryList_Data*)INST_DATA(cl,obj);
	char buf[256];

	buf[0] = 0;

	if (xget(data->show_realname_item,MUIA_Menuitem_Checked)) strcat(buf,"COL=0,");
	if (xget(data->show_nickname_item,MUIA_Menuitem_Checked)) strcat(buf,"COL=1,");
	if (xget(data->show_description_item,MUIA_Menuitem_Checked)) strcat(buf,"COL=2,");
	if (xget(data->show_email_item,MUIA_Menuitem_Checked)) strcat(buf,"COL=3,");
	if (xget(data->show_groups_item,MUIA_Menuitem_Checked)) strcat(buf,"COL=4,");

	if (strlen(buf)) buf[strlen(buf)-1] = 0;

	set(obj, MUIA_NList_Format, buf);
}


/********************** BOOPSI Methods **********************/

/**
 * Implementation of OM_NEW.
 *
 * @param cl the class
 * @param obj the object
 * @param msg
 * @return
 */
STATIC ULONG AddressEntryList_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct AddressEntryList_Data *data;

	if (!(obj=(Object *)DoSuperNew(cl,obj,
					MUIA_Draggable, TRUE,
					TAG_MORE,msg->ops_AttrList)))
		return 0;

	data = (struct AddressEntryList_Data*)INST_DATA(cl,obj);

	list_init(&data->unvisible_list);

  data->type = GetTagData(MUIA_AddressEntryList_Type,MUIV_AddressEntryList_Type_Addressbook,msg->ops_AttrList);

	init_hook(&data->construct_hook,(HOOKFUNC)addressentry_construct);
	init_hook(&data->compare_hook,(HOOKFUNC)addressentry_compare);
	init_hook(&data->destruct_hook,(HOOKFUNC)addressentry_destruct);
	init_hook_with_data(&data->display_hook,(HOOKFUNC)addressentry_display,data);

	SetAttrs(obj,
						MUIA_NList_ConstructHook2, &data->construct_hook,
						MUIA_NList_CompareHook2, &data->compare_hook,
						MUIA_NList_DestructHook2, &data->destruct_hook,
						MUIA_NList_DisplayHook2, &data->display_hook,
						MUIA_NList_Title, TRUE,
						MUIA_NList_Format, ",,,,",
						MUIA_NList_MultiSelect, TRUE,
						TAG_DONE);

	if (data->type == MUIV_AddressEntryList_Type_Main)
	{
		data->title_menu = MenustripObject,
			Child, MenuObjectT(_("Addresslist Settings")),
				Child, data->show_realname_item = MenuitemObject, MUIA_ObjectID, MAKE_ID('A','L','S','R'),MUIA_Menuitem_Title, _("Show Realname?"), MUIA_UserData, 1, MUIA_Menuitem_Checked, TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
				Child, data->show_nickname_item = MenuitemObject, MUIA_ObjectID, MAKE_ID('A','L','S','N'),MUIA_Menuitem_Title, _("Show Nickname?"), MUIA_UserData, 2, MUIA_Menuitem_Checked, TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
				Child, data->show_description_item = MenuitemObject, MUIA_ObjectID, MAKE_ID('A','L','S','D'),MUIA_Menuitem_Title, _("Show Description?"), MUIA_UserData, 3, MUIA_Menuitem_Checked, TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
				Child, data->show_email_item = MenuitemObject, MUIA_ObjectID, MAKE_ID('A','L','S','A'),MUIA_Menuitem_Title, _("Show E-Mail Address?"), MUIA_UserData, 4, MUIA_Menuitem_Checked, TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
				Child, data->show_groups_item = MenuitemObject, MUIA_ObjectID, MAKE_ID('A','L','S','G'),MUIA_Menuitem_Title, _("Show Groups?"), MUIA_UserData, 4, MUIA_Menuitem_Checked, TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
				Child, MenuitemObject, MUIA_Menuitem_Title, -1, End,
				Child, MenuitemObject, MUIA_Menuitem_Title, _("Default Width: this"), MUIA_UserData, MUIV_NList_Menu_DefWidth_This, End,
				Child, MenuitemObject, MUIA_Menuitem_Title, _("Default Width: all"), MUIA_UserData, MUIV_NList_Menu_DefWidth_All, End,
				Child, MenuitemObject, MUIA_Menuitem_Title, _("Default Order: this"), MUIA_UserData, MUIV_NList_Menu_DefOrder_This, End,
				Child, MenuitemObject, MUIA_Menuitem_Title, _("Default Order: all"), MUIA_UserData, MUIV_NList_Menu_DefOrder_All, End,
				End,
			End;

			AddressEntryList_UpdateFormat(cl,obj);
	}

	return (ULONG)obj;
}

/**
 * Implementation of OM_DISPOSE.
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG AddressEntryList_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
	struct AddressEntryList_Data *data = (struct AddressEntryList_Data*)INST_DATA(cl,obj);
	if (data->title_menu) MUI_DisposeObject(data->title_menu);
	return DoSuperMethodA(cl,obj,msg);
}

/**
 * Implementation of OM_SET.
 *
 * @param cl the class
 * @param obj the object
 * @param msg
 * @param msg the parameter of the method
 */
STATIC ULONG AddressEntryList_Set(struct IClass *cl, Object *obj, struct opSet *msg)
{
	struct AddressEntryList_Data *data = (struct AddressEntryList_Data*)INST_DATA(cl,obj);
	struct TagItem *tstate, *tag;

	tstate = (struct TagItem *)msg->ops_AttrList;

	while ((tag = NextTagItem (&tstate)))
	{
		switch (tag->ti_Tag)
		{
			case	MUIA_AddressEntryList_GroupName:
						if (mystrcmp(data->visible_group,(char*)tag->ti_Data))
						{
							free(data->visible_group);
							data->visible_group = mystrdup((char*)tag->ti_Data);
							DoMethod(obj, MUIM_AddressEntryList_Refresh);
						}
						break;
		}
	}

	return DoSuperMethodA(cl,obj,(Msg)msg);
}

/**
 * Implementation of MUIM_AddressEntryList_Refresh.
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG AddressEntryList_Refresh(struct IClass *cl, Object *obj, struct MUIP_AddressEntryList_Refresh *msg)
{
	struct AddressEntryList_Data *data = (struct AddressEntryList_Data*)INST_DATA(cl,obj);
	struct addressbook_entry_new *entry;
	struct entry_node *node;

	/* Cleanup unvisible entries */
	while ((node = (struct entry_node*)list_remove_tail(&data->unvisible_list)))
	{
		addressbook_free_entry_new(node->entry);
		free(node);
	}

	set(obj, MUIA_NList_Quiet, TRUE);
	DoMethod(obj, MUIM_NList_Clear);

	/* Add new elements. Elements which are not displayed are added to the
	 * unvisible list */
	entry = addressbook_first_entry();
	while (entry)
	{
		if (data->visible_group && !array_contains_utf8(entry->group_array,data->visible_group))
		{
			struct entry_node *node = (struct entry_node *)malloc(sizeof(*node));
			if (node)
			{
				if ((node->entry = addressbook_duplicate_entry_new(entry)))
					list_insert_tail(&data->unvisible_list,&node->node);
			}
		} else DoMethod(obj, MUIM_NList_InsertSingle, (ULONG)entry, MUIV_NList_Insert_Sorted);
		entry = addressbook_next_entry(entry);
	}
	set(obj, MUIA_NList_Quiet, FALSE);

	return 0;
}

/**
 * Implementation of MUIM_AddressEntryList_Store.
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG AddressEntryList_Store(struct IClass *cl, Object *obj, Msg msg)
{
	unsigned int i;
	struct entry_node *node;
	struct AddressEntryList_Data *data = (struct AddressEntryList_Data*)INST_DATA(cl,obj);

	node = (struct entry_node*)list_first(&data->unvisible_list);
	while (node)
	{
		addressbook_add_entry_duplicate(node->entry);
		node = (struct entry_node*)node_next(&node->node);
	}

	for (i=0;i<xget(obj,MUIA_NList_Entries);i++)
	{
		struct addressbook_entry_new *entry;

		DoMethod(obj, MUIM_NList_GetEntry, i, (ULONG)&entry);
		addressbook_add_entry_duplicate(entry);
	}
	return 0;
}

/**
 * Implementation of MUIM_ContextMenuChoice.
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG AddressEntryList_ContextMenuChoice(struct IClass *cl, Object *obj, struct MUIP_ContextMenuChoice *msg)
{
	switch (xget(msg->item,MUIA_UserData))
	{
		case	1:
		case	2:
		case	3:
		case	4:
					AddressEntryList_UpdateFormat(cl,obj);
					break;

		default:
					return DoSuperMethodA(cl,obj,(Msg)msg);
	}
  return 0;
}

/**
 * Implementation of MUIM_NList_ContextMenuBuild
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG AddressEntryList_ContextMenuBuild(struct IClass *cl, Object * obj, struct MUIP_NList_ContextMenuBuild *msg)
{
	struct AddressEntryList_Data *data = (struct AddressEntryList_Data*)INST_DATA(cl,obj);

	if (data->type != MUIV_AddressEntryList_Type_Main) return DoSuperMethodA(cl,obj,(Msg)msg);

	if (msg->ontop) return (ULONG)data->title_menu;
	return 0;
}

/**
 * Implementation of MUIM_DragQuery
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG AddressEntryList_DragQuery(struct IClass *cl, Object *obj, struct MUIP_DragQuery *msg)
{
/*	struct AddressTreelist_Data *data = (struct AddressTreelist_Data*)INST_DATA(cl,obj);*/

	if (OCLASS(msg->obj) == CL_MailTreelist->mcc_Class) return MUIV_DragQuery_Accept;
	return MUIV_DragQuery_Refuse;
}

/**
 * Implementation of MUIM_DragDrop.
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG AddressEntryList_DragDrop(struct IClass *cl, Object *obj, struct MUIP_DragDrop *msg)
{
	if (OCLASS(msg->obj) != CL_MailTreelist->mcc_Class) return DoSuperMethodA(cl,obj,(Msg)msg);
	callback_get_address();
	return 0;
}

/**
 * The Boopsi Dispatcher for the address entry list class.
 */
STATIC MY_BOOPSI_DISPATCHER(ULONG,AddressEntryList_Dispatcher,cl,obj,msg)
{
	switch(msg->MethodID)
	{
		case	OM_NEW: return AddressEntryList_New(cl,obj,(struct opSet*)msg);
		case	OM_DISPOSE: return AddressEntryList_Dispose(cl,obj,msg);
		case	OM_SET: return AddressEntryList_Set(cl,obj,(struct opSet*)msg);
		case	MUIM_ContextMenuChoice: return AddressEntryList_ContextMenuChoice(cl, obj, (struct MUIP_ContextMenuChoice *)msg);
		case	MUIM_NList_ContextMenuBuild: return AddressEntryList_ContextMenuBuild(cl,obj,(struct MUIP_NList_ContextMenuBuild *)msg);
		case	MUIM_DragQuery: return AddressEntryList_DragQuery(cl,obj,(struct MUIP_DragQuery *)msg);
		case	MUIM_DragDrop:  return AddressEntryList_DragDrop(cl,obj,(struct MUIP_DragDrop *)msg);
		case	MUIM_AddressEntryList_Refresh: return AddressEntryList_Refresh(cl,obj,(struct MUIP_AddressEntryList_Refresh *)msg);
		case	MUIM_AddressEntryList_Store: return AddressEntryList_Store(cl,obj,msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}
/*****************************************************************************/

struct MUI_CustomClass *CL_AddressEntryList;

/*****************************************************************************/

int create_addressentrylist_class(void)
{
	SM_ENTER;
	if ((CL_AddressEntryList = CreateMCC(MUIC_NList,NULL,sizeof(struct AddressEntryList_Data),AddressEntryList_Dispatcher)))
	{
		SM_DEBUGF(15,("Create CL_AddressEntryList: 0x%lx\n",CL_AddressEntryList));
		SM_RETURN(1,"%ld");
	}
	SM_DEBUGF(5,("FAILED! Create CL_AddressEntryList\n"));
	SM_RETURN(0,"%ld");
}

/*****************************************************************************/

void delete_addressentrylist_class(void)
{
	SM_ENTER;
	if (CL_AddressEntryList)
	{
		if (MUI_DeleteCustomClass(CL_AddressEntryList))
		{
			SM_DEBUGF(15,("Deleted CL_AddressEntryList: 0x%lx\n",CL_AddressEntryList));
			CL_AddressEntryList = NULL;
		} else
		{
			SM_DEBUGF(5,("FAILED! Delete CL_AddressEntryList: 0x%lx\n",CL_AddressEntryList));
		}
	}
	SM_LEAVE;
}
