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
** configtreelistclass.c
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <libraries/iffparse.h>
#include <libraries/mui.h>
#include <mui/NListview_MCC.h>
#include <mui/NListtree_Mcc.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>

#include "addressbook.h"
#include "codesets.h"
#include "configuration.h"
#include "folder.h"
#include "mail.h"
#include "parse.h"
#include "simplemail.h"
#include "smintl.h"
#include "support_indep.h"
#include "debug.h"

#include "addressbookwnd.h"
#include "addresstreelistclass.h"
#include "compiler.h"
#include "mailtreelistclass.h"
#include "mainwnd.h"
#include "muistuff.h"

#define TYPE_MAIN 0
#define TYPE_ADDRESSBOOK 1
#define TYPE_MATCHLIST 2

struct AddressTreelist_Data
{
	struct Hook compare_hook;
	struct Hook construct_hook;
	struct Hook destruct_hook;
	struct Hook display_hook;

	int type;
	char *filter; /* the filter string */

	Object *title_menu;
	Object *show_realname_item;
	Object *show_nickname_item;
	Object *show_description_item;
	Object *show_email_item;

	char alias_buf[64];
	char realname_buf[128];
	char description_buf[256];
};

STATIC ASM SAVEDS LONG address_compare(REG(a0, struct Hook *h), REG(a2, Object *obj), REG(a1,struct MUIP_NListtree_CompareMessage *msg))
{
	struct addressbook_entry *entry1 = (struct addressbook_entry *)msg->TreeNode1->tn_User;
	struct addressbook_entry *entry2 = (struct addressbook_entry *)msg->TreeNode2->tn_User;

	return mystricmp(entry1->u.person.realname,entry2->u.person.realname);
}

STATIC ASM SAVEDS struct addressbook_entry *address_construct(REG(a0, struct Hook *h), REG(a2, Object *obj),REG(a1,struct MUIP_NListtree_ConstructMessage *msg))
{
	struct addressbook_entry *entry = (struct addressbook_entry *)msg->UserData;
	return addressbook_duplicate_entry(entry);
}

STATIC ASM SAVEDS VOID address_destruct(REG(a0, struct Hook *h), REG(a2, Object *obj),REG(a1,struct MUIP_NListtree_DestructMessage *msg))
{
	struct addressbook_entry *entry = (struct addressbook_entry *)msg->UserData;
	addressbook_free_entry(entry);
}

STATIC ASM SAVEDS VOID address_display(REG(a0, struct Hook *h),REG(a2,Object *obj), REG(a1,struct MUIP_NListtree_DisplayMessage *msg))
{
	struct AddressTreelist_Data *data = (struct AddressTreelist_Data*)INST_DATA(CL_AddressTreelist->mcc_Class,obj);

	if (msg->TreeNode)
	{
		struct addressbook_entry *entry = (struct addressbook_entry *)msg->TreeNode->tn_User;

		utf8tostr(entry->alias, data->alias_buf, sizeof(data->alias_buf), user.config.default_codeset);
		utf8tostr(entry->description, data->description_buf, sizeof(data->description_buf), user.config.default_codeset);

		switch (entry->type)
		{
			case	ADDRESSBOOK_ENTRY_GROUP:
						*msg->Array++ = data->alias_buf;
						*msg->Array++ = NULL;
						*msg->Array++ = data->description_buf;
						*msg->Array = NULL;
						break;

			case	ADDRESSBOOK_ENTRY_PERSON:
						utf8tostr(entry->u.person.realname, data->realname_buf, sizeof(data->realname_buf), user.config.default_codeset);

						*msg->Array++ = data->realname_buf;
						*msg->Array++ = data->alias_buf;
						*msg->Array++ = data->description_buf;
						if (entry->u.person.num_emails)
							*msg->Array = entry->u.person.emails[0];
						if (data->type == TYPE_MATCHLIST && data->filter)
						{
							int match_type;
							if (addressbook_completed_by_entry(data->filter, entry, &match_type))
							{
								if (match_type == 1) msg->Preparse[0] = "\033b";
								else if (match_type == 0) msg->Preparse[1] = "\033b";
								else if (match_type > 1) msg->Preparse[3] = "\033b";
							}
						}
						break;

			case	ADDRESSBOOK_ENTRY_LIST:
						*msg->Array++ = entry->u.list.nameofml;
						*msg->Array++ = data->alias_buf;
						*msg->Array++ = data->description_buf;
						*msg->Array = NULL;
						break;
		}
	} else
	{
		*msg->Array++ = Q_("?people:Name");
		*msg->Array++ = _("Alias");
		*msg->Array++ = _("Description");
		*msg->Array = _("Address");
	}
}

STATIC VOID AddressTreelist_UpdateFormat(struct IClass *cl,Object *obj)
{
	struct AddressTreelist_Data *data = (struct AddressTreelist_Data*)INST_DATA(cl,obj);
	char buf[256];

	buf[0] = 0;

	if (xget(data->show_realname_item,MUIA_Menuitem_Checked)) strcat(buf,"COL=0,");
	if (xget(data->show_nickname_item,MUIA_Menuitem_Checked)) strcat(buf,"COL=1,");
	if (xget(data->show_description_item,MUIA_Menuitem_Checked)) strcat(buf,"COL=2,");
	if (xget(data->show_email_item,MUIA_Menuitem_Checked)) strcat(buf,"COL=3,");

	if (strlen(buf)) buf[strlen(buf)-1] = 0;

	set(obj, MUIA_NListtree_Format, buf);
}

STATIC ULONG AddressTreelist_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct AddressTreelist_Data *data;
	struct TagItem *ti = FindTagItem(MUIA_AddressTreelist_InAddressbook,msg->ops_AttrList);
	int type;

	if (ti && ti->ti_Data) type = TYPE_ADDRESSBOOK;
	else
	{
		ti = FindTagItem(MUIA_AddressTreelist_AsMatchList,msg->ops_AttrList);
		if (ti && ti->ti_Data) type = TYPE_MATCHLIST;
	}

	if (!(obj=(Object *)DoSuperNew(cl,obj,
/*					in_addressbook?TAG_IGNORE:MUIA_NListtree_MultiSelect,MUIV_NListtree_MultiSelect_Default, */
					MUIA_NListtree_Title, type != TYPE_MATCHLIST,
					MUIA_NListtree_Format, ",,,",
					MUIA_NListtree_DoubleClick, MUIV_NListtree_DoubleClick_Off,
					MUIA_NListtree_ShowTree, type == TYPE_ADDRESSBOOK,
					MUIA_NList_ShowDropMarks, type == TYPE_ADDRESSBOOK,
					TAG_MORE,msg->ops_AttrList)))
		return 0;

	data = (struct AddressTreelist_Data*)INST_DATA(cl,obj);
	data->type = type;

	init_hook(&data->compare_hook,(HOOKFUNC)address_compare);
	init_hook(&data->construct_hook,(HOOKFUNC)address_construct);
	init_hook(&data->destruct_hook,(HOOKFUNC)address_destruct);
	init_hook(&data->display_hook,(HOOKFUNC)address_display);

	SetAttrs(obj,
						type == TYPE_ADDRESSBOOK?TAG_IGNORE:MUIA_NListtree_CompareHook, &data->compare_hook,
						MUIA_NListtree_ConstructHook, &data->construct_hook,
						MUIA_NListtree_DestructHook, &data->destruct_hook,
						MUIA_NListtree_DisplayHook, &data->display_hook,
						MUIA_UserData, 1, /* used in addresstringclass.c */
						TAG_DONE);

	if (data->type == TYPE_MAIN)
	{
		data->title_menu = MenustripObject,
			Child, MenuObjectT(_("Addresslist Settings")),
				Child, data->show_realname_item = MenuitemObject, MUIA_ObjectID, MAKE_ID('A','L','S','R'),MUIA_Menuitem_Title, _("Show Realname?"), MUIA_UserData, 1, MUIA_Menuitem_Checked, TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
				Child, data->show_nickname_item = MenuitemObject, MUIA_ObjectID, MAKE_ID('A','L','S','N'),MUIA_Menuitem_Title, _("Show Nickname?"), MUIA_UserData, 2, MUIA_Menuitem_Checked, TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
				Child, data->show_description_item = MenuitemObject, MUIA_ObjectID, MAKE_ID('A','L','S','D'),MUIA_Menuitem_Title, _("Show Description?"), MUIA_UserData, 3, MUIA_Menuitem_Checked, TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
				Child, data->show_email_item = MenuitemObject, MUIA_ObjectID, MAKE_ID('A','L','S','A'),MUIA_Menuitem_Title, _("Show E-Mail Address?"), MUIA_UserData, 4, MUIA_Menuitem_Checked, TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
				Child, MenuitemObject, MUIA_Menuitem_Title, -1, End,
				Child, MenuitemObject, MUIA_Menuitem_Title, _("Default Width: this"), MUIA_UserData, MUIV_NList_Menu_DefWidth_This, End,
				Child, MenuitemObject, MUIA_Menuitem_Title, _("Default Width: all"), MUIA_UserData, MUIV_NList_Menu_DefWidth_All, End,
				Child, MenuitemObject, MUIA_Menuitem_Title, _("Default Order: this"), MUIA_UserData, MUIV_NList_Menu_DefOrder_This, End,
				Child, MenuitemObject, MUIA_Menuitem_Title, _("Default Order: all"), MUIA_UserData, MUIV_NList_Menu_DefOrder_All, End,
				End,
			End;
	}

	return (ULONG)obj;
}

STATIC ULONG AddressTreelist_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
	struct AddressTreelist_Data *data = (struct AddressTreelist_Data*)INST_DATA(cl,obj);
	if (data->title_menu) MUI_DisposeObject(data->title_menu);
	if (data->filter) free(data->filter);
	return DoSuperMethodA(cl,obj,msg);
}

STATIC ULONG AddressTreelist_AskMinMax(struct IClass *cl,Object *obj, struct MUIP_AskMinMax *msg)
{
  ULONG rc = DoSuperMethodA(cl, obj, (Msg) msg);
  if (_screen(obj)->Height <= 300)
	  msg->MinMaxInfo->DefHeight = msg->MinMaxInfo->MinHeight + 3*_font(obj)->tf_YSize;
  return rc;
}

STATIC ULONG AddressTreelist_Export(struct IClass *cl, Object *obj, struct MUIP_Export *msg)
{
	struct AddressTreelist_Data *data = (struct AddressTreelist_Data*)INST_DATA(cl,obj);

	if (data->type == TYPE_MAIN)
	{
		DoMethodA(data->show_realname_item, (Msg)msg);
		DoMethodA(data->show_nickname_item, (Msg)msg);
		DoMethodA(data->show_description_item, (Msg)msg);
		DoMethodA(data->show_email_item, (Msg)msg);
	}
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG AddressTreelist_Import(struct IClass *cl, Object *obj, struct MUIP_Import *msg)
{
	struct AddressTreelist_Data *data = (struct AddressTreelist_Data*)INST_DATA(cl,obj);

	if (data->type == TYPE_MAIN)
	{
		DoMethodA(data->show_realname_item, (Msg)msg);
		DoMethodA(data->show_nickname_item, (Msg)msg);
		DoMethodA(data->show_description_item, (Msg)msg);
		DoMethodA(data->show_email_item, (Msg)msg);
		AddressTreelist_UpdateFormat(cl,obj);
	}

	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG AddressTreelist_DragQuery(struct IClass *cl, Object *obj, struct MUIP_DragQuery *msg)
{
	struct AddressTreelist_Data *data = (struct AddressTreelist_Data*)INST_DATA(cl,obj);
	if (OCLASS(msg->obj) == CL_MailTreelist->mcc_Class) return MUIV_DragQuery_Accept;
	if (data->type == TYPE_ADDRESSBOOK) return DoSuperMethodA(cl,obj,(Msg)msg);
	return MUIV_DragQuery_Refuse;
}

STATIC ULONG AddressTreelist_DragDrop(struct IClass *cl, Object *obj, struct MUIP_DragDrop *msg)
{
	if (OCLASS(msg->obj) != CL_MailTreelist->mcc_Class) return DoSuperMethodA(cl,obj,(Msg)msg);

	callback_get_address();
	return 0;
}

STATIC ULONG AddressTreelist_DropType(struct IClass *cl, Object *obj,struct MUIP_NListtree_DropType *msg)
{
	struct AddressTreelist_Data *data = (struct AddressTreelist_Data*)INST_DATA(cl,obj);
	ULONG rv;

	if (data->type == TYPE_ADDRESSBOOK) return DoSuperMethodA(cl,obj,(Msg)msg);

	rv = DoSuperMethodA(cl,obj,(Msg)msg);
	*msg->Type = MUIV_NListtree_DropType_Above;
	return rv;
}

STATIC VOID AddressTreelist_Add(struct IClass *cl, Object *obj, struct addressbook_entry *entry, APTR listnode, char *pat)
{
	struct AddressTreelist_Data *data = (struct AddressTreelist_Data*)INST_DATA(cl,obj);

	for (entry = addressbook_first(entry);entry;entry = addressbook_next(entry))
	{
		if (data->type == TYPE_ADDRESSBOOK)
		{
			struct MUI_NListtree_TreeNode *treenode;
			LONG flags = 0;

			if (entry->type == ADDRESSBOOK_ENTRY_GROUP) flags = TNF_LIST|TNF_OPEN;

			treenode = (struct MUI_NListtree_TreeNode*)DoMethod(obj, MUIM_NListtree_Insert, "" /*name*/, entry, /*udata */
							   listnode,MUIV_NListtree_Insert_PrevNode_Tail,flags);

			if (treenode && (flags & TNF_LIST))
			{
				AddressTreelist_Add(cl,obj,entry,treenode,pat);
			}
		} else
		{
			if (entry->type == ADDRESSBOOK_ENTRY_GROUP)
			{
				AddressTreelist_Add(cl,obj,entry,NULL,pat);
			} else
			{
				if (pat)
				{
					if (!addressbook_completed_by_entry(pat,entry,NULL)) continue;
				}

				DoMethod(obj, MUIM_NListtree_Insert, "" /*name*/, entry, /*udata */ NULL,MUIV_NListtree_Insert_PrevNode_Sorted,0);
			}
		}
	}
}

STATIC ULONG AddressTreelist_Refresh(struct IClass *cl, Object *obj, struct MUIP_AddressTreelist_Refresh *msg)
{
	struct AddressTreelist_Data *data = (struct AddressTreelist_Data*)INST_DATA(cl,obj);
	if (data->filter) free(data->filter);
	data->filter = mystrdup(msg->str);
	set(obj, MUIA_NListtree_Quiet, TRUE);
	DoMethod(obj, MUIM_NListtree_Remove, MUIV_NListtree_Remove_ListNode_Root, MUIV_NListtree_Remove_TreeNode_All, 0);
	AddressTreelist_Add(cl,obj,NULL, MUIV_NListtree_Insert_ListNode_Root,msg->str);
	set(obj, MUIA_NListtree_Quiet, FALSE);
	return NULL;
}

STATIC ULONG AddressTreelist_NList_ContextMenuBuild(struct IClass *cl, Object * obj, struct MUIP_NList_ContextMenuBuild *msg)
{
	struct AddressTreelist_Data *data = (struct AddressTreelist_Data*)INST_DATA(cl,obj);

	if (data->type != TYPE_MAIN) return DoSuperMethodA(cl,obj,(Msg)msg);

	if (msg->ontop) return (ULONG)data->title_menu;
	return NULL;
}

STATIC ULONG AddressTreelist_ContextMenuChoice(struct IClass *cl, Object *obj, struct MUIP_ContextMenuChoice *msg)
{
	switch (xget(msg->item,MUIA_UserData))
	{
		case	1:
		case  2:
		case  3:
		case  4:
				  AddressTreelist_UpdateFormat(cl,obj);
				  break;
		default: 
		{
			return DoSuperMethodA(cl,obj,(Msg)msg);
		}
	}
  return 0;
}


STATIC BOOPSI_DISPATCHER(ULONG,AddressTreelist_Dispatcher,cl,obj,msg)
{
	switch(msg->MethodID)
	{
		case	OM_NEW: return AddressTreelist_New(cl,obj,(struct opSet*)msg);
		case	OM_DISPOSE: return AddressTreelist_Dispose(cl,obj,msg);
		case	MUIM_AskMinMax: return AddressTreelist_AskMinMax(cl,obj,(struct MUIP_AskMinMax*)msg);
		case	MUIM_Export: return AddressTreelist_Export(cl,obj,(struct MUIP_Export*)msg);
		case	MUIM_Import: return AddressTreelist_Import(cl,obj,(struct MUIP_Import*)msg);
		case	MUIM_AddressTreelist_Refresh: return AddressTreelist_Refresh(cl,obj,(struct MUIP_AddressTreelist_Refresh*)msg);
		case	MUIM_DragQuery: return AddressTreelist_DragQuery(cl,obj,(struct MUIP_DragQuery *)msg);
		case	MUIM_DragDrop:  return AddressTreelist_DragDrop (cl,obj,(struct MUIP_DragDrop *)msg);
		case	MUIM_NListtree_DropType: return AddressTreelist_DropType(cl,obj,(struct MUIP_NListtree_DropType*)msg);
		case	MUIM_ContextMenuChoice: return AddressTreelist_ContextMenuChoice(cl, obj, (struct MUIP_ContextMenuChoice *)msg);
		case	MUIM_NList_ContextMenuBuild: return AddressTreelist_NList_ContextMenuBuild(cl,obj,(struct MUIP_NList_ContextMenuBuild *)msg);

		default: return DoSuperMethodA(cl,obj,msg);
	}
}

struct MUI_CustomClass *CL_AddressTreelist;

int create_addresstreelist_class(void)
{
	SM_ENTER;
	if ((CL_AddressTreelist = CreateMCC(MUIC_NListtree,NULL,sizeof(struct AddressTreelist_Data),AddressTreelist_Dispatcher)))
	{
		SM_DEBUGF(15,("Create CL_AddressTreelist: 0x%lx\n",CL_AddressTreelist));
		SM_RETURN(1,"%ld");
	}
	SM_DEBUGF(5,("FAILED! Create CL_AddressTreelist\n"));
	SM_RETURN(0,"%ld");
}

void delete_addresstreelist_class(void)
{
	SM_ENTER;
	if (CL_AddressTreelist)
	{
		if (MUI_DeleteCustomClass(CL_AddressTreelist))
		{
			SM_DEBUGF(15,("Deleted CL_AddressTreelist: 0x%lx\n",CL_AddressTreelist));
			CL_AddressTreelist = NULL;
		} else
		{
			SM_DEBUGF(5,("FAILED! Delete CL_AddressTreelist: 0x%lx\n",CL_AddressTreelist));
		}
	}
	SM_LEAVE;
}
