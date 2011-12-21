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
** $Id$
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <libraries/iffparse.h> /* MAKE_ID */
#include <libraries/mui.h>
#include <mui/NListview_mcc.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "configuration.h"
#include "debug.h"
#include "lists.h"
#include "progmon.h"
#include "smintl.h"
#include "support_indep.h"

#include "muistuff.h"
#include "progmonwnd.h"

static Object *progmon_wnd;
static Object *progmon_group;
static Object *progmon_noop;

struct progmon_gui_node
{
	struct node node;
	Object *progmon_group;
	Object *progmon_gauge;

	utf8 working_on_utf8[80];
	char working_on[80];
};

static struct list progmon_gui_list;

static void progmonwnd_init(void)
{
	if (!MUIMasterBase) return;

	progmon_wnd = WindowObject,
		MUIA_Window_ID, MAKE_ID('P','R','M','O'),
		MUIA_Window_Title, _("SimpleMail - Progress Monitors"),
		WindowContents, progmon_group = VGroupV,
			VirtualFrame,
			Child, progmon_noop = VGroup,
				Child, HVSpace,
				Child, TextObject,
					MUIA_Text_PreParse, "\33c" ,
					MUIA_Text_Contents, ("No pending operations"),
					End,
				Child, HVSpace,
				End,
			End,
		End;

	if (progmon_wnd)
	{
		DoMethod(App, OM_ADDMEMBER, (ULONG)progmon_wnd);
		DoMethod(progmon_wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)progmon_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);

		list_init(&progmon_gui_list);
	}
}

/**
 * Called for every progress monitor.
 *
 * @param info
 * @param udata
 */
static void progmonwnd_scan_entry(struct progmon_info *info, void *udata)
{
	struct progmon_gui_node **next_node_ptr;
	struct progmon_gui_node *next_node;

	SM_ENTER;

	next_node_ptr = (struct progmon_gui_node**)udata;
	if (!(next_node = *next_node_ptr))
	{
		if (!(next_node = (struct progmon_gui_node*)malloc(sizeof(*next_node))))
			return;

		mystrlcpy((char*)next_node->working_on_utf8,(char*)info->working_on,sizeof(next_node->working_on));
		utf8tostr(next_node->working_on_utf8,next_node->working_on,sizeof(next_node->working_on),user.config.default_codeset);

		next_node->progmon_group = HGroup,
				Child, next_node->progmon_gauge = GaugeObject,
					GaugeFrame,
					MUIA_Gauge_Horiz, TRUE,
					MUIA_Gauge_Current, info->work_done,
					MUIA_Gauge_Max, info->work,
					MUIA_Gauge_InfoText, next_node->working_on,
					End,
				End;

		if (!next_node->progmon_group)
			return;

		DoMethod(progmon_group, OM_ADDMEMBER, (ULONG)next_node->progmon_group);
		list_insert_tail(&progmon_gui_list,&next_node->node);

		SM_DEBUGF(20,("Node %p added\n",next_node));
	} else
	{
		mystrlcpy((char*)next_node->working_on_utf8,(char*)info->working_on,sizeof(next_node->working_on));
		utf8tostr(next_node->working_on_utf8,next_node->working_on,sizeof(next_node->working_on),user.config.default_codeset);

		SetAttrs(next_node->progmon_gauge,
				MUIA_Gauge_Current, info->work_done,
				MUIA_Gauge_Max, info->work,
				MUIA_Gauge_InfoText, next_node->working_on,
				TAG_DONE);

		*next_node_ptr = (struct progmon_gui_node *)node_next(&next_node->node);
	}

	SM_LEAVE;
}

/**
 * Updates the content of the progress monitor window.
 *
 * @param force
 */
void progmonwnd_update(int force)
{
	struct progmon_gui_node *node;
	struct progmon_gui_node *next_node;
	int num;

	if (!progmon_wnd && !force)
		return;

	DoMethod(progmon_group, MUIM_Group_InitChange);
	node = (struct progmon_gui_node*)list_first(&progmon_gui_list);
	num = progmon_scan(progmonwnd_scan_entry,&node);
	set(progmon_noop,MUIA_ShowMe, num == 0);

	/* Clear the remaining gauges if any */
	while (node)
	{
		SM_DEBUGF(20,("Removing node %p\n",node));

		next_node = (struct progmon_gui_node*)node_next(&node->node);
		node_remove(&node->node);
		DoMethod(progmon_group, OM_REMMEMBER, (ULONG)node->progmon_group);
		MUI_DisposeObject(node->progmon_group);
		free(node);
		node = next_node;
	}
	DoMethod(progmon_group, MUIM_Group_ExitChange);

}

/**
 * Opens the progess monitor window.
 */
void progmonwnd_open(void)
{
	if (!progmon_wnd) progmonwnd_init();
	if (!progmon_wnd) return;

	progmonwnd_update(1);
	set(progmon_wnd,MUIA_Window_Open,TRUE);
}
