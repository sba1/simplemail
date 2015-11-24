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
 * @file iconclass.c
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <libraries/mui.h>
#include <workbench/workbench.h>
#include <workbench/icon.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/icon.h>
#include <proto/intuition.h>
#include <proto/wb.h>
#include <proto/layers.h>

#include "support_indep.h"
#include "debug.h"

#include "amigasupport.h"
#include "compiler.h"
#include "datatypesclass.h"
#include "iconclass.h"
#include "muistuff.h"

/**********************************************************************/

STATIC ULONG Icon_Set(struct IClass *cl,Object *obj,struct opSet *msg);

/**********************************************************************/

struct Icon_Data
{
	char *type;
	char *subtype;

	void *buffer;
	int buffer_len;

	struct DiskObject *obj;

	struct MUI_EventHandlerNode ehnode;
	ULONG select_secs,select_mics;

	char *drop_path;
};


/**
 * Implementation of OM_NEW
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG Icon_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	if (!(obj=(Object *)DoSuperNew(cl,obj,
/*					MUIA_Draggable, WorkbenchBase->lib_Version >= 45,*/
					TAG_MORE,msg->ops_AttrList)))
		return 0;

	Icon_Set(cl,obj,msg);
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
STATIC VOID Icon_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
	struct Icon_Data *data = (struct Icon_Data*)INST_DATA(cl,obj);
	if (data->buffer) FreeVec(data->buffer);
	if (data->type) FreeVec(data->type);
	if (data->subtype) FreeVec(data->subtype);
	if (data->drop_path) FreeVec(data->drop_path);

	DoSuperMethodA(cl,obj,msg);
}

/**
 * Implementation of OM_SET
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG Icon_Set(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct Icon_Data *data = (struct Icon_Data*)INST_DATA(cl,obj);
	struct TagItem *tstate, *tag;
	void *new_buffer = NULL;
	int new_buffer_len = 0;

	tstate = (struct TagItem *)msg->ops_AttrList;

	while ((tag = NextTagItem ((APTR)&tstate)))
	{
		ULONG tidata = tag->ti_Data;

		switch (tag->ti_Tag)
		{
			case	MUIA_Icon_MimeType:
						if (data->type) FreeVec(data->type);
						data->type = StrCopy((STRPTR)tidata);
						break;

			case	MUIA_Icon_MimeSubType:
						if (data->subtype) FreeVec(data->subtype);
						data->subtype = StrCopy((STRPTR)tidata);
						break;

			case	MUIA_Icon_Buffer:
						if (data->buffer) FreeVec(data->buffer);
						data->buffer = NULL;
						new_buffer = (void*)tidata;
						break;

			case	MUIA_Icon_BufferLen:
						new_buffer_len = tidata;
						break;

		}
	}
	if (msg->MethodID == OM_SET) return DoSuperMethodA(cl,obj,(Msg)msg);

	if (new_buffer_len && new_buffer)
	{
		if ((data->buffer = AllocVec(new_buffer_len,0x0)))
		{
			data->buffer_len = new_buffer_len;
			CopyMem(new_buffer,data->buffer,new_buffer_len);
		}
	}

	return 1;
}

/**
 * Implementation of OM_GET
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG Icon_Get(struct IClass *cl, Object *obj, struct opGet *msg)
{
	struct Icon_Data *data = (struct Icon_Data*)INST_DATA(cl,obj);
	if (msg->opg_AttrID == MUIA_Icon_DoubleClick)
	{
		*msg->opg_Storage = 1;
		return 1;
	}
  if (msg->opg_AttrID == MUIA_Icon_DropPath)
  {
		*msg->opg_Storage = (ULONG)data->drop_path;
		return 1;
  }
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

/**
 * Implementation of MUIM_Setup
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG Icon_Setup(struct IClass *cl, Object *obj, struct MUIP_Setup *msg)
{
	struct Icon_Data *data = (struct Icon_Data*)INST_DATA(cl,obj);

	if (!DoSuperMethodA(cl,obj,(Msg)msg)) return 0;

	if (IconBase->lib_Version >= 44 && data->buffer && FindPort("DEFICONS"))
	{
		/* If DefIcons is available, write the supplied buffer into a file
		 * and open the file's icon via icon.library. DefIcons delivers
		 * a proper icon in this case */

		BPTR fh;
		char filename[40];

		sprintf(filename,"T:SM_icon_%p",FindTask(NULL));
		if ((fh = Open(filename, MODE_NEWFILE)))
		{
			Write(fh,data->buffer,data->buffer_len);
			Close(fh);

			data->obj = GetIconTags(filename,
				ICONGETA_FailIfUnavailable,FALSE,
				ICONGETA_Screen, _screen(obj),
				TAG_DONE);

			DeleteFile(filename);
		}
	}

	if (!data->obj)
	{
		if (IconBase->lib_Version >= 44)
		{
			char *def;

			/* No icon yet, build a icon default name */
			if (!mystricmp(data->type, "image")) def = "picture";
			else if (!mystricmp(data->type, "audio")) def = "audio";
			else
			{
				if (!mystricmp(data->type, "text"))
				{
					if (!mystricmp(data->subtype, "html")) def = "html";
					else if (!mystricmp(data->subtype, "plain")) def = "ascii";
					else def = "text";
				} else def = "attach";
			}

			data->obj = GetIconTags(NULL,
					ICONGETA_GetDefaultName, def,
					ICONGETA_Screen, _screen(obj),
					TAG_DONE);

			if (!data->obj)
			{
				/* Still no icon, now use a project icon */
				data->obj = GetIconTags(NULL,
					ICONGETA_GetDefaultType, WBPROJECT,
					ICONGETA_Screen, _screen(obj),
					TAG_DONE);
			}
		} else
		{
			/* Old OS, we can only use a project icon */
			data->obj = GetDefDiskObject(WBPROJECT);
		}
	}

	data->ehnode.ehn_Priority = -1;
	data->ehnode.ehn_Flags    = 0;
	data->ehnode.ehn_Object   = obj;
	data->ehnode.ehn_Class    = cl;
	data->ehnode.ehn_Events   = IDCMP_MOUSEBUTTONS;

	DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&data->ehnode);

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
STATIC ULONG Icon_Cleanup(struct IClass *cl, Object *obj, Msg msg)
{
	struct Icon_Data *data = (struct Icon_Data*)INST_DATA(cl,obj);
	DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&data->ehnode);
	if (data->obj)
	{
		FreeDiskObject(data->obj);
		data->obj = NULL;
	}
	DoSuperMethodA(cl,obj,msg);
	return 0;
}

/**
 * Implementation of MUIM_AskMinMax
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG Icon_AskMinMax(struct IClass *cl,Object *obj, struct MUIP_AskMinMax *msg)
{
	int w,h;
	struct Icon_Data *data = (struct Icon_Data*)INST_DATA(cl,obj);
  DoSuperMethodA(cl, obj, (Msg) msg);

	if (data->obj)
	{
		if (IconBase->lib_Version >= 44)
		{
			struct Rectangle rect;
			GetIconRectangle(NULL, data->obj, NULL, &rect,
						ICONDRAWA_Borderless, TRUE,
						TAG_DONE);

			w = rect.MaxX - rect.MinX + 1;
			h = rect.MaxY - rect.MinY + 1;
		} else
		{
			w = ((struct Image*)data->obj->do_Gadget.GadgetRender)->Width;
			h = ((struct Image*)data->obj->do_Gadget.GadgetRender)->Height;
		}
	} else
	{
		w = 30;
		h = 30;
	}

  msg->MinMaxInfo->MinWidth += w;
  msg->MinMaxInfo->DefWidth += w;
  msg->MinMaxInfo->MaxWidth += w;

  msg->MinMaxInfo->MinHeight += h;
  msg->MinMaxInfo->DefHeight += h;
  msg->MinMaxInfo->MaxHeight += h;
  return 0;
}

/**
 * Implementation of MUIM_Draw
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG Icon_Draw(struct IClass *cl, Object *obj, struct MUIP_Draw *msg)
{
	struct Icon_Data *data = (struct Icon_Data*)INST_DATA(cl,obj);
	DoSuperMethodA(cl,obj,(Msg)msg);

	if (data->obj)
	{
		int sel = xget(obj,MUIA_Selected);
		if (IconBase->lib_Version >= 44)
		{
			DrawIconState(_rp(obj),data->obj, NULL, _mleft(obj), _mtop(obj), sel?IDS_SELECTED:IDS_NORMAL,
#ifdef ICONDRAWA_Transparency
					ICONDRAWA_Transparency, 255,
#endif
					TAG_DONE);
		} else
		{
			if (sel && data->obj->do_Gadget.Flags & GFLG_GADGHIMAGE)
				DrawImage(_rp(obj),((struct Image*)data->obj->do_Gadget.SelectRender),_mleft(obj),_mtop(obj));
			else
				DrawImage(_rp(obj),((struct Image*)data->obj->do_Gadget.GadgetRender),_mleft(obj),_mtop(obj));
		}
	}

	return 1;
}

/**
 * Implementation of MUIM_HandleEvent
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG Icon_HandleEvent(struct IClass *cl, Object *obj, struct MUIP_HandleEvent *msg)
{
	struct Icon_Data *data = (struct Icon_Data*)INST_DATA(cl,obj);
	if (msg->imsg && msg->imsg->Class == IDCMP_MOUSEBUTTONS)
	{
		if (!(_isinobject(obj,msg->imsg->MouseX,msg->imsg->MouseY))) return 0;

		if (msg->imsg->Code == SELECTDOWN)
		{
			ULONG secs, mics;
			CurrentTime(&secs,&mics);

			set(obj,MUIA_Selected, TRUE);

			if (DoubleClick(data->select_secs,data->select_mics, secs, mics))
			{
				set(obj,MUIA_Icon_DoubleClick, TRUE);
			}
			data->select_secs = secs;
			data->select_mics = mics;

			if (WorkbenchBase->lib_Version >= 45)
			{
				DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&data->ehnode);
				data->ehnode.ehn_Events |= IDCMP_MOUSEMOVE;
				DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&data->ehnode);
			}

			return MUI_EventHandlerRC_Eat;
		}

		if (msg->imsg->Code == SELECTUP)
		{
			if (WorkbenchBase->lib_Version >= 45)
			{
				DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&data->ehnode);
				data->ehnode.ehn_Events &= ~IDCMP_MOUSEMOVE;
				DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&data->ehnode);
			}
			return MUI_EventHandlerRC_Eat;
		}

	}

	if (msg->imsg->Class == IDCMP_MOUSEMOVE)
	{
		DoMethod(obj, MUIM_DoDrag, msg->imsg->MouseX - _mleft(obj), msg->imsg->MouseY - _mtop(obj));
	}
	return 0;
}

struct Selection_Msg
{
	struct Layer *l;
	LONG mx,my;

  char *drawer;
	char *dest_name;
	int finish;
};

#ifndef __AMIGAOS4__
/***********************************************************
 The selection hook functions. It checks whether the
 mouse is placed on an icon
************************************************************/
STATIC ASM SAVEDS ULONG selection_func(REG(a0,struct Hook *h), REG(a2, Object *obj), REG(a1,struct IconSelectMsg *ism))
{
	struct Selection_Msg *msg = (struct Selection_Msg *)h->h_Data;
	struct Window *wnd = ism->ism_ParentWindow;
	if (!wnd) return ISMACTION_Stop;
	if (wnd->WLayer != msg->l) return ISMACTION_Stop;
  msg->finish = 1;

  if ((ism->ism_Left + wnd->LeftEdge <= msg->mx) && (msg->mx <= wnd->LeftEdge + ism->ism_Left + ism->ism_Width - 1) &&
      (ism->ism_Top + wnd->TopEdge <= msg->my) && (msg->my <= wnd->TopEdge + ism->ism_Top + ism->ism_Height - 1))
	{
		if (ism->ism_Type == WBDRAWER)
		{
			msg->dest_name = StrCopy(ism->ism_Name);
		} else if (ism->ism_Type == WBDISK)
		{
			if ((msg->dest_name = AllocVec(strlen(ism->ism_Name)+2,0)))
			{
				strcpy(msg->dest_name,ism->ism_Name);
				strcat(msg->dest_name,":");
			}
		}
		return ISMACTION_Stop;
	}

	return ISMACTION_Ignore;
}
#endif

/**
 * Implementation of MUIM_DeleteDragImage
 *
 * This method is issued, to delete the image data when the
 * dragged object is released (done by Area class in this
 * case). We use it to determine a possible workbench drawer.
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG Icon_DeleteDragImage(struct IClass *cl, Object *obj, Msg msg)
{
	struct Icon_Data *data = (struct Icon_Data*)INST_DATA(cl,obj);
#ifdef __AMIGAOS4__
	ULONG which;
	char *buf;

	if (data->drop_path)
	{
		FreeVec(data->drop_path);
		data->drop_path = NULL;
	}

	if ((buf = (STRPTR)AllocVec(2048,MEMF_SHARED)))
	{
		char name[256];
		ULONG type = ~0;

		struct TagItem ti[6] =
		{
				{WBOBJA_DrawerPath, (ULONG)buf},
				{WBOBJA_DrawerPathSize, 2048},
				{WBOBJA_Name, (ULONG)name},
				{WBOBJA_NameSize, (ULONG)sizeof(name)},
				{WBOBJA_Type, (ULONG)&type},
				{TAG_DONE}
		};

		buf[0] = 0;

		/* Note that we use WhichWorkbenchObjectA() and not WhichWorkbenchObject()
		 * because the latter wasn't implemented in workbench.library < 51.9 */
		which = WhichWorkbenchObjectA(NULL, _screen(obj)->MouseX,_screen(obj)->MouseY,ti);

		if (which == WBO_ICON)
		{
			if (type == WBDRAWER) AddPart(buf,name,2048);
			else if (type == WBDISK)
			{
				mystrlcpy(buf,name,2047);
				strcat(buf,":");
			}
			which = WBO_DRAWER;
		}
		if (which == WBO_DRAWER && buf[0])
		{
			if ((data->drop_path = StrCopy(buf)))
			{
				DoMethod(_app(obj),MUIM_Application_PushMethod, obj, 3, MUIM_Set, MUIA_Icon_DropPath, data->drop_path);
			}
		}
		FreeVec(buf);
	}
#else
	if (WorkbenchBase->lib_Version >= 45)
	{
		struct Layer *l = WhichLayer(&_screen(obj)->LayerInfo,_screen(obj)->MouseX,_screen(obj)->MouseY);
		if (l)
		{
			struct List *path_list;
			if (WorkbenchControl(NULL,WBCTRLA_GetOpenDrawerList, &path_list, TAG_DONE))
			{
				struct Hook hook;
				struct Selection_Msg sel_msg;
		    struct Node *n;

        sel_msg.l = l;
        sel_msg.mx = _screen(obj)->MouseX;
        sel_msg.my = _screen(obj)->MouseY;
        sel_msg.dest_name = NULL;
        sel_msg.finish = 0;

				init_hook_with_data(&hook, (HOOKFUNC)selection_func, &sel_msg);

				if (data->drop_path)
				{
					FreeVec(data->drop_path);
					data->drop_path = NULL;
				}

		    for (n = path_list->lh_Head; n->ln_Succ; n = n->ln_Succ)
		    {
					if ((sel_msg.drawer = (char*)StrCopy(n->ln_Name)))
					{
						ChangeWorkbenchSelectionA(n->ln_Name, (struct Hook*)&hook, NULL);
						if (sel_msg.finish)
						{
							if (!sel_msg.dest_name)
							{
								data->drop_path = sel_msg.drawer;
								sel_msg.drawer = NULL;
							} else
							{
								int len = strlen(sel_msg.dest_name) + strlen(sel_msg.drawer) + 10;

								if ((data->drop_path = (char*)AllocVec(len,0)))
								{
									strcpy(data->drop_path,sel_msg.drawer);
									AddPart(data->drop_path,sel_msg.dest_name,len);
								}
								FreeVec(sel_msg.dest_name);
							}
						}
						if (sel_msg.drawer) FreeVec(sel_msg.drawer);
						if (sel_msg.finish) break;
					}
		    }

				WorkbenchControl(NULL,WBCTRLA_FreeOpenDrawerList, path_list, TAG_DONE);

				if (!sel_msg.finish)
				{
					sel_msg.drawer = NULL;
					ChangeWorkbenchSelectionA(NULL, (struct Hook*)&hook, NULL);
					if (sel_msg.finish && sel_msg.dest_name)
						data->drop_path = sel_msg.dest_name;
				}

				if (data->drop_path)
				{
					DoMethod(_app(obj),MUIM_Application_PushMethod, (ULONG)obj, 3, MUIM_Set, MUIA_Icon_DropPath, (ULONG)data->drop_path);
				}
			}
		}
	}
#endif

	DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&data->ehnode);
	data->ehnode.ehn_Events &= ~IDCMP_MOUSEMOVE;
	DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&data->ehnode);

	return DoSuperMethodA(cl,obj,(Msg)msg);
}

/**********************************************************************/

STATIC MY_BOOPSI_DISPATCHER(ULONG, Icon_Dispatcher, cl, obj, msg)
{
	switch(msg->MethodID)
	{
		case	OM_NEW:				return Icon_New(cl,obj,(struct opSet*)msg);
		case  OM_DISPOSE:		Icon_Dispose(cl,obj,msg); return 0;
		case  OM_SET:				return Icon_Set(cl,obj,(struct opSet*)msg);
		case	OM_GET:				return Icon_Get(cl,obj,(struct opGet*)msg);
		case  MUIM_AskMinMax: return Icon_AskMinMax(cl,obj,(struct MUIP_AskMinMax *)msg);
		case	MUIM_Setup:		return Icon_Setup(cl,obj,(struct MUIP_Setup*)msg);
		case	MUIM_Cleanup:	return Icon_Cleanup(cl,obj,msg);
		case	MUIM_Draw:			return Icon_Draw(cl,obj,(struct MUIP_Draw*)msg);
		case	MUIM_HandleEvent: return Icon_HandleEvent(cl,obj,(struct MUIP_HandleEvent*)msg);
		case  MUIM_DeleteDragImage: return Icon_DeleteDragImage(cl,obj,msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

/*****************************************************************************/

struct MUI_CustomClass *CL_Icon;

/*****************************************************************************/

int create_icon_class(void)
{
	SM_ENTER;
	if ((CL_Icon = CreateMCC(MUIC_Area,NULL,sizeof(struct Icon_Data),Icon_Dispatcher)))
	{
		SM_DEBUGF(15,("Create CL_Icon: 0x%lx\n",CL_Icon));
		SM_RETURN(1,"%ld");
	}
	SM_DEBUGF(5,("FAILED! Create CL_Icon\n"));
	SM_RETURN(0,"%ld");
}

void delete_icon_class(void)
{
	SM_ENTER;
	if (CL_Icon)
	{
		if (MUI_DeleteCustomClass(CL_Icon))
		{
			SM_DEBUGF(15,("Deleted CL_Icon: 0x%lx\n",CL_Icon));
			CL_Icon = NULL;
		} else
		{
			SM_DEBUGF(5,("FAILED! Delete CL_Icon: 0x%lx\n",CL_Icon));
		}
	}
	SM_LEAVE;
}
