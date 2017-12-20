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
 * @file addressmatchlistclass.c
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
#include "simplemail.h"
#include "smintl.h"
#include "support.h"
#include "support_indep.h"

#include "addressmatchlistclass.h"
#include "compiler.h"
#include "gui_main_amiga.h"
#include "mailtreelistclass.h"
#include "muistuff.h"
#include "picturebuttonclass.h"

struct AddressMatchList_Data
{
	struct Hook construct_hook;
	struct Hook compare_hook;
	struct Hook destruct_hook;
	struct Hook display_hook;

	char alias_buf[64];
	char realname_buf[64];
	char description_buf[128];
	char email_buf[128];
	char group_buf[128];

	char *pattern;

	APTR status_group;
	Object *status_group_obj;
};

/**
 * Constructor for address match entries.
 */
STATIC ASM SAVEDS struct address_match_entry *matchentry_construct(REG(a0, struct Hook *h), REG(a2, Object *obj),REG(a1,struct NList_ConstructMessage *msg))
{
	struct address_match_entry *entry = (struct address_match_entry *)msg->entry;
	struct address_match_entry *new_entry;

	if (!(new_entry = (struct address_match_entry*)malloc(sizeof(*new_entry))))
		return NULL;

	switch (new_entry->type = entry->type)
	{
		case	AMET_GROUP:
				if (!(new_entry->o.group = addressbook_duplicate_group(entry->o.group)))
					goto err;
				break;

		case	AMET_ENTRY:
				if (!(new_entry->o.entry = addressbook_duplicate_entry_new(entry->o.entry)))
					goto err;
				break;

		default:
				goto err;
	}

	return new_entry;
err:
	free(new_entry);
	return NULL;
}

/**
 * Destructor for address match entries.
 */
STATIC ASM SAVEDS VOID matchentry_destruct(REG(a0, struct Hook *h), REG(a2, Object *obj),REG(a1,struct NList_DestructMessage *msg))
{
	struct address_match_entry *entry = (struct address_match_entry *)msg->entry;
	switch (entry->type)
	{
		case	AMET_GROUP:
				addressbook_free_group(entry->o.group);
				break;

		case	AMET_ENTRY:
				addressbook_free_entry_new(entry->o.entry);
				break;
	}

	free(entry);
}

/**
 * Display function for address match entries.
 */
STATIC ASM SAVEDS VOID matchentry_display(REG(a0,struct Hook *h),REG(a2,Object *obj), REG(a1,struct NList_DisplayMessage *msg))
{
	char **array = msg->strings;
	char **preparse = msg->preparses;
	struct address_match_entry *entry = (struct address_match_entry*)msg->entry;
	struct AddressMatchList_Data *data = (struct AddressMatchList_Data*)h->h_Data;

	int i, count;

	if (!entry)
	{
		*array++ = Q_("?people:Name");
		*array++ = _("Alias");
		*array++ = _("Description");
		*array++ = _("Address");
		*array = _("Groups");
		return;
	}

	if (entry->type == AMET_GROUP)
	{
		int l;

		sprintf(data->realname_buf,"\33O[%08lx]",(ULONG)data->status_group);
		l = strlen(data->realname_buf);
		
		utf8tostr(entry->o.group->name, data->realname_buf + l, sizeof(data->realname_buf) - l, user.config.default_codeset);	
		utf8tostr(entry->o.group->description, data->description_buf, sizeof(data->description_buf), user.config.default_codeset);
		*array++ = data->realname_buf;
		*array++ = NULL;
		*array++ = data->description_buf;
		*array++ = NULL;
		*array++ = NULL;
		*array = NULL;
	} else if (entry->type == AMET_ENTRY)
	{
		struct addressbook_entry_new *addr_entry = entry->o.entry;

		utf8tostr(addr_entry->alias, data->alias_buf, sizeof(data->alias_buf), user.config.default_codeset);
		utf8tostr(addr_entry->realname, data->realname_buf, sizeof(data->realname_buf), user.config.default_codeset);
		utf8tostr(addr_entry->description, data->description_buf, sizeof(data->description_buf), user.config.default_codeset);

		*array++ = data->realname_buf;
		*array++ = data->alias_buf;
		*array++ = data->description_buf;

		if (addr_entry->email_array) *array = addr_entry->email_array[0];
		else *array = NULL;

		/* pattern */
		if (data->pattern)
		{
			int pl = strlen(data->pattern);

			if (!utf8stricmp_len(addr_entry->alias, data->pattern, pl)) preparse[1] = "\033b";
			if (!utf8stricmp_len(addr_entry->realname, data->pattern, pl)) preparse[0] = "\033b";

			for (i=0;i<array_length(addr_entry->email_array);i++)
			{
				if (!utf8stricmp_len(addr_entry->email_array[i],data->pattern, pl))
				{
					/* Check if this email is displayed, if not append it */
					if (i != 0)
					{
						sm_snprintf(data->email_buf, sizeof(data->email_buf), "%s,\033b%s", addr_entry->email_array[0], addr_entry->email_array[i]);
						*array = data->email_buf;
					} else preparse[3] = "\033b";
					break;
				}
			}
		}

		array++;

		/* group */
		count = 0;
		data->group_buf[0] = 0;
		for (i=0;i<array_length(addr_entry->group_array);i++)
		{
			if (i)
			{
				if (count + 2 >= sizeof(data->group_buf)) break;
				data->group_buf[count] = ',',
				data->group_buf[++count] = ' ';
				data->group_buf[++count] = 0;
			}
			count += utf8tostr(addr_entry->group_array[i], &data->group_buf[count], sizeof(data->group_buf) - count, user.config.default_codeset);
		}
		*array++ = data->group_buf;
	}
}

/**
 * Compare function for address match entries.
 */
STATIC ASM SAVEDS LONG matchentry_compare(REG(a0, struct Hook *h), REG(a2, Object *obj), REG(a1,struct NList_CompareMessage *msg))
{
	char *str1, *str2;
	struct address_match_entry *entry1 = (struct address_match_entry *)msg->entry1;
	struct address_match_entry *entry2 = (struct address_match_entry *)msg->entry2;

	if (entry1->type == AMET_GROUP) str1 = entry1->o.group->name;
	else if (entry1->type == AMET_ENTRY) str1 = entry1->o.entry->realname;
	else str1 = "";

	if (entry2->type == AMET_GROUP) str2 = entry2->o.group->name;
	else if (entry1->type == AMET_ENTRY) str2 = entry2->o.entry->realname;
	else str2 = "";

	return utf8stricmp(str1,str2);
}


/********************** BOOPSI Methods **********************/

/**
 * Implementation of OM_NEW
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG AddressMatchList_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct AddressMatchList_Data *data;

	if (!(obj=(Object *)DoSuperNew(cl,obj,
					MUIA_Draggable, TRUE,
					TAG_MORE,msg->ops_AttrList)))
		return 0;

	data = (struct AddressMatchList_Data*)INST_DATA(cl,obj);

	init_hook(&data->construct_hook,(HOOKFUNC)matchentry_construct);
	init_hook(&data->compare_hook,(HOOKFUNC)matchentry_compare);
	init_hook(&data->destruct_hook,(HOOKFUNC)matchentry_destruct);
	init_hook_with_data(&data->display_hook,(HOOKFUNC)matchentry_display,data);

	SetAttrs(obj,
						MUIA_NList_ConstructHook2, &data->construct_hook,
						MUIA_NList_CompareHook2, &data->compare_hook,
						MUIA_NList_DestructHook2, &data->destruct_hook,
						MUIA_NList_DisplayHook2, &data->display_hook,
						MUIA_NList_Title, TRUE,
						MUIA_NList_Format, ",,,,",
						TAG_DONE);

	data->status_group_obj = PictureButtonObject, MUIA_PictureButton_Directory, gui_get_images_directory(), MUIA_PictureButton_Filename, "status_group", End;

	return (ULONG)obj;
}

/**
 * Implementation of OM_DISPOSE
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG AddressMatchList_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
	struct AddressMatchList_Data *data = (struct AddressMatchList_Data*)INST_DATA(cl,obj);
	free(data->pattern);
	if (data->status_group_obj) MUI_DisposeObject(data->status_group_obj);
	return DoSuperMethodA(cl,obj,msg);
}

/**
 * Implementation of MUIM_Setup
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG AddressMatchList_Setup(struct IClass *cl, Object *obj, struct MUIP_Setup *msg)
{
	struct AddressMatchList_Data *data = (struct AddressMatchList_Data*)INST_DATA(cl,obj);

	if (!DoSuperMethodA(cl,obj,(Msg)msg))
		return 0;

	data->status_group = (APTR)DoMethod(obj, MUIM_NList_CreateImage, (ULONG)data->status_group_obj, 0);
	return 1;
}

/**
 * Implementation of MUIM_Cleanup
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG AddressMatchList_Cleanup(struct IClass *cl, Object *obj, Msg msg)
{
	struct AddressMatchList_Data *data = (struct AddressMatchList_Data*)INST_DATA(cl,obj);
	if (data->status_group) DoMethod(obj, MUIM_NList_DeleteImage, (ULONG)data->status_group);
	return DoSuperMethodA(cl,obj,msg);
}

/**
 * Implementation of MUIM_AddressMatchList_Refresh
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG AddressMatchList_Refresh(struct IClass *cl, Object *obj, struct MUIP_AddressMatchList_Refresh *msg)
{
	struct AddressMatchList_Data *data = (struct AddressMatchList_Data*)INST_DATA(cl,obj);
	struct addressbook_entry_new *addr_entry;
	struct addressbook_group *group;
	struct address_match_entry entry;
	char *pattern = msg->pattern;
	int pattern_len = strlen(pattern);

	free(data->pattern);
	data->pattern = mystrdup(pattern);

	set(obj, MUIA_NList_Quiet, TRUE);
	DoMethod(obj, MUIM_NList_Clear);

	/* Add the address entries */
	addr_entry = addressbook_first_entry();
	while (addr_entry)
	{
		if (!msg->pattern || (msg->pattern && addressbook_get_entry_completing_part(addr_entry, msg->pattern, NULL)))
		{
			entry.type = AMET_ENTRY;
			entry.o.entry = addr_entry;
			DoMethod(obj, MUIM_NList_InsertSingle, (ULONG)&entry, MUIV_NList_Insert_Sorted);
		}
		addr_entry = addressbook_next_entry(addr_entry);
	}
	
	/* Add the groups */
	group = addressbook_first_group();
	while (group)
	{
		if (!msg->pattern || (msg->pattern && !utf8stricmp_len(msg->pattern,group->name,pattern_len)))
		{
			entry.type = AMET_GROUP;
			entry.o.group = group;
			DoMethod(obj, MUIM_NList_InsertSingle, (ULONG)&entry, MUIV_NList_Insert_Sorted);
		}

		group = addressbook_next_group(group);
	}

	set(obj, MUIA_NList_Quiet, FALSE);

	return 0;
}

/**
 * The Boopsi Dispatcher for the address match list class.
 */
STATIC MY_BOOPSI_DISPATCHER(ULONG,AddressMatchList_Dispatcher,cl,obj,msg)
{
	switch(msg->MethodID)
	{
		case	OM_NEW: return AddressMatchList_New(cl,obj,(struct opSet*)msg);
		case	OM_DISPOSE: return AddressMatchList_Dispose(cl,obj,msg);
		case	MUIM_Setup:		return AddressMatchList_Setup(cl,obj,(struct MUIP_Setup*)msg);
		case	MUIM_Cleanup:	return AddressMatchList_Cleanup(cl,obj,msg);
		case	MUIM_AddressMatchList_Refresh: return AddressMatchList_Refresh(cl,obj,(struct MUIP_AddressMatchList_Refresh *)msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

/*****************************************************************************/

struct MUI_CustomClass *CL_AddressMatchList;

/*****************************************************************************/

int create_addressmatchlist_class(void)
{
	SM_ENTER;
	if ((CL_AddressMatchList = CreateMCC(MUIC_NList,NULL,sizeof(struct AddressMatchList_Data),AddressMatchList_Dispatcher)))
	{
		SM_DEBUGF(15,("Create CL_AddressMatchList: 0x%lx\n",CL_AddressMatchList));
		SM_RETURN(1,"%ld");
	}
	SM_DEBUGF(5,("FAILED! Create CL_AddressMatchList\n"));
	SM_RETURN(0,"%ld");
}

/*****************************************************************************/

void delete_addressmatchlist_class(void)
{
	SM_ENTER;
	if (CL_AddressMatchList)
	{
		if (MUI_DeleteCustomClass(CL_AddressMatchList))
		{
			SM_DEBUGF(15,("Deleted CL_AddressMatchList: 0x%lx\n",CL_AddressMatchList));
			CL_AddressMatchList = NULL;
		} else
		{
			SM_DEBUGF(5,("FAILED! Delete CL_AddressMatchList: 0x%lx\n",CL_AddressMatchList));
		}
	}
	SM_LEAVE;
}
