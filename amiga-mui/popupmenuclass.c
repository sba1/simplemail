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
** popupmenuclass.c
*/


#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <libraries/mui.h>
#include <mui/NListview_MCC.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>
#include <proto/graphics.h>

#include "lists.h"
#include "mail.h"
#include "support.h"

#include "amigasupport.h"
#include "compiler.h"
#include "muistuff.h"
#include "popupmenuclass.h"

struct popup_node
{
	struct node node;
	char *name;
	void *udata;
};

struct Popupmenu_Data
{
	LONG show;
	struct MUI_EventHandlerNode mb_handler;
	struct MUI_EventHandlerNode mv_handler;

	struct Window *popup_window;
	int selected;
	struct list popup_list;
};

STATIC VOID Popupmenu_DrawEntry(struct IClass *cl, Object *obj, LONG entry, LONG selected)
{
	struct Popupmenu_Data *data = (struct Popupmenu_Data*)INST_DATA(cl,obj);
	struct RastPort *rp;
	LONG y = entry * _font(obj)->tf_YSize + 2;
	LONG pen1,pen2;
	struct popup_node *node;

	if (entry < 0) return;
	if (!data->popup_window) return;

	rp = data->popup_window->RPort;

	if (!selected)
	{
		pen1 = _dri(obj)->dri_Pens[BARBLOCKPEN];
		pen2 = _dri(obj)->dri_Pens[BARDETAILPEN];
	} else
	{
		pen2 = _dri(obj)->dri_Pens[BARBLOCKPEN];
		pen1 = _dri(obj)->dri_Pens[BARDETAILPEN];
	}


	node = (struct popup_node *)list_find(&data->popup_list,entry);

	SetDrMd(rp,JAM1);
	SetAPen(rp,pen1);
	RectFill(rp,2,y,data->popup_window->Width-3,y+_font(obj)->tf_YSize-1);
	SetAPen(rp,pen2);
	Move(rp, 3, y + _font(obj)->tf_Baseline);
	Text(rp, node->name, strlen(node->name));
}

STATIC VOID Popupmenu_OpenWindow(struct IClass *cl,Object *obj)
{
	struct Popupmenu_Data *data = (struct Popupmenu_Data*)INST_DATA(cl,obj);
	LONG x,y,width,height;
	struct Window *wnd;
	struct RastPort rp;
	int i;
	struct popup_node *node = (struct popup_node*)list_first(&data->popup_list);

	if (data->popup_window) CloseWindow(data->popup_window);

	x = _left(obj) + _window(obj)->LeftEdge + _width(obj)/2;
	y = _top(obj) + _window(obj)->TopEdge;
	height = _font(obj)->tf_YSize * list_length(&data->popup_list) + 4;
	width = 0;

	InitRastPort(&rp);
	SetFont(&rp,_font(obj));

	while (node)
	{
		LONG nw = TextLength(&rp,node->name,strlen(node->name));
		if (width < nw) width = nw;
		node = (struct popup_node*)node_next(&node->node);
	}

	width += 8;

	x -= width / 2;
	if (x<0) x=0;

	if ((wnd = data->popup_window = OpenWindowTags( NULL,
				WA_CustomScreen, _screen(obj),
				WA_Borderless, TRUE,
				WA_Left, x,
				WA_Top, y,
				WA_Height,height,
				WA_Width,width,
				WA_BackFill, LAYERS_NOBACKFILL,
				TAG_DONE)))
	{
		SetAPen(wnd->RPort,_dri(obj)->dri_Pens[BARBLOCKPEN]);
		RectFill(wnd->RPort,0,0,wnd->Width-1,wnd->Height-1);
		SetAPen(wnd->RPort,_dri(obj)->dri_Pens[BARDETAILPEN]);
		Move(wnd->RPort,0,0);
		Draw(wnd->RPort,wnd->Width-1,0);
		Draw(wnd->RPort,wnd->Width-1,wnd->Height-1);
		Draw(wnd->RPort,0,wnd->Height-1);
		Draw(wnd->RPort,0,1);
		SetFont(wnd->RPort,_font(obj));

		for (i=0;i<list_length(&data->popup_list);i++)
		{
			Popupmenu_DrawEntry(cl,obj,i,0);
		}
		data->selected = -1;
	}
}

STATIC VOID Popupmenu_CloseWindow(struct IClass *cl, Object *obj)
{
	struct Popupmenu_Data *data = (struct Popupmenu_Data*)INST_DATA(cl,obj);
	if (data->popup_window)
	{
		CloseWindow(data->popup_window);
		data->popup_window = NULL;
	}
}

STATIC ULONG Popupmenu_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct Popupmenu_Data *data;

	if (!(obj=(Object *)DoSuperNew(cl,obj,
					TAG_MORE,msg->ops_AttrList)))
		return 0;

	data = (struct Popupmenu_Data*)INST_DATA(cl,obj);

	data->mb_handler.ehn_Priority = 1;
	data->mb_handler.ehn_Flags    = 0;
	data->mb_handler.ehn_Object   = obj;
	data->mb_handler.ehn_Class    = cl;
	data->mb_handler.ehn_Events   = IDCMP_MOUSEBUTTONS;

	data->mv_handler.ehn_Priority = 100;
	data->mv_handler.ehn_Flags    = 0;
	data->mv_handler.ehn_Object   = obj;
	data->mv_handler.ehn_Class    = cl;
	data->mv_handler.ehn_Events   = IDCMP_MOUSEMOVE|IDCMP_MENUVERIFY;

	list_init(&data->popup_list);

	return (ULONG)obj;
}

STATIC ULONG Popupmenu_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
	Popupmenu_CloseWindow(cl,obj);
	DoMethod(obj,MUIM_Popupmenu_Clear);
	return DoSuperMethodA(cl,obj,msg);
}

STATIC ULONG Popupmenu_Get(struct IClass *cl, Object *obj, struct opGet *msg)
{
	struct Popupmenu_Data *data = (struct Popupmenu_Data*)INST_DATA(cl,obj);
	if (msg->opg_AttrID == MUIA_Popupmenu_Selected)
	{
		*msg->opg_Storage = data->selected;
		return 1;
	} else if (msg->opg_AttrID == MUIA_Popupmenu_SelectedData)
	{
		struct popup_node *node = (struct popup_node*)list_find(&data->popup_list,data->selected);
		if (node) *msg->opg_Storage = (ULONG)node->udata;
		else *msg->opg_Storage = NULL;
		return 1;
	}
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG Popupmenu_Setup(struct IClass *cl, Object *obj, struct MUIP_Setup *msg)
{
	ULONG retval = DoSuperMethodA(cl,obj,(Msg)msg);
	return retval;
}

STATIC ULONG Popupmenu_Cleanup(struct IClass *cl, Object *obj, Msg msg)
{
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG Popupmenu_Show(struct IClass *cl, Object *obj, struct MUIP_Show *msg)
{
	struct Popupmenu_Data *data = (struct Popupmenu_Data*)INST_DATA(cl,obj);
	if (!DoSuperMethodA(cl,obj,(Msg)msg)) return 0;
	data->show = 1;
	DoMethod(_win(obj), MUIM_Window_AddEventHandler, &data->mb_handler);
	return 1;
}

STATIC ULONG Popupmenu_Hide(struct IClass *cl, Object *obj, Msg msg)
{
	struct Popupmenu_Data *data = (struct Popupmenu_Data*)INST_DATA(cl,obj);
	data->show = 0;
	DoMethod(_win(obj), MUIM_Window_RemEventHandler, &data->mb_handler);
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG Popupmenu_HandleEvent(struct IClass *cl, Object *obj, struct MUIP_HandleEvent *msg)
{
	struct IntuiMessage *imsg;
	struct Popupmenu_Data *data = (struct Popupmenu_Data*)INST_DATA(cl,obj);
	if ((imsg = msg->imsg))
	{
		LONG x = imsg->MouseX;
		LONG y = imsg->MouseY;

		if (imsg->Class == IDCMP_MOUSEBUTTONS)
		{
			if (imsg->Code == SELECTDOWN)
			{
				if (x < _left(obj) || x > _right(obj) || y < _top(obj) || y > _bottom(obj))
					return 0;

				set(obj, MUIA_Selected, TRUE);
				if (data->show)
				{
					DoMethod(_win(obj), MUIM_Window_AddEventHandler, &data->mv_handler);
					Popupmenu_OpenWindow(cl,obj);
				}
				return MUI_EventHandlerRC_Eat;
			}

			if (xget(obj,MUIA_Selected) && (imsg->Code == SELECTUP || imsg->Code == MENUDOWN))
			{
				set(obj, MUIA_Selected, FALSE);
				if (data->show)
				{
					DoMethod(_win(obj), MUIM_Window_RemEventHandler, &data->mv_handler);
					Popupmenu_CloseWindow(cl,obj);
				}

				if (data->selected != -1)
					set(obj,MUIA_Popupmenu_Selected,data->selected);

				return MUI_EventHandlerRC_Eat;
			}
		}

		if (imsg->Class == IDCMP_MENUVERIFY && xget(obj,MUIA_Selected))
		{
			set(obj, MUIA_Selected, FALSE);
			if (data->show)
			{
				DoMethod(_win(obj), MUIM_Window_RemEventHandler, &data->mv_handler);
				Popupmenu_CloseWindow(cl,obj);
			}		
			return MUI_EventHandlerRC_Eat;
		}

		if (imsg->Class == IDCMP_MOUSEMOVE)
		{
			if (xget(obj,MUIA_Selected))
			{
				LONG newselected = (data->popup_window->MouseY - 2)/_font(obj)->tf_YSize;
				int max_entries = list_length(&data->popup_list);

				if (newselected >= max_entries) newselected = -1;
				if (newselected < 0) newselected = -1;
				if (data->popup_window->MouseX < 0) newselected = -1;
				if (data->popup_window->MouseX >= data->popup_window->Width) newselected = -1;

				if (newselected != data->selected)
				{
					Popupmenu_DrawEntry(cl, obj, data->selected, 0);
					data->selected = newselected;
					Popupmenu_DrawEntry(cl, obj, data->selected, 1);
				}
			}
		}
	}
	return 0;
}

STATIC ULONG Popupmenu_Clear(struct IClass *cl, Object *obj, Msg msg)
{
	struct Popupmenu_Data *data = (struct Popupmenu_Data*)INST_DATA(cl,obj);
	struct popup_node *node;

	while ((node = (struct popup_node*)list_remove_tail(&data->popup_list)))
	{
		if (node->name) FreeVec(node->name);
		FreeVec(node);
	}

	return 1;
}

STATIC ULONG Popupmenu_AddEntry(struct IClass *cl, Object *obj,struct MUIP_Popupmenu_AddEntry *msg)
{
	struct Popupmenu_Data *data = (struct Popupmenu_Data*)INST_DATA(cl,obj);
	struct popup_node *node;

	if (!msg->Entry) return 0;

	if ((node = (struct popup_node*)AllocVec(sizeof(struct popup_node),0)))
	{
		char *name = (char*)AllocVec(strlen(msg->Entry)+1,0);
		if (name)
		{
			strcpy(name,msg->Entry);
			node->name = name;
			node->udata = msg->UData;
			list_insert_tail(&data->popup_list,&node->node);
			return 1;
		}
		FreeVec(node);
	}
	return 0;
}

STATIC BOOPSI_DISPATCHER(ULONG, Popupmenu_Dispatcher,cl,obj,msg)
{
	switch(msg->MethodID)
	{
		case	OM_NEW: return Popupmenu_New(cl,obj,(struct opSet*)msg);
		case	OM_DISPOSE: return Popupmenu_Dispose(cl, obj, msg);
		case	OM_GET: return Popupmenu_Get(cl,obj,(struct opGet*)msg);
		case	MUIM_Setup: return Popupmenu_Setup(cl,obj,(struct MUIP_Setup*)msg);
		case	MUIM_Cleanup: return Popupmenu_Cleanup(cl,obj,msg);
		case	MUIM_Show: return Popupmenu_Show(cl,obj,(struct MUIP_Show*)msg);
		case	MUIM_Hide: return Popupmenu_Hide(cl,obj,msg);
		case	MUIM_HandleEvent: return Popupmenu_HandleEvent(cl,obj,(struct MUIP_HandleEvent*)msg);
		case	MUIM_Popupmenu_Clear: return Popupmenu_Clear(cl,obj,msg);
		case	MUIM_Popupmenu_AddEntry: return Popupmenu_AddEntry(cl,obj,(struct MUIP_Popupmenu_AddEntry*)msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

struct MUI_CustomClass *CL_Popupmenu;

int create_popupmenu_class(void)
{
	if ((CL_Popupmenu = CreateMCC(MUIC_Image,NULL,sizeof(struct Popupmenu_Data),Popupmenu_Dispatcher)))
		return 1;
	return 0;
}

void delete_popupmenu_class(void)
{
	if (CL_Popupmenu) MUI_DeleteCustomClass(CL_Popupmenu);
}
