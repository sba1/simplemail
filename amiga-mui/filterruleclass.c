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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <intuition/intuitionbase.h>
#include <libraries/mui.h>
#include <mui/BetterString_mcc.h>

#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include "filter.h"
#include "smintl.h"
#include "debug.h"
#include "support.h"

#include "compiler.h"
#include "muistuff.h"
#include "multistringclass.h"
#include "filterruleclass.h"
#include "picturebuttonclass.h"

struct FilterRule_Data
{
	int type;
	int flags;

	Object *group;

	Object *object1;
	Object *object2;
	Object *object3;
	Object *object4;

	Object *type_cycle;

	Object *substr_check;
	Object *case_check;
	Object *patt_check;

	struct filter_rule get_rule;
};

struct rule
{
	char *name;
	int type;
};

const static char *status_labels_untranslated[] =
{
	N_("New"),N_("Read"),N_("Unread"),N_("Replied"),N_("Forwarded"),N_("Pending"),N_("Sent"),NULL
};

char *status_labels[sizeof(status_labels_untranslated)/sizeof(char*)];

const static char *status_filenames[] =
{
	"PROGDIR:Images/status_new",
	"PROGDIR:Images/status_old",
	"PROGDIR:Images/status_unread",
	"PROGDIR:Images/status_reply",
	"PROGDIR:Images/status_forward",
	"PROGDIR:Images/status_waitsend",
	"PROGDIR:Images/status_sent",
	NULL
};

const static struct rule rules[] = {
	{N_("Sender"), RULE_FROM_MATCH},
	{N_("Receiver"), RULE_RCPT_MATCH},
	{N_("Subject"), RULE_SUBJECT_MATCH},
	{N_("Header"), RULE_HEADER_MATCH},
	{N_("Has attachments"), RULE_ATTACHMENT_MATCH},
	{N_("Status is"), RULE_STATUS_MATCH},
	{NULL,0},
};
static char *rule_cycle_array[sizeof(rules)/sizeof(struct rule)];


void status_cycle_active(Object **objs)
{
	int active = xget(objs[0],MUIA_Cycle_Active);
	set(objs[1],MUIA_PictureButton_Filename,status_filenames[active]);
}

STATIC VOID FilterRule_FlagsUpdate(struct FilterRule_Data **pdata)
{
	struct FilterRule_Data *data = *pdata;

	data->flags = 0;
	if (xget(data->substr_check, MUIA_Selected))  data->flags |= SM_PATTERN_SUBSTR;
	if (!(xget(data->case_check, MUIA_Selected))) data->flags |= SM_PATTERN_NOCASE;
	if (!(xget(data->patt_check, MUIA_Selected))) data->flags |= SM_PATTERN_NOPATT;

	if (!(data->flags & SM_PATTERN_NOPATT))
	{
		if (data->type == RULE_HEADER_MATCH)
		{
			set(data->object2, MUIA_Text_Contents, _("matches"));
		} else
		{
			set(data->object1, MUIA_Text_Contents, _("matches"));
		}
	} else if ((data->flags & SM_PATTERN_SUBSTR))
	{
		if (data->type == RULE_HEADER_MATCH)
		{
			set(data->object2, MUIA_Text_Contents, _("contains"));
		} else
		{
			set(data->object1, MUIA_Text_Contents, _("contains"));
		}
	} else
	{
		if (data->type == RULE_HEADER_MATCH)
		{
			set(data->object2, MUIA_Text_Contents, _("equals"));
		} else
		{
			set(data->object1, MUIA_Text_Contents, _("equals"));
		}
	}
}

STATIC BOOL FilterRule_CreateObjects(struct FilterRule_Data *data)
{
	Object *group1, *group2;
	DoMethod(data->group, MUIM_Group_InitChange);
	DisposeAllChilds(data->group);

	data->object1 = data->object2 = data->object3 = data->object4 = NULL;
	group1 = group2 = NULL;

	if (data->type == RULE_FROM_MATCH)
	{
		data->object1 = TextObject, TextFrame, MUIA_Text_Contents, _("equals"),End;
		data->object2 = MultiStringObject, StringFrame, End;
	} else if (data->type == RULE_RCPT_MATCH)
	{
		data->object1 = TextObject, TextFrame, MUIA_Text_Contents, _("equals"),End;
		data->object2 = MultiStringObject, StringFrame, End;
	} else if (data->type == RULE_SUBJECT_MATCH)
	{
		data->object1 = TextObject, TextFrame, MUIA_Text_Contents, _("equals"),End;
		data->object2 = MultiStringObject, StringFrame, End;
	} else if (data->type == RULE_HEADER_MATCH)
	{
		data->object1 = BetterStringObject, StringFrame, MUIA_CycleChain,1,End;
		data->object2 = TextObject, TextFrame, MUIA_Text_Contents, _("equals"),End;
		data->object3 = MultiStringObject, StringFrame, End;
	} else if (data->type == RULE_STATUS_MATCH)
	{
		data->object1 = MakeCycle(NULL,status_labels);
		data->object2 = PictureButtonObject, End;
		DoMethod(data->object1, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, (ULONG)App, 5, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)status_cycle_active, (ULONG)data->object1, (ULONG)data->object2);
	}

	data->flags = 0;
	switch (data->type)
	{
		case	RULE_FROM_MATCH:
		case	RULE_RCPT_MATCH:
		case	RULE_BODY_MATCH:
		case	RULE_HEADER_MATCH:
		case	RULE_SUBJECT_MATCH:
		    	group2 = HGroup,
		    		Child, MakeLabel(_("Substring")),
		    		Child, data->substr_check = MakeCheck(NULL, FALSE),
		    		Child, HVSpace,
		    		Child, MakeLabel(_("Case sensitive")),
		    		Child, data->case_check = MakeCheck(NULL, FALSE),
		    		Child, HVSpace,
		    		Child, MakeLabel(_("Pattern")),
		    		Child, data->patt_check = MakeCheck(NULL, FALSE),
		    		End;
		    	if (group2)
		    	{
		    		char *substr_help_txt, *case_help_txt, *patt_help_txt;

		    		substr_help_txt = _("If activated the string must only be a part\n"
		    		                    "of the searched field.");
		    		case_help_txt = _("If activated the search is case sensitive.");
		    		patt_help_txt = _("If activated the string is an AmigaDOS pattern\n"
		    		                  "and patternmatching will be performed.\n"
		    		                  "Special chars are: ? # | ~ ( ) [ ] % '");

		    		set(data->substr_check, MUIA_ShortHelp, substr_help_txt);
		    		set(data->case_check, MUIA_ShortHelp, case_help_txt);
		    		set(data->patt_check, MUIA_ShortHelp, patt_help_txt);

		    		data->flags = SM_PATTERN_NOCASE|SM_PATTERN_NOPATT;
		    		DoMethod(data->substr_check, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)FilterRule_FlagsUpdate, (ULONG)data);
		    		DoMethod(data->case_check, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)FilterRule_FlagsUpdate, (ULONG)data);
		    		DoMethod(data->patt_check, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)FilterRule_FlagsUpdate, (ULONG)data);
		    	}
		    	break;
	}

	if (data->object1 || data->object2 || data->object3 || data->object4)
	{
		group1 = HGroup, End;
		if (group1)
		{
			if (data->object1) DoMethod(group1, OM_ADDMEMBER, (ULONG)data->object1);
			if (data->object2) DoMethod(group1, OM_ADDMEMBER, (ULONG)data->object2);
			if (data->object3) DoMethod(group1, OM_ADDMEMBER, (ULONG)data->object3);
			if (data->object4) DoMethod(group1, OM_ADDMEMBER, (ULONG)data->object4);
		}
	}

	if (group1)
		DoMethod(data->group, OM_ADDMEMBER, (ULONG)group1);
	else
		DoMethod(data->group, OM_ADDMEMBER, (ULONG)HVSpace);

	if (group2)
	{
		DoMethod(data->group, OM_ADDMEMBER, (ULONG)group2);
		DoMethod(data->group, OM_ADDMEMBER, (ULONG)RectangleObject,MUIA_Rectangle_HBar, TRUE, End);
	}

	DoMethod(data->group, MUIM_Group_ExitChange);
	return TRUE;
}

STATIC VOID FilterRule_SetRule(struct FilterRule_Data *data,struct filter_rule *fr)
{
	data->type = fr->type;
	data->flags = fr->flags;
	set(data->type_cycle,MUIA_Cycle_Active,data->type);
	DoMethod(data->group, MUIM_Group_InitChange);
	FilterRule_CreateObjects(data);
	switch(data->type)
	{
		case	RULE_FROM_MATCH:
					set(data->object2,MUIA_MultiString_ContentsArray,fr->u.from.from);
					break;
		case	RULE_RCPT_MATCH:
					set(data->object2,MUIA_MultiString_ContentsArray,fr->u.rcpt.rcpt);
					break;
		case	RULE_SUBJECT_MATCH:
					set(data->object2,MUIA_MultiString_ContentsArray,fr->u.subject.subject);
					break;
		case	RULE_HEADER_MATCH:
					set(data->object1,MUIA_String_Contents,fr->u.header.name);
					set(data->object3,MUIA_MultiString_ContentsArray,fr->u.header.contents);
					break;
		case	RULE_ATTACHMENT_MATCH:
					break;
		case	RULE_STATUS_MATCH:
					set(data->object1,MUIA_Cycle_Active,fr->u.status.status);
					break;
	}
	if ((fr->flags & SM_PATTERN_SUBSTR)) set(data->substr_check, MUIA_Selected, TRUE);
	if (!(fr->flags & SM_PATTERN_NOCASE)) set(data->case_check, MUIA_Selected, TRUE);
	if (!(fr->flags & SM_PATTERN_NOPATT)) set(data->patt_check, MUIA_Selected, TRUE);

	DoMethod(data->group, MUIM_Group_ExitChange);
}

STATIC VOID FilterRule_TypeCycleActive(struct FilterRule_Data **pdata)
{
	struct FilterRule_Data *data = *pdata;
	data->type = xget(data->type_cycle, MUIA_Cycle_Active);
	FilterRule_CreateObjects(data);
}

STATIC ULONG FilterRule_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct FilterRule_Data *data;
	struct TagItem *ti;
	Object *type_cycle,*group;

	if (!(obj=(Object *)DoSuperNew(cl,obj,
		MUIA_Group_Horiz, TRUE,
		Child, type_cycle = MakeCycle(NULL,rule_cycle_array),
		Child, group = VGroup, End,
		TAG_MORE,msg->ops_AttrList)))
		return 0;

	data = (struct FilterRule_Data*)INST_DATA(cl,obj);

	data->group = group;
	data->type_cycle = type_cycle;

	set(data->type_cycle,MUIA_Weight,0);

	if ((ti = FindTagItem(MUIA_FilterRule_Data,msg->ops_AttrList)))
	{
		FilterRule_SetRule(data,(struct filter_rule*)ti->ti_Data);
	} else FilterRule_CreateObjects(data);

	DoMethod(data->type_cycle, MUIM_Notify,MUIA_Cycle_Active, MUIV_EveryTime, (ULONG)obj, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)FilterRule_TypeCycleActive, (ULONG)data);

	return (ULONG)obj;
}

STATIC ULONG FilterRule_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
	return DoSuperMethodA(cl,obj,msg);
}

STATIC ULONG FilterRule_Set(struct IClass *cl, Object *obj, struct opSet *msg)
{
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG FilterRule_Get(struct IClass *cl, Object *obj, struct opGet *msg)
{
	struct FilterRule_Data *data = (struct FilterRule_Data*)INST_DATA(cl,obj);
	switch (msg->opg_AttrID)
	{
		case	MUIA_FilterRule_Data:
					memset(&data->get_rule,0,sizeof(struct filter_rule));
					data->get_rule.type = data->type;
					data->get_rule.flags = data->flags;
					switch (data->get_rule.type)
					{
						case	RULE_FROM_MATCH:
									data->get_rule.u.from.from = (char**)xget(data->object2,MUIA_MultiString_ContentsArray);
									break;
						case	RULE_RCPT_MATCH:
									data->get_rule.u.rcpt.rcpt = (char**)xget(data->object2,MUIA_MultiString_ContentsArray);
									break;
						case	RULE_SUBJECT_MATCH:
									data->get_rule.u.subject.subject = (char**)xget(data->object2,MUIA_MultiString_ContentsArray);
									break;
						case	RULE_HEADER_MATCH:
									data->get_rule.u.header.name = (char*)xget(data->object1,MUIA_String_Contents);
									data->get_rule.u.header.contents = (char**)xget(data->object3,MUIA_MultiString_ContentsArray);
									break;
						case	RULE_STATUS_MATCH:
									data->get_rule.u.status.status = (int)xget(data->object1,MUIA_Cycle_Active);
									break;
					}
					*msg->opg_Storage = (ULONG)&data->get_rule;
					return TRUE;
					break;

		default:
					return DoSuperMethodA(cl,obj,(Msg)msg);
	}
}

STATIC ULONG FilterRule_DragQuery(struct IClass *cl,Object *obj,struct MUIP_DragQuery *msg)
{
  return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG FilterRule_DragDrop(struct IClass *cl,Object *obj,struct MUIP_DragDrop *msg)
{
  return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC MY_BOOPSI_DISPATCHER(ULONG, FilterRule_Dispatcher, cl, obj, msg)
{
	switch(msg->MethodID)
	{
		case OM_NEW:     return FilterRule_New(cl,obj,(struct opSet*)msg);
		case OM_DISPOSE: return FilterRule_Dispose(cl,obj,msg);
		case OM_SET:     return FilterRule_Set(cl,obj,(struct opSet*)msg);
		case OM_GET:     return FilterRule_Get(cl,obj,(struct opGet*)msg);
		case MUIM_DragQuery: return FilterRule_DragQuery (cl,obj,(struct MUIP_DragQuery *)msg);
		case MUIM_DragDrop:  return FilterRule_DragDrop (cl,obj,(struct MUIP_DragDrop *)msg);
		default:         return DoSuperMethodA(cl,obj,msg);
	}
}

struct MUI_CustomClass *CL_FilterRule;

int create_filterrule_class(void)
{
	SM_ENTER;
	if ((CL_FilterRule = CreateMCC(MUIC_Group,NULL,sizeof(struct FilterRule_Data),FilterRule_Dispatcher)))
	{
		int i;

		for (i=0;i<sizeof(rules)/sizeof(struct rule);i++)
		{
			if (rules[i].name)
				rule_cycle_array[i] = _(rules[i].name);
		}

		for (i=0;i<sizeof(status_labels_untranslated)/sizeof(char*);i++)
		{
			if (status_labels[i])
				status_labels[i] = _(status_labels_untranslated[i]);
		}

		SM_DEBUGF(15,("Create CL_FilterRule: 0x%lx\n",CL_FilterRule));
		SM_RETURN(1,"%ld");
	}
	SM_DEBUGF(5,("FAILED! Create CL_FilterRule\n"));
	SM_RETURN(0,"%ld");
}

void delete_filterrule_class(void)
{
	SM_ENTER;
	if (CL_FilterRule)
	{
		if (MUI_DeleteCustomClass(CL_FilterRule))
		{
			SM_DEBUGF(15,("Deleted CL_FilterRule: 0x%lx\n",CL_FilterRule));
			CL_FilterRule = NULL;
		} else
		{
			SM_DEBUGF(5,("FAILED! Delete CL_FilterRule: 0x%lx\n",CL_FilterRule));
		}
	}
	SM_LEAVE;
}
