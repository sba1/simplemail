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

#include "configuration.h"
#include "debug.h"
#include "lists.h"
#include "mail.h"
#include "parse.h"
#include "smintl.h"
#include "support.h"
#include "support_indep.h"

#include "amigasupport.h"
#include "compiler.h"
#include "mailinfoclass.h"
#include "muistuff.h"

struct text_node
{
	struct node node;

	char *text;
};

struct field
{
	struct node node;
	int clickable;
	char *name;

	struct list text_list;
};

struct MailInfoArea_Data
{
	int setup;

	struct mail_info *mail;
	struct list field_list;

	int background_pen;

	int fieldname_width;
	int entries;
};

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

	memset(f,0,sizeof(f));
	f->name = mystrdup(name);

	list_init(&f->text_list);
	t->text = mystrdup(text);
	list_insert_tail(&f->text_list,&t->node);

	list_insert_tail(list,&f->node);

	return f;
}

static struct field *field_add_addresses(struct list *list, char *name, struct list *addr_list)
{
	struct field *f;
	struct address *addr;

	if (!(f = malloc(sizeof(*f)))) return 0;

	memset(f,0,sizeof(f));
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

		if (!(t->text = mystrdup(buf)))
			goto out;

		list_insert_tail(&f->text_list,&t->node);

		addr = (struct address*)node_next(&addr->node);
	}

	list_insert_tail(list,&f->node);

	return f;
out:
	free(f);
	return 0;
}

STATIC VOID MailInfoArea_DetermineSizes(Object *obj, struct MailInfoArea_Data *data)
{
	struct field *f;
	struct RastPort rp;

	int field_width = 0;
	int entries = 0;

	if (!data->setup)
	{
		data->entries = list_length(&data->field_list);
		return;
	}

	InitRastPort(&rp);
	SetFont(&rp,_font(obj));

	f = (struct field *)list_first(&data->field_list);
	while (f)
	{
		int new_field_width = TextLength(&rp,f->name,strlen(f->name));
		if (new_field_width > field_width) field_width = new_field_width;
		f = (struct field*)node_next(&f->node);
		entries++;
	}

	data->fieldname_width = field_width + 10;

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

VOID MailInfoArea_SetMailInfo(Object *obj, struct MailInfoArea_Data *data, struct mail_info *mi)
{
	if (data->mail) mail_dereference(data->mail);
	if ((data->mail = mi))
	{
		struct field *f;
		char buf[300];

		mail_reference(data->mail);

		list_init(&data->field_list);

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
				f->clickable = 1;
		}

		if (user.config.header_flags & (SHOW_HEADER_TO))
		{
			field_add_addresses(&data->field_list, _("To"), mi->to_list);
		}

		if ((user.config.header_flags & (SHOW_HEADER_CC)) && mi->cc_list)
		{
			field_add_addresses(&data->field_list, _("Copies to"), mi->cc_list);
		}

		if (user.config.header_flags & (SHOW_HEADER_SUBJECT))
		{
			field_add_text(&data->field_list,_("Subject"),mi->subject);
		}

		if (user.config.header_flags & (SHOW_HEADER_DATE))
		{
			field_add_text(&data->field_list,_("Date"),sm_get_date_long_str(mi->seconds));
		}
	}

	MailInfoArea_DetermineSizes(obj, data);
}

STATIC VOID MailInfoArea_DrawField(Object *obj, struct MailInfoArea_Data *data, struct field *f, int y)
{
	int ytext = y + _mtop(obj) + _font(obj)->tf_Baseline;
	struct text_node *text;

	int space_left;
	int comma_width;

	comma_width = TextLength(_rp(obj),",",1);

	SetAPen(_rp(obj), _dri(obj)->dri_Pens[TEXTPEN]);
	SetFont(_rp(obj), _font(obj));
	Move(_rp(obj),_mleft(obj)+2,ytext);
	Text(_rp(obj),f->name,strlen(f->name));

	Move(_rp(obj),_mleft(obj) + data->fieldname_width, ytext);

	space_left = _mwidth(obj) - data->fieldname_width - 4;

	text = (struct text_node*)list_first(&f->text_list);
	while (text)
	{
		struct text_node *next_text;

		next_text = (struct text_node*)node_next(&text->node);

		if (text->text)
		{
			int cnt;
			struct TextExtent te;

			cnt = TextFit(_rp(obj),text->text,strlen(text->text),&te,NULL,1,space_left,_font(obj)->tf_YSize);
			if (!cnt) break;

			Text(_rp(obj),text->text,cnt);
			space_left -= te.te_Width;
			if (next_text)
			{
				if (space_left >= comma_width)
				{
					Text(_rp(obj),",",1);
					space_left -= comma_width;
				} else break;
			}
		}

		text = next_text;
	}
}

/*************************************************************************/

/*************************************************************************
 OM_NEW
**************************************************************************/
STATIC ULONG MailInfoArea_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct MailInfoArea_Data *data;

	if (!(obj=(Object *)DoSuperNew(cl,obj,
					TAG_MORE,msg->ops_AttrList)))
		return 0;

	data = (struct MailInfoArea_Data*)INST_DATA(cl,obj);

	list_init(&data->field_list);

	set(obj, MUIA_FillArea, FALSE);

	return (ULONG)obj;
}

/*************************************************************************
 OM_DISPOSE
**************************************************************************/
STATIC ULONG MailInfoArea_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
	struct MailInfoArea_Data *data = (struct MailInfoArea_Data*)INST_DATA(cl,obj);
	if (data->mail) mail_dereference(data->mail);
	return DoSuperMethodA(cl,obj,msg);
}

/*************************************************************************
 OM_SET
**************************************************************************/
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

/*************************************************************************
 MUIM_Setup
**************************************************************************/
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

	return rc;
}

/*************************************************************************
 MUIM_Cleanup
**************************************************************************/
STATIC LONG MailInfoArea_Cleanup(struct IClass *cl, Object *obj, Msg msg)
{
	struct MailInfoArea_Data *data = (struct MailInfoArea_Data*)INST_DATA(cl,obj);
	ReleasePen(_screen(obj)->ViewPort.ColorMap,data->background_pen);
	data->setup = 0;
	return DoSuperMethodA(cl,obj,(Msg)msg);;
}

/*************************************************************************
 MUIM_AskMinMax
**************************************************************************/
STATIC LONG MailInfoArea_AskMinMax(struct IClass *cl, Object *obj, struct MUIP_AskMinMax *msg)
{
	struct MailInfoArea_Data *data = (struct MailInfoArea_Data*)INST_DATA(cl,obj);
	int entries;

  DoSuperMethodA(cl, obj, (Msg) msg);

	entries = data->entries;

  msg->MinMaxInfo->MinWidth += 10;
  msg->MinMaxInfo->DefWidth += 20;
  msg->MinMaxInfo->MaxWidth = MUI_MAXMAX;

  msg->MinMaxInfo->MinHeight += entries * _font(obj)->tf_YSize + 4;
  msg->MinMaxInfo->DefWidth  += entries * _font(obj)->tf_YSize + 4;
  msg->MinMaxInfo->MaxHeight += entries * _font(obj)->tf_YSize + 4;

  return 0;
}

/*************************************************************************
 MUIM_Draw
**************************************************************************/
STATIC LONG MailInfoArea_Draw(struct IClass *cl, Object *obj, struct MUIP_Draw *msg)
{	
	struct field *f;
	struct MailInfoArea_Data *data;
	int y = 2;

	data = (struct MailInfoArea_Data*)INST_DATA(cl,obj);
	DoSuperMethodA(cl,obj,(Msg)msg);

	SetAPen(_rp(obj), data->background_pen);
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
