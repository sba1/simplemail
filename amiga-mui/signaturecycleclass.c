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
	int has_default_entry;    /* if there are NO signatures, a default entry will be showen */
	char **sign_array;        /* The array which contains the signature names (converted)*/
	char **utf8_sign_array;   /* The array which contains the signature names (UTF8) */
	int sign_array_default;   /* points to the defaultentry */
	int sign_array_utf8count; /* The count of the array to free the memory,
	                             it also points to "No Signature" */
};

STATIC ULONG SignatureCycle_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct SignatureCycle_Data *data;
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
		return NULL;
	}

	if (!(obj=(Object *)DoSuperNew(cl,obj, MUIA_Cycle_Entries, sign_array, TAG_MORE, msg->ops_AttrList)))
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
		return NULL;
	}

	if ((ti = FindTagItem(MUIA_SignatureCycle_Signature,msg->ops_AttrList)))
	{
		if (mystrcmp((char*)ti->ti_Data, MUIV_SignatureCycle_NoSignature) == 0)
		{
			set(obj, MUIA_Cycle_Active, sign_array_utf8count);
		}
		else if (mystrcmp((char*)ti->ti_Data, MUIV_SignatureCycle_Default) == 0)
		{
			set(obj, MUIA_Cycle_Active, sign_array_default);
		}
		else
		{
			int status_found = FALSE;
			if (sign_array)
			{
				int i;
				for (i=0;i<sign_array_utf8count;i++)
				{
					if (mystrcmp((char *)ti->ti_Data, utf8_sign_array[i]) == 0)
					{
						set(obj, MUIA_Cycle_Active, i);
						status_found = TRUE;
						break;
					}
				}
			}
			if (!status_found)
			{
				/* in case we don't find it in the Array, set to Default */
				set(obj, MUIA_Cycle_Active, sign_array_default);
			}
		}
	}

	data = (struct SignatureCycle_Data*)INST_DATA(cl,obj);
	data->obj = obj;
	data->has_default_entry = has_default_entry;
	data->sign_array = sign_array;
	data->utf8_sign_array = utf8_sign_array;
	data->sign_array_default = sign_array_default;
	data->sign_array_utf8count = sign_array_utf8count;

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

STATIC ULONG SignatureCycle_Set(struct IClass *cl, Object *obj, struct opSet *msg)
{
	struct SignatureCycle_Data *data = (struct SignatureCycle_Data*)INST_DATA(cl,obj);
	struct TagItem *ti;

	/* Change MUIA_SignatureCycle_Signature to MUIA_Cycle_Active */
	if ((ti = FindTagItem(MUIA_SignatureCycle_Signature,msg->ops_AttrList)))
	{
		if (mystrcmp((char*)ti->ti_Data, MUIV_SignatureCycle_NoSignature) == 0)
		{
			set(data->obj, MUIA_Cycle_Active, data->sign_array_utf8count);
		}
		else if (mystrcmp((char*)ti->ti_Data, MUIV_SignatureCycle_Default) == 0)
		{
			set(data->obj, MUIA_Cycle_Active, data->sign_array_default);
		}
		else
		{
			int status_found = FALSE;
			if (data->sign_array)
			{
				int i;
				for (i=0;i<data->sign_array_utf8count;i++)
				{
					if (mystrcmp((char*)ti->ti_Data, data->utf8_sign_array[i]) == 0)
					{
						set(data->obj, MUIA_Cycle_Active, i);
						status_found = TRUE;
						break;
					}
				}
			}
			if (!status_found)
			{
				/* in case we don't find it in the Array, set to Default */
				set(data->obj, MUIA_Cycle_Active, data->sign_array_default);
			}
		}
	}

	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG SignatureCycle_Get(struct IClass *cl, Object *obj, struct opGet *msg)
{
	struct SignatureCycle_Data *data = (struct SignatureCycle_Data*)INST_DATA(cl,obj);

	if (msg->opg_AttrID == MUIA_SignatureCycle_Signature)
	{
		ULONG result;

	 	/* Change MUIA_SignatureCycle_Signature to MUIA_Cycle_Active */
	 	result = xget(data->obj, MUIA_Cycle_Active);

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
	} else return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC BOOPSI_DISPATCHER(ULONG, SignatureCycle_Dispatcher, cl, obj, msg)
{
	switch(msg->MethodID)
	{
		case OM_NEW:     return SignatureCycle_New(cl,obj,(struct opSet*)msg);
		case OM_DISPOSE: return SignatureCycle_Dispose(cl,obj,msg);
		case OM_SET:     return SignatureCycle_Set(cl,obj,(struct opSet*)msg);
		case OM_GET:     return SignatureCycle_Get(cl,obj,(struct opGet*)msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

struct MUI_CustomClass *CL_SignatureCycle;

int create_signaturecycle_class(void)
{
	SM_ENTER;
	if ((CL_SignatureCycle = CreateMCC(MUIC_Cycle, NULL, sizeof(struct SignatureCycle_Data), SignatureCycle_Dispatcher)))
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
