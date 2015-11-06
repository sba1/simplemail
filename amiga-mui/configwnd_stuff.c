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
 * @file configwnd_stuff.c
 */

#include <ctype.h>
#include <stdlib.h>

#include <libraries/mui.h>
#include <mui/TextEditor_mcc.h>
#include <mui/NListview_mcc.h>

#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "parse.h"
#include "debug.h"
#include "smintl.h"
#include "support_indep.h"

#include "compiler.h"
#include "muistuff.h"
#include "configwnd_stuff.h"
#include "request.h"
#include "support.h"

/*****************************************************************************/

char **array_of_addresses_from_texteditor(Object *editor, int page, int *error_ptr, Object *config_wnd, Object *config_list)
{
	char *addresses;
	char **new_array = NULL;

	/* Check the validity of the e-mail addresses first */
	if ((addresses = (char*)DoMethod(editor, MUIM_TextEditor_ExportText)))
	{
		struct mailbox mb;
		char *buf = addresses;

		while (isspace((unsigned char)*buf) && *buf) buf++;

		if (*buf)
		{
			while ((buf = parse_mailbox(buf,&mb)))
			{
				/* ensures that buf != NULL if no error */
				while (isspace((unsigned char)*buf)) buf++;
				if (*buf == 0) break;
				free(mb.addr_spec);
				free(mb.phrase);
			}
			if (!buf)
			{
				set(config_list, MUIA_NList_Active, page);
				set(config_wnd,MUIA_Window_ActiveObject,editor);
				sm_request(NULL,_("You have entered an invalid email address. Please correct it."),_("Ok"),NULL);
				FreeVec(addresses);
				array_free(new_array);
				*error_ptr = 1;
				return NULL;
			}

			buf = addresses;
			while ((buf = parse_mailbox(buf,&mb)))
			{
				new_array = array_add_string(new_array,mb.addr_spec);
				free(mb.addr_spec);
				free(mb.phrase);
			}
		}
		FreeVec(addresses);		
	}
	*error_ptr = 0;
	return new_array;
}

/**
 * The Boopsi dispatcher for the size custom class.
 */
STATIC MY_BOOPSI_DISPATCHER(ULONG, Sizes_Dispatcher, cl, obj, msg)
{
	switch(msg->MethodID)
	{
		case	MUIM_Numeric_Stringify:
					{
						static char buf[64];
						LONG val = ((struct MUIP_Numeric_Stringify*)msg)->value;
						if (!val) return (ULONG)_("All messages");
						val = value2size(val);
						sprintf(buf, _("> %ld KB"),val);
						return (ULONG)buf;
					}
					break;
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

struct MUI_CustomClass *CL_Sizes;

/*****************************************************************************/

int create_sizes_class(void)
{
	SM_ENTER;
	if ((CL_Sizes = CreateMCC(MUIC_Slider,NULL,4,Sizes_Dispatcher)))
	{
		SM_DEBUGF(15,("Create CL_Sizes: 0x%lx\n",CL_Sizes));
		SM_RETURN(1,"%ld");
	}
	SM_DEBUGF(5,("FAILED! Create CL_Sizes\n"));
	SM_RETURN(0,"%ld");
}

/*****************************************************************************/

void delete_sizes_class(void)
{
	SM_ENTER;
	if (CL_Sizes)
	{
		if (MUI_DeleteCustomClass(CL_Sizes))
		{
			SM_DEBUGF(15,("Deleted CL_Sizes: 0x%lx\n",CL_Sizes));
			CL_Sizes = NULL;
		} else
		{
			SM_DEBUGF(5,("Delete CL_Sizes: 0x%lx\n",CL_Sizes));
		}
	}
	SM_LEAVE;
}

/*****************************************************************************/

int value2size(int val)
{
	if (val > 35) val = (val - 33)*100;
	else if (val > 16) val = (val - 15)*10;
	return val;
}

/*****************************************************************************/

int size2value(int val)
{
	if (val >= 300) return (val/100)+33;
	if (val >= 20) return (val/10)+15;
	if (val >= 16) return 16;
	return val;
}
