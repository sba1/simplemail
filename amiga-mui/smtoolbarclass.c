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
** smtoolbarclass.c
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <libraries/mui.h>
#include <mui/thebar_mcc.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>

#include "configuration.h"
#include "debug.h"
#include "smintl.h"
#include "support.h"
#include "support_indep.h"

#include "compiler.h"
#include "muistuff.h"
#include "smtoolbarclass.h"
#include "picturebuttonclass.h"

struct SMToolbar_Data
{
	Object *obj;
	Object *toolbar;
	int button_count;
	int used_thebar_mcc;
	struct MUIS_TheBar_Button *toolbar_buttons; /* array of the toolbar buttons */
};

STATIC ULONG SMToolbar_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct SMToolbar_Data *data;
	struct TagItem *ti;
	struct MUIS_SMToolbar_Button *buttons;
	struct MUIS_TheBar_Button *toolbar_buttons;
	int i=0, j, button_count=0, used_thebar_mcc=0;
	int use_vgroup=0, add_hvspace=0;
	Object *horiz_group, *current_group, *toolbar=NULL;

	/* we need some buttons */
	if ((ti = FindTagItem(MUIA_SMToolbar_Buttons, msg->ops_AttrList)))
		buttons = (struct MUIS_SMToolbar_Button *)ti->ti_Data;
	else
		return NULL;

	if ((ti = FindTagItem(MUIA_SMToolbar_InVGroup, msg->ops_AttrList)))
		use_vgroup = !!ti->ti_Data;

	if ((ti = FindTagItem(MUIA_SMToolbar_AddHVSpace, msg->ops_AttrList)))
		add_hvspace = !!ti->ti_Data;

	/* how much button do we have? */
	while (buttons[button_count].pos != MUIV_SMToolbar_End) button_count++;
	if (!(button_count)) return NULL;

	/* now copy the buttonarray to our own */
	if (!(toolbar_buttons = (struct MUIS_TheBar_Button*)malloc((button_count + 1)*(sizeof(struct MUIS_TheBar_Button)))))
		return NULL;
	memset(toolbar_buttons,0,(button_count + 1)*(sizeof(struct MUIS_TheBar_Button)));

	for (i=0;i<button_count;i++)
	{
		if (buttons[i].pos != MUIV_SMToolbar_Space)
		{
			toolbar_buttons[i].img = buttons[i].pos;
			toolbar_buttons[i].ID = buttons[i].id;
			toolbar_buttons[i].text = mystrdup(_(buttons[i].name));
			if (buttons[i].help)
				toolbar_buttons[i].help = mystrdup(_(buttons[i].help));
			if (buttons[i].flags & MUIV_SMToolbar_ButtonFlag_Toggle)
				toolbar_buttons[i].flags |= MUIV_TheBar_ButtonFlag_Toggle;
			if (buttons[i].flags & MUIV_SMToolbar_ButtonFlag_Disabled)
				toolbar_buttons[i].flags |= MUIV_TheBar_ButtonFlag_Disabled;
			if (buttons[i].flags & MUIV_SMToolbar_ButtonFlag_Selected)
				toolbar_buttons[i].flags |= MUIV_TheBar_ButtonFlag_Selected;
			if (buttons[i].flags & MUIV_SMToolbar_ButtonFlag_Hide)
				toolbar_buttons[i].flags |= MUIV_TheBar_ButtonFlag_Hide;
		} else toolbar_buttons[i].img = MUIV_TheBar_BarSpacer;
	}
	toolbar_buttons[i].img = MUIV_TheBar_End;

	/* First try TheBar */
	if (!user.config.dont_use_thebar_mcc)
	{
		if (use_vgroup)
		{
			HGroupV,
				toolbar = TheBarVirtObject,
					MUIA_Group_Horiz,             TRUE,
					MUIA_TheBar_MinVer,           19,        /* because of gfx corruptions in older version */
					MUIA_TheBar_EnableKeys,       TRUE,
					MUIA_TheBar_IgnoreAppareance, FALSE,
					MUIA_TheBar_Buttons,          toolbar_buttons,
					MUIA_TheBar_Strip,            "PROGDIR:Images/images",
					MUIA_TheBar_SelStrip,         "PROGDIR:Images/images_S",
					MUIA_TheBar_DisStrip,         "PROGDIR:Images/images_G",
					MUIA_TheBar_StripRows,        SMTOOLBAR_STRIP_ROWS,
					MUIA_TheBar_StripCols,        SMTOOLBAR_STRIP_COLS,
					MUIA_TheBar_StripHorizSpace,  SMTOOLBAR_STRIP_HSPACE,
					MUIA_TheBar_StripVertSpace,   SMTOOLBAR_STRIP_VSPACE,
					End,
				End;
		} else
		{
			toolbar = TheBarObject,
				MUIA_Group_Horiz,             TRUE,
				MUIA_TheBar_MinVer,           19,        /* because of gfx corruptions in older version */
				MUIA_TheBar_EnableKeys,       TRUE,
				MUIA_TheBar_IgnoreAppareance, FALSE,
				MUIA_TheBar_Buttons,          toolbar_buttons,
				MUIA_TheBar_Strip,            "PROGDIR:Images/images",
				MUIA_TheBar_SelStrip,         "PROGDIR:Images/images_S",
				MUIA_TheBar_DisStrip,         "PROGDIR:Images/images_G",
				MUIA_TheBar_StripRows,        SMTOOLBAR_STRIP_ROWS,
				MUIA_TheBar_StripCols,        SMTOOLBAR_STRIP_COLS,
				MUIA_TheBar_StripHorizSpace,  SMTOOLBAR_STRIP_HSPACE,
				MUIA_TheBar_StripVertSpace,   SMTOOLBAR_STRIP_VSPACE,
				End;
		}
		if (toolbar) used_thebar_mcc = 1;
	}

	if (!toolbar)
	{
		/* TheBar was not available, build own toolbar */
		if (use_vgroup)
		{
			toolbar = HGroupV,
			          	Child, horiz_group = HGroup,
			             	MUIA_VertWeight,0,
			          		End,
			          	End;
		} else
		{
			toolbar = HGroup,
			          	Child, horiz_group = HGroup,
			             	MUIA_VertWeight,0,
			          		End,
			          	End;
		}

		if (horiz_group)
		{
			current_group = NULL;

			for (i=0,j=0;i<button_count;i++)
			{
				if (!current_group)
				{
					if ((current_group = HGroup,
					     	MUIA_Group_Spacing, 0,
					     	End))
					{
						DoMethod(horiz_group,OM_ADDMEMBER,current_group);
					} else
					{
						MUI_DisposeObject(toolbar);
						toolbar = NULL;
						break;
					}
				}

				if (buttons[i].pos != MUIV_SMToolbar_Space)
				{
					char name_buf[100];

					j++;
					strcpy(name_buf,"PROGDIR:Images");
					sm_add_part(name_buf,buttons[i].imagename,sizeof(name_buf));

					if ((toolbar_buttons[i].obj = MakePictureButton(toolbar_buttons[i].text,name_buf)))
					{
						if (toolbar_buttons[i].help)
							set(toolbar_buttons[i].obj, MUIA_ShortHelp, toolbar_buttons[i].help);
						if (buttons[i].flags & MUIV_SMToolbar_ButtonFlag_Toggle)
							set(toolbar_buttons[i].obj, MUIA_InputMode, MUIV_InputMode_Toggle);
						if (buttons[i].flags & MUIV_SMToolbar_ButtonFlag_Disabled)
							set(toolbar_buttons[i].obj, MUIA_Disabled, TRUE);
						if (buttons[i].flags & MUIV_SMToolbar_ButtonFlag_Selected)
							set(toolbar_buttons[i].obj, MUIA_Selected, TRUE);
						if (buttons[i].flags & MUIV_SMToolbar_ButtonFlag_Hide)
							set(toolbar_buttons[i].obj, MUIA_ShowMe, FALSE);

						DoMethod(current_group,OM_ADDMEMBER,toolbar_buttons[i].obj);
					} else
					{
						MUI_DisposeObject(toolbar);
						toolbar = NULL;
						break;
					}
				} else
				{
					set(current_group, MUIA_Weight, 50 * j);
					j = 0;
					current_group = NULL;
				}
			}
			if (toolbar)
			{
				set(current_group, MUIA_Weight, 50 * j);
				if (add_hvspace)
					DoMethod(horiz_group,OM_ADDMEMBER,HVSpace);
			}
		}
	}

	if (!toolbar)
	{
		/* Something failed */
		for (i=0;i<button_count;i++)
		{
			free(toolbar_buttons[i].text);
			free(toolbar_buttons[i].help);
		}
		free(toolbar_buttons);
		return NULL;
	}

	if (!(obj=(Object *)DoSuperNew(cl, obj, TAG_MORE, msg->ops_AttrList)))
		return NULL;

	data = (struct SMToolbar_Data*)INST_DATA(cl,obj);
	data->obj             = obj;
	data->toolbar         = toolbar;
	data->button_count    = button_count;
	data->used_thebar_mcc = used_thebar_mcc;
	data->toolbar_buttons = toolbar_buttons;

	DoMethod(obj, OM_ADDMEMBER, toolbar);

	return (ULONG)obj;
}

STATIC ULONG SMToolbar_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
	struct SMToolbar_Data *data = (struct SMToolbar_Data*)INST_DATA(cl,obj);
	int i;

	for (i=0;i<data->button_count;i++)
	{
		free(data->toolbar_buttons[i].text);
		free(data->toolbar_buttons[i].help);
	}
	free(data->toolbar_buttons);

	return DoSuperMethodA(cl,obj,msg);
}

STATIC ULONG SMToolbar_SetAttr(struct IClass *cl, Object *obj, struct MUIP_SMToolbar_SetAttr *msg)
{
	struct SMToolbar_Data *data = (struct SMToolbar_Data*)INST_DATA(cl,obj);
	int i;

	switch (msg->attr)
	{
		case	MUIA_SMToolbar_Attr_Hide:
		    	if (data->used_thebar_mcc)
		    	{
		    		DoMethod(data->toolbar, MUIM_TheBar_SetAttr, msg->id, MUIV_TheBar_Attr_Hide, msg->value);
		    	} else
		    	{
		    		for (i=0;i<data->button_count;i++)
		    		{
		    			if (data->toolbar_buttons[i].ID == msg->id)
		    			{
				    		set(data->toolbar_buttons[i].obj, MUIA_ShowMe, !msg->value);
				    		break;
				    	}
				    }
		    	}
		    	break;

		case	MUIA_SMToolbar_Attr_Disabled:
		    	if (data->used_thebar_mcc)
		    	{
		    		DoMethod(data->toolbar, MUIM_TheBar_SetAttr, msg->id, MUIV_TheBar_Attr_Disabled, msg->value);
		    	} else
		    	{
		    		for (i=0;i<data->button_count;i++)
		    		{
		    			if (data->toolbar_buttons[i].ID == msg->id)
		    			{
				    		set(data->toolbar_buttons[i].obj, MUIA_Disabled, msg->value);
				    		break;
				    	}
				    }
		    	}
		    	break;

		case	MUIA_SMToolbar_Attr_Selected:
		    	if (data->used_thebar_mcc)
		    	{
		    		DoMethod(data->toolbar, MUIM_TheBar_SetAttr, msg->id, MUIV_TheBar_Attr_Selected, msg->value);
		    	} else
		    	{
		    		for (i=0;i<data->button_count;i++)
		    		{
		    			if (data->toolbar_buttons[i].ID == msg->id)
		    			{
				    		set(data->toolbar_buttons[i].obj, MUIA_Selected, msg->value);
				    		break;
				    	}
				    }
		    	}
		    	break;
	}

	return 0;
}

STATIC ULONG SMToolbar_GetAttr(struct IClass *cl, Object *obj, struct MUIP_SMToolbar_GetAttr *msg)
{
	struct SMToolbar_Data *data = (struct SMToolbar_Data*)INST_DATA(cl,obj);
	int i;

	switch (msg->attr)
	{
		case	MUIA_SMToolbar_Attr_Hide:
		    	if (data->used_thebar_mcc)
		    	{
		    		DoMethod(data->toolbar, MUIM_TheBar_GetAttr, msg->id, MUIV_TheBar_Attr_Hide, msg->storage);
		    	} else
		    	{
		    		for (i=0;i<data->button_count;i++)
		    		{
		    			if (data->toolbar_buttons[i].ID == msg->id)
		    			{
				    		*msg->storage = !xget(data->toolbar_buttons[i].obj, MUIA_ShowMe);
				    		break;
				    	}
				    }
		    	}
		    	return 1;

		case	MUIA_SMToolbar_Attr_Disabled:
		    	if (data->used_thebar_mcc)
		    	{
		    		DoMethod(data->toolbar, MUIM_TheBar_GetAttr, msg->id, MUIV_TheBar_Attr_Disabled, msg->storage);
		    	} else
		    	{
		    		for (i=0;i<data->button_count;i++)
		    		{
		    			if (data->toolbar_buttons[i].ID == msg->id)
		    			{
				    		*msg->storage = xget(data->toolbar_buttons[i].obj, MUIA_Disabled);
				    		break;
				    	}
				    }
		    	}
		    	return 1;

		case	MUIA_SMToolbar_Attr_Selected:
		    	if (data->used_thebar_mcc)
		    	{
		    		DoMethod(data->toolbar, MUIM_TheBar_GetAttr, msg->id, MUIV_TheBar_Attr_Selected, msg->storage);
		    	} else
		    	{
		    		for (i=0;i<data->button_count;i++)
		    		{
		    			if (data->toolbar_buttons[i].ID == msg->id)
		    			{
				    		*msg->storage = xget(data->toolbar_buttons[i].obj, MUIA_Selected);
				    		break;
				    	}
				    }
		    	}
		    	return 1;
	}
	return 0;
}

STATIC ULONG SMToolbar_GetObject(struct IClass *cl, Object *obj, struct MUIP_SMToolbar_GetObject *msg)
{
	struct SMToolbar_Data *data = (struct SMToolbar_Data*)INST_DATA(cl,obj);
	int i;

	if (data->used_thebar_mcc)
		return DoMethod(data->toolbar, MUIM_TheBar_GetObject, msg->id);

	for (i=0;i<data->button_count;i++)
	{
		if (data->toolbar_buttons[i].ID == msg->id)
			return (ULONG)data->toolbar_buttons[i].obj;
	}

	return NULL;}

STATIC BOOPSI_DISPATCHER(ULONG, SMToolbar_Dispatcher, cl, obj, msg)
{
	switch(msg->MethodID)
	{
		case OM_NEW:     return SMToolbar_New(cl,obj,(struct opSet*)msg);
		case OM_DISPOSE: return SMToolbar_Dispose(cl,obj,msg);
		case MUIM_SMToolbar_GetObject: return SMToolbar_GetObject(cl,obj,(struct MUIP_SMToolbar_GetObject*)msg);
		case MUIM_SMToolbar_GetAttr: return SMToolbar_GetAttr(cl,obj,(struct MUIP_SMToolbar_GetAttr*)msg);
		case MUIM_SMToolbar_SetAttr: return SMToolbar_SetAttr(cl,obj,(struct MUIP_SMToolbar_SetAttr*)msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

struct MUI_CustomClass *CL_SMToolbar;

int create_smtoolbar_class(void)
{
	SM_ENTER;
	if ((CL_SMToolbar = CreateMCC(MUIC_Group, NULL, sizeof(struct SMToolbar_Data), SMToolbar_Dispatcher)))
	{
		SM_DEBUGF(15,("Create CL_SMToolbar: 0x%lx\n",CL_SMToolbar));
		SM_RETURN(1,"%ld");
	}
	SM_DEBUGF(5,("FAILED! Create CL_SMToolbar\n"));
	SM_RETURN(0,"%ld");
}

void delete_smtoolbar_class(void)
{
	SM_ENTER;
	if (CL_SMToolbar)
	{
		if (MUI_DeleteCustomClass(CL_SMToolbar))
		{
			SM_DEBUGF(15,("Deleted CL_SMToolbar: 0x%lx\n",CL_SMToolbar));
			CL_SMToolbar = NULL;
		} else
		{
			SM_DEBUGF(5,("FAILED! Delete CL_SMToolbar: 0x%lx\n",CL_SMToolbar));
		}
	}
	SM_LEAVE;
}
