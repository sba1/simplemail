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
** shutdownwnd.c
*/

#include <string.h>

#include <datatypes/pictureclass.h>

#include <proto/intuition.h>
#include <proto/datatypes.h>
#include <proto/graphics.h>

#include "configuration.h"
#include "smintl.h"

#include "datatypescache.h"
#include "shutdownwnd.h"

static struct Window *shutdown_wnd;

void shutdownwnd_open(void)
{
	struct Screen *scr;

	if ((scr = LockPubScreen(NULL)))
	{
		Object *obj;
		LONG pen;

		/* get a white pen for the color of our text */
		pen = ObtainBestPenA(scr->ViewPort.ColorMap,0xffffffff,0xffffffff,0xffffffff,NULL);

		if ((obj = LoadPicture("PROGDIR:Images/shutdown",scr)))
		{
			struct BitMapHeader *bmhd = NULL;
			struct BitMap *bitmap = NULL;

			GetDTAttrs(obj,PDTA_BitMapHeader,&bmhd,TAG_DONE);
			GetDTAttrs(obj,PDTA_DestBitMap,&bitmap,TAG_DONE);
			if (!bitmap) GetDTAttrs(obj,PDTA_BitMap,&bitmap,TAG_DONE);

			if (bmhd && bitmap)
			{
				int width = bmhd->bmh_Width;
				int height = bmhd->bmh_Height;

				int wndleft,wndtop;

				wndleft = (scr->Width - width)/2;
				wndtop = (scr->Height - height)/2;

				if ((shutdown_wnd = OpenWindowTags(NULL,
					WA_SmartRefresh, TRUE,
					WA_NoCareRefresh, TRUE,
					WA_Borderless, TRUE,
					WA_Width, width,
					WA_Height, height,
					WA_PubScreen, scr,
					WA_Left, wndleft,
					WA_Top, wndtop,
					WA_BackFill, LAYERS_NOBACKFILL,
					TAG_DONE)))
				{
					BltBitMapRastPort(bitmap,0,0,
					                  shutdown_wnd->RPort, 0, 0, width, height,
					                  0xc0);

					if (!user.config.dont_show_shutdown_text)
					{
						struct TextExtent te;
						char *txt = _("Shutting down...");

						SetDrMd(shutdown_wnd->RPort,JAM1);
						SetAPen(shutdown_wnd->RPort,pen);
						TextExtent(shutdown_wnd->RPort,txt,strlen(txt),&te);
						if ((te.te_Width < width) && (te.te_Height < height))
						{
							/* only draw the text if there is enought space for it */
							Move(shutdown_wnd->RPort,(width - te.te_Width)/2, height - te.te_Height - 4 + shutdown_wnd->RPort->TxBaseline);
							Text(shutdown_wnd->RPort,txt,strlen(txt));
						}
					}
				}
			}
			DisposeDTObject(obj);
		}
		ReleasePen(scr->ViewPort.ColorMap,pen);
		UnlockPubScreen(NULL,scr);
	}
}

void shutdownwnd_close(void)
{
	if (shutdown_wnd)
	{
		CloseWindow(shutdown_wnd);
		shutdown_wnd = NULL;
	}
}
