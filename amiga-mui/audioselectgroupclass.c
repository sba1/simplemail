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
** audioselectgroupclass.c
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dos.h>

#include <intuition/intuitionbase.h>
#include <libraries/mui.h>
#include <mui/betterstring_mcc.h>
#include <datatypes/pictureclass.h>

#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/utility.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/datatypes.h>
#include <proto/muimaster.h>

#include "support_indep.h"

#include "amigasupport.h"
#include "audioselectgroupclass.h"
#include "compiler.h"
#include "datatypescache.h"
#include "muistuff.h"

struct AudioSelectGroup_Data
{
	Object *file_string;
	Object *play_button;

	Object *sound_obj;
};

STATIC VOID AudioSelectGroup_Play(struct AudioSelectGroup_Data **pdata)
{
	struct AudioSelectGroup_Data *data = *pdata;

	if (data->sound_obj) DisposeObject(data->sound_obj);
	if ((data->sound_obj = NewDTObject((char*)xget(data->file_string,MUIA_String_Contents),
				DTA_GroupID, GID_SOUND,
				TAG_DONE)))
	{
		struct dtTrigger dtt;

		/* Fill out the method message */
		dtt.MethodID     = DTM_TRIGGER;
		dtt.dtt_GInfo    = NULL;
		dtt.dtt_Function = STM_PLAY;
		dtt.dtt_Data     = NULL;

		/* Play the sound */
		DoMethodA(data->sound_obj, (Msg)&dtt);
	}
}

STATIC ULONG AudioSelectGroup_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct AudioSelectGroup_Data *data;
	Object *file_string, *play_button;

	if (!(obj=(Object *)DoSuperNew(cl,obj,
					MUIA_Group_Spacing, 0,
					MUIA_Group_Horiz, TRUE,
					Child, PopaslObject,
						MUIA_Popstring_String, file_string = BetterStringObject,MUIA_CycleChain,1,StringFrame, End,
						MUIA_Popstring_Button, PopButton(MUII_PopFile),
						End,
					Child, play_button = PopButton(MUII_TapePlay),
					TAG_MORE, msg->ops_AttrList)))
	        return 0;

	data = (struct AudioSelectGroup_Data*)INST_DATA(cl,obj);

	data->file_string = file_string;
	data->play_button = play_button;

	DoMethod(play_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, AudioSelectGroup_Play, data);

	return (ULONG)obj;
}

STATIC ULONG AudioSelectGroup_Cleanup(struct IClass *cl,Object *obj,Msg msg)
{
	struct AudioSelectGroup_Data *data = (struct AudioSelectGroup_Data*)INST_DATA(cl,obj);
	if (data->sound_obj)
	{
		DisposeDTObject(data->sound_obj);
		data->sound_obj = NULL;
	}
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

ASM STATIC ULONG AudioSelectGroup_Dispatcher(register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	putreg(REG_A4,cl->cl_UserData);
	switch (msg->MethodID)
	{
		case OM_NEW        : return AudioSelectGroup_New      (cl,obj,(struct opSet*)msg);
		case MUIM_Cleanup  : return AudioSelectGroup_Cleanup  (cl,obj,msg);
	}
	
	return DoSuperMethodA(cl,obj,msg);
}

struct MUI_CustomClass *CL_AudioSelectGroup;

int create_audioselectgroup_class(void);
void delete_audioselectgroup_class(void);

int create_audioselectgroup_class(void)
{
	if ((CL_AudioSelectGroup = MUI_CreateCustomClass(NULL, MUIC_Group, NULL, sizeof(struct AudioSelectGroup_Data), AudioSelectGroup_Dispatcher)))
	{
		CL_AudioSelectGroup->mcc_Class->cl_UserData = getreg(REG_A4);
		return TRUE;
	}
	return FALSE;
}

void delete_audioselectgroup_class(void)
{
	if (CL_AudioSelectGroup)
	{
		MUI_DeleteCustomClass(CL_AudioSelectGroup);
		CL_AudioSelectGroup = NULL;
	}
}
