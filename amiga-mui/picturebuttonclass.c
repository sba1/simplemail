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
 * @file picturebuttonclass.c
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <intuition/intuitionbase.h>
#include <libraries/mui.h>
#include <datatypes/pictureclass.h>

#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/utility.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/datatypes.h>
#include <proto/muimaster.h>

#include "support_indep.h"
#include "debug.h"

#include "amigasupport.h"
#include "compiler.h"
#include "datatypescache.h"
#include "muistuff.h"
#include "picturebuttonclass.h"
#include "support.h"

/**
 * Duplicate the given string but omit the underscore.
 *
 * @param str the string to be copied.
 * @return the duplicated string with memory being allocated via AllocVec().
 */
STATIC STRPTR StrNoUnderscoreCopy(STRPTR str)
{
	STRPTR buf = (STRPTR)AllocVec(strlen(str)+1,0);
	if (buf)
	{
		char c;
		STRPTR dest = buf;
		while(( c = *str++))
		{
			if (c!='_')
				*dest++ = c;
		}
		*dest = 0;
	}
	return buf;
}

static const UWORD GhostPattern[] =
{
	0x4444,
	0x1111,
};

#define SetAfPt(w,p,n)	{(w)->AreaPtrn = p;(w)->AreaPtSz = n;}

/* Apply a ghosting pattern to a given rectangle in a rastport */
STATIC VOID Ghost(struct RastPort *rp, UWORD pen, UWORD x0, UWORD y0, UWORD x1, UWORD y1)
{
	SetABPenDrMd(rp,pen,0,JAM1);
	SetAfPt(rp,(UWORD*)GhostPattern,1);
	RectFill(rp,x0,y0,x1,y1);
	SetAfPt(rp,NULL,0);
}

struct PictureButton_Data
{
	STRPTR name;
	STRPTR label;
	int free_vert;
	int show_label;
	struct dt_node *dt;
	int setup;

	int label_height;
};

/**
 * Load the picture from disk.
 *
 * @param data instance data
 * @param obj the actual picture button object
 */
STATIC VOID PictureButton_Load(struct PictureButton_Data *data, Object *obj)
{
	if (data->name)
	{
		data->dt = dt_load_picture(data->name, _screen(obj));
	}
}

/**
 * Unload the picture.
 *
 * @param data instance data
 */
STATIC VOID PictureButton_Unload(struct PictureButton_Data *data)
{
	if (data->dt)
	{
		dt_dispose_picture(data->dt);
		data->dt = NULL;
	}
}

/**
 * Implementation of OM_NEW
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG PictureButton_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct PictureButton_Data *data;
	char *name;
	char *directory;

/*	if (!(obj=(Object *)DoSuperNew(cl,obj,
					MUIA_Font, MUIV_Font_Tiny,
					TAG_MORE, msg->ops_AttrList)))
	        return 0;
*/
	if (!(obj = (Object*)DoSuperMethodA(cl,obj,(Msg)msg)))
		return 0;

	data = (struct PictureButton_Data*)INST_DATA(cl,obj);
	name = (char*)GetTagData(MUIA_PictureButton_Filename,0,msg->ops_AttrList);
	directory = (char*)GetTagData(MUIA_PictureButton_Directory,0,msg->ops_AttrList);

	if (directory && name)
	{
		int l = strlen(directory) + strlen(name) + 4;
		if ((data->name = (char*)malloc(l)))
		{
			strcpy(data->name,directory);
			sm_add_part(data->name,name,l);
		}
	} else
	{
		data->name = mystrdup(name);
	}
	/* We can live with a NULL data->name */

	data->label = (char *)GetTagData(MUIA_PictureButton_Label,0,msg->ops_AttrList);
	data->free_vert = (int)GetTagData(MUIA_PictureButton_FreeVert,1,msg->ops_AttrList);
	data->show_label = (int)GetTagData(MUIA_PictureButton_ShowLabel,1,msg->ops_AttrList);

	/* tell MUI not to care about filling our background during MUIM_Draw */
/*	set(obj,MUIA_FillArea,FALSE);*/

	return((ULONG)obj);
}

/**
 * Implementation of OM_DISPOSE
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG PictureButton_Dispose(struct IClass *cl,Object *obj,Msg msg)
{
	struct PictureButton_Data *data = (struct PictureButton_Data*)INST_DATA(cl,obj);
	free(data->name);
	return DoSuperMethodA(cl,obj,msg);
}

/**
 * Implementation of OM_SET
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG PictureButton_Set(struct IClass *cl,Object *obj, struct opSet *msg)
{
	struct PictureButton_Data *data = (struct PictureButton_Data*)INST_DATA(cl,obj);
	int relayout = 0;
	struct TagItem *ti;

	if ((ti = FindTagItem(MUIA_PictureButton_ShowLabel,msg->ops_AttrList)))
	{
		if (data->show_label != (int)ti->ti_Data)
		{
			data->show_label = (int)ti->ti_Data;
			relayout = 1;
		}
	}

	if ((ti = FindTagItem(MUIA_PictureButton_Filename,msg->ops_AttrList)))
	{
		if (data->name) free(data->name);
		data->name = mystrdup((char*)ti->ti_Data);
		relayout = 1;

		if (data->setup)
		{
			PictureButton_Unload(data);
			PictureButton_Load(data,obj);
			MUI_Redraw(obj,MADF_DRAWOBJECT);
		}
	}

	if (relayout && data->setup)
	{
		Object *parent;

		if ((parent = (Object*)xget(obj,MUIA_Parent)))
		{
			/* New size if needed */
			DoMethod(parent,MUIM_Group_InitChange);
			DoMethod(parent,MUIM_Group_ExitChange);
		}
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
STATIC ULONG PictureButton_Setup(struct IClass *cl,Object *obj,Msg msg)
{
	struct PictureButton_Data *data = (struct PictureButton_Data*)INST_DATA(cl,obj);

	if (!DoSuperMethodA(cl,obj,msg))
		return 0;

	PictureButton_Load(data,obj);

	data->setup = 1;
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
STATIC ULONG PictureButton_Cleanup(struct IClass *cl,Object *obj,Msg msg)
{
	struct PictureButton_Data *data = (struct PictureButton_Data*)INST_DATA(cl,obj);
	PictureButton_Unload(data);
	data->setup = 0;
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

/**
 * Implementation of MUIM_AskMinMax
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG PictureButton_AskMinMax(struct IClass *cl,Object *obj,struct MUIP_AskMinMax *msg)
{
	struct PictureButton_Data *data = (struct PictureButton_Data*)INST_DATA(cl,obj);
	struct MUI_MinMax *mi;

	int minwidth,minheight;

	DoSuperMethodA(cl,obj,(Msg)msg);

	mi = msg->MinMaxInfo;

	if (data->dt)
	{
		minwidth  = dt_width(data->dt);
		minheight = dt_height(data->dt);
	} else
	{
		minwidth = 0;
		minheight = 0;
	}
	data->label_height = 0;

	if (data->label && data->show_label)
	{
		struct RastPort rp;
		int width;
		char *str = data->label;
		char *uptr;

		data->label_height = _font(obj)->tf_YSize;

		minheight += data->label_height + (!!data->dt);
		InitRastPort(&rp);
		SetFont(&rp,_font(obj));

		if (!(uptr = strchr(str,'_')))
		{
			width = TextLength(&rp,str,strlen(str));
		} else
		{
			width = TextLength(&rp,str,uptr - str);
			width += TextLength(&rp,uptr+1,strlen(uptr+1));
		}
		if (width > minwidth) minwidth = width;
	}

	mi->MinHeight += minheight;
	mi->DefHeight += minheight;
	if (data->free_vert) mi->MaxHeight = MUI_MAXMAX;
	else mi->MaxHeight += minheight;
	mi->MinWidth += minwidth;
	mi->DefWidth += minwidth;
	mi->MaxWidth = MUI_MAXMAX;

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
STATIC ULONG PictureButton_Draw(struct IClass *cl,Object *obj,struct MUIP_Draw *msg)
{
	struct PictureButton_Data *data = (struct PictureButton_Data*)INST_DATA(cl,obj);
  LONG disabled = xget(obj,MUIA_Disabled);
  LONG rel_y = 0;
  struct RastPort *rp = _rp(obj);

	DoSuperMethodA(cl,obj,(Msg)msg);

	if (data->dt)
	{
		dt_put_on_rastport(data->dt,rp, _mleft(obj)+(_mwidth(obj) - dt_width(data->dt))/2, _mtop(obj) + (_mheight(obj) - data->label_height - dt_height(data->dt))/2);
		rel_y += dt_height(data->dt);
	}

	if (data->label && data->show_label)
	{
		STRPTR ufreestr = StrNoUnderscoreCopy(data->label);
		struct TextExtent te;
		LONG ufreelen;

		if (ufreestr)
		{
			LONG len = ufreelen = strlen(ufreestr);
			SetFont(rp,_font(obj));

			len = TextFit(rp,ufreestr,ufreelen, &te, NULL, 1, _mwidth(obj), _font(obj)->tf_YSize);
			if (len>0)
			{
				LONG left = _mleft(obj);
				left += (_mwidth(obj) - te.te_Width)/2;
				Move(rp,left,_mbottom(obj)+_font(obj)->tf_Baseline - data->label_height);
				SetAPen(rp,_dri(obj)->dri_Pens[TEXTPEN]);
				SetDrMd(rp,JAM1);

				{
					char *str = data->label;
					char *uptr;

					if (!(uptr = strchr(str,'_')))
					{
						Text(rp,str,strlen(str));
					} else
					{
						Text(rp,str,uptr - str);
						if (uptr[1])
						{
							SetSoftStyle(rp,FSF_UNDERLINED,AskSoftStyle(rp));
							Text(rp,uptr+1,1);
							SetSoftStyle(rp,FS_NORMAL,0xffff);
							Text(rp,uptr+2,strlen(uptr+2));
						}
					}
				}

			}
			FreeVec(ufreestr);
		}

		if (disabled) Ghost(rp, _dri(obj)->dri_Pens[TEXTPEN], _left(obj), _top(obj), _right(obj), _bottom(obj));
	}
	return 0;
}

/**
 * The Boopsi dispatcher for the picture button class.
 */
STATIC MY_BOOPSI_DISPATCHER(ULONG, PictureButton_Dispatcher, cl, obj, msg)
{
	switch (msg->MethodID)
	{
		case OM_NEW        : return PictureButton_New      (cl,obj,(struct opSet*)msg);
		case OM_DISPOSE    : return PictureButton_Dispose  (cl,obj,msg);
		case OM_SET        : return PictureButton_Set      (cl,obj,(struct opSet*)msg);
		case MUIM_Setup    : return PictureButton_Setup    (cl,obj,msg);
		case MUIM_Cleanup  : return PictureButton_Cleanup  (cl,obj,msg);
		case MUIM_AskMinMax: return PictureButton_AskMinMax(cl,obj,(struct MUIP_AskMinMax*)msg);
		case MUIM_Draw     : return PictureButton_Draw     (cl,obj,(struct MUIP_Draw*)msg);
	}

	return DoSuperMethodA(cl,obj,msg);
}

/*****************************************************************************/

struct MUI_CustomClass *CL_PictureButton;

/*****************************************************************************/

int create_picturebutton_class(void)
{
	SM_ENTER;
	if ((CL_PictureButton = CreateMCC(MUIC_Area, NULL, sizeof(struct PictureButton_Data), PictureButton_Dispatcher)))
	{
		SM_DEBUGF(15,("Create CL_PictureButton: 0x%lx\n",CL_PictureButton));
		SM_RETURN(TRUE,"%ld");
	}
	SM_DEBUGF(5,("FAILED! Create CL_PictureButton\n"));
	SM_RETURN(FALSE,"%ld");
}
/*****************************************************************************/

void delete_picturebutton_class(void)
{
	SM_ENTER;
	if (CL_PictureButton)
	{
		if (MUI_DeleteCustomClass(CL_PictureButton))
		{
			SM_DEBUGF(15,("Deleted CL_PictureButton: 0x%lx\n",CL_PictureButton));
			CL_PictureButton = NULL;
		} else
		{
			SM_DEBUGF(5,("FAILED! Delete CL_PictureButton: 0x%lx\n",CL_PictureButton));
		}
	}
	SM_LEAVE;
}

/*****************************************************************************/

Object *MakePictureButton(const char *label, const char *filename)
{
	int control_char = GetControlChar(label);

	return PictureButtonObject,
		ButtonFrame,
		MUIA_Background, MUII_ButtonBack,
		MUIA_Font, MUIV_Font_Tiny,
		control_char?MUIA_ControlChar:TAG_IGNORE, control_char,
		MUIA_InputMode, MUIV_InputMode_RelVerify,
		MUIA_PictureButton_Label,label,
		MUIA_PictureButton_Filename,filename,
		End;
}
