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
** composeeditorclass.c
*/

#include <dos.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <libraries/mui.h>
#include <mui/nlisttree_mcc.h>
#include <mui/texteditor_mcc.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>
#include <proto/graphics.h>

#include "codesets.h"
#include "mail.h"
#include "support.h"

#include "addressbook.h"
#include "addresstreelistclass.h"
#include "amigasupport.h"
#include "composeeditorclass.h"
#include "compiler.h"
#include "mailtreelistclass.h"
#include "muistuff.h"
#include "parse.h"

struct ComposeEditor_Data
{
	LONG cmap[8];
	char **array;
};

STATIC ULONG ComposeEditor_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct ComposeEditor_Data *data;

	if (!(obj=(Object *)DoSuperNew(cl,obj,
					TAG_MORE,msg->ops_AttrList)))
		return 0;

	data = (struct ComposeEditor_Data*)INST_DATA(cl,obj);

	set(obj,MUIA_TextEditor_ColorMap, data->cmap);

	return (ULONG)obj;
}

STATIC ULONG ComposeEditor_Set(struct IClass *cl, Object *obj, struct opSet *msg)
{
	struct ComposeEditor_Data *data = (struct ComposeEditor_Data*)INST_DATA(cl,obj);
	struct TagItem *tstate, *tag;

	tstate = (struct TagItem *)msg->ops_AttrList;

	while (tag = NextTagItem(&tstate))
	{
		switch (tag->ti_Tag)
		{
			case	MUIA_ComposeEditor_Array:
						{
							char **array = (char**)tag->ti_Data;
							char *buf;
							int i,cnt = 0, len = 0;

							if (!array) set(obj,MUIA_TextEditor_Contents,"");
							else
							{
								while (array[cnt])
								{
									len += strlen(array[cnt])+1; /* for the \n */
									cnt++;
								}
								if ((buf = AllocVec(len+2,0)))
								{
									buf[0] = 0;
									for (i=0;i<cnt;i++)
									{
										strcat(buf,array[i]);
										strcat(buf,"\n");
									}
									set(obj,MUIA_TextEditor_Contents,buf);
									FreeVec(buf);
								}
							}
						}
						break;

			case	MUIA_ComposeEditor_UTF8Contents:
						break;
		}
	}
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

/*
STATIC ULONG ComposeEditor_Get(struct IClass *cl, Object *obj, struct opGet *msg)
{
	struct ComposeEditor_Data *data = (struct ComposeEditor_Data*)INST_DATA(cl,obj);

	if (msg->opg_AttrID == MUIA_ComposeEditor_EMailArray)
	{
		return 1;
	}

	return DoSuperMethodA(cl,obj,(Msg)msg);
}
*/

STATIC ULONG ComposeEditor_Setup(struct IClass *cl, Object *obj, struct MUIP_Setup *msg)
{
	struct ComposeEditor_Data *data = (struct ComposeEditor_Data*)INST_DATA(cl,obj);
	ULONG retval = DoSuperMethodA(cl,obj,(Msg)msg);

	if (retval)
	{
/*		data->cmap[0] = ObtainBestPenA(_screen(obj)->ViewPort.ColorMap,0xffffffff,0xffffffff,0xffffffff,NULL);*/
		data->cmap[7] = ObtainBestPenA(_screen(obj)->ViewPort.ColorMap,0xffffffff,0xffffffff,0x80000000,NULL);
	} else
	{
		DoSuperMethod(cl,obj,MUIM_Cleanup);
		return 0;
	}
	return retval;
}

STATIC ULONG ComposeEditor_Cleanup(struct IClass *cl, Object *obj, Msg msg)
{
	struct ComposeEditor_Data *data = (struct ComposeEditor_Data*)INST_DATA(cl,obj);

/*	ReleasePen(_screen(obj)->ViewPort.ColorMap,data->cmap[0]);*/
	ReleasePen(_screen(obj)->ViewPort.ColorMap,data->cmap[7]);

	return DoSuperMethodA(cl,obj,(Msg)msg);
}

/*STATIC ULONG ComposeEditor_InsertText(struct IClass *cl, Object *obj, struct MUIP_TextEditor_InsertText *msg)
{
	return DoSuperMethodA(cl,obj,(Msg)msg);
}*/


STATIC ULONG ComposeEditor_DragQuery(struct IClass *cl, Object *obj, struct MUIP_DragQuery *msg)
{
	if (OCLASS(msg->obj) == CL_MailTreelist->mcc_Class) return MUIV_DragQuery_Accept;
	if (OCLASS(msg->obj) == CL_AddressTreelist->mcc_Class) return MUIV_DragQuery_Accept;
	return MUIV_DragQuery_Refuse;
}

STATIC ULONG ComposeEditor_DragDrop(struct IClass *cl, Object *obj, struct MUIP_DragDrop *msg)
{
	if (OCLASS(msg->obj) == CL_AddressTreelist->mcc_Class)
	{
		struct MUI_NListtree_TreeNode *treenode = (struct MUI_NListtree_TreeNode*)xget(msg->obj,MUIA_NListtree_Active);
		if (treenode)
		{
			struct addressbook_entry *entry = (struct addressbook_entry *)treenode->tn_User;
			if (entry->u.person.emails && entry->u.person.emails[0])
			{
				DoMethod(obj,MUIM_TextEditor_InsertText,entry->u.person.emails[0],MUIV_TextEditor_InsertText_Cursor);
			}
		}
	} else if (OCLASS(msg->obj) == CL_MailTreelist->mcc_Class)
	{
		struct mail *mail = (struct mail*)xget(msg->obj,MUIA_MailTree_Active);
		if (mail)
		{
			char *from = mail_get_from_address(mail);
			if (from)
			{
				DoMethod(obj,MUIM_TextEditor_InsertText,from,MUIV_TextEditor_InsertText_Cursor);
				free(from);
			}
		}
	}
	return 0;
}


STATIC ASM ULONG ComposeEditor_Dispatcher(register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	putreg(REG_A4,cl->cl_UserData);
	switch(msg->MethodID)
	{
		case	OM_NEW: return ComposeEditor_New(cl,obj,(struct opSet*)msg);
		case	OM_SET: return ComposeEditor_Set(cl,obj,(struct opSet*)msg);

/*
		case	OM_GET: return ComposeEditor_Get(cl,obj,(struct opGet*)msg);
*/
		case	MUIM_Setup: return ComposeEditor_Setup(cl,obj,(struct MUIP_Setup*)msg);
		case	MUIM_Cleanup: return ComposeEditor_Cleanup(cl,obj,msg);

		case	MUIM_DragQuery: return ComposeEditor_DragQuery(cl, obj, (struct MUIP_DragQuery *)msg);
		case	MUIM_DragDrop: return ComposeEditor_DragDrop(cl, obj, (struct MUIP_DragDrop *)msg);

/*		case	MUIM_TextEditor_InsertText: return ComposeEditor_InsertText(cl,obj,(struct MUIP_TextEditor_InsertText*)msg);*/
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

struct MUI_CustomClass *CL_ComposeEditor;

int create_composeeditor_class(void)
{
	if ((CL_ComposeEditor = MUI_CreateCustomClass(NULL,MUIC_TextEditor,NULL,sizeof(struct ComposeEditor_Data),ComposeEditor_Dispatcher)))
	{
		CL_ComposeEditor->mcc_Class->cl_UserData = getreg(REG_A4);
		return 1;
	}
	return 0;
}

void delete_composeeditor_class(void)
{
	if (CL_ComposeEditor) MUI_DeleteCustomClass(CL_ComposeEditor);
}
