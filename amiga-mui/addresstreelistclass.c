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

#include <dos.h>
#include <libraries/mui.h>
#include <mui/NListview_MCC.h>
#include <mui/NListtree_Mcc.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>

#include "addressbook.h"
#include "mail.h"
#include "parse.h"
#include "simplemail.h"
#include "support_indep.h"

#include "addressbookwnd.h"
#include "addresstreelistclass.h"
#include "compiler.h"
#include "mailtreelistclass.h"
#include "muistuff.h"

struct AddressTreelist_Data
{
	struct Hook compare_hook;
	struct Hook construct_hook;
	struct Hook destruct_hook;
	struct Hook display_hook;

	int in_addressbook;
};

STATIC ASM int address_compare(register __a1 struct MUIP_NListtree_CompareMessage *msg)
{
	struct addressbook_entry *entry1 = (struct addressbook_entry *)msg->TreeNode1->tn_User;
	struct addressbook_entry *entry2 = (struct addressbook_entry *)msg->TreeNode2->tn_User;

	return mystricmp(entry1->u.person.realname,entry2->u.person.realname);
}

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
						*msg->Array++ = entry->u.person.realname;
						*msg->Array++ = entry->u.person.alias;
						*msg->Array++ = entry->u.person.description;
						if (entry->u.person.num_emails)
							*msg->Array = entry->u.person.emails[0];
						break;

			case	ADDRESSBOOK_ENTRY_LIST:
						*msg->Array++ = entry->u.list.nameofml;
						*msg->Array++ = entry->u.list.alias;
						*msg->Array++ = entry->u.list.description;
						*msg->Array = NULL;
						break;
		}
	} else
	{
		*msg->Array++ = "Name";
		*msg->Array++ = "Alias";
		*msg->Array++ = "Description";
		*msg->Array = "Address";
	}
}

STATIC ULONG AddressTreelist_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct AddressTreelist_Data *data;
	struct TagItem *ti = FindTagItem(MUIA_AddressTreelist_InAddressbook,msg->ops_AttrList);
	int in_addressbook;

	if (ti) in_addressbook = ti->ti_Data;
	else in_addressbook = 0;

	if (!(obj=(Object *)DoSuperNew(cl,obj,
/*					in_addressbook?TAG_IGNORE:MUIA_NListtree_MultiSelect,MUIV_NListtree_MultiSelect_Default, */
					MUIA_NListtree_Title, TRUE,
					MUIA_NListtree_Format,",,,",
					MUIA_NListtree_DoubleClick, MUIV_NListtree_DoubleClick_Tree,
					MUIA_NListtree_ShowTree, in_addressbook,
					MUIA_NList_ShowDropMarks, in_addressbook,
					TAG_MORE,msg->ops_AttrList)))
		return 0;

	data = (struct AddressTreelist_Data*)INST_DATA(cl,obj);
	data->in_addressbook = in_addressbook;
	init_hook(&data->compare_hook,(HOOKFUNC)address_compare);
	init_hook(&data->construct_hook,(HOOKFUNC)address_construct);
	init_hook(&data->destruct_hook,(HOOKFUNC)address_destruct);
	init_hook(&data->display_hook,(HOOKFUNC)address_display);

	SetAttrs(obj,
						in_addressbook?TAG_IGNORE:MUIA_NListtree_CompareHook, &data->compare_hook,
						MUIA_NListtree_ConstructHook, &data->construct_hook,
						MUIA_NListtree_DestructHook, &data->destruct_hook,
						MUIA_NListtree_DisplayHook, &data->display_hook,
						MUIA_UserData, 1, /* used in addresstringclass.c */
						TAG_DONE);

	return (ULONG)obj;
}

STATIC ULONG AddressTreelist_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
//	struct AddressTreelist_Data *data = (struct AddressTreelist_Data*)INST_DATA(cl,obj);
//	if (data->context_menu) MUI_DisposeObject(data->context_menu);
	return DoSuperMethodA(cl,obj,msg);
}


STATIC ULONG AddressTreelist_DragQuery(struct IClass *cl, Object *obj, struct MUIP_DragQuery *msg)
{
	struct AddressTreelist_Data *data = (struct AddressTreelist_Data*)INST_DATA(cl,obj);
	if (OCLASS(msg->obj) == CL_MailTreelist->mcc_Class) return MUIV_DragQuery_Accept;
	if (data->in_addressbook) return DoSuperMethodA(cl,obj,(Msg)msg);
	return MUIV_DragQuery_Refuse;
}

STATIC ULONG AddressTreelist_DragDrop(struct IClass *cl, Object *obj, struct MUIP_DragDrop *msg)
{
	struct AddressTreelist_Data *data = (struct AddressTreelist_Data*)INST_DATA(cl,obj);
	struct MUI_NListtree_TreeNode *treenode;

	if (OCLASS(msg->obj) != CL_MailTreelist->mcc_Class) return DoSuperMethodA(cl,obj,(Msg)msg);

	if ((treenode = (struct MUI_NListtree_TreeNode*)xget(msg->obj,MUIA_NListtree_Active)))
	{
		struct mail *mail = (struct mail*)treenode->tn_User;
		if (mail && (ULONG)mail != (ULONG)MUIV_MailTreelist_UserData_Name)
		{
			char *from = mail_find_header_contents(mail,"from");
			if (from)
			{
				struct mailbox mb;
				if (parse_mailbox(from,&mb))
				{
					if (!addressbook_get_realname(mb.addr_spec))
					{
						addressbook_open_with_new_address(mail);
					}
				}
			}
		}
	}
	return 0;
}

STATIC ULONG AddressTreelist_DropType(struct IClass *cl, Object *obj,struct MUIP_NListtree_DropType *msg)
{
	struct AddressTreelist_Data *data = (struct AddressTreelist_Data*)INST_DATA(cl,obj);
	ULONG rv;

	if (data->in_addressbook) return DoSuperMethodA(cl,obj,(Msg)msg);

	rv = DoSuperMethodA(cl,obj,(Msg)msg);
	*msg->Type = MUIV_NListtree_DropType_Above;
	return rv;
}

STATIC VOID AddressTreelist_Add(struct IClass *cl, Object *obj, struct addressbook_entry *entry, APTR listnode)
{
	struct AddressTreelist_Data *data = (struct AddressTreelist_Data*)INST_DATA(cl,obj);

	entry = addressbook_first(entry);

	while (entry)
	{
		if (data->in_addressbook)
		{
			struct MUI_NListtree_TreeNode *treenode;
			LONG flags = 0;

			if (entry->type == ADDRESSBOOK_ENTRY_GROUP) flags = TNF_LIST|TNF_OPEN;

			treenode = (struct MUI_NListtree_TreeNode*)DoMethod(obj, MUIM_NListtree_Insert, "" /*name*/, entry, /*udata */
							   listnode,MUIV_NListtree_Insert_PrevNode_Tail,flags);

			if (treenode && (flags & TNF_LIST))
			{
				AddressTreelist_Add(cl,obj,entry,treenode);
			}
		} else
		{
			if (entry->type == ADDRESSBOOK_ENTRY_GROUP)
			{
				AddressTreelist_Add(cl,obj,entry,NULL);
			} else DoMethod(obj, MUIM_NListtree_Insert, "" /*name*/, entry, /*udata */ NULL,MUIV_NListtree_Insert_PrevNode_Sorted,0);
		}
		entry = addressbook_next(entry);
	}
}

STATIC ULONG AddressTreelist_Refresh(struct IClass *cl, Object *obj, Msg msg)
{
	set(obj, MUIA_NListtree_Quiet, TRUE);
	DoMethod(obj, MUIM_NListtree_Remove, MUIV_NListtree_Remove_ListNode_Root, MUIV_NListtree_Remove_TreeNode_All, 0);
	AddressTreelist_Add(cl,obj,NULL, MUIV_NListtree_Insert_ListNode_Root);
	set(obj, MUIA_NListtree_Quiet, FALSE);
	return NULL;
}

STATIC ASM ULONG AddressTreelist_Dispatcher(register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	putreg(REG_A4,cl->cl_UserData);
	switch(msg->MethodID)
	{
		case	OM_NEW: return AddressTreelist_New(cl,obj,(struct opSet*)msg);
		case	OM_DISPOSE: return AddressTreelist_Dispose(cl,obj,msg);
		case	MUIM_AddressTreelist_Refresh: return AddressTreelist_Refresh(cl,obj,msg);
    case  MUIM_DragQuery: return AddressTreelist_DragQuery(cl,obj,(struct MUIP_DragQuery *)msg);
    case  MUIM_DragDrop:  return AddressTreelist_DragDrop (cl,obj,(struct MUIP_DragDrop *)msg);
    case	MUIM_NListtree_DropType: return AddressTreelist_DropType(cl,obj,(struct MUIP_NListtree_DropType*)msg);

		default: return DoSuperMethodA(cl,obj,msg);
	}
}

struct MUI_CustomClass *CL_AddressTreelist;

int create_addresstreelist_class(void)
{
	if ((CL_AddressTreelist = MUI_CreateCustomClass(NULL,MUIC_NListtree,NULL,sizeof(struct AddressTreelist_Data),AddressTreelist_Dispatcher)))
	{
		CL_AddressTreelist->mcc_Class->cl_UserData = getreg(REG_A4);
		return 1;
	}
	return 0;
}

void delete_addresstreelist_class(void)
{
	if (CL_AddressTreelist) MUI_DeleteCustomClass(CL_AddressTreelist);
}
