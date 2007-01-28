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

#include <diskfont/diskfonttag.h>
#include <libraries/iffparse.h>

#include <clib/alib_protos.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <proto/exec.h>
#include <proto/layers.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>
#include <proto/ttengine.h>

#include "configuration.h"
#include "folder.h"
#include "mail.h"
#include "smintl.h"
#include "simplemail.h"
#include "support_indep.h"

#include "amigasupport.h"
#include "compiler.h"
#include "datatypescache.h"
#include "debug.h"
#include "mailtreelistclass.h"
#include "muistuff.h"
#include "support.h"

/**************************************************************************/

#ifdef IDCMP_EXTENDEDMOUSE
#define HAVE_EXTENDEDMOUSE
#else
/* support for the newmouse wheel standard */
#include <devices/newmouse.h>
#endif

/**************************************************************************/

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

/**************************************************************************/

#define MENU_SETSTATUS_MARK   9
#define MENU_SETSTATUS_UNMARK 10
#define MENU_SETSTATUS_READ   11
#define MENU_SETSTATUS_UNREAD 12
#define MENU_SETSTATUS_HOLD	13
#define MENU_SETSTATUS_WAITSEND  14
#define MENU_SETSTATUS_SPAM 15
#define MENU_SETSTATUS_HAM 16
#define MENU_SPAMCHECK 17
#define MENU_DELETE 18
#define MENU_SETSTATUS_SENT 19
#define MENU_SETSTATUS_ERROR 20
#define MENU_SETSTATUS_REPLIED 21
#define MENU_SETSTATUS_FORWARD 22
#define MENU_SETSTATUS_REPLFORW 23
#define MENU_RESET_THIS_COLUMN_WIDTH 24
#define MENU_RESET_ALL_COLUMN_WIDTHS 25

/**************************************************************************/

#if 0
/***********************************************************************
 Open the given text font as a ttengine font. Returns NULL on failure
 (e.g. if no ttengine has been opened, or if given font is no ttf font)
************************************************************************/
STATIC APTR OpenTTEngineFont(struct TextFont *font)
{
	char filename[256];
	APTR ttengine_font = NULL;

	if (!TTEngineBase) return NULL;

	strcpy(filename,"FONTS:");
	if (AddPart(filename,font->tf_Message.mn_Node.ln_Name,sizeof(filename)))
	{
		char *ending = strrchr(filename,'.');
		if (strlen(ending) >= 5)
		{
			BPTR file;
			struct FileInfoBlock *fib;
			struct TagItem *otags;

			strcpy(&ending[1],"otag");
			if ((file = Open(filename,MODE_OLDFILE)))
			{
			  if ((fib = AllocDosObject(DOS_FIB, NULL)))
  			{
		      if (ExamineFH(file, fib))
    		  {
		        if ((otags = (struct TagItem *) AllocVec(fib->fib_Size, MEMF_CLEAR)))
    		    {
        		  if (Read(file, (UBYTE *) otags, fib->fib_Size))
          		{
		            if ((otags->ti_Tag == OT_FileIdent)               /* Test to see if */
    		            && (otags->ti_Data == (ULONG) fib->fib_Size)) /* the OTAG file  */
		            {                                                /* is valid.      */
		            	char *fontname;
									if ((fontname = (char*)GetTagData(OT_FontFile,0,otags)))
									{
										fontname += (ULONG)otags;
										ttengine_font = TT_OpenFont(
												TT_FontFile, fontname,
												TT_FontSize, font->tf_YSize - 3,
												TAG_DONE);
									}
		            }
              }
              FreeVec(otags);
            }
          }
          FreeDosObject(DOS_FIB,fib);
	      }
  	    Close(file);
			}
    }
  }
  return ttengine_font;
}
#endif

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

#define MUIA_MailTreelist_VertScrollbar						MUIA_MailTreelist_Private
#define MUIA_MailTreelist_HorizScrollbar					MUIA_MailTreelist_Private2
#define MUIA_MailTreelist_GroupOfHorizScrollbar		MUIA_MailTreelist_Private3
#define MUIA_MailTreelist_HorizontalFirst					MUIA_MailTreelist_Private4

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

#define COLUMN_FLAG_AUTOWIDTH (1L << 0)
#define COLUMN_FLAG_SORT1UP   (1L << 1)
#define COLUMN_FLAG_SORT1DOWN (1L << 2)

struct MailTreelist_Data
{
	APTR pool;

	int inbetween_setup;
	int inbetween_show;
	int mouse_pressed;
  struct MUI_EventHandlerNode ehn_mousebuttons;
  struct MUI_EventHandlerNode ehn_mousemove;

	Object *vert_scroller; /* attached vertical scroller */
	Object *horiz_scroller; /* attached vertical scroller */
	Object *horiz_scroller_group;

	struct dt_node *images[IMAGE_MAX];

	/* List managment, currently we use a simple flat array, which is not good if many entries are inserted/deleted */
	LONG entries_num; /* Number of Entries in the list (excludes title) */
	LONG entries_allocated;
	struct ListEntry **entries;

	LONG entries_first; /* first visible entry */
	LONG entries_visible; /* number of visible entries */
	LONG entries_active; /* index of active entry */
	LONG entries_minselected; /* the lowest index of any selected entry */ 
	LONG entries_maxselected; /* the highest index of any selected entry */

	LONG column_drag; /* -1 if no column is dragged */
	LONG column_drag_org_width;
	LONG column_drag_mx;

	LONG title_column_click;
	LONG title_column_click2;
	LONG right_title_click;

	LONG entry_maxheight; /* Max height of an list entry */
	LONG title_height;

	LONG horiz_first; /* first horizontal visible pixel */

	struct ColumnInfo ci[MAX_COLUMNS];
	int columns_active[MAX_COLUMNS]; /* Indices of active columns */
	int column_spacing;
	
	LONG threepoints_width; /* Width of ... */

	char buf[2048];
	char buf2[2048];
	char bubblehelp_buf[2048];

	int quiet; /* needed for rendering, if > 0, don't call super method */
	int make_visible;

	int drawupdate; /* 1 - selection changed, 2 - first changed */
	int drawupdate_old_first;

	struct RastPort rp; /* Rastport for font calculations */
	struct RastPort dragRP; /* Rastport for drag image rastport */

	APTR ttengine_font;
	int ttengine_baseline;

	/* double click */
	ULONG last_secs;
	ULONG last_mics;
	ULONG last_active;

	/* buffering */
	struct Layer *buffer_layer;
	struct Layer_Info *buffer_li;
	struct BitMap *buffer_bmap;
	struct RastPort *buffer_rp;
	
	/* translated strings (faster to hold the translation) */
	char *status_text;
	char *from_text;
	char *to_text;
	char *subject_text;
	char *reply_text;
	char *date_text;
	char *size_text;
	char *filename_text;
	char *pop3_text;
	char *received_text;
	
	/* other stuff */
	Object *context_menu;
	Object *title_menu;

	Object *show_from_item;
	Object *show_subject_item;
	Object *show_reply_item;
	Object *show_date_item;
	Object *show_size_item;
	Object *show_filename_item;
	Object *show_pop3_item;
	Object *show_recv_item;
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

/**************************************************************************
 Prepare the displayed types of columns
**************************************************************************/
static void PrepareDisplayedColumns(struct MailTreelist_Data *data)
{
	int pos;

	data->columns_active[0] = COLUMN_TYPE_STATUS;

	pos = 1;

	if (xget(data->show_from_item,MUIA_Menuitem_Checked)) data->columns_active[pos++] = COLUMN_TYPE_FROMTO; 
	if (xget(data->show_subject_item,MUIA_Menuitem_Checked)) data->columns_active[pos++] = COLUMN_TYPE_SUBJECT; 
	if (xget(data->show_reply_item,MUIA_Menuitem_Checked)) data->columns_active[pos++] = COLUMN_TYPE_REPLYTO; 
	if (xget(data->show_date_item,MUIA_Menuitem_Checked)) data->columns_active[pos++] = COLUMN_TYPE_DATE; 
	if (xget(data->show_size_item,MUIA_Menuitem_Checked))  data->columns_active[pos++] = COLUMN_TYPE_SIZE;
	if (xget(data->show_filename_item,MUIA_Menuitem_Checked))  data->columns_active[pos++] = COLUMN_TYPE_FILENAME;
	if (xget(data->show_pop3_item,MUIA_Menuitem_Checked))  data->columns_active[pos++] = COLUMN_TYPE_POP3;
	if (xget(data->show_recv_item,MUIA_Menuitem_Checked))  data->columns_active[pos++] = COLUMN_TYPE_RECEIVED;

	for (;pos < sizeof(data->columns_active)/sizeof(data->columns_active[0]);pos++)
		data->columns_active[pos++] = 0;
}

/**************************************************************************/


/**************************************************************************
 Allocate a single list entry and initialized it with default values
**************************************************************************/
static struct ListEntry *AllocListEntry(struct MailTreelist_Data *data)
{
	struct ListEntry *le = (struct ListEntry*)AllocPooled(data->pool, sizeof(struct ListEntry));
 	if (!le) return NULL;
	memset(le,0,sizeof(*le));
	return le;
}

/**************************************************************************
 Deallocate a single list entry, does not deinitialize it
**************************************************************************/
static void FreeListEntry(struct MailTreelist_Data *data, struct ListEntry *entry)
{
	FreePooled(data->pool, entry, sizeof(*entry));
}

/**************************************************************************
 Ensures that there can be at least the given amount of entries within
 the list. Returns 0 if not. It also allocates the space for the title.
 It can be accesses with data->entries[ENTRY_TITLE]. Can be used to
 increase the number of entries.
**************************************************************************/
static int SetListSize(struct MailTreelist_Data *data, LONG size)
{
	struct ListEntry **new_entries;
	int new_entries_allocated;
	
	SM_DEBUGF(10,("size=%ld allocated=%ld\n",size + 1, data->entries_allocated));
	
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
 Calc dimensions of given mail info. Return whether a width of a column
 has been changed.
**************************************************************************/
static int CalcEntry(struct MailTreelist_Data *data, Object *obj, struct mail_info * m)
{
	int col;
	int changed = 0;

	for (col=0;col<MAX_COLUMNS;col++)
	{
		int active;
		struct ColumnInfo *ci;
		
		active = data->columns_active[col];
		if (!active) continue;
		ci = &data->ci[active];

		if (ci->flags & COLUMN_FLAG_AUTOWIDTH)
		{
			int is_ascii7 = 1;
			char *txt = NULL;
			int used_images = 0;
			int images[10];
	
			switch (ci->type)
			{
				case	COLUMN_TYPE_STATUS:
							if (m)
							{
								GetStatusImages(m,images,&used_images);
							} else txt = data->status_text;
							break;

				case	COLUMN_TYPE_FROMTO:
							if (m)
							{
								if (m->flags & MAIL_FLAGS_GROUP) images[used_images++] = IMAGE_GROUP;
								GetFromText(m,&txt,&is_ascii7);
							}
							else txt = data->from_text;
							break;
	
				case	COLUMN_TYPE_SUBJECT:
							if (m)
							{
								txt = m->subject;
								is_ascii7 = !!(m->flags & MAIL_FLAGS_SUBJECT_ASCII7);
							}
							else txt = data->subject_text;
							break;

				case	COLUMN_TYPE_REPLYTO:
							if (m)
							{
								txt = m->reply_addr;
								is_ascii7 = !!(m->flags & MAIL_FLAGS_REPLYTO_ADDR_ASCII7);
							} else txt = data->reply_text;
							break;

				case	COLUMN_TYPE_DATE:
							if (m)
							{
								SecondsToString(data->buf2, m->seconds);
								txt = data->buf2;
								is_ascii7 = TRUE;
							} else txt = data->date_text;
							break;

			case	COLUMN_TYPE_FILENAME:
						if (m)
						{
							txt = m->filename;
							is_ascii7 = TRUE;
						} else txt = data->filename_text;
						break;

			case	COLUMN_TYPE_SIZE:
						if (m)
						{
							sprintf(data->buf2,"%d",m->size);
							is_ascii7 = TRUE;
							txt = data->buf2;
						} else txt = data->size_text;
						break;

			case	COLUMN_TYPE_POP3:
						if (m)
						{
							txt = m->pop3_server;
							is_ascii7 = TRUE;
						} else txt = data->pop3_text;
						break;

			case	COLUMN_TYPE_RECEIVED:
						if (m)
						{
							SecondsToString(data->buf2, m->received);
							txt = data->buf2;
							is_ascii7 = TRUE;
						} else txt = data->received_text;
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
					if (data->ttengine_font)
					{
						TT_TextExtent(&data->rp, txt, utf8len(txt), &te);
						new_width += te.te_Extent.MaxX - te.te_Extent.MinX + 1;
					} else
					{
						if (!is_ascii7)
						{
							utf8tostr(txt,data->buf,sizeof(data->buf),user.config.default_codeset);
							txt = data->buf;
						}

						TextExtent(&data->rp, txt, strlen(txt), &te);
						new_width += te.te_Extent.MaxX - te.te_Extent.MinX + 1;
					}
				} else
				{
					/* If new_width contains a non 0 integer, at least a image is available.
					 * Because there is no text available, we subtract the last space */
					if (new_width) new_width -= IMAGE_HORIZ_SPACE;
				}
				if (new_width > ci->width)
				{
					ci->width = new_width;
					changed = 1;
				}
			}
		}
	}
	return changed;
}

/**************************************************************************
 Calc entire entries dimensions
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

	/* Title */
	CalcEntry(data, obj, NULL);

	for (i=0;i<data->entries_num;i++)
	{
		CalcEntry(data, obj, data->entries[i]->mail_info);
	}
}

/**************************************************************************
 Calc number of visible entries informs the scroller gadgets about this
**************************************************************************/
static void CalcVisible(struct MailTreelist_Data *data, Object *obj)
{
	if (data->entry_maxheight)
	{
		data->entries_visible = (_mheight(obj) - data->title_height)/data->entry_maxheight;
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
 Calculate the total horizontal space required by the elements.
**************************************************************************/
static void CalcHorizontalTotal(struct MailTreelist_Data *data)
{
	if (data->horiz_scroller)
	{
		int col;
		int total_width = 0;

		for (col = 0;col < MAX_COLUMNS; col++)
		{
			int active;
			struct ColumnInfo *ci;

			active = data->columns_active[col];
			if (!active) continue;
			ci = &data->ci[active];

			total_width += ci->width + data->column_spacing;
		}

		total_width -= data->column_spacing;

		set(data->horiz_scroller, MUIA_Prop_Entries, total_width);
	}
}

/**************************************************************************
 Calculate the visible horizontal space
**************************************************************************/
static void CalcHorizontalVisible(struct MailTreelist_Data *data, Object *obj)
{
	if (data->horiz_scroller)
	{
		int total = xget(data->horiz_scroller, MUIA_Prop_Entries);
		if (data->horiz_scroller_group)
		{
			DoMethod(_app(obj), MUIM_Application_PushMethod, data->horiz_scroller_group, 3, MUIM_Set, MUIA_ShowMe, !!(total > _mwidth(obj)));
		}

		set(data->horiz_scroller, MUIA_Prop_Visible, _mwidth(obj));
	}
}

/**************************************************************************
 Draw an entry at entry_pos at the given y location.
**************************************************************************/
static void DrawEntry(struct MailTreelist_Data *data, Object *obj, int entry_pos, struct RastPort *rp, int x, int y)
{
	int col;
	int x1;
	int fonty;
	int entry_height;
	int prev_active; /* previously drawed column */

	struct ListEntry *entry;
	struct mail_info *m;

	int first = 1;

	x1 = x;

	if (!(entry = data->entries[entry_pos]))
		return;

	m = entry->mail_info;
	fonty = _font(obj)->tf_YSize;
	entry_height = data->entry_maxheight;
	prev_active = -1;

	SetDrMd(rp,JAM1);

	for (col = 0;col < MAX_COLUMNS; col++)
	{
		int col_width;
		int is_ascii7;
		char *txt;
		int used_images;
		int images[10];

		int active;
		struct ColumnInfo *ci;

		active = data->columns_active[col];
		if (!active) continue;
		ci = &data->ci[active];

		if (!first)
		{
			int shine;
			int shadow;

			if (data->column_drag != -1 && data->column_drag == prev_active)
			{
				shadow = _pens(obj)[MPEN_SHINE];
				shine = _pens(obj)[MPEN_SHADOW];
			} else
			{
				shine = _pens(obj)[MPEN_SHINE];
				shadow = _pens(obj)[MPEN_SHADOW];
			}

			SetAPen(rp,shadow);
			Move(rp, x1-3, y);
			Draw(rp, x1-3, y + entry_height - 1);
			SetAPen(rp,shine);
			Move(rp, x1-2, y);
			Draw(rp, x1-2, y + entry_height - 1);
		} else first = 0;

		SetAPen(rp,_pens(obj)[MPEN_TEXT]);

		col_width = ci->width;
		is_ascii7 = 1;
		txt = NULL;
		used_images = 0;

		switch (ci->type)
		{
			case	COLUMN_TYPE_STATUS:
						if (m)
						{
							GetStatusImages(m,images,&used_images);
						} else txt = data->status_text;
						break;

			case	COLUMN_TYPE_FROMTO:
						if (m)
						{
							if (m->flags & MAIL_FLAGS_GROUP) images[used_images++] = IMAGE_GROUP;
							GetFromText(m,&txt,&is_ascii7);
						}
						else txt = data->from_text;
						break;

			case	COLUMN_TYPE_SUBJECT:
						if (m)
						{
							txt = m->subject;
							is_ascii7 = !!(m->flags & MAIL_FLAGS_SUBJECT_ASCII7);
						}
						else txt = data->subject_text;
						break;

			case	COLUMN_TYPE_REPLYTO:
						if (m)
						{
							txt = m->reply_addr;
							is_ascii7 = !!(m->flags & MAIL_FLAGS_REPLYTO_ADDR_ASCII7);
						} else txt = data->reply_text;
						break;

			case	COLUMN_TYPE_DATE:
						if (m)
						{
							SecondsToString(data->buf2, m->seconds);
							txt = data->buf2;
							is_ascii7 = TRUE;
						} else txt = data->date_text;
						break;

			case	COLUMN_TYPE_FILENAME:
						if (m)
						{
							txt = m->filename;
							is_ascii7 = TRUE;
						} else txt = data->filename_text;
						break;

			case	COLUMN_TYPE_SIZE:
						if (m)
						{
							sprintf(data->buf2,"%d",m->size);
							is_ascii7 = TRUE;
							txt = data->buf2;
						} else txt = data->size_text;
						break;

			case	COLUMN_TYPE_POP3:
						if (m)
						{
							txt = m->pop3_server;
							is_ascii7 = TRUE;
						} else txt = data->pop3_text;
						break;

			case	COLUMN_TYPE_RECEIVED:
						if (m)
						{
							SecondsToString(data->buf2, m->received);
							txt = data->buf2;
							is_ascii7 = TRUE;
						} else txt = data->received_text;
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
				if (data->ttengine_font)
				{
					Move(rp,xstart,y + data->ttengine_baseline);

					/* use ttengine functions */
					txt_len = utf8len(txt);
					fit = TT_TextFit(rp,txt,txt_len,&te,NULL,1,available_col_width,fonty);
					if (fit < txt_len)
					{
						fit = TT_TextFit(rp,txt,txt_len,&te,NULL,1,available_col_width - data->threepoints_width,fonty);
					}

					TT_Text(rp,txt,fit);
		
					if (fit < txt_len)
						TT_Text(rp,"...",3);
				} else
				{
					Move(rp,xstart,y + _font(obj)->tf_Baseline);

					/* use amiga os text functions to bring up the text  */
					if (!is_ascii7)
					{
						utf8tostr(txt,data->buf,sizeof(data->buf),user.config.default_codeset);
						txt = data->buf;
					}
		
					txt_len = strlen(txt);
					fit = TextFit(rp,txt,txt_len,&te,NULL,1,available_col_width,fonty);
					if (fit < txt_len && (available_col_width > data->threepoints_width))
					{
						fit = TextFit(rp,txt,txt_len,&te,NULL,1,available_col_width - data->threepoints_width,fonty);
					}
		
					Text(rp,txt,fit);
		
					if (fit < txt_len && (available_col_width > data->threepoints_width))
						Text(rp,"...",3);
				}
			}
		}

		x1 += col_width + data->column_spacing;
		prev_active = active;
	}
}

/**************************************************************************
 Reset all column widths
**************************************************************************/
static void MailTreelist_ResetAllColumnWidth(struct MailTreelist_Data *data, Object *obj)
{
	int i;

	for (i=0;i<MAX_COLUMNS;i++)
	{
		data->ci[i].width = 0;
		data->ci[i].flags |= COLUMN_FLAG_AUTOWIDTH; 
	}
	
	CalcEntries(data,obj);
	CalcHorizontalTotal(data);
	MUI_Redraw(obj,MADF_DRAWOBJECT);
}

/**************************************************************************
 Reset column width
**************************************************************************/
static void MailTreelist_ResetClickedColumnWidth(struct MailTreelist_Data *data, Object *obj)
{
	if (data->right_title_click != -1)
	{
		data->ci[data->right_title_click].width = 0;
		data->ci[data->right_title_click].flags |= COLUMN_FLAG_AUTOWIDTH; 

		CalcEntries(data,obj);
		CalcHorizontalTotal(data);
		MUI_Redraw(obj,MADF_DRAWOBJECT);

		/* Reset the state */
		data->right_title_click = -1;
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
		MUIA_Font, MUIV_Font_List,
		MUIA_ShortHelp, TRUE,
		MUIA_ContextMenu, 1,
		TAG_MORE,msg->ops_AttrList)))
		return 0;

	data = (struct MailTreelist_Data*)INST_DATA(cl,obj);

	if (!(data->pool = CreatePool(MEMF_ANY,16384,16384)))
	{
		CoerceMethod(cl,obj,OM_DISPOSE);
		return 0;
	}

	/* Title preparations */
	if (!(SetListSize(data, 0)))
	{
		CoerceMethod(cl,obj,OM_DISPOSE);
		return 0;
	}
	if (!(data->entries[ENTRY_TITLE] = AllocListEntry(data)))
	{
		CoerceMethod(cl,obj,OM_DISPOSE);
		return 0;
	}

	data->status_text = _("Status");
	data->from_text = _("From");
	data->to_text = _("To");
	data->subject_text = _("Subject");
	data->reply_text = _("Reply");
	data->date_text = _("Date");
	data->size_text = _("Size");
	data->filename_text = _("Filename");
	data->pop3_text = _("POP3 Server");
	data->received_text = _("Received");

	data->ci[COLUMN_TYPE_STATUS].type = COLUMN_TYPE_STATUS;
	data->ci[COLUMN_TYPE_STATUS].width = 50;
	data->ci[COLUMN_TYPE_STATUS].flags = COLUMN_FLAG_AUTOWIDTH;

	data->ci[COLUMN_TYPE_FROMTO].type = COLUMN_TYPE_FROMTO;
	data->ci[COLUMN_TYPE_FROMTO].width = 150;
	data->ci[COLUMN_TYPE_FROMTO].flags = COLUMN_FLAG_AUTOWIDTH;

	data->ci[COLUMN_TYPE_SUBJECT].type = COLUMN_TYPE_SUBJECT;
	data->ci[COLUMN_TYPE_SUBJECT].width = 200;
	data->ci[COLUMN_TYPE_SUBJECT].flags = COLUMN_FLAG_AUTOWIDTH;

	data->ci[COLUMN_TYPE_REPLYTO].type = COLUMN_TYPE_REPLYTO;
	data->ci[COLUMN_TYPE_REPLYTO].width = 200;
	data->ci[COLUMN_TYPE_REPLYTO].flags = COLUMN_FLAG_AUTOWIDTH;

	data->ci[COLUMN_TYPE_DATE].type = COLUMN_TYPE_DATE;
	data->ci[COLUMN_TYPE_DATE].width = 200;
	data->ci[COLUMN_TYPE_DATE].flags = COLUMN_FLAG_AUTOWIDTH;

	data->ci[COLUMN_TYPE_SIZE].type = COLUMN_TYPE_SIZE;
	data->ci[COLUMN_TYPE_SIZE].width = 60;
	data->ci[COLUMN_TYPE_SIZE].flags = COLUMN_FLAG_AUTOWIDTH;

	data->ci[COLUMN_TYPE_FILENAME].type = COLUMN_TYPE_FILENAME;
	data->ci[COLUMN_TYPE_FILENAME].width = 200;
	data->ci[COLUMN_TYPE_FILENAME].flags = COLUMN_FLAG_AUTOWIDTH;

	data->ci[COLUMN_TYPE_POP3].type = COLUMN_TYPE_POP3;
	data->ci[COLUMN_TYPE_POP3].width = 200;
	data->ci[COLUMN_TYPE_POP3].flags = COLUMN_FLAG_AUTOWIDTH;

	data->ci[COLUMN_TYPE_RECEIVED].type = COLUMN_TYPE_RECEIVED;
	data->ci[COLUMN_TYPE_RECEIVED].width = 200;
	data->ci[COLUMN_TYPE_RECEIVED].flags = COLUMN_FLAG_AUTOWIDTH;

	PrepareDisplayedColumns(data);
	data->column_spacing = 4;

#ifdef HAVE_EXTENDEDMOUSE
  data->ehn_mousebuttons.ehn_Events   = IDCMP_MOUSEBUTTONS|IDCMP_EXTENDEDMOUSE;
#else
	/* NewMouse standard wheel support */
  data->ehn_mousebuttons.ehn_Events   = IDCMP_MOUSEBUTTONS|IDCMP_RAWKEY;
#endif
  data->ehn_mousebuttons.ehn_Priority = 0;
  data->ehn_mousebuttons.ehn_Flags    = 0;
  data->ehn_mousebuttons.ehn_Object   = obj;
  data->ehn_mousebuttons.ehn_Class    = cl;

  data->ehn_mousemove.ehn_Events   = IDCMP_MOUSEMOVE;
  data->ehn_mousemove.ehn_Priority = 0;
  data->ehn_mousemove.ehn_Flags    = 0;
  data->ehn_mousemove.ehn_Object   = obj;
  data->ehn_mousemove.ehn_Class    = cl;

	data->column_drag = -1;
	data->title_column_click = -1;
	data->title_column_click2 = -1;

	if ((data->vert_scroller = (Object*)GetTagData(MUIA_MailTreelist_VertScrollbar,0,msg->ops_AttrList)))
	{
		DoMethod(data->vert_scroller, MUIM_Notify,  MUIA_Prop_First, MUIV_EveryTime, (ULONG)obj, 3, MUIM_Set, MUIA_MailTreelist_First, MUIV_TriggerValue);
	}

	if ((data->horiz_scroller = (Object*)GetTagData(MUIA_MailTreelist_HorizScrollbar,0,msg->ops_AttrList)))
	{
		DoMethod(data->horiz_scroller, MUIM_Notify,  MUIA_Prop_First, MUIV_EveryTime, (ULONG)obj, 3, MUIM_Set, MUIA_MailTreelist_HorizontalFirst, MUIV_TriggerValue);
	}

	data->horiz_scroller_group = (Object*)GetTagData(MUIA_MailTreelist_GroupOfHorizScrollbar,0,msg->ops_AttrList);

	data->title_menu = MenustripObject,
		Child, MenuObjectT(_("Mail Settings")),
			Child, data->show_from_item = MenuitemObject, MUIA_ObjectID, MAKE_ID('M','S','F','T'),MUIA_Menuitem_Title, _("Show From/To?"), MUIA_UserData, 1, MUIA_Menuitem_Checked, TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
			Child, data->show_subject_item = MenuitemObject, MUIA_ObjectID, MAKE_ID('M','S','S','B'),MUIA_Menuitem_Title, _("Show Subject?"), MUIA_UserData, 2, MUIA_Menuitem_Checked, TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
			Child, data->show_reply_item = MenuitemObject, MUIA_ObjectID, MAKE_ID('M','S','R','T'),MUIA_Menuitem_Title, _("Show Reply-To?"), MUIA_UserData, 3, MUIA_Menuitem_Checked, TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
			Child, data->show_date_item = MenuitemObject, MUIA_ObjectID, MAKE_ID('M','S','D','T'),MUIA_Menuitem_Title, _("Show Date?"), MUIA_UserData, 4, MUIA_Menuitem_Checked, TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
			Child, data->show_size_item = MenuitemObject, MUIA_ObjectID, MAKE_ID('M','S','S','Z'),MUIA_Menuitem_Title, _("Show Size?"), MUIA_UserData, 5, MUIA_Menuitem_Checked, TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
			Child, data->show_filename_item = MenuitemObject, MUIA_ObjectID, MAKE_ID('M','S','F','N'), MUIA_Menuitem_Title, _("Show Filename?"), MUIA_UserData, 6,  MUIA_Menuitem_Checked, TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
			Child, data->show_pop3_item = MenuitemObject, MUIA_ObjectID, MAKE_ID('M','S','P','3'),MUIA_Menuitem_Title, _("Show POP3 Server?"), MUIA_UserData, 7, MUIA_Menuitem_Checked, TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
			Child, data->show_recv_item = MenuitemObject, MUIA_ObjectID, MAKE_ID('M','S','R','V'), MUIA_Menuitem_Title, _("Show Received?"), MUIA_UserData, 8,  MUIA_Menuitem_Checked, TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
			Child, MenuitemObject, MUIA_Menuitem_Title, -1, End,
			Child, MenuitemObject, MUIA_Menuitem_Title, _("Reset this column's width"), MUIA_UserData, MENU_RESET_THIS_COLUMN_WIDTH, End,
			Child, MenuitemObject, MUIA_Menuitem_Title, _("Reset all columns' widths"), MUIA_UserData, MENU_RESET_ALL_COLUMN_WIDTHS, End,
//			Child, MenuitemObject, MUIA_Menuitem_Title, _("Default Order"), MUIA_UserData, MUIV_NList_Menu_DefOrder_All, End,
			End,
		End;

	return (ULONG)obj;
}

/*************************************************************************
 OM_DISPOSE
*************************************************************************/
STATIC ULONG MailTreelist_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);
	if (data->pool) DeletePool(data->pool);
	if (data->context_menu) MUI_DisposeObject(data->context_menu);
	if (data->title_menu) MUI_DisposeObject(data->title_menu);

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
									data->drawupdate = 1;
									MUI_Redraw(obj,MADF_DRAWUPDATE);
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
								if (data->vert_scroller) set(data->vert_scroller, MUIA_Prop_First, data->entries_first);
								MUI_Redraw(obj,MADF_DRAWUPDATE);
							}
						}
						break;

			case	MUIA_MailTreelist_HorizontalFirst:
						{
							data->horiz_first = tidata;
							if (data->horiz_first < 0) data->horiz_first = 0;
							MUI_Redraw(obj, MADF_DRAWUPDATE);
						}
						break;

			case	MUIA_MailTreelist_TitleMark:
						{
							int col;
							int decreasing;

				  		/* Clear all sorting flags before setting the selected one */
							for (col = 0;col < MAX_COLUMNS; col++)
								data->ci[col].flags &= ~(COLUMN_FLAG_SORT1DOWN|COLUMN_FLAG_SORT1UP);

							decreasing = !!(tidata & MUIV_MailTreelist_TitleMark_Decreasing);
							col = tidata & (~MUIV_MailTreelist_TitleMark_Decreasing);
							if (col > 0 && col < MAX_COLUMNS)
							{
								if (decreasing)
									data->ci[col].flags |= COLUMN_FLAG_SORT1DOWN;
								else
									data->ci[col].flags |= COLUMN_FLAG_SORT1UP;
							}
							MUI_Redraw(obj, MADF_DRAWOBJECT);
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

	if (msg->opg_AttrID == MUIA_MailTreelist_TitleClick)
	{
		*msg->opg_Storage = data->title_column_click;
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

	/* Setup rastports */
	InitRastPort(&data->rp);
  SetFont(&data->rp,_font(obj));

	InitRastPort(&data->dragRP);
  SetFont(&data->dragRP,_font(obj));

	/* Find out, if the supplied font is a ttf font, and open it as a ttengine
	 * font */
#if 0
	if ((data->ttengine_font = OpenTTEngineFont(_font(obj))))
	{
		ULONG val;

		TT_SetFont(&data->rp, data->ttengine_font);
		TT_SetAttrs(&data->rp,
					TT_Screen, _screen(obj),
					TT_Encoding, TT_Encoding_UTF8,
/*					TT_Antialias, TT_Antialias_On,*/
					TAG_DONE);
		TT_GetAttrs(&data->rp, TT_FontMaxTop, &val);
		data->ttengine_baseline = val;

		TT_GetAttrs(&data->rp, TT_FontHeight, &val);
		data->entry_maxheight = val;
	} else
#endif
	{
		data->entry_maxheight = _font(obj)->tf_YSize;
	}

	data->title_height = data->entry_maxheight + 2;

 	for (i=0;i<IMAGE_MAX;i++)
	{
		strcpy(filename,"PROGDIR:Images/");
		strcat(filename,image_names[i]);
		data->images[i] = dt_load_picture(filename, _screen(obj));
		/* It doesn't matter if this fails */
	}

	data->inbetween_setup = 1;
	CalcEntries(data,obj);
	CalcHorizontalTotal(data);
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

	if (data->ttengine_font)
	{
		TT_DoneRastPort(&data->rp);
		TT_CloseFont(data->ttengine_font);
		data->ttengine_font = NULL;
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

	/* Clear draw update in any case */
	data->drawupdate = 0;

	data->inbetween_show = 1;
	CalcVisible(data,obj);
	CalcHorizontalVisible(data,obj);
  DoMethod(_win(obj),MUIM_Window_AddEventHandler, &data->ehn_mousebuttons);

  data->threepoints_width = TextLength(&data->rp,"...",3);

	/* Setup buffer for a single entry, double buffering is optional */
	depth = GetBitMapAttr(_screen(obj)->RastPort.BitMap,BMA_DEPTH);
	if ((data->buffer_bmap = AllocBitMap(_mwidth(obj),data->entry_maxheight,depth,BMF_MINPLANES,_screen(obj)->RastPort.BitMap)))
	{
		if ((data->buffer_li = NewLayerInfo()))
		{
			if ((data->buffer_layer = CreateBehindLayer(data->buffer_li, data->buffer_bmap,0,0,_mwidth(obj)-1,data->entry_maxheight-1,LAYERSIMPLE,NULL)))
			{
				data->buffer_rp = data->buffer_layer->rp;
				if (data->ttengine_font)
				{
					TT_SetFont(data->buffer_rp, data->ttengine_font);
					TT_SetAttrs(data->buffer_rp, 
							TT_Screen, _screen(obj),
							TT_Encoding, TT_Encoding_UTF8,
							TT_Antialias, TT_Antialias_On,
							TAG_DONE);
				}
			}
		}
	}

	if (data->ttengine_font)
	{
		TT_SetAttrs(_rp(obj),
				TT_Screen, _screen(obj),
				TT_Encoding, TT_Encoding_UTF8,
				TT_Antialias, TT_Antialias_On,
				TAG_DONE);
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
	
	if (data->ttengine_font) TT_DoneRastPort(_rp(obj));

	if (data->buffer_layer)
	{
		if (data->ttengine_font) TT_DoneRastPort(data->buffer_rp);
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
 Draw sort markers
*************************************************************************/
static void DrawMarker(struct MailTreelist_Data *data, Object *obj, struct RastPort *rp, int xoff, int y)
{
	int col;

	xoff -= data->horiz_first;

	/* Draw markers */
	for (col = 0;col < MAX_COLUMNS; col++)
	{
		int col_width;
		int active;
		struct ColumnInfo *ci;
	
		active = data->columns_active[col];
		if (!active) continue;
		ci = &data->ci[active];

		col_width = ci->width;
		
		if (ci->flags & COLUMN_FLAG_SORT1UP)
		{
			int width = ((data->title_height - 8)/2)*2+1;

			if (width < col_width - 2)
			{
				SetAPen(rp, _pens(obj)[MPEN_SHADOW]);
				Move(rp, xoff + col_width - 2 - width/2, y+2+width);
				Draw(rp, xoff + col_width - 2 - width,   y+2);
				Draw(rp, xoff + col_width - 2,           y+2);
				SetAPen(rp, _pens(obj)[MPEN_SHINE]);
				Draw(rp, xoff + col_width - 2 - width/2, y+2+width);
			}
			break;
		}

		if (ci->flags & COLUMN_FLAG_SORT1DOWN)
		{
			int width = ((data->title_height - 8)/2)*2+1;

			if (width < col_width - 2)
			{
				SetAPen(rp, _pens(obj)[MPEN_SHINE]);
				Move(rp, xoff + col_width - 2 - width/2, y+2);
				Draw(rp, xoff + col_width - 2 - width,   y+2+width);
				Draw(rp, xoff + col_width - 2,           y+2+width);
				SetAPen(rp, _pens(obj)[MPEN_SHADOW]);
				Draw(rp, xoff + col_width - 2 - width/2, y+2);
			}
			break;
		}
	
		xoff += ci->width + data->column_spacing;
	}
}

/*************************************************************************
 Note, if you draw buffered, you must have _rp(obj) set to the
 buffer_rp before!
*************************************************************************/
static void DrawEntryAndBackgroundBuffered(struct IClass *cl, Object *obj, int cur, struct RastPort *buffer_rp, struct RastPort *window_rp, int window_y)
{
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);

	if (buffer_rp)
	{
		DoMethod(obj, MUIM_DrawBackground, 0, 0, _mwidth(obj), data->entry_maxheight, 0,0);
		DrawEntry(data,obj,cur,buffer_rp,-data->horiz_first,0);
		if (cur == ENTRY_TITLE) DrawMarker(data,obj,buffer_rp,0,0);
		BltBitMapRastPort(data->buffer_bmap, 0, 0,
											window_rp, _mleft(obj), window_y, _mwidth(obj), data->entry_maxheight, 0xc0);
	} else
	{
		DoMethod(obj, MUIM_DrawBackground, _mleft(obj), window_y, _mwidth(obj), data->entry_maxheight, 0,0);
		DrawEntry(data,obj,cur,window_rp,_mleft(obj),window_y);
		if (cur == ENTRY_TITLE) DrawMarker(data,obj,window_rp,_mleft(obj),window_y);
	}
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

	/* Don't do anything when being in quite mode (e.g. used for custom background drawing) */
	if (data->quiet)
		return 0;

	DoSuperMethodA(cl,obj,(Msg)msg);

	if (msg->flags & MADF_DRAWUPDATE) drawupdate = data->drawupdate;
	else drawupdate = 0;

	data->drawupdate = 0;

	start = data->entries_first;
	end = start + data->entries_visible;
	y = _mtop(obj) + data->title_height;

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
	if (data->ttengine_font) TT_SetFont(rp,data->ttengine_font);

	background = MUII_ListBack;
	data->quiet++;

	/* Get correct position of horizontal scroller */
	if (data->horiz_scroller) data->horiz_first = xget(data->horiz_scroller,MUIA_Prop_First);

	/* Draw title */
	if (!drawupdate)
	{
		set(obj, MUIA_Background, MUII_HSHADOWBACK);
		background = MUII_HSHADOWBACK;
		DrawEntryAndBackgroundBuffered(cl, obj, ENTRY_TITLE, data->buffer_rp, old_rp, _mtop(obj));

		SetAPen(old_rp,_pens(obj)[MPEN_SHADOW]);
		Move(old_rp,_mleft(obj),_mtop(obj) + data->title_height - 2);
		Draw(old_rp,_mright(obj),_mtop(obj) + data->title_height - 2);
		SetAPen(old_rp,_pens(obj)[MPEN_SHINE]);
		Move(old_rp,_mleft(obj),_mtop(obj) + data->title_height - 1);
		Draw(old_rp,_mright(obj),_mtop(obj) + data->title_height - 1);
	}

	/* Draw all entries between start and end, their current background
	 * is stored */
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

		DrawEntryAndBackgroundBuffered(cl, obj, cur, data->buffer_rp, old_rp, y);
	
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
	data->entries_minselected = 0x7fffffff;
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

	/* Recalc column dimensions and redraw if necessary */
	if (data->inbetween_setup)
	{
		CalcEntries(data,obj);
		CalcHorizontalTotal(data);

		if (data->inbetween_show)
		{
			CalcVisible(data,obj);
			CalcHorizontalVisible(data,obj);

			EnsureActiveEntryVisibility(data);

			MUI_Redraw(obj,MADF_DRAWOBJECT);
		}
	}

	IssueTreelistActiveNotify(cl, obj, data);
	return 1;
}

/*************************************************************************
 MUIM_MailTreelist_InsertMail
*************************************************************************/
static ULONG MailTreelist_InsertMail(struct IClass *cl, Object *obj, struct MUIP_MailTreelist_InsertMail *msg)
{
	struct MailTreelist_Data *data = INST_DATA(cl, obj);

	int after = msg->after;
	struct mail_info *mail = msg->m;
	struct ListEntry *entry;

	SM_DEBUGF(10,("msg->after = %d\n",after));

	/* Ensure vaild after. Note that if after == -2 we mean the last element */
	if (after == -2 || after > data->entries_num) after = data->entries_num - 1;
	else if (after < -2) after = -1;

	/* Ensure that we can hold an additional entry */
	if (!(SetListSize(data,data->entries_num + 1)))
		return 0;

	if (!(entry = AllocListEntry(data)))
		return 0;

	SM_DEBUGF(10,("after = %d, data->entries_num = %d\n",after,data->entries_num));

	/* The new element is placed at position after + 1, hence we must
	 * move all elements starting from this position one position below */
	if (after + 1 != data->entries_num)
	{
		int entries_to_move = data->entries_num - after - 1;
		SM_DEBUGF(10,("entries_to_move = %d\n",entries_to_move));
		memmove(&data->entries[after+2],&data->entries[after+1], entries_to_move*sizeof(data->entries[0]));
	}

	entry->mail_info = mail;
	data->entries[after+1] = entry;
	data->entries_num++;

	/* Update the active element if any and necessary */
	if (data->entries_active >= after + 1)
		data->entries_active++;
	if (data->last_active >= after + 1)
		data->last_active++;

	if (data->vert_scroller) set(data->vert_scroller,MUIA_Prop_Entries,data->entries_num);

	/* Recalc column dimensions and redraw if necessary */
	if (data->inbetween_setup)
	{
		CalcEntry(data,obj,mail); /* Calculate only the newly added entry */
		CalcHorizontalTotal(data);

		if (data->inbetween_show)
		{
			CalcVisible(data,obj);

			/* Redraw, if new element has been inserted above the visible bottom
			 * (could be optimized better) */
			if (after + 1 < data->entries_first + data->entries_visible)
				MUI_Redraw(obj,MADF_DRAWOBJECT);
		}
	}
	return 0;
}


/*************************************************************************
 MUIM_MailTreelist_RemoveSelected
*************************************************************************/
STATIC ULONG MailTreelist_RemoveSelected(struct IClass *cl, Object *obj, Msg msg)
{
	struct MailTreelist_Data *data = INST_DATA(cl, obj);
	int i = 0,from = -1, to = -1;

	/* TODO: Implement the easy case (if entries_maxselected == -1) in an easier way */

	while (1)
	{
		while (i < data->entries_num && ((!(data->entries[i]->flags & LE_FLAG_SELECTED)) && i != data->entries_active))
 			i++;

		if (from != -1)
		{
			memmove(&data->entries[from],&data->entries[to],sizeof(data->entries[0])*(i-to));

			data->entries_num -= to - from;
			i -= to - from;

			if (data->entries_active >= to) data->entries_active -= to;
			else if (data->entries_active >= from) data->entries_active = from;
		}

		/* If the last element was selected, i could be indead > data->entries_num */
		if (i >= data->entries_num) break;
		from = i;

		while (i < data->entries_num && (data->entries[i]->flags & LE_FLAG_SELECTED || i == data->entries_active))
			i++;

		to = i;

		i++;
	}

	/* Fix data->entries_active if not already done */
	if (data->entries_active >= data->entries_num) data->entries_active = data->entries_num - 1;

	data->entries_minselected = 0x7fffffff;
	data->entries_maxselected = -1;

	if (data->vert_scroller) set(data->vert_scroller,MUIA_Prop_Entries,data->entries_num);

	if (data->inbetween_show)
	{
		/* TODO: Inform also the scroller about the entries_num change */
		CalcVisible(data,obj);
		MUI_Redraw(obj,MADF_DRAWOBJECT);
	}

	return 0;
}

/*************************************************************************
 MUIM_MailTreelist_ReplaceMail
*************************************************************************/
STATIC ULONG MailTreelist_ReplaceMail(struct IClass *cl, Object *obj, struct MUIP_MailTreelist_ReplaceMail *msg)
{
	struct MailTreelist_Data *data = INST_DATA(cl, obj);

	int i;

	for (i=0;i<data->entries_num;i++)
	{
		if (data->entries[i]->mail_info == msg->oldmail)
		{
			data->entries[i]->mail_info = msg->newmail;

			if (data->inbetween_setup)
			{
				CalcEntry(data,obj, msg->newmail); /* Calculate only the newly added entry */
				CalcHorizontalTotal(data);
				MUI_Redraw(obj,MADF_DRAWOBJECT);
			}

			break;
		}
	}

	return 0;
}

/*************************************************************************
 MUIM_MailTreelist_RefreshMail
*************************************************************************/
STATIC ULONG MailTreelist_RefreshMail(struct IClass *cl, Object *obj, struct MUIP_MailTreelist_RefreshMail *msg)
{
	struct MailTreelist_Data *data = INST_DATA(cl, obj);

	int i;

	for (i=0;i<data->entries_num;i++)
	{
		if (data->entries[i]->mail_info == msg->m)
		{
			CalcEntry(data,obj, msg->m); /* Calculate only entry to be refreshed */
			CalcHorizontalTotal(data);
			MUI_Redraw(obj,MADF_DRAWOBJECT);
			break;
		}
	}

	return 0;
}

/*************************************************************************
 MUIM_MailTreelist_RefreshSelected
*************************************************************************/
STATIC ULONG MailTreelist_RefreshSelected(struct IClass *cl, Object *obj, Msg msg)
{
	struct MailTreelist_Data *data = INST_DATA(cl, obj);

	int i;
	
	int num_to_be_refreshed = 0;

	for (i=0;i<data->entries_num;i++)
	{
		if (data->entries[i]->flags & LE_FLAG_SELECTED)
		{
			CalcEntry(data,obj, data->entries[i]->mail_info); /* Calculate only entry to be refreshed */
			num_to_be_refreshed++;
		}
	}

	if (num_to_be_refreshed > 0)
	{
			CalcHorizontalTotal(data);
			MUI_Redraw(obj,MADF_DRAWOBJECT);
	}

	return 0;
}

/*************************************************************************
 MUIM_MailTreelist_GetFirstSelected
*************************************************************************/
static ULONG MailTreelist_GetFirstSelected(struct IClass *cl, Object *obj, struct MUIP_MailTreelist_GetFirstSelected *msg)
{
	struct MailTreelist_Data *data = INST_DATA(cl, obj);
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

	if (handle != -1)
	{
		*handle_ptr = handle;
		return (ULONG)data->entries[handle]->mail_info;
	}
	
	return 0;
}

/*************************************************************************
 MUIM_MailTreelist_GetNextSelected
*************************************************************************/
static ULONG MailTreelist_GetNextSelected(struct IClass *cl, Object *obj, struct MUIP_MailTreelist_GetNextSelected *msg)
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
 MUIM_HandleEvent
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
#ifdef HAVE_EXTENDEDMOUSE
			case	IDCMP_EXTENDEDMOUSE:
						if (msg->imsg->Code & IMSGCODE_INTUIWHEELDATA)
						{
							struct IntuiWheelData *iwd = (struct IntuiWheelData *)msg->imsg->IAddress;

							if (_isinobject(obj, msg->imsg->MouseX, msg->imsg->MouseY))
							{
								LONG visible, first, delta;
								GetAttr(MUIA_Prop_Visible, data->vert_scroller, (ULONG*)&visible);

								delta = (visible + 3)/6;
								if (delta < 1) delta = 1;

								GetAttr(MUIA_Prop_First, data->vert_scroller, (ULONG*)&first);
								if (first < 0) first = 0;

								if (iwd->WheelY < 0)
									first -= delta;
								else if(iwd->WheelY > 0)
									first += delta;
																	
								set(data->vert_scroller, MUIA_Prop_First, first);
								return MUI_EventHandlerRC_Eat;
							}
						}
						break;
#else
			/* NewMouse Standard Wheel support */
			case	IDCMP_RAWKEY:
						if (msg->imsg->Code == NM_WHEEL_UP || msg->imsg->Code == NM_WHEEL_DOWN)
						{
							if (_isinobject(obj, msg->imsg->MouseX, msg->imsg->MouseY))
							{
								LONG visible, delta;
								GetAttr(MUIA_Prop_Visible, data->vert_scroller, (ULONG*)&visible);

								delta = (visible + 3)/6;
								if (delta < 1) delta = 1;

								if (msg->imsg->Code == NM_WHEEL_DOWN)
									delta *= -1;

								DoMethod(data->vert_scroller, MUIM_Prop_Decrease, delta);
								return MUI_EventHandlerRC_Eat;
							}
						}
						break;
#endif

	    case    IDCMP_MOUSEBUTTONS:
	    				/* reset some states */
	    				data->right_title_click = -1;

	    				if (msg->imsg->Code == SELECTDOWN)
	    				{
	    					if (mx >= 0 && my >= 0 && mx < _mwidth(obj) && my < _mheight(obj))
	    					{
	    						int new_entries_active;
	    						int selected_changed;
	    						int double_click = 0;

									/* column dragging */
									int col;

									int xoff = 0;
									if (data->horiz_scroller) xoff = - xget(data->horiz_scroller,MUIA_Prop_First);

									for (col = 0;col < MAX_COLUMNS; col++)
									{
										int active;
										struct ColumnInfo *ci;

										active = data->columns_active[col];
										if (!active) continue;
										ci = &data->ci[active];

										/* Clicked inside a title column? */
										if (mx > xoff && mx <= xoff + ci->width)
										{
											if (my < data->title_height)
												data->title_column_click = active;
											break;
										}

										xoff += ci->width + data->column_spacing;

										if (mx == xoff - 1 || mx == xoff - 2 || mx == xoff - 3)
										{
											data->column_drag = active;
											data->column_drag_org_width = ci->width;
											data->column_drag_mx = mx;
											MUI_Redraw(obj,MADF_DRAWOBJECT);
											break;
										}
									}

									if (data->column_drag != -1 || data->title_column_click != -1)
									{
										/* Enable mouse move notifies */
										if (!data->mouse_pressed)
										{
											DoMethod(_win(obj),MUIM_Window_AddEventHandler, &data->ehn_mousemove);
								  		data->mouse_pressed = 1;
										}
									} else if (my >= data->title_height)
									{
										new_entries_active = (my - data->title_height) / data->entry_maxheight + data->entries_first;
										if (new_entries_active < 0) new_entries_active = 0;
										else if (new_entries_active >= data->entries_num) new_entries_active = data->entries_num - 1;
	
										/* Unselected entries if some have been selected */
										if (data->entries_maxselected != -1)
										{
											int cur;
											
											for (cur = data->entries_minselected;cur <= data->entries_maxselected; cur++)
												data->entries[cur]->flags &= ~LE_FLAG_SELECTED;
	
											selected_changed = 1;			
											data->entries_minselected = 0x7fffffff;
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
												double_click = DoubleClick(data->last_secs,data->last_mics,msg->imsg->Seconds,msg->imsg->Micros);
										}
	
										data->last_mics = msg->imsg->Micros;
										data->last_secs = msg->imsg->Seconds;
										data->last_active = new_entries_active;
	
										/* Enable mouse move notifies */
										if (!data->mouse_pressed)
										{
											DoMethod(_win(obj),MUIM_Window_AddEventHandler, &data->ehn_mousemove);
								  		data->mouse_pressed = 1;
										}
	
										/* On successful double click, issue the notify but also disable move move notifies */
										if (double_click)
										{
											IssueTreelistDoubleClickNotify(cl,obj,data);
											DoMethod(_win(obj),MUIM_Window_RemEventHandler, &data->ehn_mousemove);
											data->mouse_pressed = 0;
										}
		    					}
	    					}
	    				} else if (msg->imsg->Code == SELECTUP && data->mouse_pressed)
	    				{
								/* Disable mouse move notifies */
							  DoMethod(_win(obj),MUIM_Window_RemEventHandler, &data->ehn_mousemove);
							  data->mouse_pressed = 0;
							  if (data->title_column_click != -1)
							  	set(obj, MUIA_MailTreelist_TitleClick, data->title_column_click);

								if (data->column_drag != -1)
								{
									/* Fixate the width */
									data->ci[data->column_drag].flags &= ~COLUMN_FLAG_AUTOWIDTH;
								  data->column_drag = -1;
									MUI_Redraw(obj,MADF_DRAWOBJECT);
								} 

							  data->title_column_click = -1;
							  data->title_column_click2 = -1;
	    				} else if (msg->imsg->Code == MENUDOWN)
	    				{
	    					if (data->mouse_pressed)
	    					{
		    					if (data->column_drag != -1)
		    					{
		    						data->ci[data->column_drag].width = data->column_drag_org_width;
									  data->column_drag = -1;								
										MUI_Redraw(obj,MADF_DRAWOBJECT);
										CalcHorizontalTotal(data);
	
										/* Disable mouse move notifies */
									  DoMethod(_win(obj),MUIM_Window_RemEventHandler, &data->ehn_mousemove);
									  data->mouse_pressed = 0;
		    					}
									return MUI_EventHandlerRC_Eat;
	    					} else
	    					{
	    						if (mx >= 0 && my >= 0 && mx < _mwidth(obj) && my < _mheight(obj) && my < data->title_height)
	    						{
										/* column dragging */
										int col;

										int xoff = 0;
										if (data->horiz_scroller) xoff = - xget(data->horiz_scroller,MUIA_Prop_First);

										for (col = 0;col < MAX_COLUMNS; col++)
										{
											int active;
											struct ColumnInfo *ci;

											active = data->columns_active[col];
											if (!active) continue;
											ci = &data->ci[active];

											/* Clicked inside a title column? */
											if (mx > xoff && mx <= xoff + ci->width)
											{
												data->right_title_click = active;
												break;
											}

											xoff += ci->width + data->column_spacing;
										}
	    						}
	    					}
	    				}
	    				break;

			case		IDCMP_MOUSEMOVE:
							if (data->column_drag != -1)
							{
								struct ColumnInfo *ci;
								ci = &data->ci[data->column_drag];
								ci->width = data->column_drag_org_width + mx - data->column_drag_mx;
								if (ci->width < 0) ci->width = 0;
								MUI_Redraw(obj,MADF_DRAWOBJECT);
								CalcHorizontalTotal(data);
							} else
    					{
    						int new_entries_active, old_entries_active;

    						old_entries_active = data->entries_active;

								if (old_entries_active != -1)
								{
									new_entries_active = (my - data->title_height) / data->entry_maxheight + data->entries_first;
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

										if (new_entries_active < data->entries_first)
										{
											set(data->vert_scroller,MUIA_Prop_First,new_entries_active);
										} else
										{
											if (new_entries_active >= data->entries_first + data->entries_visible)
											{
												set(data->vert_scroller, MUIA_Prop_First, new_entries_active - data->entries_visible + 1);
											}
										}

										IssueTreelistActiveNotify(cl,obj,data);
									}
								}

								if ((mx < 0 || mx > _mwidth(obj)) && data->entries_active != -1)
								{
									/* Disable mouse move notifies */
								  DoMethod(_win(obj),MUIM_Window_RemEventHandler, &data->ehn_mousemove);
								  data->mouse_pressed = 0;

									DoMethod(obj, MUIM_DoDrag, 0, 0, 0);
								}
    					}
							break;
		}
  }

	return 0;
}

/*************************************************************************
 MUIM_DragQuery
*************************************************************************/
static ULONG MailTreelist_DragQuery(struct IClass *cl, Object *obj, struct MUIP_DragQuery *msg)
{
	return MUIV_DragQuery_Refuse; /* mails should not be resorted by the user */
}

/*************************************************************************
 MUIM_CreateDragImage
*************************************************************************/
static ULONG MailTreelist_CreateDragImage(struct IClass *cl, Object *obj, struct MUIP_CreateDragImage *msg)
{
	struct MUI_DragImage *img = (struct MUI_DragImage *)AllocVec(sizeof(struct MUIP_CreateDragImage),MEMF_CLEAR);
	if (img)
	{
		struct MailTreelist_Data *data = INST_DATA(cl, obj);
		LONG depth = GetBitMapAttr(_screen(obj)->RastPort.BitMap,BMA_DEPTH);
		int txt_len,num_selected=0;
		int i;

		for (i=0;i<data->entries_num;i++)
		{
			if (data->entries[i]->flags & LE_FLAG_SELECTED)
				num_selected++;
		}

		if (num_selected == 0 && data->entries_active != -1)
			num_selected = 1;

		if (num_selected == 1)
		{
			strcpy(data->buf, _("Dragging a single message"));
		}
		else
		{
			sm_snprintf(data->buf,sizeof(data->buf),_("Dragging %d messages"),num_selected);
		}

		txt_len = strlen(data->buf);
		img->width = TextLength(&data->dragRP, data->buf, txt_len) + 2;
		img->height = data->dragRP.Font->tf_YSize + 2;

		if ((img->bm = AllocBitMap(img->width, img->height, depth, BMF_MINPLANES, _screen(obj)->RastPort.BitMap)))
		{
			data->dragRP.BitMap = img->bm;
			SetAPen(&data->dragRP, _dri(obj)->dri_Pens[FILLPEN]);
			SetDrMd(&data->dragRP, JAM1);
			RectFill(&data->dragRP, 0,0,img->width-1,img->height-1);
			SetAPen(&data->dragRP, _dri(obj)->dri_Pens[FILLTEXTPEN]);
			Move(&data->dragRP,1,data->dragRP.Font->tf_Baseline+1);
			Text(&data->dragRP,data->buf,txt_len);
		}

		img->touchx = msg->touchx;
		img->touchy = msg->touchy + img->height / 2;
		img->flags = 0;
	}
	return (ULONG)img;
}

/**************************************************************************
 MUIM_DeleteDragImage
**************************************************************************/
static ULONG MailTreelist_DeleteDragImage(struct IClass *cl, Object *obj, struct MUIP_DeleteDragImage *msg)
{
	if (msg->di)
	{
		if (msg->di->bm) FreeBitMap(msg->di->bm);
		FreeVec(msg->di);
	}
	return 0;
}

/**************************************************************************
 MUIM_CreateShortHelp
**************************************************************************/
STATIC ULONG MailTreelist_CreateShortHelp(struct IClass *cl,Object *obj,struct MUIP_CreateShortHelp *msg)
{
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);
	struct mail_info *m = NULL;
	int mx = msg->mx - _mleft(obj);
	int my = msg->my - _mtop(obj);

	if (mx >= 0 && my >= data->title_height && mx < _mwidth(obj) && my < _mheight(obj))
	{
		int over_entry = (my - data->title_height) / data->entry_maxheight + data->entries_first;
		if (over_entry >= 0 && over_entry < data->entries_num)
		{
			if ((m = data->entries[over_entry]->mail_info))
			{
				char *from = mail_get_from_address(m);
				char *to = mail_get_to_address(m);
				char *replyto = mail_get_replyto_address(m);
				char date_buf[64];
				char recv_buf[64];
				char *buf = data->bubblehelp_buf;

				SecondsToString(date_buf,m->seconds);
				SecondsToString(recv_buf,m->received);

#define BUFFER_SPACE_LEFT (sizeof(data->bubblehelp_buf) - (buf - data->bubblehelp_buf))

				/* Help bubble text */
				sm_snprintf(buf,BUFFER_SPACE_LEFT,"\33b%s\33n",_("Message"));
				buf += strlen(buf);
				if (m->subject)
				{
					*buf++ = '\n';
					buf = mystpcpy(buf,data->subject_text);
					*buf++ = ':';
					*buf++ = ' ';
					buf += utf8tostr(m->subject,buf,BUFFER_SPACE_LEFT,user.config.default_codeset);
				}

				if (from)
				{
					*buf++ = '\n';
					buf = mystpcpy(buf,data->from_text);
					*buf++ = ':';
					*buf++ = ' ';
					buf += utf8tostr(from,buf,BUFFER_SPACE_LEFT,user.config.default_codeset);
				}

				if (to)
				{
					*buf++ = '\n';
					buf = mystpcpy(buf,data->to_text);
					*buf++ = ':';
					*buf++ = ' ';
					buf += utf8tostr(to,buf,BUFFER_SPACE_LEFT,user.config.default_codeset);
				}

				if (replyto)
				{
					*buf++ = '\n';
					buf = mystpcpy(buf,data->reply_text);
					*buf++ = ':';
					*buf++ = ' ';
					buf = mystpcpy(buf,replyto);
				}

				sm_snprintf(buf,BUFFER_SPACE_LEFT,"\n%s: %s\n%s: %s\n%s: %d\n%s: %s\n%s: %s",
								data->date_text, date_buf,
								data->received_text, recv_buf,
								data->size_text, m->size,
								data->pop3_text, m->pop3_server?m->pop3_server:"",
								data->filename_text, m->filename);

				free(replyto);
				free(to);
				free(from);

				return (ULONG)data->bubblehelp_buf;
			}
		}
	}	
	return 0L;
}

STATIC ULONG MailTreelist_ContextMenuBuild(struct IClass *cl, Object * obj, struct MUIP_ContextMenuBuild *msg)
{
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);
  Object *context_menu, *stati_menu, *last_menu, *hidden_menu;

  if (data->context_menu)
  {
  	MUI_DisposeObject(data->context_menu);
  	data->context_menu = NULL;
  }

	if (msg->my >= _mtop(obj) && msg->my < _mtop(obj) + data->title_height)
		return (ULONG)data->title_menu;

	context_menu = MenustripObject,
		Child, MenuObjectT(_("Mail")),
			Child, MenuitemObject, MUIA_Menuitem_Title, _("Spam Check"), MUIA_UserData, MENU_SPAMCHECK, End,
			Child, stati_menu = MenuitemObject, MUIA_Menuitem_Title, _("Set status"),
				Child, MenuitemObject, MUIA_Menuitem_Title, _("Mark"), MUIA_UserData, MENU_SETSTATUS_MARK, End,
				Child, MenuitemObject, MUIA_Menuitem_Title, _("Unmark"), MUIA_UserData, MENU_SETSTATUS_UNMARK, End,
				Child, MenuitemObject, MUIA_Menuitem_Title, ~0, End,
				Child, MenuitemObject, MUIA_Menuitem_Title, _("Hold"), MUIA_UserData, MENU_SETSTATUS_HOLD, End,
				Child, MenuitemObject, MUIA_Menuitem_Title, _("Pending"), MUIA_UserData, MENU_SETSTATUS_WAITSEND, End,
				Child, MenuitemObject, MUIA_Menuitem_Title, ~0, End,
				Child, MenuitemObject, MUIA_Menuitem_Title, _("Read"), MUIA_UserData, MENU_SETSTATUS_READ, End,
				Child, MenuitemObject, MUIA_Menuitem_Title, _("Unread"), MUIA_UserData, MENU_SETSTATUS_UNREAD, End,
				Child, MenuitemObject, MUIA_Menuitem_Title, ~0, End,
				Child, MenuitemObject, MUIA_Menuitem_Title, _("Is Spam"), MUIA_UserData, MENU_SETSTATUS_SPAM, End,
				Child, last_menu = MenuitemObject, MUIA_Menuitem_Title, _("Is Ham"), MUIA_UserData, MENU_SETSTATUS_HAM, End,
				End,
			Child, MenuitemObject, MUIA_Menuitem_Title, _("Delete"), MUIA_UserData, MENU_DELETE, End,
			End,
		End;

	if (user.config.set_all_stati)
	{
		hidden_menu = MenuitemObject, MUIA_Menuitem_Title, ~0, End;
		DoMethod(stati_menu, MUIM_Family_Insert, (ULONG)hidden_menu, (ULONG)last_menu);
		last_menu = hidden_menu;
		hidden_menu = MenuitemObject, MUIA_Menuitem_Title, _("Sent"), MUIA_UserData, MENU_SETSTATUS_SENT, End;
		DoMethod(stati_menu, MUIM_Family_Insert, (ULONG)hidden_menu, (ULONG)last_menu);
		last_menu = hidden_menu;
		hidden_menu = MenuitemObject, MUIA_Menuitem_Title, _("Error"), MUIA_UserData, MENU_SETSTATUS_ERROR, End;
		DoMethod(stati_menu, MUIM_Family_Insert, (ULONG)hidden_menu, (ULONG)last_menu);
		last_menu = hidden_menu;
		hidden_menu = MenuitemObject, MUIA_Menuitem_Title, _("Replied"), MUIA_UserData, MENU_SETSTATUS_REPLIED, End;
		DoMethod(stati_menu, MUIM_Family_Insert, (ULONG)hidden_menu, (ULONG)last_menu);
		last_menu = hidden_menu;
		hidden_menu = MenuitemObject, MUIA_Menuitem_Title, _("Forward"), MUIA_UserData, MENU_SETSTATUS_FORWARD, End;
		DoMethod(stati_menu, MUIM_Family_Insert, (ULONG)hidden_menu, (ULONG)last_menu);
		last_menu = hidden_menu;
		hidden_menu = MenuitemObject, MUIA_Menuitem_Title, _("Repl. & Forw."), MUIA_UserData, MENU_SETSTATUS_REPLFORW, End;
		DoMethod(stati_menu, MUIM_Family_Insert, (ULONG)hidden_menu, (ULONG)last_menu);
	}
	
  data->context_menu = context_menu;
  return (ULONG) context_menu;
}


STATIC ULONG MailTreelist_ContextMenuChoice(struct IClass *cl, Object *obj, struct MUIP_ContextMenuChoice *msg)
{
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);

	switch (xget(msg->item,MUIA_UserData))
	{
		case	1:
		case  2:
		case  3:
		case  4:
		case  5:
		case  6:
		case	7:
		case	8:
					PrepareDisplayedColumns(data);
					CalcHorizontalTotal(data);
					MUI_Redraw(obj,MADF_DRAWOBJECT);
				  break;

		case	MENU_SETSTATUS_MARK: callback_mails_mark(1); break;
		case	MENU_SETSTATUS_UNMARK: callback_mails_mark(0); break;
		case	MENU_SETSTATUS_READ: callback_mails_set_status(MAIL_STATUS_READ); break;
		case	MENU_SETSTATUS_UNREAD: callback_mails_set_status(MAIL_STATUS_UNREAD); break;
		case	MENU_SETSTATUS_HOLD: callback_mails_set_status(MAIL_STATUS_HOLD); break;
		case	MENU_SETSTATUS_WAITSEND: callback_mails_set_status(MAIL_STATUS_WAITSEND); break;
		case  MENU_SETSTATUS_SPAM: callback_selected_mails_are_spam();break;
		case  MENU_SETSTATUS_HAM: callback_selected_mails_are_ham();break;
		case	MENU_SETSTATUS_SENT: callback_mails_set_status(MAIL_STATUS_SENT); break;
		case	MENU_SETSTATUS_ERROR: callback_mails_set_status(MAIL_STATUS_ERROR); break;
		case	MENU_SETSTATUS_REPLIED: callback_mails_set_status(MAIL_STATUS_REPLIED); break;
		case	MENU_SETSTATUS_FORWARD: callback_mails_set_status(MAIL_STATUS_FORWARD); break;
		case	MENU_SETSTATUS_REPLFORW: callback_mails_set_status(MAIL_STATUS_REPLFORW); break;
		case  MENU_SPAMCHECK: callback_check_selected_mails_if_spam();break;
		case  MENU_DELETE: callback_delete_mails();break;
		case	MENU_RESET_ALL_COLUMN_WIDTHS: MailTreelist_ResetAllColumnWidth(data, obj);break;
		case	MENU_RESET_THIS_COLUMN_WIDTH: MailTreelist_ResetClickedColumnWidth(data, obj);break;
		default: 
		{
			return DoSuperMethodA(cl,obj,(Msg)msg);
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
		case	MUIM_DragQuery:		return MailTreelist_DragQuery(cl,obj,(struct MUIP_DragQuery *)msg);
		case	MUIM_CreateDragImage: return MailTreelist_CreateDragImage(cl,obj,(struct MUIP_CreateDragImage *)msg);
		case	MUIM_DeleteDragImage: return MailTreelist_DeleteDragImage(cl,obj,(struct MUIP_DeleteDragImage *)msg);
		case	MUIM_CreateShortHelp: return MailTreelist_CreateShortHelp(cl,obj,(struct MUIP_CreateShortHelp *)msg);
		case	MUIM_ContextMenuBuild: return MailTreelist_ContextMenuBuild(cl,obj,(struct MUIP_ContextMenuBuild *)msg);
		case	MUIM_ContextMenuChoice: return MailTreelist_ContextMenuChoice(cl,obj,(APTR)msg);

		case	MUIM_MailTreelist_Clear:					return MailTreelist_Clear(cl, obj, (APTR)msg);
		case	MUIM_MailTreelist_SetFolderMails: return MailTreelist_SetFolderMails(cl, obj, (APTR)msg);
		case	MUIM_MailTreelist_InsertMail:	return MailTreelist_InsertMail(cl, obj, (APTR)msg);
		case	MUIM_MailTreelist_GetFirstSelected: return MailTreelist_GetFirstSelected(cl, obj, (APTR)msg);
		case	MUIM_MailTreelist_GetNextSelected: return MailTreelist_GetNextSelected(cl, obj, (APTR)msg);
		case	MUIM_MailTreelist_RemoveSelected: return MailTreelist_RemoveSelected(cl, obj, (APTR)msg);
		case	MUIM_MailTreelist_ReplaceMail: return MailTreelist_ReplaceMail(cl,obj, (APTR)msg);
		case	MUIM_MailTreelist_RefreshMail: return MailTreelist_RefreshMail(cl,obj,(APTR)msg);
		case	MUIM_MailTreelist_RefreshSelected: return MailTreelist_RefreshSelected(cl,obj,(APTR)msg);

		default: return DoSuperMethodA(cl,obj,msg);
	}
}

/**************************************************************************/

#if 0
static struct Hook layout_hook; 

ULONG MailTreelist_Layout_Function(struct Hook *hook, Object *obj, struct MUI_LayoutMsg *lm)
{
	struct MUI_ListviewData *data = (struct MUI_ListviewData *)hook->h_Data;

	Object *list;
	Object *vert;
	Object *horiz;

	Object *cstate = (Object *)lm->lm_Children->mlh_Head;

	list = NextObject(&cstate);
	vert = NextObject(&cstate);
	horiz = NextObject(&cstate);

	switch (lm->lm_Type)
	{
		case	MUILM_MINMAX:
		{
		    /* Calculate the minmax dimension of the group,
		    ** We only have a fixed number of children, so we need no NextObject()
		    */
		    lm->lm_MinMax.MinWidth = _minwidth(list) + _minwidth(vert);
		    lm->lm_MinMax.DefWidth = _defwidth(list) + _defwidth(vert);
		    lm->lm_MinMax.MaxWidth = _maxwidth(list) + _maxwidth(vert);
		    lm->lm_MinMax.MaxWidth = MIN(lm->lm_MinMax.MaxWidth, MUI_MAXMAX);

		    lm->lm_MinMax.MinHeight = MAX(_minheight(list), _minheight(vert));
		    lm->lm_MinMax.DefHeight = MAX(_defheight(list), lm->lm_MinMax.MinHeight);
		    lm->lm_MinMax.MaxHeight = MIN(_maxheight(list), _maxheight(vert));
		    lm->lm_MinMax.MaxHeight = MIN(lm->lm_MinMax.MaxHeight, MUI_MAXMAX);
		    return 0;
		}

		case MUILM_LAYOUT:
		{
		    /* Now place the objects between
		     * (0, 0, lm->lm_Layout.Width - 1, lm->lm_Layout.Height - 1)
		    */

		    LONG vert_width = _minwidth(vert);
		    LONG horiz_width = _minheight(horiz);
		    LONG lay_width = lm->lm_Layout.Width;
		    LONG lay_height = lm->lm_Layout.Height;
		    LONG cont_width;
		    LONG cont_height;

		    /* We need all scrollbars and the button */
		    set(data->vert, MUIA_ShowMe, TRUE); /* We could also overload MUIM_Show... */
		    cont_width = lay_width - vert_width;
		    cont_height = lay_height;

		    MUI_Layout(data->vert, cont_width, 0, vert_width, cont_height,0);

		    /* Layout the group a second time, note that setting _mwidth() and
		       _mheight() should be enough, or we invent a new flag */
		    MUI_Layout(data->list, 0, 0, cont_width, cont_height, 0);
		    return 1;
		}
    }
    return 0;
}
#endif

/**************************************************************************/

Object *MakeNewMailTreelist(ULONG userid, Object **list)
{
	Object *vscrollbar = ScrollbarObject, MUIA_Group_Horiz, FALSE, End;
	Object *hscrollbar = ScrollbarObject, MUIA_Group_Horiz, TRUE, End;
	Object *hscrollbargroup = HGroup, MUIA_ShowMe, FALSE, Child, hscrollbar, End;
	
	Object *group = VGroup,
			MUIA_Group_Spacing, 0,
			Child, HGroup,
				MUIA_Group_Spacing, 0,
//			MUIA_Group_LayoutHook, &layout_hook,
				Child, *list = MailTreelistObject,
					InputListFrame,
					MUIA_ObjectID, userid,
					MUIA_MailTreelist_VertScrollbar, vscrollbar,
					MUIA_MailTreelist_HorizScrollbar, hscrollbar,
					MUIA_MailTreelist_GroupOfHorizScrollbar, hscrollbargroup,
					End,
				Child, vscrollbar,
				End,
			Child, hscrollbargroup,
			End;

	return group;
}

/**************************************************************************/

//struct MUI_CustomClass *CL_MailTreelist;

int create_new_mailtreelist_class(void)
{
	SM_ENTER;
	if ((CL_MailTreelist = CreateMCC(MUIC_Area, NULL, sizeof(struct MailTreelist_Data), MailTreelist_Dispatcher)))
	{
//		init_hook(&layout_hook,(HOOKFUNC)MailTreelist_Layout_Function);

		SM_DEBUGF(15,("Create CL_MailTreelist: 0x%lx\n",CL_MailTreelist));
		SM_RETURN(1,"%ld");
	}
	SM_DEBUGF(5,("FAILED! Create CL_MailTreelist\n"));
	SM_RETURN(0,"%ld");
}

void delete_new_mailtreelist_class(void)
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
