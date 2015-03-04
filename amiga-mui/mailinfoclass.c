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

#include "addresslist.h"
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
#define CONTENTS_OFFSET 	8	/* offset from field column to text column */

/**************************************************************************/

struct text_node
{
	struct node node;

	char *text;
	char *link;

	int x_start;
	int x_end;
	int y_start;
	int y_end;
};

struct field
{
	struct node node;
	int clickable;

	char *name;
	int name_width;

	struct list text_list;
};

/**************************************************************************/

struct TinyButton_Data
{
	int dummy;
};

STATIC ULONG TinyButton_New(struct IClass *cl, Object *obj, struct opSet *msg)
{
	if (!(obj=(Object *)DoSuperNew(cl,obj,
					MUIA_InputMode, MUIV_InputMode_Toggle,
					MUIA_ShowSelState, FALSE,
					TAG_MORE,msg->ops_AttrList)))
		return 0;

	set(obj,MUIA_FillArea,FALSE);

	return (ULONG)obj;
}

STATIC ULONG TinyButton_AskMinMax(struct IClass *cl, Object *obj, struct MUIP_AskMinMax *msg)
{
	int fonty;

	DoSuperMethodA(cl, obj, (Msg) msg);

	fonty = _font(obj)->tf_YSize;

  msg->MinMaxInfo->MinWidth +=  fonty;
  msg->MinMaxInfo->DefWidth = msg->MinMaxInfo->MinWidth;
  msg->MinMaxInfo->MaxWidth = msg->MinMaxInfo->MinWidth;

  msg->MinMaxInfo->MinHeight += fonty;
  msg->MinMaxInfo->DefHeight = msg->MinMaxInfo->MinHeight;
  msg->MinMaxInfo->MaxHeight = msg->MinMaxInfo->MinHeight;
  return 0;
}

STATIC ULONG TinyButton_Draw(struct IClass *cl, Object *obj, struct MUIP_Draw *msg)
{
	int x1,y1,x2,y2;

	DoSuperMethodA(cl,obj,(Msg)msg);

	x1 = _mleft(obj);
	x2 = _mright(obj);
	y1 = _mtop(obj);
	y2 = _mbottom(obj);

	DoMethod(obj, MUIM_DrawBackground, x1, y1, x2 - x1 + 1, y2 - y1 + 1,0,0,0);

	/* the diff should be even that the plus sign will be symetric */
	if (((x2-x1)%2) == 1) x2--;
	if (((y2-y1)%2) == 1) y2--;

	SetAPen(_rp(obj),_dri(obj)->dri_Pens[SHADOWPEN]);
	Move(_rp(obj),x1,y2);
	Draw(_rp(obj),x1,y1);
	Draw(_rp(obj),x2,y1);
	Move(_rp(obj),x2-1,y1+1);
	Draw(_rp(obj),x2-1,y2-1);
	Draw(_rp(obj),x1+1,y2-1);

	SetAPen(_rp(obj),_dri(obj)->dri_Pens[SHINEPEN]);
	Move(_rp(obj),x1+1,y2-1);
	Draw(_rp(obj),x1+1,y1+1);
	Draw(_rp(obj),x2-1,y1+1);
	Move(_rp(obj),x2,y1+1);
	Draw(_rp(obj),x2,y2);
	Draw(_rp(obj),x1+1,y2);

	SetAPen(_rp(obj),_dri(obj)->dri_Pens[TEXTPEN]);

	if (xget(obj, MUIA_Selected))
	{
		Move(_rp(obj), x1 + 3, y1 + (y2 - y1)/2);
		Draw(_rp(obj), x2 - 3, y1 + (y2 - y1)/2);
		Move(_rp(obj), x1 + (x2 - x1)/2, y1 + 3);
		Draw(_rp(obj), x1 + (x2 - x1)/2, y2 - 3);
	} else
	{
		Move(_rp(obj), x1 + 3, y1 + (y2 - y1)/2);
		Draw(_rp(obj), x2 - 3, y1 + (y2 - y1)/2);
	}

	return 0;
}

STATIC MY_BOOPSI_DISPATCHER(ULONG, TinyButton_Dispatcher, cl, obj, msg)
{
	switch (msg->MethodID)
	{
		case	OM_NEW: return TinyButton_New(cl,obj,(struct opSet*)msg);
		case	MUIM_Draw: return TinyButton_Draw(cl,obj,(struct MUIP_Draw*)msg);
		case	MUIM_AskMinMax: return TinyButton_AskMinMax(cl,obj,(struct MUIP_AskMinMax *)msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

/**************************************************************************/

struct MailInfoArea_Data
{
	struct Hook *layout_hook;

	int setup;

	Object *switch_button;

	struct MUI_EventHandlerNode mb_handler;
	struct MUI_EventHandlerNode mv_handler;

	struct mail_info *mail;
	struct list field_list;

	struct field *selected_field;
	struct text_node *selected_text;
	int selected_mouse_over;

	int text_pen;
	int link_pen;
	struct text_node *redraw_text;

	int fieldname_left;			/* excluding BORDER */
	int fieldname_width;
	int entries;

	int compact;

	char background[60];
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

	char buf[64];

	if (!(f = malloc(sizeof(*f)))) return 0;
	if (!(t = malloc(sizeof(*t))))
	{
		free(f);
		return 0;
	}

	mystrlcpy(buf,name,sizeof(buf)-1);
	strcat(buf,":");

	memset(f,0,sizeof(*f));
	f->name = mystrdup(buf);
	f->clickable = 0;

	memset(t,0,sizeof(*t));
	list_init(&f->text_list);
	t->text = utf8tostrcreate(text,user.config.default_codeset);
	list_insert_tail(&f->text_list,&t->node);

	list_insert_tail(list,&f->node);

	return f;
}

/********************************************************************
 Add a new address field into the given field list
*********************************************************************/
static struct field *field_add_addresses(struct list *list, char *name, struct address_list *addr_list)
{
	struct field *f;
	struct address *addr;

	char buf[64];

	if (!(f = malloc(sizeof(*f)))) return 0;

	mystrlcpy(buf,name,sizeof(buf)-1);
	strcat(buf,":");

	memset(f,0,sizeof(*f));
	f->name = mystrdup(buf);
	f->clickable = 1;

	list_init(&f->text_list);

	addr = (struct address*)list_first(&addr_list->list);
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

	/* "find" correct text field */
	f = (struct field *)list_first(field_list);
	while (f)
	{
		text = (struct text_node*)list_first(&f->text_list);
		while (text)
		{
			if (y >= text->y_start && y <= text->y_end &&
			    x >= text->x_start && x <= text->x_end)
			{
				*field_ptr = f;
				*text_ptr = text;
				return;
			}
			text = (struct text_node*)node_next(&text->node);
		}
		f = (struct field*)node_next(&f->node);
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

	int field_width = 0;

	SM_DEBUGF(20,("Enter\n"));

	if (!data->setup)
		return;

	InitRastPort(&rp);
	SetFont(&rp,_font(obj));

	/* 1st pass, determine sizes of the field name column */
	SetSoftStyle(&rp,FSF_BOLD,AskSoftStyle(&rp));
	f = (struct field *)list_first(&data->field_list);
	while (f)
	{
		f->name_width = TextLength(&rp,f->name,strlen(f->name));
		if ((!data->compact && f->name_width > field_width) ||
		    (data->compact && !field_width)) field_width = f->name_width;
		f = (struct field*)node_next(&f->node);
	}
	data->fieldname_width = field_width + CONTENTS_OFFSET;

	/* determine the link positions is now done in MailInfoArea_DrawField() */

	SM_DEBUGF(20,("Leave\n"));
}

/********************************************************************
 Sets a new mail info
*********************************************************************/
VOID MailInfoArea_SetMailInfo(Object *obj, struct MailInfoArea_Data *data, struct mail_info *mi)
{
	struct field *f;
	int entries;

	SM_DEBUGF(20,("Enter\n"));

	data->selected_field = NULL;
	data->selected_text = NULL;

	if (data->mail) mail_dereference(data->mail);
	while ((f = (struct field*)list_remove_tail(&data->field_list)))
		field_free(f);

	if ((data->mail = mi))
	{
		char buf[300];

		mail_reference(data->mail);

		if (user.config.header_flags & (SHOW_HEADER_SUBJECT))
			field_add_text(&data->field_list,_("Subject"),mi->subject);

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

		if (user.config.header_flags & (SHOW_HEADER_DATE))
			field_add_text(&data->field_list,_("Date"),sm_get_date_long_str_utf8(mi->seconds));
	}

	entries = list_length(&data->field_list);
	if (data->entries != entries)
	{
		Object *group;
		group = (Object*)xget(obj, MUIA_Parent);

		DoMethod(group, MUIM_Group_InitChange);
		DoMethod(group, OM_REMMEMBER, (ULONG)obj);
		data->entries = entries;
		DoMethod(group, OM_ADDMEMBER, (ULONG)obj);
		DoMethod(group, MUIM_Group_ExitChange);
	} else
	{
		MailInfoArea_DetermineSizes(obj, data);
		MUI_Redraw(obj,MADF_DRAWOBJECT);
	}

	SM_DEBUGF(20,("Leave\n"));
}

/********************************************************************
 Draw a given field at the given y position (relativ to the object)
 Setting update to 1 means, that only data->redraw_text is rendered
*********************************************************************/
STATIC VOID MailInfoArea_DrawField(Object *obj, struct MailInfoArea_Data *data,
                                   struct field *f, int y, int update)
{
	int ytext = y + _mtop(obj) + _font(obj)->tf_Baseline;
	struct field *second_f = NULL;
	struct text_node *text, *second_text = NULL;
	int cnt;

	struct TextExtent te;

	int space_left, real_space_left;
	int comma_width, space_width, second_width;
	int dots_width, draw_dots;

	int fieldname_width, fieldname_width2;

	SetAPen(_rp(obj), data->text_pen);
	SetFont(_rp(obj), _font(obj));
	SetSoftStyle(_rp(obj),FS_NORMAL,AskSoftStyle(_rp(obj)));
	comma_width = TextLength(_rp(obj),",",1);
	space_width = TextLength(_rp(obj)," ",1);
	dots_width  = TextLength(_rp(obj),"...",3);

	/* display the field name, which is right aligned */
	fieldname_width = data->fieldname_width;
	fieldname_width2 = _mwidth(obj) - 2 * BORDER - data->fieldname_left;
	if (fieldname_width > fieldname_width2) fieldname_width = fieldname_width2;

	Move(_rp(obj), _mleft(obj) + BORDER + data->fieldname_left + data->fieldname_width - f->name_width - CONTENTS_OFFSET, ytext);
	SetSoftStyle(_rp(obj),FSF_BOLD,AskSoftStyle(_rp(obj)));

	space_left = fieldname_width - data->fieldname_width + f->name_width + CONTENTS_OFFSET;
	if (space_left < 0) return;
	cnt = TextFit(_rp(obj),f->name,strlen(f->name),&te,NULL,1,space_left,_font(obj)->tf_YSize);
	if (!cnt) return;
	if (!update) Text(_rp(obj),f->name,cnt);
	if (cnt < strlen(f->name)) return;

	/* display the rest */
	SetSoftStyle(_rp(obj),FS_NORMAL,AskSoftStyle(_rp(obj)));
	space_left = _mwidth(obj) - data->fieldname_left - 2 * BORDER;
	real_space_left = _mwidth(obj) - data->fieldname_left - 2 * BORDER;
	if (data->compact &&
		  (second_f = (struct field*)node_next(&f->node)) &&
			(second_text = (struct text_node*)list_first(&second_f->text_list)))
	{
		/* if we are in compact mode, show two fields. Display space is half/half except
		   when the second fieldlength (only the first text) is smaller as the half than
		   the second one will be shown full and as much as possible for the first field.
		   Otherwise the first field will be shown full and as much as possible of the
		   second field (all text), understand? :)
		*/
		space_left /= 2;
		second_width = CONTENTS_OFFSET + second_f->name_width + space_width + TextLength(_rp(obj), second_text->text, strlen(second_text->text));
		if (second_width < space_left)
			space_left = real_space_left - second_width;
	}
	space_left -= data->fieldname_width;
	real_space_left -= data->fieldname_width;
	if (space_left <= 0) space_left = real_space_left;
	if (real_space_left <= 0) return;

	Move(_rp(obj),_mleft(obj) + data->fieldname_left + data->fieldname_width + BORDER, ytext);
	text = (struct text_node*)list_first(&f->text_list);
	while (text)
	{
		struct text_node *next_text;

		next_text = (struct text_node*)node_next(&text->node);

		if (text->text)
		{
			int draw_text;

			cnt = TextFit(_rp(obj),text->text,strlen(text->text),&te,NULL,1,space_left,_font(obj)->tf_YSize);
			/* calculate linkfield */
			text->x_start = _rp(obj)->cp_x - _mleft(obj);
			text->x_end   = text->x_start + te.te_Width - 1;
			text->y_start = y;
			text->y_end   = text->y_start + _font(obj)->tf_YSize - 1;
			if (!cnt)
			{
				if (data->compact && node_index(&text->node) == 0)
				{
					/* if the first text of the first field doesn't match on the screen
					   switch of the second field */
					space_left = real_space_left;
					cnt = TextFit(_rp(obj),text->text,strlen(text->text),&te,NULL,1,space_left,_font(obj)->tf_YSize);
					text->x_end   = text->x_start + te.te_Width - 1;
				} else break;
			}
			if (cnt < strlen(text->text) && space_left > dots_width)
			{
				cnt = TextFit(_rp(obj),text->text,strlen(text->text),&te,NULL,1,space_left-dots_width,_font(obj)->tf_YSize);
				if (cnt > 0)
				{
					text->x_end   = text->x_start + te.te_Width + dots_width - 1;
					draw_dots = 1;
				} else
				{
					cnt = TextFit(_rp(obj),text->text,strlen(text->text),&te,NULL,1,space_left,_font(obj)->tf_YSize);
					text->x_end   = text->x_start + te.te_Width - 1;
					draw_dots = 0;
				}
			} else draw_dots = 0;

			if (update && text == data->redraw_text)
			{
				DoMethod(obj, MUIM_DrawBackground, _rp(obj)->cp_x, _mtop(obj) + y,
													 te.te_Width, _font(obj)->tf_YSize,0,0,0);
				draw_text = 1;
			} else draw_text = !update;

			if (f->clickable)
			{
				if (f == data->selected_field && data->selected_mouse_over)
					SetAPen(_rp(obj),_dri(obj)->dri_Pens[HIGHLIGHTTEXTPEN]);
				else SetAPen(_rp(obj),data->link_pen);
			}

			if (draw_text) Text(_rp(obj),text->text,cnt);
			else Move(_rp(obj),_rp(obj)->cp_x + te.te_Width, _rp(obj)->cp_y);
			if (draw_dots)
			{
				if (draw_text) Text(_rp(obj),"...",3);
				else Move(_rp(obj),_rp(obj)->cp_x + dots_width, _rp(obj)->cp_y);
				space_left -= dots_width;
				real_space_left -= dots_width;
			}
			space_left -= te.te_Width;
			real_space_left -= te.te_Width;

			if (next_text)
			{
				if (space_left >= comma_width && strlen(text->text) == cnt)
				{
					if (f->clickable) SetAPen(_rp(obj),data->text_pen);
					if (!update) Text(_rp(obj),",",1);
					else Move(_rp(obj),_rp(obj)->cp_x + comma_width, _rp(obj)->cp_y);

					space_left -= comma_width;
					real_space_left -= comma_width;
				} else break;
			}
			if (space_left <= 0) break;
		}
		text = next_text;
	}

	if (data->compact)
	{
		/* draw the second field in the same line */
		struct text_node *next_text;
		int draw_text;

		if (!second_f) return;
		if (!second_text) return;
		if (real_space_left < CONTENTS_OFFSET) return;

		Move(_rp(obj),_rp(obj)->cp_x + CONTENTS_OFFSET, _rp(obj)->cp_y);
		real_space_left -= CONTENTS_OFFSET;
		if (real_space_left <= 0) return;

		SetAPen(_rp(obj), data->text_pen);
		SetSoftStyle(_rp(obj),FSF_BOLD,AskSoftStyle(_rp(obj)));
		cnt = TextFit(_rp(obj),second_f->name,strlen(second_f->name),&te,NULL,1,real_space_left,_font(obj)->tf_YSize);
		if (!cnt) return;
		if (!update) Text(_rp(obj),second_f->name,cnt);
		else Move(_rp(obj), _rp(obj)->cp_x + te.te_Width, _rp(obj)->cp_y);
		if (cnt < strlen(second_f->name)) return;

		real_space_left -= te.te_Width;
		if (real_space_left < space_width) return;

		SetSoftStyle(_rp(obj),FS_NORMAL,AskSoftStyle(_rp(obj)));
		if (!update) Text(_rp(obj)," ",1);
		else Move(_rp(obj), _rp(obj)->cp_x + space_width, _rp(obj)->cp_y);
		real_space_left -= space_width;
		if (real_space_left <= 0) return;

		while (second_text)
		{
			next_text = (struct text_node*)node_next(&second_text->node);
			if (second_text->text)
			{
				cnt = TextFit(_rp(obj),second_text->text,strlen(second_text->text),&te,NULL,1,real_space_left,_font(obj)->tf_YSize);
				/* calculate linkfield */
				second_text->x_start = _rp(obj)->cp_x - _mleft(obj);
				second_text->x_end   = second_text->x_start + te.te_Width - 1;
				second_text->y_start = y;
				second_text->y_end   = second_text->y_start + _font(obj)->tf_YSize - 1;
				if (!cnt) return;
				if (cnt < strlen(second_text->text) && real_space_left > dots_width)
				{
					cnt = TextFit(_rp(obj),second_text->text,strlen(second_text->text),&te,NULL,1,real_space_left-dots_width,_font(obj)->tf_YSize);
					second_text->x_end   = second_text->x_start + te.te_Width + dots_width - 1;
					draw_dots = 1;
				} else draw_dots = 0;

				if (update && second_text == data->redraw_text)
				{
					DoMethod(obj, MUIM_DrawBackground, _rp(obj)->cp_x, _mtop(obj) + y,
													 te.te_Width, _font(obj)->tf_YSize,0,0,0);
					draw_text = 1;
				} else draw_text = !update;

				if (second_f->clickable)
				{
					if (second_f == data->selected_field && data->selected_mouse_over)
						SetAPen(_rp(obj),_dri(obj)->dri_Pens[HIGHLIGHTTEXTPEN]);
					else SetAPen(_rp(obj),data->link_pen);
				}
				if (draw_text) Text(_rp(obj),second_text->text,cnt);
				else Move(_rp(obj), _rp(obj)->cp_x + te.te_Width, _rp(obj)->cp_y);
				real_space_left -= te.te_Width;
				if (draw_dots)
				{
					if (draw_text) Text(_rp(obj),"...",3);
					else Move(_rp(obj),_rp(obj)->cp_x + dots_width, _rp(obj)->cp_y);
					real_space_left -= dots_width;
				}

				if (next_text)
				{
					if (space_left >= comma_width && strlen(second_text->text) == cnt)
					{
						if (second_f->clickable) SetAPen(_rp(obj),data->text_pen);
						if (!update) Text(_rp(obj),",",1);
						else Move(_rp(obj), _rp(obj)->cp_x + comma_width, _rp(obj)->cp_y);

						real_space_left -= comma_width;
					} else return;
				}
				if (real_space_left <= 0) return;
			}
			second_text = next_text;
		}
	}
}

/*************************************************************************/

#define MUIM_MailInfo_CompactChanged 0x6e37289

/********************************************************************
 Layout hook function
*********************************************************************/
static ASM ULONG MailInfoArea_LayoutFunc( REG(a0, struct Hook *hook), REG(a2, Object *obj), REG(a1,struct MUI_LayoutMsg *lm))
{
	extern struct MUI_CustomClass *CL_MailInfoArea;

	SM_ENTER;

  switch (lm->lm_Type)
  {
    case  MUILM_MINMAX:
          {
						struct MailInfoArea_Data *data = (struct MailInfoArea_Data*)INST_DATA(CL_MailInfoArea->mcc_Class,obj);
						int minheight, entries;

						if (data->compact) entries = 1;
						else entries = data->entries;

						minheight = entries * _font(obj)->tf_YSize + 2 * BORDER;

						/* We only have one child and we know which one */
						if (minheight < _minheight(data->switch_button) + 2 * BORDER)
							minheight = _minheight(data->switch_button) + 2 * BORDER;

						entries = data->entries;

						lm->lm_MinMax.MinWidth  = 2 * BORDER + _minwidth(data->switch_button);
						lm->lm_MinMax.MinHeight = minheight;
						lm->lm_MinMax.DefWidth  = lm->lm_MinMax.MinWidth * 10;
						lm->lm_MinMax.DefHeight = minheight;
						lm->lm_MinMax.MaxWidth  = MUI_MAXMAX;
						lm->lm_MinMax.MaxHeight = minheight;

						data->fieldname_left = _minwidth(data->switch_button) + 4;
						MailInfoArea_DetermineSizes(obj, data);
						return 0;
          }

	  case  MUILM_LAYOUT:
	  			{
						struct MailInfoArea_Data *data = (struct MailInfoArea_Data*)INST_DATA(CL_MailInfoArea->mcc_Class,obj);
	  				int mw = _minwidth(data->switch_button);
					  int mh = _minheight(data->switch_button);
					  int fonty = _font(obj)->tf_YSize;
					  int offset = (mh - fonty)/2;

						if (offset < 0) offset = 0;

						MUI_Layout(data->switch_button, BORDER, BORDER, mw, mh + offset, 0);
	  			}
	  			return 1;
  }
  return MUILM_UNKNOWN;
}

/********************************************************************
 OM_NEW
*********************************************************************/
STATIC ULONG MailInfoArea_New(struct IClass *cl, Object *obj, struct opSet *msg)
{
	struct MailInfoArea_Data *data;
	Object *switch_button;

	struct Hook *layout_hook;

	extern struct MUI_CustomClass *CL_TinyButton;

	struct TagItem *oid_tag;

	if (!(layout_hook = (struct Hook*)AllocVec(sizeof(*layout_hook),MEMF_CLEAR)))
		return 0;

	/* Filter out MUIA_ObjectID tag as this is used for the switch_button */
	if ((oid_tag = FindTagItem(MUIA_ObjectID, msg->ops_AttrList)))
		oid_tag->ti_Tag = TAG_IGNORE;

	init_hook(layout_hook, (HOOKFUNC)MailInfoArea_LayoutFunc);

	if (!(obj=(Object *)DoSuperNew(cl,obj,
					MUIA_Font, MUIV_Font_Tiny,
					MUIA_Group_Child, switch_button = MyNewObject(CL_TinyButton->mcc_Class, NULL, oid_tag?MUIA_ObjectID:TAG_IGNORE, oid_tag?oid_tag->ti_Data:0, TAG_DONE),
					MUIA_Group_LayoutHook, layout_hook,
					TAG_MORE,msg->ops_AttrList)))
	{
		FreeVec(layout_hook);
		return 0;
	}

	data = (struct MailInfoArea_Data*)INST_DATA(cl,obj);

	sm_snprintf(data->background,sizeof(data->background),"2:%08x,%08x,%08x",
					MAKECOLOR32((user.config.read_header_background & 0xff0000)>>16),
					MAKECOLOR32((user.config.read_header_background & 0xff00)>>8),
					MAKECOLOR32((user.config.read_header_background & 0xff)));

	set(obj,MUIA_Background, data->background);


	list_init(&data->field_list);

	data->switch_button = switch_button;

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

	DoMethod(switch_button, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, (ULONG)obj, 1, MUIM_MailInfo_CompactChanged);

	return (ULONG)obj;
}

/********************************************************************
 OM_DISPOSE
*********************************************************************/
STATIC ULONG MailInfoArea_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
	struct MailInfoArea_Data *data;
	struct field *f;
	struct Hook *layout_hook;
	ULONG rc;

	data = (struct MailInfoArea_Data*)INST_DATA(cl,obj);
	if (data->mail) mail_dereference(data->mail);

	while ((f = (struct field*)list_remove_tail(&data->field_list)))
		field_free(f);

	/* Layout hook should be freed after object's disposion */
	layout_hook = data->layout_hook;

	rc = DoSuperMethodA(cl,obj,msg);

	FreeVec(layout_hook);
	return rc;
}

/********************************************************************
 OM_SET
*********************************************************************/
STATIC ULONG MailInfoArea_Set(struct IClass *cl, Object *obj, struct opSet *msg)
{
	struct MailInfoArea_Data *data = (struct MailInfoArea_Data*)INST_DATA(cl,obj);
	ULONG rc;
	struct TagItem *tstate, *tag;
	tstate = (struct TagItem *)msg->ops_AttrList;

	while ((tag = NextTagItem ((APTR)&tstate)))
	{
		ULONG tidata = tag->ti_Data;

		switch (tag->ti_Tag)
		{
			case	MUIA_MailInfo_MailInfo:
						MailInfoArea_SetMailInfo(obj,data,(struct mail_info*)tidata);
						break;
		}
	}

	rc = DoSuperMethodA(cl,obj,(Msg)msg);
	return rc;
}

/********************************************************************
 MUIM_Setup
*********************************************************************/
STATIC ULONG MailInfoArea_Setup(struct IClass *cl, Object *obj, struct MUIP_Setup *msg)
{
	struct MailInfoArea_Data *data = (struct MailInfoArea_Data*)INST_DATA(cl,obj);
	ULONG rc;

	rc = DoSuperMethodA(cl,obj,(Msg)msg);
	if (!rc) return 0;

	data->setup = 1;

	data->link_pen  = ObtainBestPenA(_screen(obj)->ViewPort.ColorMap,
				MAKECOLOR32((user.config.read_link & 0xff0000) >> 16),
				MAKECOLOR32((user.config.read_link & 0xff00) >> 8),
				MAKECOLOR32((user.config.read_link & 0xff)), NULL);

	data->text_pen  = ObtainBestPenA(_screen(obj)->ViewPort.ColorMap,
				MAKECOLOR32((user.config.read_text & 0xff0000) >> 16),
				MAKECOLOR32((user.config.read_text & 0xff00) >> 8),
				MAKECOLOR32((user.config.read_text & 0xff)), NULL);

	return rc;
}

/********************************************************************
 MUIM_Cleanup
*********************************************************************/
STATIC ULONG MailInfoArea_Cleanup(struct IClass *cl, Object *obj, Msg msg)
{
	struct MailInfoArea_Data *data = (struct MailInfoArea_Data*)INST_DATA(cl,obj);
	ReleasePen(_screen(obj)->ViewPort.ColorMap,data->text_pen);
	ReleasePen(_screen(obj)->ViewPort.ColorMap,data->link_pen);
	data->setup = 0;
	return DoSuperMethodA(cl,obj,(Msg)msg);;
}

/********************************************************************
 MUIM_Show
*********************************************************************/
STATIC ULONG MailInfoArea_Show(struct IClass *cl, Object *obj, Msg msg)
{
	struct MailInfoArea_Data *data = (struct MailInfoArea_Data*)INST_DATA(cl,obj);
	ULONG rc;

	rc = DoSuperMethodA(cl,obj,(Msg)msg);

	DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&data->mb_handler);

	return rc;
}

/********************************************************************
 MUIM_Cleanup
*********************************************************************/
STATIC ULONG MailInfoArea_Hide(struct IClass *cl, Object *obj, Msg msg)
{
	struct MailInfoArea_Data *data = (struct MailInfoArea_Data*)INST_DATA(cl,obj);
	DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&data->mb_handler);
	return DoSuperMethodA(cl,obj,(Msg)msg);;
}

/********************************************************************
 MUIM_Draw
*********************************************************************/
STATIC ULONG MailInfoArea_Draw(struct IClass *cl, Object *obj, struct MUIP_Draw *msg)
{
	struct field *f;
	struct MailInfoArea_Data *data;
	int y = 2;
	int update;

	data = (struct MailInfoArea_Data*)INST_DATA(cl,obj);

	DoSuperMethodA(cl,obj,(Msg)msg);

	update = !!(msg->flags & MADF_DRAWUPDATE);

	f = (struct field *)list_first(&data->field_list);
	while (f)
	{
		MailInfoArea_DrawField(obj, data, f, y, update);

		if (data->compact) break;

		y += _font(obj)->tf_YSize;
		f = (struct field*)node_next(&f->node);
	}

	return 0;
}

/********************************************************************
 MUIM_HandleEvent
*********************************************************************/
STATIC ULONG MailInfoArea_HandleEvent(struct IClass *cl, Object *obj, struct MUIP_HandleEvent *msg)
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

		/* normalize positions */
		x -= _mleft(obj);
		y -= _mtop(obj);

		if (imsg->Class == IDCMP_MOUSEBUTTONS)
		{
			if (imsg->Code == SELECTDOWN)
			{
				if (x < 0 || x >= _mwidth(obj) || y < 0 || y >= _height(obj))
					return 0;

				field_find(&data->field_list, x, y, _font(obj)->tf_YSize, &f, &t);

				if (f && t && f->clickable)
				{
					DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&data->mv_handler);

					data->selected_field = f;
					data->selected_text = t;
					data->selected_mouse_over = 1;

					data->redraw_text = data->selected_text;
					MUI_Redraw(obj, MADF_DRAWUPDATE);
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

					DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&data->mv_handler);

					data->redraw_text = data->selected_text; /* for update operation */

					data->selected_field = NULL;
					data->selected_text = NULL;
					data->selected_mouse_over = 0;

					MUI_Redraw(obj, MADF_DRAWUPDATE);
					return MUI_EventHandlerRC_Eat;
				}
			}
		} else if (imsg->Class == IDCMP_MOUSEMOVE)
		{
			if (x < 0 || x >= _mwidth(obj) || y < 0 || y >= _height(obj))
			{
				if (data->selected_mouse_over)
				{
					data->selected_mouse_over = 0;
					data->redraw_text = data->selected_text;
					MUI_Redraw(obj, MADF_DRAWUPDATE);
				}
				return MUI_EventHandlerRC_Eat;
			}

			field_find(&data->field_list, x, y, _font(obj)->tf_YSize, &f, &t);

			if (t && t == data->selected_text)
			{
				if (!data->selected_mouse_over)
				{
					data->selected_mouse_over = 1;
					data->redraw_text = data->selected_text;
					MUI_Redraw(obj, MADF_DRAWUPDATE);
				}
			} else
			{
				if (data->selected_mouse_over)
				{
					data->selected_mouse_over = 0;
					data->redraw_text = data->selected_text;
					MUI_Redraw(obj, MADF_DRAWUPDATE);
				}
			}
		}
	}

	return 0;
}

/********************************************************************
 MUIM_MailInfo_CompactChanged
*********************************************************************/
STATIC ULONG MailInfo_CompactChanged(struct IClass *cl, Object *obj, Msg msg)
{
	struct MailInfoArea_Data *data = (struct MailInfoArea_Data*)INST_DATA(cl,obj);
	int compact;

	compact = xget(data->switch_button, MUIA_Selected);
	if (compact != data->compact)
	{
		Object *group;
		group = (Object*)xget(obj, MUIA_Parent);

		DoMethod(group, MUIM_Group_InitChange);
		DoMethod(group, OM_REMMEMBER, (ULONG)obj);
		data->compact = compact;
		DoMethod(group, OM_ADDMEMBER, (ULONG)obj);
		DoMethod(group, MUIM_Group_ExitChange);
	}
	return 0;
}

STATIC MY_BOOPSI_DISPATCHER(ULONG, MailInfoArea_Dispatcher, cl, obj, msg)
{
	switch(msg->MethodID)
	{
		case	OM_NEW: return MailInfoArea_New(cl,obj,(struct opSet*)msg);
		case	OM_DISPOSE: return MailInfoArea_Dispose(cl,obj,msg);
		case	OM_SET: return MailInfoArea_Set(cl,obj,(struct opSet*)msg);
		case	MUIM_Setup: return MailInfoArea_Setup(cl,obj,(struct MUIP_Setup*)msg);
		case	MUIM_Cleanup: return MailInfoArea_Cleanup(cl,obj,msg);
		case	MUIM_Show: return MailInfoArea_Show(cl,obj,msg);
		case	MUIM_Hide: return MailInfoArea_Hide(cl,obj,msg);
		case	MUIM_Draw: return MailInfoArea_Draw(cl,obj,(struct MUIP_Draw*)msg);
		case	MUIM_HandleEvent: return MailInfoArea_HandleEvent(cl,obj,(struct MUIP_HandleEvent*)msg);
		case	MUIM_MailInfo_CompactChanged: return MailInfo_CompactChanged(cl,obj,msg);
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

	struct TagItem *oid_tag;

	/* Filter out MUIA_ObjectID tag as this is used for the switch_button */
	if ((oid_tag = FindTagItem(MUIA_ObjectID, msg->ops_AttrList)))
		oid_tag->ti_Tag = TAG_IGNORE;

	if (!(obj=(Object *)DoSuperNew(cl,obj,
					MUIA_Group_Child, mailinfo = MyNewObject(CL_MailInfoArea->mcc_Class, NULL, oid_tag?MUIA_ObjectID:TAG_IGNORE, oid_tag?oid_tag->ti_Data:0, TAG_DONE),
					TAG_MORE,msg->ops_AttrList)))
		return 0;

	data = (struct MailInfo_Data*)INST_DATA(cl,obj);
	data->mailinfo = mailinfo;

	return (ULONG)obj;
}


STATIC MY_BOOPSI_DISPATCHER(ULONG, MailInfo_Dispatcher, cl, obj, msg)
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
struct MUI_CustomClass *CL_TinyButton;

int create_mailinfo_class(void)
{
	SM_ENTER;

	if ((CL_TinyButton = CreateMCC(MUIC_Area, NULL, sizeof(struct TinyButton_Data), TinyButton_Dispatcher)))
	{
		SM_DEBUGF(15,("Created CL_TinyButton: 0x%lx\n",CL_TinyButton));

		if ((CL_MailInfoArea = CreateMCC(MUIC_Group, NULL, sizeof(struct MailInfoArea_Data), MailInfoArea_Dispatcher)))
		{
			SM_DEBUGF(15,("Created CL_MailInfoArea: 0x%lx\n",CL_MailInfoArea));

			if ((CL_MailInfo = CreateMCC(MUIC_Group, NULL, sizeof(struct MailInfo_Data), MailInfo_Dispatcher)))
			{
				SM_DEBUGF(15,("Created CL_MailInfo: 0x%lx\n",CL_MailInfoArea));

				SM_RETURN(1,"%ld");
			}
		}
	}

	SM_DEBUGF(5,("FAILED! Create CL_MailInfoArea\n"));

	delete_mailinfo_class();

	SM_RETURN(0,"%ld");
}

void delete_mailinfo_class(void)
{
	SM_ENTER;

	if (CL_MailInfo)
	{
		if (MUI_DeleteCustomClass(CL_MailInfo))
		{
			SM_DEBUGF(15,("Deleted CL_MailInfo: 0x%lx\n",CL_MailInfo));
			CL_MailInfo = NULL;
		} else
		{
			SM_DEBUGF(5,("FAILED! Delete CL_MailInfo: 0x%lx\n",CL_MailInfo));
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

	if (CL_TinyButton)
	{
		if (MUI_DeleteCustomClass(CL_TinyButton))
		{
			SM_DEBUGF(15,("Deleted CL_TinyButton: 0x%lx\n",CL_TinyButton));
			CL_MailInfo = NULL;
		} else
		{
			SM_DEBUGF(5,("FAILED! Delete CL_TinyButton: 0x%lx\n",CL_TinyButton));
		}
	}
	SM_LEAVE;
}
