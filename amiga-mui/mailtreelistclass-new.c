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
** mailtreelistclass-new.c
**
** Based upon work done for the listclass of Zune.
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>

#include "configuration.h"
#include "folder.h"
#include "mail.h"
#include "smintl.h"

#include "compiler.h"
#include "datatypescache.h"
#include "debug.h"
#include "mailtreelistclass.h"
#include "muistuff.h"

/**************************************************************************/

#define MIN(a,b) ((a)<(b)?(a):(b))

/**************************************************************************/

/* Horizontal space between images */
#define IMAGE_HORIZ_SPACE 2

enum
{
	IMAGE_UNREAD = 0,
	IMAGE_UNREAD_PARTIAL,
	IMAGE_READ_PARTIAL,
	IMAGE_REPLY_PARTIAL,
	IMAGE_READ,
	IMAGE_WAITSEND,
	IMAGE_SENT,
	IMAGE_MARK,
	IMAGE_HOLD,
	IMAGE_REPLY,
	IMAGE_FORWARD,
	IMAGE_NORCPT,
	IMAGE_NEW_PARTIAL,
	IMAGE_NEW_SPAM,
	IMAGE_UNREAD_SPAM,
	IMAGE_ERROR,
	IMAGE_IMPORTANT,
	IMAGE_ATTACH,
	IMAGE_GROUP,
	IMAGE_NEW,
	IMAGE_CRYPT,
	IMAGE_SIGNED,
	IMAGE_TRASHCAN,
	IMAGE_MAX
};

/* Must match those above (old = read)*/
static const char *image_names[] =
{
	"status_unread",
	"status_unread_partial",
	"status_old_partial",
	"status_reply_partial",
	"status_old",
  "status_waitsend",
	"status_sent",
	"status_mark",
	"status_hold",
	"status_reply",
	"status_forward", 
	"status_norcpt",
	"status_new_partial",
	"status_new_spam",
	"status_unread_spam",
	"status_error",
	"status_urgent",
	"status_attach",
	"status_group",
	"status_new",
	"status_crypt",
	"status_signed",
	"status_trashcan",
	NULL
};

/**************************************************************************/

#define MAX_COLUMNS 10

#define ENTRY_TITLE (-1)

struct ListEntry
{
	struct mail_info *mail_info;
	LONG widths[MAX_COLUMNS]; /* Widths of the columns */
	LONG width;   /* Line width */
	LONG height;  /* Line height */
	WORD flags;   /* see below */
	WORD parents; /* number of entries parent's, used for the list tree stuff */
};

#define LE_FLAG_PARENT      (1<<0)  /* Entry is a parent, possibly containing children */
#define LE_FLAG_CLOSED      (1<<1)  /* The entry (parent) is closed (means that all children are invisible) */
#define LE_FLAG_VISIBLE     (1<<2)  /* The entry is visible */
#define LE_FLAG_SELECTED    (1<<3)  /* The entry is selected */
#define LE_FLAG_HASCHILDREN (1<<4)  /* The entry really has children */

struct ColumnInfo
{
	LONG width;
	WORD type;
	WORD flags;
};

#define COLUMN_TYPE_FROMTO  1
#define COLUMN_TYPE_SUBJECT 2

#define COLUMN_FLAG_AUTOWIDTH (1L << 0)

struct MailTreelist_Data
{
	APTR pool;

	int inbetween_setup;
	int inbetween_show;
  struct MUI_EventHandlerNode ehn;

	struct dt_node *images[IMAGE_MAX];

	/* List managment, currently we use a simple flat array, which is not good if many entries are inserted/deleted */
	LONG entries_num; /* Number of Entries in the list (excludes title) */
	LONG entries_allocated;
	struct ListEntry **entries;

	LONG entries_first; /* first visible entry */
	LONG entries_visible; /* number of visible entries */
	LONG entries_active;

	LONG entry_maxheight; /* Max height of an list entry */
	LONG title_height;

	struct ColumnInfo ci[MAX_COLUMNS];
	
	LONG threepoints_width; /* Width of ... */

	char buf[2048];

	int quiet; /* needed for rendering, if > 0, don't call super method */

	struct RastPort rp; /* Rastport for font calculations */
};

/**************************************************************************/

static void IssueTreelistActiveNotify(struct IClass *cl, Object *obj, struct MailTreelist_Data *data)
{
	struct TagItem tags[2];

	tags[0].ti_Tag = MUIA_MailTreelist_Active;

	if (data->entries_active != -1)
		tags[0].ti_Data = (ULONG)(data->entries[data->entries_active]->mail_info);
	else
		tags[0].ti_Data = 0;

	tags[1].ti_Tag = TAG_DONE;

	/* issue the notify */
	DoSuperMethod(cl,obj,OM_SET,tags, NULL);
}

/**************************************************************************/

STATIC VOID GetFromText(struct mail_info *m, char **txt_ptr, int *ascii7_ptr)
{
	int is_ascii7 = 1;
	char *txt;

	if ((txt = m->from_phrase))
		is_ascii7 = !!(m->flags & MAIL_FLAGS_FROM_ASCII7);

	if (!txt)
	{
		if ((txt = m->from_addr))
			is_ascii7 = !!(m->flags & MAIL_FLAGS_FROM_ADDR_ASCII7);
	}

	if (!txt)
	{
		txt = "";
		is_ascii7 = 1;
	}

	*ascii7_ptr = is_ascii7;
	*txt_ptr = txt;
}

/**************************************************************************/


/**************************************************************************
 Allocate a single list entry, does not initialize it (except the pointer)
**************************************************************************/
static struct ListEntry *AllocListEntry(struct MailTreelist_Data *data)
{
    ULONG *mem;
    struct ListEntry *le;
    int size = sizeof(struct ListEntry) + 4; /* sizeinfo */

    mem = (ULONG*)AllocPooled(data->pool, size);
    if (!mem) return NULL;

    mem[0] = size; /* Save the size */
    le = (struct ListEntry*)(mem+1);
    return le;
}

/**************************************************************************
 Deallocate a single list entry, does not deinitialize it
**************************************************************************/
static void FreeListEntry(struct MailTreelist_Data *data, struct ListEntry *entry)
{
    ULONG *mem = ((ULONG*)entry)-1;
    FreePooled(data->pool, mem, mem[0]);
}

/**************************************************************************
 Ensures that we there can be at least the given amount of entries within
 the list. Returns 0 if not. It also allocates the space for the title.
 It can be accesses with data->entries[ENTRY_TITLE]
**************************************************************************/
static int SetListSize(struct MailTreelist_Data *data, LONG size)
{
	struct ListEntry **new_entries;
	int new_entries_allocated;
	
	SM_DEBUGF(10,("%ld %ld\n",size + 1, data->entries_allocated));
	
	if (size + 1 <= data->entries_allocated)
		return 1;

 	new_entries_allocated = data->entries_allocated * 2 + 4;
 	if (new_entries_allocated < size + 1)
		new_entries_allocated = size + 1 + 10; /* 10 is just random */

	SM_DEBUGF(10,("SetListSize allocating %ld bytes\n",
								new_entries_allocated * sizeof(struct ListEntry *)));

  if (!(new_entries = (struct ListEntry**)AllocVec(new_entries_allocated * sizeof(struct ListEntry *),0)))
  	return 0;

  if (data->entries)
  {
		CopyMem(data->entries - 1, new_entries,(data->entries_num + 1) * sizeof(struct ListEntry*));
		FreeVec(data->entries - 1);
  }
  data->entries = new_entries + 1;
  data->entries_allocated = new_entries_allocated;
  return 1;
}

/**************************************************************************
 Calc entries dimensions
**************************************************************************/
static void CalcEntries(struct MailTreelist_Data *data, Object *obj)
{
	int i,col;
	int maxheight = 0;

	for (col=0;col<MAX_COLUMNS;col++)
	{
		/* Discard widths of auto width columns */
		if (data->ci[col].flags & COLUMN_FLAG_AUTOWIDTH)
			data->ci[col].width = 0;
	}

	for (i=0;i<data->entries_num;i++)
	{
		struct mail_info *m;

		/* Entry height, very simple at the momement */
		int entry_height = _font(obj)->tf_YSize;
		if (entry_height > maxheight) maxheight = entry_height;

		m = data->entries[i]->mail_info;

		for (col=0;col<MAX_COLUMNS;col++)
		{
			if (data->ci[col].flags & COLUMN_FLAG_AUTOWIDTH)
			{
				int col_width = data->ci[col].width;
				int is_ascii7 = 1;
				char *txt = NULL;
				int used_images = 0;
				int images[4];
		
				switch (data->ci[col].type)
				{
					case	COLUMN_TYPE_FROMTO:
								if (m)
								{
									if (m->flags & MAIL_FLAGS_GROUP) images[used_images++] = IMAGE_GROUP;
									GetFromText(m,&txt,&is_ascii7);
								}
								else txt = _("From");
								break;
		
					case	COLUMN_TYPE_SUBJECT:
								if (m)
								{
									txt = m->subject;
									is_ascii7 = !!(m->flags & MAIL_FLAGS_SUBJECT_ASCII7);
								}
								else txt = _("Subject");
								break;
				}

				if (txt || used_images)
				{
					int new_width = 0;
					int cur_image;

					/* put the images at first */
					for (cur_image = 0; cur_image < used_images; cur_image++)
					{
						struct dt_node *dt = data->images[images[cur_image]];
						if (dt) new_width += dt_width(dt) + IMAGE_HORIZ_SPACE;
					}

					if (txt)
					{
						if (!is_ascii7)
						{
							utf8tostr(txt,data->buf,sizeof(data->buf),user.config.default_codeset);
							txt = data->buf;
						}
						new_width += TextLength(&data->rp, txt, strlen(txt));
						if (new_width > data->ci[col].width) data->ci[col].width = new_width;
					}
				}

			}
		}
	}
	
	data->entry_maxheight = maxheight;
}

/**************************************************************************
 Calc number of visible entries
**************************************************************************/
static void CalcVisible(struct MailTreelist_Data *data, Object *obj)
{
	if (data->entry_maxheight)
	{
		data->entries_visible = _mheight(obj)/data->entry_maxheight;
	} else
	{
		data->entries_visible = 10;
	}
}

/**************************************************************************
 Draw an entry at entry_pos at the given y location. To draw the title,
 set pos to ENTRY_TITLE
**************************************************************************/
static void DrawEntry(struct MailTreelist_Data *data, Object *obj, int entry_pos, int y)
{
	int col;
	int x1;
	int fonty;
	int entry_height;

	struct ListEntry *entry;
	struct mail_info *m;

	x1 = _mleft(obj);

	if (!(entry = data->entries[entry_pos]))
		return;

	m = entry->mail_info;
	fonty = _font(obj)->tf_YSize;
	entry_height = data->entry_maxheight;

	SetABPenDrMd(_rp(obj),_pens(obj)[MPEN_TEXT],0,JAM1);

	for (col = 0;col < MAX_COLUMNS; col++)
	{
		int col_width = data->ci[col].width;
		int is_ascii7 = 1;
		char *txt = NULL;
		int used_images = 0;
		int images[4];

		switch (data->ci[col].type)
		{
			case	COLUMN_TYPE_FROMTO:
						if (m)
						{
							if (m->flags & MAIL_FLAGS_GROUP) images[used_images++] = IMAGE_GROUP;
							GetFromText(m,&txt,&is_ascii7);
						}
						else txt = _("From");
						break;

			case	COLUMN_TYPE_SUBJECT:
						if (m)
						{
							txt = m->subject;
							is_ascii7 = !!(m->flags & MAIL_FLAGS_SUBJECT_ASCII7);
						}
						else txt = _("Subject");
						break;
		}

		/* Bring the text or images on screen */
		if (txt || used_images)
		{
			int txt_len;
			int fit;
			struct TextExtent te;
			int cur_image;
			int available_width = col_width;
			int xstart = x1;

			/* put the images at first */
			for (cur_image = 0; cur_image < used_images; cur_image++)
			{
				struct dt_node *dt = data->images[images[cur_image]];
				if (dt)
				{
					if (dt_width(dt) <= available_width)
					{
						dt_put_on_rastport(dt,_rp(obj),xstart,y + (entry_height - dt_height(dt))/2);
						available_width -= dt_width(dt) + IMAGE_HORIZ_SPACE;
						xstart += dt_width(dt) + IMAGE_HORIZ_SPACE;
					} else
					{
						available_width = 0;
					}
				}
			}

			/* now put the text, but only if there is really space left */
			if (available_width > 0 && txt)
			{
				if (!is_ascii7)
				{
					utf8tostr(txt,data->buf,sizeof(data->buf),user.config.default_codeset);
					txt = data->buf;
				}
	
				txt_len = strlen(txt);
				
				fit = TextFit(_rp(obj),txt,txt_len,&te,NULL,1,available_width,fonty);
				if (fit < txt_len)
				{
					fit = TextFit(_rp(obj),txt,txt_len,&te,NULL,1,available_width - data->threepoints_width,fonty);
				}
	
				Move(_rp(obj),xstart,y + _font(obj)->tf_Baseline);
				Text(_rp(obj),txt,fit);
	
				if (fit < txt_len)
					Text(_rp(obj),"...",3);
			}
		}

		x1 += col_width;
	}
}

/**************************************************************************/

/*************************************************************************
 OM_NEW
*************************************************************************/
STATIC ULONG MailTreelist_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct MailTreelist_Data *data;


	if (!(obj=(Object *)DoSuperNew(cl,obj,
		MUIA_InputMode, MUIV_InputMode_None,
		MUIA_ShowSelState, FALSE,
		MUIA_FillArea, FALSE,
		MUIA_Background, MUII_ListBack,
/*		MUIA_ShortHelp, TRUE,*/
		TAG_MORE,msg->ops_AttrList)))
		return 0;

	data = (struct MailTreelist_Data*)INST_DATA(cl,obj);

	if (!(data->pool = CreatePool(MEMF_ANY,16384,16384)))
	{
		CoerceMethodA(cl,obj,(Msg)msg);
		return 0;
	}

	data->ci[0].type = COLUMN_TYPE_FROMTO;
	data->ci[0].width = 150;
	data->ci[0].flags = COLUMN_FLAG_AUTOWIDTH;
	data->ci[1].type = COLUMN_TYPE_SUBJECT;
	data->ci[1].width = 200;
	data->ci[1].flags = COLUMN_FLAG_AUTOWIDTH;

  data->ehn.ehn_Events   = IDCMP_MOUSEBUTTONS;
  data->ehn.ehn_Priority = 0;
  data->ehn.ehn_Flags    = 0;
  data->ehn.ehn_Object   = obj;
  data->ehn.ehn_Class    = cl;

	return (ULONG)obj;
}

/*************************************************************************
 OM_DISPOSE
*************************************************************************/
STATIC ULONG MailTreelist_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);
	if (data->pool) DeletePool(data->pool);

	return DoSuperMethodA(cl,obj,msg);
}

/*************************************************************************
 OM_SET
*************************************************************************/
STATIC ULONG MailTreelist_Set(struct IClass *cl, Object *obj, struct opSet *msg)
{
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);
	struct TagItem *tstate, *tag;

	tstate = (struct TagItem *)msg->ops_AttrList;

	while ((tag = NextTagItem (&tstate)))
	{
		ULONG tidata = tag->ti_Data;

		switch (tag->ti_Tag)
		{
			case	MUIA_MailTreelist_Active:
						{
							data->entries_active = -1;
							if (tidata)
							{
								int i;
								for (i=0;i<data->entries_num;i++)
								{
									if (tidata == (ULONG)data->entries[i]->mail_info)
									{
										data->entries_active = i;
										break;
									}
								}
							} 
						}
						break;
		}
	}

	return DoSuperMethodA(cl,obj,(Msg)msg);
}

/*************************************************************************
 OM_GET
*************************************************************************/
STATIC ULONG MailTreelist_Get(struct IClass *cl, Object *obj, struct opGet *msg)
{
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);

	if (msg->opg_AttrID == MUIA_MailTreelist_Active)
	{
		if (data->entries_active >= 0 && data->entries_active < data->entries_num)
			*msg->opg_Storage = (ULONG)data->entries[data->entries_active]->mail_info;
		else
			*msg->opg_Storage = 0;
		return 1;
	}

	if (msg->opg_AttrID == MUIA_MailTreelist_DoubleClick)
	{
		*msg->opg_Storage = 0;
		return 1;
	}

	return DoSuperMethodA(cl,obj,(Msg)msg);
}

/*************************************************************************
 MUIM_Setup
*************************************************************************/
STATIC ULONG MailTreelist_Setup(struct IClass *cl, Object *obj, struct MUIP_Setup *msg)
{
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);
	char filename[256];
	int i;

	if (!DoSuperMethodA(cl,obj,(Msg)msg))
		return 0;

	/* Setup rastport */
	InitRastPort(&data->rp);
  SetFont(&data->rp,_font(obj));

	for (i=0;i<IMAGE_MAX;i++)
	{
		strcpy(filename,"PROGDIR:Images/");
		strcat(filename,image_names[i]);
		data->images[i] = dt_load_picture(filename, _screen(obj));
		/* It doesn't matter if this fails */
	}

	data->inbetween_setup = 1;
	CalcEntries(data,obj);
	return 1;
}

/*************************************************************************
 MUIM_Cleanup
*************************************************************************/
STATIC ULONG MailTreelist_Cleanup(struct IClass *cl, Object *obj, Msg msg)
{
	int i;
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);

	data->inbetween_setup = 0;

	for (i=0;i<IMAGE_MAX;i++)
	{
		if (data->images[i])
		{
			dt_dispose_picture(data->images[i]);
			data->images[i] = NULL;
		}
	}

	return DoSuperMethodA(cl,obj,msg);
}

/*************************************************************************
 MUIM_AskMinMax
*************************************************************************/
STATIC ULONG MailTreelist_AskMinMax(struct IClass *cl,Object *obj, struct MUIP_AskMinMax *msg)
{
	struct MUI_MinMax *mi;
/*	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);*/
  DoSuperMethodA(cl, obj, (Msg) msg);

	mi = msg->MinMaxInfo;

	mi->MaxHeight = MUI_MAXMAX;
	mi->MaxWidth = MUI_MAXMAX;

	return 1;
}

/*************************************************************************
 MUIM_Show
*************************************************************************/
STATIC ULONG MailTreelist_Show(struct IClass *cl, Object *obj, struct MUIP_Show *msg)
{
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);

	data->inbetween_show = 1;
	CalcVisible(data,obj);
  DoMethod(_win(obj),MUIM_Window_AddEventHandler, &data->ehn);

  data->threepoints_width = TextLength(&data->rp,"...",3);
	return 1;
}

/*************************************************************************
 MUIM_Hide
*************************************************************************/
STATIC ULONG MailTreelist_Hide(struct IClass *cl, Object *obj, struct MUIP_Hide *msg)
{
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);
  DoMethod(_win(obj),MUIM_Window_RemEventHandler, &data->ehn);
	data->inbetween_show = 0;
	return 1;
}

/*************************************************************************
 MUIM_Draw
*************************************************************************/
STATIC ULONG MailTreelist_Draw(struct IClass *cl, Object *obj, struct MUIP_Draw *msg)
{
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);

	int start,cur,end;
	int y;

	if (data->quiet)
		return 0;

	DoSuperMethodA(cl,obj,(Msg)msg);

	SetFont(_rp(obj),_font(obj));

	start = 0;
	end = MIN(start + data->entries_visible, data->entries_num);
	y = _mtop(obj);

	for (cur = start; cur < end; cur++)
	{
		if (cur == data->entries_active)
		{
			data->quiet++;
			set(obj, MUIA_Background, MUII_ListCursor);
		}

		DoMethod(obj, MUIM_DrawBackground, _mleft(obj), y, _mwidth(obj), data->entry_maxheight, 0,0);
		DrawEntry(data,obj,cur,y);

		if (cur == data->entries_active)
		{
			set(obj, MUIA_Background, MUII_ListBack);
			data->quiet--;
		}

		y += data->entry_maxheight;
	}

	if (y <= _mbottom(obj))
	{
		DoMethod(obj, MUIM_DrawBackground, _mleft(obj), y, _mwidth(obj), _mbottom(obj) - y + 1, 0,0);
	}

	return 0;
}

/*************************************************************************
 MUIM_MailTreelist_Clear
*************************************************************************/
STATIC ULONG MailTreelist_Clear(struct IClass *cl, Object *obj, Msg msg)
{
	int i;
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);

	for (i=0;i<data->entries_num;i++)
	{
		if (data->entries[i])
			FreeListEntry(data,data->entries[i]);
	}

	SetListSize(data,0);
	data->entries_num = 0;
	data->entries_active = -1;
	return 1;
}

/*************************************************************************
 MUIM_MailTreelist_SetFolderMails
*************************************************************************/
STATIC ULONG MailTreelist_SetFolderMails(struct IClass *cl, Object *obj, struct MUIP_MailTreelist_SetFolderMails *msg)
{
	struct folder *f;
	int i,num_mails;
	void *handle;
	struct mail_info *m;
	struct MailTreelist_Data *data;

	/* Clear previous contents */
	MailTreelist_Clear(cl,obj,(Msg)msg);
	if (!(f = msg->f)) return 1;

	data = (struct MailTreelist_Data*)INST_DATA(cl,obj);

	/* Nobody else must access this folder now */
	folder_lock(f);

	/* Determine number of mails, but ensure that index file is loaded */
	handle = NULL;
	folder_next_mail_info(f, &handle);
	num_mails = f->num_mails;

	if (!(SetListSize(data,num_mails)))
		return 0;

	i = 0;	/* current entry number */
	handle = NULL;
	while ((m = folder_next_mail_info(f, &handle)))
	{
		struct ListEntry *le;

		if (!(le = AllocListEntry(data)))
		{
			/* Panic */
			break;
		}

		le->mail_info = m;
		le->parents = 0;

		if (mail_get_status_type(m) == MAIL_STATUS_UNREAD) data->entries_active = i;
		data->entries[i++] = le;
	}

	/* folder can be accessed again */
	folder_unlock(f);

	/* i contains the number of sucessfully added mails */
	SM_DEBUGF(10,("Added %ld mails into list\n",i));
	data->entries_num = i;

	if (data->inbetween_setup)
	{
		CalcEntries(data,obj);
		if (data->inbetween_show)
		{
			CalcVisible(data,obj);
			MUI_Redraw(obj,MADF_DRAWOBJECT);
		}
	}

	return 1;
}

/*************************************************************************
 MUIM_MailTreelist_HandleEvent
*************************************************************************/
static ULONG MailTreelist_HandleEvent(struct IClass *cl, Object *obj, struct MUIP_HandleEvent *msg)
{
	struct MailTreelist_Data *data = INST_DATA(cl, obj);

	if (msg->imsg)
  {
		LONG mx = msg->imsg->MouseX - _mleft(obj);
		LONG my = msg->imsg->MouseY - _mtop(obj);

		switch (msg->imsg->Class)
		{
	    case    IDCMP_MOUSEBUTTONS:
	    				if (msg->imsg->Code == SELECTDOWN)
	    				{
	    					if (mx >= 0 && my >= 0 && mx < _mwidth(obj) && my < _mheight(obj))
	    					{
	    						int new_entry_active = my / data->entry_maxheight;

									if (new_entry_active < 0) new_entry_active = 0;
									else if (new_entry_active >= data->entries_num) new_entry_active = data->entries_num - 1;

									if (new_entry_active != data->entries_active)
									{
										data->entries_active = new_entry_active;
										MUI_Redraw(obj,MADF_DRAWOBJECT);
										IssueTreelistActiveNotify(cl,obj,data);
									}
	    					}
	    				}
	    				break;
		}
  }

	return 0;
}

/**************************************************************************/

STATIC BOOPSI_DISPATCHER(ULONG, MailTreelist_Dispatcher, cl, obj, msg)
{
	switch(msg->MethodID)
	{
		case	OM_NEW:						return MailTreelist_New(cl,obj,(struct opSet*)msg);
		case	OM_DISPOSE:				return MailTreelist_Dispose(cl,obj,msg);
		case	OM_SET:						return MailTreelist_Set(cl,obj,(struct opSet*)msg);
		case	OM_GET:						return MailTreelist_Get(cl,obj,(struct opGet*)msg);
		case	MUIM_Setup:				return MailTreelist_Setup(cl,obj,(struct MUIP_Setup*)msg);
		case	MUIM_Cleanup:			return MailTreelist_Cleanup(cl,obj,msg);
		case	MUIM_AskMinMax:		return MailTreelist_AskMinMax(cl,obj,(struct MUIP_AskMinMax*)msg);
		case	MUIM_Show:				return MailTreelist_Show(cl,obj,(struct MUIP_Show*)msg);
		case	MUIM_Hide:				return MailTreelist_Hide(cl,obj,(struct MUIP_Hide*)msg);
		case	MUIM_Draw:				return MailTreelist_Draw(cl,obj,(struct MUIP_Draw*)msg);
		case	MUIM_HandleEvent: return MailTreelist_HandleEvent(cl,obj,(struct MUIP_HandleEvent *)msg);

		case	MUIM_MailTreelist_Clear:					return MailTreelist_Clear(cl, obj, (APTR)msg);
		case	MUIM_MailTreelist_SetFolderMails: return MailTreelist_SetFolderMails(cl, obj, (APTR)msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

/**************************************************************************/

Object *MakeMailTreelist(ULONG userid, Object **list)
{
	return *list = MailTreelistObject,
					InputListFrame,
					MUIA_ObjectID, userid,
					End;
}

/**************************************************************************/

struct MUI_CustomClass *CL_MailTreelist;

int create_mailtreelist_class(void)
{
	SM_ENTER;
	if ((CL_MailTreelist = CreateMCC(MUIC_Area, NULL, sizeof(struct MailTreelist_Data), MailTreelist_Dispatcher)))
	{
		SM_DEBUGF(15,("Create CL_MailTreelist: 0x%lx\n",CL_MailTreelist));
		SM_RETURN(1,"%ld");
	}
	SM_DEBUGF(5,("FAILED! Create CL_MailTreelist\n"));
	SM_RETURN(0,"%ld");
}

void delete_mailtreelist_class(void)
{
	SM_ENTER;
	if (CL_MailTreelist)
	{
		if (MUI_DeleteCustomClass(CL_MailTreelist))
		{
			SM_DEBUGF(15,("Deleted CL_MailTreelist: 0x%lx\n",CL_MailTreelist));
			CL_MailTreelist = NULL;
		} else
		{
			SM_DEBUGF(5,("FAILED! Delete CL_MailTreelist: 0x%lx\n",CL_MailTreelist));
		}
	}
	SM_LEAVE;
}
