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
** mailinfoclass.c
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <libraries/mui.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>

#include "codesets.h"
#include "configuration.h"
#include "debug.h"
#include "lists.h"
#include "mail.h"
#include "parse.h"
#include "simplemail.h"
#include "smintl.h"
#include "support.h"
#include "support_indep.h"

#include "amigasupport.h"
#include "compiler.h"
#include "mailinfoclass.h"
#include "muistuff.h"

/**************************************************************************/

#define BORDER						2		/* border */
#define CONTENTS_OFFSET 	10	/* offset from field column to text column */

/**************************************************************************/

struct text_node
{
	struct node node;

	char *text;
	char *link;

	int x_start;
	int x_end;
};

struct field
{
	struct node node;
	int clickable;

	char *name;

	struct list text_list;
};

/**************************************************************************/

struct MailInfoArea_Data
{
	int setup;
	struct MUI_EventHandlerNode mb_handler;
	struct MUI_EventHandlerNode mv_handler;

	struct mail_info *mail;
	struct list field_list;

	struct field *selected_field;
	struct text_node *selected_text;
	int selected_mouse_over;

	int background_pen;
	int text_pen;
	int link_pen;

	int fieldname_width;
	int entries;
};

/********************************************************************
 Free a complete field and its memory
*********************************************************************/
static void field_free(struct field *f)
{
	struct text_node *t;

	while ((t = (struct text_node*)list_remove_tail(&f->text_list)))
	{
		free(t->text);
		free(t->link);
		free(t);
	}
	free(f);
}

/********************************************************************
 Add a new text field into the given field list
*********************************************************************/
static struct field *field_add_text(struct list *list, char *name, char *text)
{
	struct field *f;
	struct text_node *t;

	if (!(f = malloc(sizeof(*f)))) return 0;
	if (!(t = malloc(sizeof(*t))))
	{
		free(f);
		return 0;
	}

	memset(f,0,sizeof(*f));
	f->name = utf8tostrcreate(name,user.config.default_codeset);
	f->clickable = 0;

	memset(t,0,sizeof(*t));
	list_init(&f->text_list);
	t->text = mystrdup(text);
	list_insert_tail(&f->text_list,&t->node);

	list_insert_tail(list,&f->node);

	return f;
}

/********************************************************************
 Add a new address field into the given field list
*********************************************************************/
static struct field *field_add_addresses(struct list *list, char *name, struct list *addr_list)
{
	struct field *f;
	struct address *addr;

	if (!(f = malloc(sizeof(*f)))) return 0;

	memset(f,0,sizeof(*f));
	f->name = mystrdup(name);
	f->clickable = 1;

	list_init(&f->text_list);

	addr = (struct address*)list_first(addr_list);
	while (addr)
	{
		char buf[256];
		struct text_node *t = (struct text_node*)malloc(sizeof(*t));
		if (!t) goto out;

		if (addr->realname)
			sm_snprintf(buf, sizeof(buf), "%s <%s>",addr->realname,addr->email);
		else
		{
			if (addr->email) mystrlcpy(buf,addr->email,sizeof(buf));
			else buf[0] = 0;
		}

		if (!(t->text = utf8tostrcreate(buf,user.config.default_codeset)))
			goto out;

		if (!(t->link = mystrdup(addr->email)))
			goto out;

		list_insert_tail(&f->text_list,&t->node);

		addr = (struct address*)node_next(&addr->node);
	}

	list_insert_tail(list,&f->node);

	return f;
out:
	field_free(f);
	return 0;
}

/********************************************************************
 Finds the text node on the given position
*********************************************************************/
static void field_find(struct list *field_list, int x, int y, int fonty, struct field **field_ptr, struct text_node **text_ptr)
{
	struct text_node *text;
	struct field *f;
	
	*field_ptr = NULL;
	*text_ptr = NULL;

	/* "find" correct line */
	f = (struct field *)list_first(field_list);
	while (f && y >= fonty)
	{
		y -= fonty;
		f = (struct field*)node_next(&f->node);
	}

	if (!f) return;

	text = (struct text_node*)list_first(&f->text_list);
	while (text)
	{
		if (x >= text->x_start && x <= text->x_end)
		{
			*field_ptr = f;
			*text_ptr = text;
			return;
		}
		text = (struct text_node*)node_next(&text->node);
	}
}

/********************************************************************
 Determine the space requirements of the current field list (if
 possible)
*********************************************************************/
STATIC VOID MailInfoArea_DetermineSizes(Object *obj, struct MailInfoArea_Data *data)
{
	struct field *f;
	struct RastPort rp;

	int x,comma_width;
	int field_width = 0;
	int entries = 0;

	if (!data->setup)
	{
		data->entries = list_length(&data->field_list);
		return;
	}

	InitRastPort(&rp);
	SetFont(&rp,_font(obj));

	comma_width = TextLength(&rp,",",1);

	/* 1st pass, determine sizes of the field name column */
	f = (struct field *)list_first(&data->field_list);
	while (f)
	{
		int new_field_width = TextLength(&rp,f->name,strlen(f->name));
		if (new_field_width > field_width) field_width = new_field_width;
		f = (struct field*)node_next(&f->node);
		entries++;
	}

	data->fieldname_width = field_width + CONTENTS_OFFSET;

	/* 2nd pass, determine the link positions */
	f = (struct field *)list_first(&data->field_list);
	while (f)
	{
		struct text_node *text;

		x = BORDER + data->fieldname_width;
		text = (struct text_node*)list_first(&f->text_list);
		while (text)
		{
			if (text->text)
			{
				text->x_start = x;
				x += TextLength(_rp(obj),text->text,strlen(text->text));
				text->x_end = x - 1;
				x += comma_width;
			}
			text = (struct text_node*)node_next(&text->node);
		}
		f = (struct field*)node_next(&f->node);
	}

	/* "Resize" the object if required */
	if (data->entries != entries)
	{
		Object *group;
		
		group = (Object*)xget(obj, MUIA_Parent);

		DoMethod(group, MUIM_Group_InitChange);
		DoMethod(group, OM_REMMEMBER, obj);
		data->entries = entries;
		DoMethod(group, OM_ADDMEMBER, obj);
		DoMethod(group, MUIM_Group_ExitChange);
	}
}

/********************************************************************
 Sets a new mail info
*********************************************************************/
VOID MailInfoArea_SetMailInfo(Object *obj, struct MailInfoArea_Data *data, struct mail_info *mi)
{
	data->selected_field = NULL;
	data->selected_text = NULL;

	if (data->mail) mail_dereference(data->mail);
	if ((data->mail = mi))
	{
		struct field *f;
		char buf[300];

		mail_reference(data->mail);

		while ((f = (struct field*)list_remove_tail(&data->field_list)))
			field_free(f);

		if (user.config.header_flags & (SHOW_HEADER_FROM))
		{
			if (mi->from_phrase)
				sm_snprintf(buf, sizeof(buf), "%s <%s>",mi->from_phrase,mi->from_addr);
			else
			{
				if (mi->from_addr) mystrlcpy(buf,mi->from_addr,sizeof(buf));
				else buf[0] = 0;
			}

			if ((f = field_add_text(&data->field_list,_("From"),buf)))
			{
				struct text_node *t;

				f->clickable = 1;

				if ((t = (struct text_node*)list_first(&f->text_list)))
					t->link = mystrdup(mi->from_addr);
			}
		}

		if (user.config.header_flags & (SHOW_HEADER_TO))
			field_add_addresses(&data->field_list, _("To"), mi->to_list);

		if ((user.config.header_flags & (SHOW_HEADER_CC)) && mi->cc_list)
			field_add_addresses(&data->field_list, _("Copies to"), mi->cc_list);

		if (user.config.header_flags & (SHOW_HEADER_SUBJECT))
			field_add_text(&data->field_list,_("Subject"),mi->subject);

		if (user.config.header_flags & (SHOW_HEADER_DATE))
			field_add_text(&data->field_list,_("Date"),sm_get_date_long_str(mi->seconds));
	}

	MailInfoArea_DetermineSizes(obj, data);
}

/********************************************************************
 Draw a given field at the given y position (relativ to the object)
*********************************************************************/
STATIC VOID MailInfoArea_DrawField(Object *obj, struct MailInfoArea_Data *data, struct field *f, int y)
{
	int ytext = y + _mtop(obj) + _font(obj)->tf_Baseline;
	struct text_node *text;
	int cnt;

	struct TextExtent te;

	int space_left;
	int comma_width;

	comma_width = TextLength(_rp(obj),",",1);

	SetAPen(_rp(obj), data->text_pen);
	SetFont(_rp(obj), _font(obj));
	Move(_rp(obj),_mleft(obj)+2,ytext);

	cnt = TextFit(_rp(obj),f->name,strlen(f->name),&te,NULL,1,_mwidth(obj),_font(obj)->tf_YSize);
	if (!cnt) return;
	Text(_rp(obj),f->name,cnt);

	space_left = _mwidth(obj) - data->fieldname_width - 2 * BORDER;
	if (space_left <= 0) return;

	Move(_rp(obj),_mleft(obj) + data->fieldname_width, ytext);

	text = (struct text_node*)list_first(&f->text_list);
	while (text)
	{
		struct text_node *next_text;

		next_text = (struct text_node*)node_next(&text->node);

		if (text->text)
		{
			cnt = TextFit(_rp(obj),text->text,strlen(text->text),&te,NULL,1,space_left,_font(obj)->tf_YSize);
			if (!cnt) break;

			if (f->clickable)
			{
				if (f == data->selected_field && data->selected_mouse_over)
					SetAPen(_rp(obj),_dri(obj)->dri_Pens[HIGHLIGHTTEXTPEN]);
				else SetAPen(_rp(obj),data->link_pen);
			}
			Text(_rp(obj),text->text,cnt);
			space_left -= te.te_Width;
			if (next_text)
			{
				if (space_left >= comma_width)
				{
					if (f->clickable) SetAPen(_rp(obj),data->text_pen);
					Text(_rp(obj),",",1);
					space_left -= comma_width;
				} else break;
			}
		}

		text = next_text;
	}
}

/*************************************************************************/

/********************************************************************
 OM_NEW
*********************************************************************/
STATIC ULONG MailInfoArea_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct MailInfoArea_Data *data;

	if (!(obj=(Object *)DoSuperNew(cl,obj,
					TAG_MORE,msg->ops_AttrList)))
		return 0;

	data = (struct MailInfoArea_Data*)INST_DATA(cl,obj);

	list_init(&data->field_list);

	data->mb_handler.ehn_Priority = 1;
	data->mb_handler.ehn_Flags    = 0;
	data->mb_handler.ehn_Object   = obj;
	data->mb_handler.ehn_Class    = cl;
	data->mb_handler.ehn_Events   = IDCMP_MOUSEBUTTONS;

	data->mv_handler.ehn_Priority = 1;
	data->mv_handler.ehn_Flags    = 0;
	data->mv_handler.ehn_Object   = obj;
	data->mv_handler.ehn_Class    = cl;
	data->mv_handler.ehn_Events   = IDCMP_MOUSEMOVE;

	set(obj, MUIA_FillArea, FALSE);

	return (ULONG)obj;
}

/********************************************************************
 OM_DISPOSE
*********************************************************************/
STATIC ULONG MailInfoArea_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
	struct MailInfoArea_Data *data;
	struct field *f;

	data = (struct MailInfoArea_Data*)INST_DATA(cl,obj);
	if (data->mail) mail_dereference(data->mail);

	while ((f = (struct field*)list_remove_tail(&data->field_list)))
		field_free(f);

	return DoSuperMethodA(cl,obj,msg);
}

/********************************************************************
 OM_SET
*********************************************************************/
STATIC ULONG MailInfoArea_Set(struct IClass *cl, Object *obj, struct opSet *msg)
{
	struct MailInfoArea_Data *data = (struct MailInfoArea_Data*)INST_DATA(cl,obj);
	struct TagItem *tstate, *tag;
	tstate = (struct TagItem *)msg->ops_AttrList;

	while ((tag = NextTagItem (&tstate)))
	{
		ULONG tidata = tag->ti_Data;

		switch (tag->ti_Tag)
		{
			case	MUIA_MailInfo_MailInfo:
						MailInfoArea_SetMailInfo(obj,data,(struct mail_info*)tidata);
						MUI_Redraw(obj,MADF_DRAWOBJECT);
						break;
		}
	}

	return DoSuperMethodA(cl,obj,(Msg)msg);
}

/********************************************************************
 MUIM_Setup
*********************************************************************/
STATIC LONG MailInfoArea_Setup(struct IClass *cl, Object *obj, struct MUIP_Setup *msg)
{
	struct MailInfoArea_Data *data = (struct MailInfoArea_Data*)INST_DATA(cl,obj);
	ULONG rc;

	rc = DoSuperMethodA(cl,obj,(Msg)msg);
	if (!rc) return 0;

	data->setup = 1;
	data->background_pen = ObtainBestPenA(_screen(obj)->ViewPort.ColorMap,
				MAKECOLOR32((user.config.read_header_background & 0xff0000) >> 16),
				MAKECOLOR32((user.config.read_header_background & 0xff00) >> 8),
				MAKECOLOR32((user.config.read_header_background & 0xff)), NULL);

	data->link_pen  = ObtainBestPenA(_screen(obj)->ViewPort.ColorMap,
				MAKECOLOR32((user.config.read_link & 0xff0000) >> 16),
				MAKECOLOR32((user.config.read_link & 0xff00) >> 8),
				MAKECOLOR32((user.config.read_link & 0xff)), NULL);

	data->text_pen  = ObtainBestPenA(_screen(obj)->ViewPort.ColorMap,
				MAKECOLOR32((user.config.read_text & 0xff0000) >> 16),
				MAKECOLOR32((user.config.read_text & 0xff00) >> 8),
				MAKECOLOR32((user.config.read_text & 0xff)), NULL);

	DoMethod(_win(obj), MUIM_Window_AddEventHandler, &data->mb_handler);

	return rc;
}

/********************************************************************
 MUIM_Cleanup
*********************************************************************/
STATIC LONG MailInfoArea_Cleanup(struct IClass *cl, Object *obj, Msg msg)
{
	struct MailInfoArea_Data *data = (struct MailInfoArea_Data*)INST_DATA(cl,obj);
	DoMethod(_win(obj), MUIM_Window_RemEventHandler, &data->mb_handler);
	ReleasePen(_screen(obj)->ViewPort.ColorMap,data->text_pen);
	ReleasePen(_screen(obj)->ViewPort.ColorMap,data->link_pen);
	ReleasePen(_screen(obj)->ViewPort.ColorMap,data->background_pen);
	data->setup = 0;
	return DoSuperMethodA(cl,obj,(Msg)msg);;
}

/********************************************************************
 MUIM_AskMinMax
*********************************************************************/
STATIC LONG MailInfoArea_AskMinMax(struct IClass *cl, Object *obj, struct MUIP_AskMinMax *msg)
{
	struct MailInfoArea_Data *data = (struct MailInfoArea_Data*)INST_DATA(cl,obj);
	int entries;

  DoSuperMethodA(cl, obj, (Msg) msg);

	entries = data->entries;

  msg->MinMaxInfo->MinWidth += 10;
  msg->MinMaxInfo->DefWidth += 200;
  msg->MinMaxInfo->MaxWidth = MUI_MAXMAX;

  msg->MinMaxInfo->MinHeight += entries * _font(obj)->tf_YSize + 2*BORDER;
  msg->MinMaxInfo->DefWidth  += entries * _font(obj)->tf_YSize + 2*BORDER;
  msg->MinMaxInfo->MaxHeight += entries * _font(obj)->tf_YSize + 2*BORDER;

  return 0;
}

/********************************************************************
 MUIM_Draw
*********************************************************************/
STATIC LONG MailInfoArea_Draw(struct IClass *cl, Object *obj, struct MUIP_Draw *msg)
{	
	struct field *f;
	struct MailInfoArea_Data *data;
	int y = 2;

	data = (struct MailInfoArea_Data*)INST_DATA(cl,obj);
	DoSuperMethodA(cl,obj,(Msg)msg);

	SetAPen(_rp(obj), data->background_pen);
	SetDrMd(_rp(obj), JAM1);
	RectFill(_rp(obj), _mleft(obj), _mtop(obj), _mright(obj), _mbottom(obj));

	f = (struct field *)list_first(&data->field_list);
	while (f)
	{
		MailInfoArea_DrawField(obj, data, f, y);

		y += _font(obj)->tf_YSize;
		f = (struct field*)node_next(&f->node);
	}

	return 0;
}

/********************************************************************
 MUIM_HandleEvent
*********************************************************************/
STATIC LONG MailInfoArea_HandleEvent(struct IClass *cl, Object *obj, struct MUIP_HandleEvent *msg)
{	
	struct IntuiMessage *imsg;
	struct MailInfoArea_Data *data;

	data = (struct MailInfoArea_Data*)INST_DATA(cl,obj);

	if ((imsg = msg->imsg))
	{
		int x = imsg->MouseX;
		int y = imsg->MouseY;

		struct field *f;
		struct text_node *t;

		if (imsg->Class == IDCMP_MOUSEBUTTONS)
		{
			if (x < _mleft(obj) || x > _mright(obj) || y < _mtop(obj) || y > _mbottom(obj))
				return 0;

			/* normalize positions */
			x -= _mleft(obj);
			y -= _mtop(obj);

			if (imsg->Code == SELECTDOWN)
			{
				field_find(&data->field_list, x, y, _font(obj)->tf_YSize, &f, &t);

				if (f && t)
				{
					DoMethod(_win(obj), MUIM_Window_AddEventHandler, &data->mv_handler);

					data->selected_field = f;
					data->selected_text = t;
					data->selected_mouse_over = 1;
					MUI_Redraw(obj, MADF_DRAWOBJECT);
					return MUI_EventHandlerRC_Eat;
				}
				return 0;
			}
			if (imsg->Code == SELECTUP)
			{
				if (data->selected_text)
				{
					field_find(&data->field_list, x, y, _font(obj)->tf_YSize, &f, &t);

					/* TODO: Instead calling this directly, this should issue a notify */
					if (t && t==data->selected_text) callback_write_mail_to_str(t->link,NULL);

					DoMethod(_win(obj), MUIM_Window_RemEventHandler, &data->mv_handler);

					data->selected_field = NULL;
					data->selected_text = NULL;
					data->selected_mouse_over = 0;
					MUI_Redraw(obj, MADF_DRAWOBJECT);
					return MUI_EventHandlerRC_Eat;
				}
			}
		} else if (imsg->Class == IDCMP_MOUSEMOVE)
		{
			if (x < _mleft(obj) || x > _mright(obj) || y < _mtop(obj) || y > _mbottom(obj))
			{
				if (data->selected_mouse_over)
				{
					data->selected_mouse_over = 0;
					MUI_Redraw(obj, MADF_DRAWOBJECT);
				}
				return MUI_EventHandlerRC_Eat;
			}

			/* normalize positions */
			x -= _mleft(obj);
			y -= _mtop(obj);

			field_find(&data->field_list, x, y, _font(obj)->tf_YSize, &f, &t);

			if (t && t == data->selected_text)
			{
				if (!data->selected_mouse_over)
				{
					data->selected_mouse_over = 1;
					MUI_Redraw(obj, MADF_DRAWOBJECT);
				}
			} else
			{
				if (data->selected_mouse_over)
				{
					data->selected_mouse_over = 0;
					MUI_Redraw(obj, MADF_DRAWOBJECT);				
				}
			}
		}
	}

	return 0;
}

STATIC BOOPSI_DISPATCHER(ULONG, MailInfoArea_Dispatcher, cl, obj, msg)
{
	switch(msg->MethodID)
	{
		case	OM_NEW: return MailInfoArea_New(cl,obj,(struct opSet*)msg);
		case	OM_DISPOSE: return MailInfoArea_Dispose(cl,obj,msg);
		case	OM_SET: return MailInfoArea_Set(cl,obj,(struct opSet*)msg);
		case	MUIM_Setup: return MailInfoArea_Setup(cl,obj,(struct MUIP_Setup*)msg);
		case	MUIM_Cleanup: return MailInfoArea_Cleanup(cl,obj,msg);
		case	MUIM_AskMinMax: return MailInfoArea_AskMinMax(cl,obj,(struct MUIP_AskMinMax*)msg);
		case	MUIM_Draw: return MailInfoArea_Draw(cl,obj,(struct MUIP_Draw*)msg);
		case	MUIM_HandleEvent: return MailInfoArea_HandleEvent(cl,obj,(struct MUIP_HandleEvent*)msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

/*************************************************************************/

struct MailInfo_Data
{
	Object *mailinfo;
};

/*************************************************************************
 OM_NEW
**************************************************************************/
STATIC ULONG MailInfo_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct MailInfo_Data *data;
	Object *mailinfo;
	extern struct MUI_CustomClass *CL_MailInfoArea;

	if (!(obj=(Object *)DoSuperNew(cl,obj,
					MUIA_Group_Child, mailinfo = MyNewObject(CL_MailInfoArea->mcc_Class, NULL, TAG_DONE),
					TAG_MORE,msg->ops_AttrList)))
		return 0;

	data = (struct MailInfo_Data*)INST_DATA(cl,obj);
	data->mailinfo = mailinfo;

	return (ULONG)obj;
}


STATIC BOOPSI_DISPATCHER(ULONG, MailInfo_Dispatcher, cl, obj, msg)
{
	switch(msg->MethodID)
	{
		case	OM_NEW: return MailInfo_New(cl,obj,(struct opSet*)msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}


/*************************************************************************/

struct MUI_CustomClass *CL_MailInfo;
struct MUI_CustomClass *CL_MailInfoArea;


int create_mailinfo_class(void)
{
	SM_ENTER;
	if ((CL_MailInfoArea = CreateMCC(MUIC_Area, NULL, sizeof(struct MailInfoArea_Data), MailInfoArea_Dispatcher)))
	{
		SM_DEBUGF(15,("Create CL_MailInfoArea: 0x%lx\n",CL_MailInfoArea));

		if ((CL_MailInfo = CreateMCC(MUIC_Group, NULL, sizeof(struct MailInfo_Data), MailInfo_Dispatcher)))
		{
			SM_RETURN(1,"%ld");
		}
	}
	SM_DEBUGF(5,("FAILED! Create CL_MailInfoArea\n"));
	SM_RETURN(0,"%ld");
}

void delete_mailinfo_class(void)
{
	SM_ENTER;
	if (CL_MailInfo)
	{
		if (MUI_DeleteCustomClass(CL_MailInfo))
		{
			SM_DEBUGF(15,("Deleted CL_MailInfoArea: 0x%lx\n",CL_MailInfo));
			CL_MailInfo = NULL;
		} else
		{
			SM_DEBUGF(5,("FAILED! Delete CL_MailInfoArea: 0x%lx\n",CL_MailInfo));
		}
	}
	if (CL_MailInfoArea)
	{
		if (MUI_DeleteCustomClass(CL_MailInfoArea))
		{
			SM_DEBUGF(15,("Deleted CL_MailInfoArea: 0x%lx\n",CL_MailInfoArea));
			CL_MailInfoArea = NULL;
		} else
		{
			SM_DEBUGF(5,("FAILED! Delete CL_MailInfoArea: 0x%lx\n",CL_MailInfoArea));
		}
	}
	SM_LEAVE;
}
