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
** picturebuttonclass.c
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dos.h>

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

#include "amigasupport.h"
#include "compiler.h"
#include "datatypescache.h"
#include "muistuff.h"
#include "picturebuttonclass.h"

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
	SetAfPt(rp,GhostPattern,1);
	RectFill(rp,x0,y0,x1,y1);
	SetAfPt(rp,NULL,0);
}

struct PictureButton_Data
{
	STRPTR name;
	STRPTR label;
	Object *dto;
	struct BitMap *bitmap;
	struct BitMapHeader *bmhd;
};


STATIC ULONG PictureButton_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct PictureButton_Data *data;

	if (!(obj=(Object *)DoSuperNew(cl,obj,
					ButtonFrame,
					MUIA_InputMode, MUIV_InputMode_RelVerify,
					MUIA_Font, MUIV_Font_Tiny,
					TAG_MORE, msg->ops_AttrList)))
	        return 0;

	data = (struct PictureButton_Data*)INST_DATA(cl,obj);

	data->name = (char *)GetTagData(MUIA_PictureButton_Filename,NULL,msg->ops_AttrList);
	data->label = (char *)GetTagData(MUIA_PictureButton_Label,NULL,msg->ops_AttrList);

	/* tell MUI not to care about filling our background during MUIM_Draw */
/*	set(obj,MUIA_FillArea,FALSE);*/

	return((ULONG)obj);
}

STATIC ULONG PictureButton_Setup(struct IClass *cl,Object *obj,Msg msg)
{
	struct PictureButton_Data *data = (struct PictureButton_Data*)INST_DATA(cl,obj);
	
	if (!DoSuperMethodA(cl,obj,msg))
		return(FALSE);
	
	if (data->name)
	{
		if (data->dto = dt_load_picture(data->name, _screen(obj)))
		{
			GetDTAttrs(data->dto,PDTA_BitMapHeader,&data->bmhd,TAG_DONE);
			if (data->bmhd)
			{
				GetDTAttrs(data->dto,PDTA_DestBitMap,&data->bitmap,TAG_DONE);
				if (!data->bitmap)
					GetDTAttrs(data->dto,PDTA_BitMap,&data->bitmap,TAG_DONE);
			}
		}
	}
	return 1;
}

STATIC ULONG PictureButton_Cleanup(struct IClass *cl,Object *obj,Msg msg)
{
	struct PictureButton_Data *data = (struct PictureButton_Data*)INST_DATA(cl,obj);
	data->bitmap = NULL;
	data->bmhd = NULL;
 
	if (data->dto)
	{
		dt_dispose_object(data->dto);
		data->dto = NULL;
	}
 
	return 0;
}

STATIC ULONG PictureButton_AskMinMax(struct IClass *cl,Object *obj,struct MUIP_AskMinMax *msg)
{
	struct PictureButton_Data *data = (struct PictureButton_Data*)INST_DATA(cl,obj);
	struct MUI_MinMax *mi;

	int minwidth,minheight;

	DoSuperMethodA(cl,obj,(Msg)msg);

	mi = msg->MinMaxInfo;

	if (data->bitmap)
	{
		minwidth  = data->bmhd->bmh_Width;
		minheight = data->bmhd->bmh_Height;
	} else
	{
		minwidth = 0;
		minheight = 0;
	}

	if (data->label)
	{
		struct RastPort rp;
		int width;
		char *str = data->label;
		char *uptr;

		minheight += _font(obj)->tf_YSize + (!!data->bitmap);
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
	mi->MaxHeight += minheight;
	mi->MinWidth += minwidth;
	mi->DefWidth += minwidth;
	mi->MaxWidth = MUI_MAXMAX;
	return 0;
}

STATIC ULONG PictureButton_Draw(struct IClass *cl,Object *obj,struct MUIP_Draw *msg)
{
	struct PictureButton_Data *data = (struct PictureButton_Data*)INST_DATA(cl,obj);
  LONG disabled = xget(obj,MUIA_Disabled);
  struct RastPort *rp = _rp(obj);

	DoSuperMethodA(cl,obj,(Msg)msg);
 
//	if (msg->flags & MADF_DRAWOBJECT)
	{
		LONG rel_y=0;
		if (data->bitmap)
		{
			APTR mask=NULL;

			GetDTAttrs(data->dto,PDTA_MaskPlane,&mask,TAG_DONE);
			if(mask)
			{
				MyBltMaskBitMapRastPort(data->bitmap,0,0,rp,_mleft(obj)+(_mwidth(obj) - data->bmhd->bmh_Width)/2,_mtop(obj),data->bmhd->bmh_Width,data->bmhd->bmh_Height,0xe2,(PLANEPTR)mask);
			} else BltBitMapRastPort(data->bitmap,0,0,rp,_mleft(obj)+(_mwidth(obj) - data->bmhd->bmh_Width)/2,_mtop(obj),data->bmhd->bmh_Width,data->bmhd->bmh_Height,0xc0);
			rel_y += data->bmhd->bmh_Height;
		}

		if (data->label)
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
					Move(rp,left,_mtop(obj)+_font(obj)->tf_Baseline+rel_y);
					SetAPen(rp,_dri(obj)->dri_Pens[TEXTPEN]);

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
								SetSoftStyle(rp,FS_NORMAL,AskSoftStyle(rp));
								Text(rp,uptr+2,strlen(uptr+2));
							}
						}
					}

				}
				FreeVec(ufreestr);
			}

			if (disabled) Ghost(rp, _dri(obj)->dri_Pens[TEXTPEN], _left(obj), _top(obj), _right(obj), _bottom(obj));
		}

	}

	return 0;
}

ASM STATIC ULONG PictureButton_Dispatcher(register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	putreg(REG_A4,cl->cl_UserData);
	switch (msg->MethodID)
	{
		case OM_NEW        : return(PictureButton_New      (cl,obj,(struct opSet*)msg));
		case MUIM_Setup    : return(PictureButton_Setup    (cl,obj,msg));
		case MUIM_Cleanup  : return(PictureButton_Cleanup  (cl,obj,msg));
		case MUIM_AskMinMax: return(PictureButton_AskMinMax(cl,obj,(struct MUIP_AskMinMax*)msg));
		case MUIM_Draw     : return(PictureButton_Draw     (cl,obj,(struct MUIP_Draw*)msg));
	}
	
	return(DoSuperMethodA(cl,obj,msg));
}

struct MUI_CustomClass *CL_PictureButton;

int create_picturebutton_class(void)
{
	if ((CL_PictureButton = MUI_CreateCustomClass(NULL, MUIC_Area, NULL, sizeof(struct PictureButton_Data), PictureButton_Dispatcher)))
	{
		CL_PictureButton->mcc_Class->cl_UserData = getreg(REG_A4);
		return TRUE;
	}
	return FALSE;
}

void delete_picturebutton_class(void)
{
	if (CL_PictureButton)
	{
		MUI_DeleteCustomClass(CL_PictureButton);
		CL_PictureButton = NULL;
	}
}

Object *MakePictureButton(char *label, char *filename)
{
	int control_char;
	char *buf = strchr(label,'_');
	if (buf) control_char = ToLower(*(buf+1));
	else control_char = 0;

	return PictureButtonObject,
		MUIA_CycleChain, 1,
		control_char?MUIA_ControlChar:TAG_IGNORE, control_char,
		MUIA_PictureButton_Label,label,
		MUIA_PictureButton_Filename,filename,
		End;
}

