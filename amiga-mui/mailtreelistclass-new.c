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
#include <proto/layers.h>
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
#define MAX(a,b) ((a)>(b)?(a):(b))

#define MUIA_MailTreelist_VertScrollbar	MUIA_MailTreelist_Private

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

	LONG drawn_background; /* the last drawn backround for this entry, a MUII_xxx value */
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
#define COLUMN_TYPE_STATUS  3

#define COLUMN_FLAG_AUTOWIDTH (1L << 0)

struct MailTreelist_Data
{
	APTR pool;

	int inbetween_setup;
	int inbetween_show;
	int mouse_pressed;
  struct MUI_EventHandlerNode ehn_mousebuttons;
  struct MUI_EventHandlerNode ehn_mousemove;

	Object *vert_scroller; /* attached vertical scroller */

	struct dt_node *images[IMAGE_MAX];

	/* List managment, currently we use a simple flat array, which is not good if many entries are inserted/deleted */
	LONG entries_num; /* Number of Entries in the list (excludes title) */
	LONG entries_allocated;
	struct ListEntry **entries;

	LONG entries_first; /* first visible entry */
	LONG entries_visible; /* number of visible entries */
	LONG entries_active;
	LONG entries_minselected;
	LONG entries_maxselected;

	LONG entry_maxheight; /* Max height of an list entry */
	LONG title_height;

	struct ColumnInfo ci[MAX_COLUMNS];
	
	LONG threepoints_width; /* Width of ... */

	char buf[2048];

	int quiet; /* needed for rendering, if > 0, don't call super method */
	int make_visible;

	int drawupdate; /* 1 - selection changed, 2 - first changed */
	int drawupdate_old_first;

	struct RastPort rp; /* Rastport for font calculations */
	
	/* double click */
	ULONG last_secs;
	ULONG last_mics;
	ULONG last_active;

	/* buffering */
	struct Layer *buffer_layer;
	struct Layer_Info *buffer_li;
	struct BitMap *buffer_bmap;
	struct RastPort *buffer_rp;
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

static void IssueTreelistDoubleClickNotify(struct IClass *cl, Object *obj, struct MailTreelist_Data *data)
{
	struct TagItem tags[2];

	tags[0].ti_Tag = MUIA_MailTreelist_DoubleClick;
	tags[0].ti_Data = TRUE;

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

STATIC VOID GetStatusImages(struct mail_info *m, int *images, int *used_images_ptr)
{
	int used_images = *used_images_ptr;

	if (m->flags & MAIL_FLAGS_AUTOSPAM)
	{
		images[used_images++] = IMAGE_NEW_SPAM;
	} else
	if (m->flags & MAIL_FLAGS_NEW)
	{
		if (mail_info_is_spam(m)) images[used_images++] = IMAGE_NEW_SPAM;
		else if (m->flags & MAIL_FLAGS_PARTIAL)	images[used_images++] = IMAGE_NEW_PARTIAL;
		else images[used_images++] = IMAGE_NEW;
	} else
	{
		if (mail_is_spam(m)) images[used_images++] = IMAGE_UNREAD_SPAM;
		else if ((m->flags & MAIL_FLAGS_NORCPT) /*&& data->folder_type == FOLDER_TYPE_SEND*/) images[used_images++] = IMAGE_NORCPT;
		else
		{
			if (m->flags & MAIL_FLAGS_PARTIAL)
			{
				switch (mail_get_status_type(m))
				{
					case MAIL_STATUS_READ: images[used_images++] = IMAGE_READ_PARTIAL; break;
					case MAIL_STATUS_REPLIED: images[used_images++] = IMAGE_REPLY_PARTIAL; break;
					default: images[used_images++] = IMAGE_UNREAD_PARTIAL; break;
				} 
			} else
			{
				switch (mail_get_status_type(m))
				{
					case	MAIL_STATUS_UNREAD: images[used_images++] = IMAGE_UNREAD; break;
					case	MAIL_STATUS_READ: images[used_images++] = IMAGE_READ; break;
					case	MAIL_STATUS_WAITSEND: images[used_images++] = IMAGE_WAITSEND;break;
					case	MAIL_STATUS_SENT: images[used_images++] = IMAGE_SENT;break;
					case	MAIL_STATUS_HOLD: images[used_images++] = IMAGE_HOLD;break;
					case	MAIL_STATUS_REPLIED: images[used_images++] = IMAGE_REPLY;break;
					case	MAIL_STATUS_FORWARD: images[used_images++] = IMAGE_FORWARD;break;
					case	MAIL_STATUS_ERROR: images[used_images++] = IMAGE_ERROR;break;
				}
			}
		}
	}


	if (m->status & MAIL_STATUS_FLAG_MARKED) images[used_images++] = IMAGE_MARK;
	if (m->flags & MAIL_FLAGS_IMPORTANT) images[used_images++] = IMAGE_IMPORTANT;
	if (m->flags & MAIL_FLAGS_CRYPT) images[used_images++] = IMAGE_CRYPT;
	else
	{
		if (m->flags & MAIL_FLAGS_SIGNED) images[used_images++] = IMAGE_SIGNED;
		else if (m->flags & MAIL_FLAGS_ATTACH) images[used_images++] = IMAGE_ATTACH;
		if (mail_is_marked_as_deleted(m)) images[used_images++] = IMAGE_TRASHCAN;
	}
	*used_images_ptr = used_images;
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

	for (col=0;col<MAX_COLUMNS;col++)
	{
		/* Discard widths of auto width columns */
		if (data->ci[col].flags & COLUMN_FLAG_AUTOWIDTH)
			data->ci[col].width = 0;
	}

	for (i=0;i<data->entries_num;i++)
	{
		struct mail_info *m;

		m = data->entries[i]->mail_info;

		for (col=0;col<MAX_COLUMNS;col++)
		{
			if (data->ci[col].flags & COLUMN_FLAG_AUTOWIDTH)
			{
				int is_ascii7 = 1;
				char *txt = NULL;
				int used_images = 0;
				int images[10];
		
				switch (data->ci[col].type)
				{
					case	COLUMN_TYPE_STATUS:
								GetStatusImages(m,images,&used_images);
								break;

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
					struct TextExtent te;
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

						TextExtent(&data->rp, txt, strlen(txt), &te);
						new_width += te.te_Extent.MaxX - te.te_Extent.MinX + 1;
					} else
					{
						/* If new_width contains a non 0 integer, at least a image is available.
						 * Because there is no text available, we subtract the last space */
						if (new_width) new_width -= IMAGE_HORIZ_SPACE;
					}
					if (new_width > data->ci[col].width) data->ci[col].width = new_width;
				}

			}
		}
	}
}

/**************************************************************************
 Calc number of visible entries informs the scroller gadgets about this
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

	if (data->vert_scroller)
	{
		set(data->vert_scroller, MUIA_Prop_Visible, data->entries_visible);
		if (data->entries_first + data->entries_visible > data->entries_num)
		{
			data->entries_first = data->entries_num - data->entries_visible;
			if (data->entries_first < 0) data->entries_first = 0;
		}
	}
}

/**************************************************************************
 Ensure that active entry is visible
**************************************************************************/
static void EnsureActiveEntryVisibility(struct MailTreelist_Data *data)
{
	if (data->entries_active != -1 && data->make_visible)
	{
		if (data->entries_active < data->entries_first) data->entries_first = data->entries_active;
		else if (data->entries_active >= data->entries_first + data->entries_visible) data->entries_first = data->entries_active - data->entries_visible + 1;
		data->make_visible = 0;
		nnset(data->vert_scroller, MUIA_Prop_First, data->entries_first);
	}
}

/**************************************************************************
 Draw an entry at entry_pos at the given y location. To draw the title,
 set pos to ENTRY_TITLE
**************************************************************************/
static void DrawEntry(struct MailTreelist_Data *data, Object *obj, int entry_pos, struct RastPort *rp, int x, int y)
{
	int col;
	int x1;
	int fonty;
	int entry_height;

	struct ListEntry *entry;
	struct mail_info *m;

	x1 = x;

	if (!(entry = data->entries[entry_pos]))
		return;

	m = entry->mail_info;
	fonty = _font(obj)->tf_YSize;
	entry_height = data->entry_maxheight;

	SetABPenDrMd(rp,_pens(obj)[MPEN_TEXT],0,JAM1);

	for (col = 0;col < MAX_COLUMNS; col++)
	{
		int col_width = data->ci[col].width;
		int is_ascii7 = 1;
		char *txt = NULL;
		int used_images = 0;
		int images[10];

		switch (data->ci[col].type)
		{
			case	COLUMN_TYPE_STATUS:
						GetStatusImages(m,images,&used_images);
						break;

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
			int available_col_width;		/* available width for the current column */
			int xstart = x1;

			available_col_width = col_width;

			/* put the images at first */
			for (cur_image = 0; cur_image < used_images; cur_image++)
			{
				struct dt_node *dt = data->images[images[cur_image]];
				if (dt)
				{
					int dtw = dt_width(dt);

					if (dtw <= available_col_width)
					{
						dt_put_on_rastport(dt,rp,xstart,y + (entry_height - dt_height(dt))/2);
						available_col_width -= dtw + IMAGE_HORIZ_SPACE;
						xstart += dtw + IMAGE_HORIZ_SPACE;
					} else
					{
						available_col_width = 0;
					}
				}
			}

			/* now put the text, but only if there is really space left */
			if (available_col_width > 0 && txt)
			{
				if (!is_ascii7)
				{
					utf8tostr(txt,data->buf,sizeof(data->buf),user.config.default_codeset);
					txt = data->buf;
				}
	
				txt_len = strlen(txt);
				fit = TextFit(rp,txt,txt_len,&te,NULL,1,available_col_width,fonty);
				if (fit < txt_len)
				{
					fit = TextFit(rp,txt,txt_len,&te,NULL,1,available_col_width - data->threepoints_width,fonty);
				}
	
				Move(rp,xstart,y + _font(obj)->tf_Baseline);
				Text(rp,txt,fit);
	
				if (fit < txt_len)
					Text(rp,"...",3);
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

	data->ci[0].type = COLUMN_TYPE_STATUS;
	data->ci[0].width = 150;
	data->ci[0].flags = COLUMN_FLAG_AUTOWIDTH;
	data->ci[1].type = COLUMN_TYPE_FROMTO;
	data->ci[1].width = 150;
	data->ci[1].flags = COLUMN_FLAG_AUTOWIDTH;
	data->ci[2].type = COLUMN_TYPE_SUBJECT;
	data->ci[2].width = 200;
	data->ci[2].flags = COLUMN_FLAG_AUTOWIDTH;

  data->ehn_mousebuttons.ehn_Events   = IDCMP_MOUSEBUTTONS;
  data->ehn_mousebuttons.ehn_Priority = 0;
  data->ehn_mousebuttons.ehn_Flags    = 0;
  data->ehn_mousebuttons.ehn_Object   = obj;
  data->ehn_mousebuttons.ehn_Class    = cl;

  data->ehn_mousemove.ehn_Events   = IDCMP_MOUSEMOVE;
  data->ehn_mousemove.ehn_Priority = 0;
  data->ehn_mousemove.ehn_Flags    = 0;
  data->ehn_mousemove.ehn_Object   = obj;
  data->ehn_mousemove.ehn_Class    = cl;

	if ((data->vert_scroller = (Object*)GetTagData(MUIA_MailTreelist_VertScrollbar,0,msg->ops_AttrList)))
	{
		DoMethod(data->vert_scroller, MUIM_Notify,  MUIA_Prop_First, MUIV_EveryTime, (ULONG)obj, 3, MUIM_Set, MUIA_MailTreelist_First, MUIV_TriggerValue);
	}

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
						break;

			case	MUIA_MailTreelist_First:
						{
							int new_entries_first = tidata;
							if (new_entries_first < 0) new_entries_first = 0;

							if (data->entries_first != new_entries_first)
							{
								data->drawupdate = 2;
								data->drawupdate_old_first = data->entries_first;
								data->entries_first = new_entries_first;
								MUI_Redraw(obj,MADF_DRAWUPDATE);
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

	data->entry_maxheight = _font(obj)->tf_YSize;

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
	ULONG depth;
	ULONG rc;

	rc = DoSuperMethodA(cl,obj,(Msg)msg);

	data->inbetween_show = 1;
	CalcVisible(data,obj);
  DoMethod(_win(obj),MUIM_Window_AddEventHandler, &data->ehn_mousebuttons);

  data->threepoints_width = TextLength(&data->rp,"...",3);

	/* Setup buffer for a single entry, double buffering is optional */
	depth = GetBitMapAttr(_screen(obj)->RastPort.BitMap,BMA_DEPTH);
	if ((data->buffer_bmap = AllocBitMap(_mwidth(obj),data->entry_maxheight,depth,BMF_MINPLANES,_screen(obj)->RastPort.BitMap)))
	{
		if ((data->buffer_li = NewLayerInfo()))
		{
			if ((data->buffer_layer = CreateBehindLayer(data->buffer_li, data->buffer_bmap,0,0,_mwidth(obj)-1,_mheight(obj)-1,LAYERSIMPLE,NULL)))
			{
				data->buffer_rp = data->buffer_layer->rp;
			}
		}
	}
	
	EnsureActiveEntryVisibility(data);

	return rc;
}

/*************************************************************************
 MUIM_Hide
*************************************************************************/
STATIC ULONG MailTreelist_Hide(struct IClass *cl, Object *obj, struct MUIP_Hide *msg)
{
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);
	
	if (data->buffer_layer)
	{
		DeleteLayer(0,data->buffer_layer);
		data->buffer_layer = NULL;
		data->buffer_rp = NULL;
	}

	if (data->buffer_li)
	{
		DisposeLayerInfo(data->buffer_li);
		data->buffer_li = NULL;
	}

	if (data->buffer_bmap)
	{
		WaitBlit();
		FreeBitMap(data->buffer_bmap);
		data->buffer_bmap = NULL;
	}
	
  DoMethod(_win(obj),MUIM_Window_RemEventHandler, &data->ehn_mousebuttons);
	data->inbetween_show = 0;
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

/*************************************************************************
 MUIM_Draw
*************************************************************************/
STATIC ULONG MailTreelist_Draw(struct IClass *cl, Object *obj, struct MUIP_Draw *msg)
{
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);

	struct RastPort *old_rp;
	struct RastPort *rp;
	APTR cliphandle;

	int start,cur,end;
	int y;
	int drawupdate;
	int background;

	if (data->quiet)
		return 0;

	DoSuperMethodA(cl,obj,(Msg)msg);

	if (msg->flags & MADF_DRAWUPDATE) drawupdate = data->drawupdate;
	else drawupdate = 0;

	data->drawupdate = 0;

	start = data->entries_first;
	end = start + data->entries_visible;
	y = _mtop(obj);

	/* If necessary, perform scrolling operations */
	if (drawupdate == 2)
	{
		int diffy = data->entries_first - data->drawupdate_old_first;
		int abs_diffy = abs(diffy);
		
		if (abs_diffy < data->entries_visible)
		{
			int scroll_caused_damage;

			/* Instally nobackfill layer hook, while scrolling */
			struct Hook *old_hook;

			old_hook = InstallLayerHook(_rp(obj)->Layer, LAYERS_NOBACKFILL);

	    scroll_caused_damage = (_rp(obj)->Layer->Flags & LAYERREFRESH) ? FALSE : TRUE;

	    ScrollRasterBF(_rp(obj), 0, diffy * data->entry_maxheight,
			 _mleft(obj), y,
			 _mright(obj), y + data->entry_maxheight * data->entries_visible - 1);

    	scroll_caused_damage = scroll_caused_damage && (_rp(obj)->Layer->Flags & LAYERREFRESH);

			InstallLayerHook(_rp(obj)->Layer,old_hook);

			if (scroll_caused_damage)
			{
				if (MUI_BeginRefresh(muiRenderInfo(obj), 0))
				{
					Object *o;
			    get(_win(obj),MUIA_Window_RootObject, &o);	       
	    		MUI_Redraw(o, MADF_DRAWOBJECT);
	    		MUI_EndRefresh(muiRenderInfo(obj), 0);
	    	}
				return 0;
			}

			if (diffy > 0)
	    {
	    	start = end - diffy;
	    	y += data->entry_maxheight * (data->entries_visible - diffy);
	    }
	    else end = start - diffy;
		}
	}

	/* Ensure validness of start and end */
	start = MAX(start, 0);
	end = MIN(end, data->entries_num);

	/* Render preparations */
	old_rp = _rp(obj);
	if (data->buffer_rp)
	{
		rp = data->buffer_rp;
		_rp(obj) = rp;
	} else
	{
		rp = _rp(obj);
		cliphandle = MUI_AddClipping(muiRenderInfo(obj),_mleft(obj),_mtop(obj),_mwidth(obj),_mheight(obj));
	}

	SetFont(rp,_font(obj));


	background = MUII_ListBack;
	data->quiet++;

	/* Draw all entries between start and end, their current background
	 * is stored
	 */
	for (cur = start; cur < end; cur++,y += data->entry_maxheight)
	{
		int new_background;
		struct ListEntry *le;

		le = data->entries[cur];

		if (cur == data->entries_active) new_background = (le->flags & LE_FLAG_SELECTED)?MUII_ListSelCur:MUII_ListCursor;
		else if (le->flags & LE_FLAG_SELECTED) new_background = MUII_ListSelect;
		else new_background = MUII_ListBack;

		if (drawupdate == 1 && le->drawn_background == new_background)
			continue;

		if (background != new_background)
		{
			set(obj, MUIA_Background, new_background);
			background = new_background;
		}

		if (data->buffer_rp)
		{
			DoMethod(obj, MUIM_DrawBackground, 0, 0, _mwidth(obj), data->entry_maxheight, 0,0);
			DrawEntry(data,obj,cur,rp,0,0);
			BltBitMapRastPort(data->buffer_bmap, 0, 0,
												old_rp, _mleft(obj), y, _mwidth(obj), data->entry_maxheight, 0xc0);
		} else
		{
			DoMethod(obj, MUIM_DrawBackground, _mleft(obj), y, _mwidth(obj), data->entry_maxheight, 0,0);
			DrawEntry(data,obj,cur,rp,_mleft(obj),y);
		}
	
		le->drawn_background = background;
	}

	/* Revert background if necessary */
	if (background != MUII_ListBack)
		set(obj, MUIA_Background, MUII_ListBack);
	data->quiet--;

	/* Revert render preparations */
	if (data->buffer_rp)
	{
		_rp(obj) = old_rp;
	} else
	{
		MUI_RemoveClipping(muiRenderInfo(obj),cliphandle);
	}

	/* erase stuff below only when rendering completly */
	if (y <= _mbottom(obj) && drawupdate == 0)
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
	data->entries_first = 0;
	data->entries_num = 0;
	data->entries_active = -1;
	data->entries_minselected = 0;
	data->entries_maxselected = -1;

	if (data->vert_scroller && !data->quiet)
	{
		set(data->vert_scroller,MUIA_Prop_Entries,0);
	}
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

	data = (struct MailTreelist_Data*)INST_DATA(cl,obj);

	/* Clear previous contents */
	data->quiet++;
	MailTreelist_Clear(cl,obj,(Msg)msg);
	data->quiet--;
	if (!(f = msg->f))
	{
		if (data->vert_scroller) set(data->vert_scroller,MUIA_Prop_Entries,0);
		return 1;
	}

	/* Nobody else must access this folder now */
	folder_lock(f);

	/* Determine number of mails, but ensure that index file is loaded */
	handle = NULL;
	folder_next_mail_info(f, &handle);
	num_mails = f->num_mails;

	if (!(SetListSize(data,num_mails)))
	{
		folder_unlock(f);
		return 0;
	}

	/* We find about the active entry within the loop */
	data->entries_active = -1;

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
		le->flags = 0;

		if (mail_get_status_type(m) == MAIL_STATUS_UNREAD && data->entries_active == -1)
			data->entries_active = i;

		data->entries[i++] = le;
	}

	/* folder can be accessed again */
	folder_unlock(f);

	/* i contains the number of sucessfully added mails */
	SM_DEBUGF(10,("Added %ld mails into list\n",i));
	data->entries_num = i;

	if (data->vert_scroller) set(data->vert_scroller,MUIA_Prop_Entries,data->entries_num);

	data->make_visible = 1;

	if (data->inbetween_setup)
	{
		CalcEntries(data,obj);
		if (data->inbetween_show)
		{
			CalcVisible(data,obj);
			EnsureActiveEntryVisibility(data);

			MUI_Redraw(obj,MADF_DRAWOBJECT);
		}
	}

	IssueTreelistActiveNotify(cl, obj, data);
	return 1;
}

/*************************************************************************
 MUIM_MailTreelist_GetFirstSelected
*************************************************************************/
ULONG MailTreelist_GetFirstSelected(struct IClass *cl, Object *obj, struct MUIP_MailTreelist_GetFirstSelected *msg)
{
	struct MailTreelist_Data *data = INST_DATA(cl, obj);
	struct mail_info *m;
	int *handle_ptr = (int*)msg->handle;
	int handle;

	if (data->entries_maxselected == -1)
	{
		if (data->entries_active != -1)
			handle = data->entries_active;
		else handle = -1;
	} else
	{
		handle = data->entries_minselected;
	}

	if (handle != -1) m = (struct mail_info*)data->entries[handle]->mail_info;
	*handle_ptr = handle;
	return (ULONG)m;
}

/*************************************************************************
 MUIM_MailTreelist_GetNextSelected
*************************************************************************/
ULONG MailTreelist_GetNextSelected(struct IClass *cl, Object *obj, struct MUIP_MailTreelist_GetNextSelected *msg)
{
	struct MailTreelist_Data *data = INST_DATA(cl, obj);
	int *handle_ptr = (int*)msg->handle;
	int handle = *handle_ptr;
	struct mail_info *m;

	if (data->entries_maxselected != -1 && handle != data->entries_maxselected)
	{
		int cur;

		/* Find out next selected entry */
		for (cur = handle+1; cur<=data->entries_maxselected; cur++)
			if (data->entries[cur]->flags & LE_FLAG_SELECTED) break;
		
		/* cur never is > data->entries_maxselected here, because data->entries_maxselected
		 * always is a selected entry and the above if condition would catch it */
		m = data->entries[cur]->mail_info;
		handle = cur;
	} else
	{
		handle = 0;
		m = NULL;
	}

	*handle_ptr = handle;
	return (ULONG)m;
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
	    						int new_entries_active;
	    						int selected_changed;

									new_entries_active = my / data->entry_maxheight + data->entries_first;
									if (new_entries_active < 0) new_entries_active = 0;
									else if (new_entries_active >= data->entries_num) new_entries_active = data->entries_num - 1;

									/* Unselected entries if some have been selected */
									if (data->entries_maxselected != -1)
									{
										int cur;
										
										for (cur = data->entries_minselected;cur <= data->entries_maxselected; cur++)
											data->entries[cur]->flags &= ~LE_FLAG_SELECTED;

										selected_changed = 1;			
										data->entries_minselected = 0;
										data->entries_maxselected = -1;
									} else selected_changed = 0;

									if (new_entries_active != data->entries_active || selected_changed)
									{
										data->entries_active = new_entries_active;

										/* Refresh */
										data->drawupdate = 1;
										MUI_Redraw(obj,MADF_DRAWUPDATE);

										IssueTreelistActiveNotify(cl,obj,data);
									} else
									{
										if (data->entries_active != -1)
										{
											if (DoubleClick(data->last_secs,data->last_mics,msg->imsg->Seconds,msg->imsg->Micros))
											{
												IssueTreelistDoubleClickNotify(cl,obj,data);
											}
										}
									}

									data->last_mics = msg->imsg->Micros;
									data->last_secs = msg->imsg->Seconds;
									data->last_active = new_entries_active;

									/* Enable mouse move notifies */
							  	data->mouse_pressed = 1;
							    DoMethod(_win(obj),MUIM_Window_AddEventHandler, &data->ehn_mousemove);
	    					}
	    				} else if (msg->imsg->Code == SELECTUP && data->mouse_pressed)
	    				{
								/* Disable mouse move notifies */
							  DoMethod(_win(obj),MUIM_Window_RemEventHandler, &data->ehn_mousemove);
							  data->mouse_pressed = 0;
	    				}
	    				break;

			case		IDCMP_MOUSEMOVE:
    					{
    						int new_entries_active, old_entries_active;

    						old_entries_active = data->entries_active;

								if (old_entries_active != -1)
								{
									new_entries_active = my / data->entry_maxheight + data->entries_first;
									if (new_entries_active < 0) new_entries_active = 0;
									else if (new_entries_active >= data->entries_num) new_entries_active = data->entries_num - 1;
	    						
									if (new_entries_active != old_entries_active)
									{
										int start,cur,end;
										if (new_entries_active < old_entries_active)
										{
											start = new_entries_active;
											end = old_entries_active;
										} else
										{
											start = old_entries_active;
											end = new_entries_active;
										}

										/* flag entries as being selected */
										for (cur = start; cur <= end; cur++)
											data->entries[cur]->flags |= LE_FLAG_SELECTED;

										/* Update min/max selected and active element */
										if (start < data->entries_minselected) data->entries_minselected = start;
										if (end > data->entries_maxselected) data->entries_maxselected = end;
										data->entries_active = new_entries_active;

										/* Refresh */
										data->drawupdate = 1;
										MUI_Redraw(obj,MADF_DRAWUPDATE);

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
		case	MUIM_MailTreelist_GetFirstSelected: return MailTreelist_GetFirstSelected(cl, obj, (APTR)msg);
		case	MUIM_MailTreelist_GetNextSelected: return MailTreelist_GetNextSelected(cl, obj, (APTR)msg);

		default: return DoSuperMethodA(cl,obj,msg);
	}
}

/**************************************************************************/

Object *MakeMailTreelist(ULONG userid, Object **list)
{
	Object *scrollbar = ScrollbarObject, End;

	return HGroup,
			MUIA_Group_Spacing, 0,
			Child, *list = MailTreelistObject,
				InputListFrame,
				MUIA_ObjectID, userid,
				MUIA_MailTreelist_VertScrollbar, scrollbar,
				End,
			Child, scrollbar,
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
