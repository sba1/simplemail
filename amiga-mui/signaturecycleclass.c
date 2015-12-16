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
** signaturecycleclass.c
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <libraries/mui.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>

#include "signature.h"
#include "configuration.h"
#include "debug.h"
#include "smintl.h"
#include "support_indep.h"

#include "compiler.h"
#include "muistuff.h"
#include "signaturecycleclass.h"

struct SignatureCycle_Data
{
	Object *obj;
	Object *sign_cycle;
	int has_default_entry;    /* a default entry will be showen */
	char **sign_array;        /* The array which contains the signature names (converted)*/
	char **utf8_sign_array;   /* The array which contains the signature names (UTF8) */
	int sign_array_default;   /* points to the defaultentry */
	int sign_array_utf8count; /* The count of the array to free the memory,
	                             it also points to "No Signature" */
};

STATIC ULONG SignatureCycle_Set(struct IClass *cl, Object *obj, struct opSet *msg, int nodosuper)
{
	struct SignatureCycle_Data *data = (struct SignatureCycle_Data*)INST_DATA(cl,obj);
	struct TagItem *tstate, *tag;
	tstate = (struct TagItem *)msg->ops_AttrList;

	while ((tag = NextTagItem ((APTR)&tstate)))
	{
		switch (tag->ti_Tag)
		{
			case	MUIA_SignatureCycle_Active:
			    	if (data->sign_cycle)
			    	{
			    		switch (tag->ti_Data)
			    		{
			    			case	MUIV_SignatureCycle_Active_Next:
			    			    	nnset(data->sign_cycle, MUIA_Cycle_Active, MUIV_Cycle_Active_Next);
			    			    	break;

			    			case	MUIV_SignatureCycle_Active_Prev:
			    			    	nnset(data->sign_cycle, MUIA_Cycle_Active, MUIV_Cycle_Active_Prev);
			    			    	break;

			    			default:
			    			    	nnset(data->sign_cycle, MUIA_Cycle_Active, tag->ti_Data);
			    		}
			    	}
			    	break;

			case	MUIA_SignatureCycle_SignatureName:
			    	if (data->sign_cycle)
			    	{
			    		if (mystrcmp((char*)tag->ti_Data, MUIV_SignatureCycle_NoSignature) == 0)
			    		{
			    			nnset(data->sign_cycle, MUIA_Cycle_Active, data->sign_array_utf8count);
			    		}
			    		else if (mystrcmp((char*)tag->ti_Data, MUIV_SignatureCycle_Default) == 0)
			    		{
			    			nnset(data->sign_cycle, MUIA_Cycle_Active, data->sign_array_default);
			    		}
			    		else
			    		{
			    			int status_found = FALSE;
			    			if (data->sign_array)
			    			{
			    				int i;
			    				for (i=0;i<data->sign_array_utf8count;i++)
			    				{
			    					if (mystrcmp((char*)tag->ti_Data, data->utf8_sign_array[i]) == 0)
			    					{
			    						nnset(data->sign_cycle, MUIA_Cycle_Active, i);
			    						status_found = TRUE;
			    						break;
			    					}
			    				}
			    			}
			    			if (!status_found)
			    			{
			    				/* in case we don't find it in the Array, set to Default */
			    				nnset(data->sign_cycle, MUIA_Cycle_Active, data->sign_array_default);
			    			}
			    		}
			    	}
			    	break;
		}
	}

	if (nodosuper) return 0;
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG SignatureCycle_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct SignatureCycle_Data *data;
	Object *sign_cycle;
	struct TagItem *ti;
	int has_default_entry=0;
	int sign_array_default=0;
	struct list *signature_list = &user.config.signature_list;
	char **sign_array = NULL;
	char **utf8_sign_array = NULL;
	int sign_array_utf8count = 0;
	int i;

	if ((ti = FindTagItem(MUIA_SignatureCycle_HasDefaultEntry, msg->ops_AttrList)))
	{
		has_default_entry = !!(ti->ti_Data);
	}

	if ((ti = FindTagItem(MUIA_SignatureCycle_SignatureList, msg->ops_AttrList)))
	{
		signature_list = (struct list *)(ti->ti_Data);
	}

	i = list_length(signature_list);
	sign_array = (char**)malloc((i+2+has_default_entry)*sizeof(char*));
	utf8_sign_array = (char**)malloc(i*sizeof(char*));
	if (sign_array && utf8_sign_array)
	{
		int j=0;
		struct signature *sign = (struct signature*)list_first(signature_list);
		while (sign)
		{
			sign_array[j]=utf8tostrcreate(sign->name, user.config.default_codeset);
			utf8_sign_array[j]=mystrdup(sign->name);
			sign = (struct signature*)node_next(&sign->node);
			j++;
		}
		sign_array_utf8count = j;
		sign_array[j] = _("No Signature");
		if (has_default_entry)
		{
			j++;
			sign_array[j] = _("<Default>");
			sign_array_default = j;
		} else
		{
			/* if there is no <Default> entry, fallback to NoSignature. */
			sign_array_default = sign_array_utf8count;
		}
		sign_array[j+1] = NULL;
	} else
	{
		if (sign_array) free(sign_array);
		if (utf8_sign_array) free(utf8_sign_array);
		return 0;
	}

	if (!(obj=(Object *)DoSuperNew(cl,obj,
		Child, sign_cycle = MakeCycle(NULL, (STRPTR *)sign_array),
		TAG_MORE, msg->ops_AttrList)))
	{
		if (sign_array)
		{
			int i;
			for (i=0;i<sign_array_utf8count;i++)
			{
				free(sign_array[i]);
				free(utf8_sign_array[i]);
			}
			free(sign_array);
			free(utf8_sign_array);
		}
		return 0;
	}

	data = (struct SignatureCycle_Data*)INST_DATA(cl,obj);
	data->obj = obj;
	data->sign_cycle = sign_cycle;
	data->has_default_entry = has_default_entry;
	data->sign_array = sign_array;
	data->utf8_sign_array = utf8_sign_array;
	data->sign_array_default = sign_array_default;
	data->sign_array_utf8count = sign_array_utf8count;

	DoMethod(data->sign_cycle, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, (ULONG)data->obj, 3,
		MUIM_Set, MUIA_SignatureCycle_Active, MUIV_TriggerValue);

	SignatureCycle_Set(cl,obj,msg,1);

	return (ULONG)obj;
}

STATIC ULONG SignatureCycle_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
	struct SignatureCycle_Data *data = (struct SignatureCycle_Data*)INST_DATA(cl,obj);

	if (data->sign_array)
	{
		int i;
		for (i=0;i<data->sign_array_utf8count;i++)
		{
			free(data->sign_array[i]);
			free(data->utf8_sign_array[i]);
		}
		free(data->sign_array);
		free(data->utf8_sign_array);
	}

	return DoSuperMethodA(cl,obj,msg);
}

STATIC ULONG SignatureCycle_Get(struct IClass *cl, Object *obj, struct opGet *msg)
{
	struct SignatureCycle_Data *data = (struct SignatureCycle_Data*)INST_DATA(cl,obj);

	switch (msg->opg_AttrID)
	{
		case	MUIA_SignatureCycle_Active:
		    	if (data->sign_cycle)
		    	{
		    		*msg->opg_Storage = xget(data->sign_cycle, MUIA_Cycle_Active);
		    	}
		    	return 1;

		case	MUIA_SignatureCycle_SignatureName:
		    	if (data->sign_cycle)
		    	{
		    		int result;

		    		/* Get the MUIA_Cycle_Active */
		    		result = xget(data->sign_cycle, MUIA_Cycle_Active);

		    		/* Now change the result back to the SignatureName */
		    		if (result == data->sign_array_utf8count)
		    		{
		    			*msg->opg_Storage = (ULONG)MUIV_SignatureCycle_NoSignature;
		    		}
		    		else if (result == data->sign_array_default)
		    		{
		    			*msg->opg_Storage = (ULONG)MUIV_SignatureCycle_Default;
		    		}
		    		else
		    		{
		    			int status_found = FALSE;
		    			if (data->sign_array)
		    			{
		    				if ((result >= 0) && (result < data->sign_array_utf8count))
		    				{
		    					*msg->opg_Storage = (ULONG)(data->utf8_sign_array[result]);
		    					status_found = TRUE;
		    				}
		    			}
		    			if (!status_found)
		    			{
		    				/* in case we don't find it in the Array, set to Default */
		    				*msg->opg_Storage = (ULONG)MUIV_SignatureCycle_Default;
		    			}
		    		}
		    		return 1;
		    	}

		default:
		    	return DoSuperMethodA(cl,obj,(Msg)msg);
	}
}

STATIC ULONG SignatureCycle_Refresh(struct IClass *cl, Object *obj, struct MUIP_SignatureCycle_Refresh *msg)
{
	struct SignatureCycle_Data *data = (struct SignatureCycle_Data*)INST_DATA(cl,obj);
	int val = (int)xget(data->sign_cycle, MUIA_Cycle_Active);
	char *sign_current = NULL;
	int i;

	if (data->sign_cycle)
	{
		/* free the cycle stuff */
		DoMethod(data->obj, MUIM_Group_InitChange);
		DoMethod(data->obj, OM_REMMEMBER, (ULONG)data->sign_cycle);
		MUI_DisposeObject(data->sign_cycle);
		data->sign_cycle = NULL;
		if (data->sign_array)
		{
			for (i=0;i<data->sign_array_utf8count;i++)
			{
				if (i == val)
				{
					sign_current = mystrdup(data->utf8_sign_array[i]);
				}
				free(data->sign_array[i]);
				free(data->utf8_sign_array[i]);
			}
			free(data->sign_array);
			free(data->utf8_sign_array);
		}

		/* make the new cycle stuff */
		i = list_length(msg->signature_list);
		data->sign_array = (char**)malloc((i+2+data->has_default_entry)*sizeof(char*));
		data->utf8_sign_array = (char**)malloc(i*sizeof(char*));
		if (data->sign_array && data->utf8_sign_array)
		{
			int j=0;
			struct signature *sign = (struct signature*)list_first(msg->signature_list);
			val = -1;
			while (sign)
			{
				data->sign_array[j]=utf8tostrcreate(sign->name, user.config.default_codeset);
				data->utf8_sign_array[j]=mystrdup(sign->name);
				if (val == -1)
				{
					if (mystrcmp(sign_current, sign->name) == 0) val = j;
				}
				sign = (struct signature*)node_next(&sign->node);
				j++;
			}
			data->sign_array_utf8count = j;
			data->sign_array[j] = _("Without"); /* was "No Signature" but "Without" is shorter */
			if (val == -1)
			{
				if (mystrcmp(sign_current, MUIV_SignatureCycle_NoSignature) == 0) val = j;
			}
			if (data->has_default_entry)
			{
				j++;
				data->sign_array[j] = _("<Default>");
				data->sign_array_default = j;
			} else
			{
				/* if there is no <Default> entry, fallback to NoSignature. */
				data->sign_array_default = data->sign_array_utf8count;
			}
			data->sign_array[j+1] = NULL;
			if (val == -1) val = data->sign_array_default;

			data->sign_cycle = CycleObject,
				MUIA_CycleChain, 1,
				MUIA_Cycle_Entries, data->sign_array,
				End;

		} else
		{
			if (data->sign_array) free(data->sign_array);
			if (data->utf8_sign_array) free(data->utf8_sign_array);
		}

		if (data->sign_cycle)
		{
			DoMethod(data->obj, OM_ADDMEMBER, (ULONG)data->sign_cycle);
			DoMethod(data->sign_cycle, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, (ULONG)data->obj, 3,
				MUIM_Set, MUIA_SignatureCycle_Active, MUIV_TriggerValue);
			set(data->sign_cycle, MUIA_Cycle_Active, val);
		}
		DoMethod(data->sign_cycle, MUIM_Group_ExitChange);
		if (sign_current) free(sign_current);
	}
	return 0;
}

STATIC MY_BOOPSI_DISPATCHER(ULONG, SignatureCycle_Dispatcher, cl, obj, msg)
{
	switch(msg->MethodID)
	{
		case OM_NEW:     return SignatureCycle_New(cl,obj,(struct opSet*)msg);
		case OM_DISPOSE: return SignatureCycle_Dispose(cl,obj,msg);
		case OM_SET:     return SignatureCycle_Set(cl,obj,(struct opSet*)msg,0);
		case OM_GET:     return SignatureCycle_Get(cl,obj,(struct opGet*)msg);
		case MUIM_SignatureCycle_Refresh: return SignatureCycle_Refresh(cl,obj,(struct MUIP_SignatureCycle_Refresh*)msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

struct MUI_CustomClass *CL_SignatureCycle;

int create_signaturecycle_class(void)
{
	SM_ENTER;
	if ((CL_SignatureCycle = CreateMCC(MUIC_Group, NULL, sizeof(struct SignatureCycle_Data), SignatureCycle_Dispatcher)))
	{
		SM_DEBUGF(15,("Create CL_SignatureCycle: 0x%lx\n",CL_SignatureCycle));
		SM_RETURN(1,"%ld");
	}
	SM_DEBUGF(5,("FAILED! Create CL_SignatureCycle\n"));
	SM_RETURN(0,"%ld");
}

void delete_signaturecycle_class(void)
{
	SM_ENTER;
	if (CL_SignatureCycle)
	{
		if (MUI_DeleteCustomClass(CL_SignatureCycle))
		{
			SM_DEBUGF(15,("Deleted CL_SignatureCycle: 0x%lx\n",CL_SignatureCycle));
			CL_SignatureCycle = NULL;
		} else
		{
			SM_DEBUGF(5,("FAILED! Delete CL_SignatureCycle: 0x%lx\n",CL_SignatureCycle));
		}
	}
	SM_LEAVE;
}
