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
** amigasupport.c
*/

#include <string.h>
#include <intuition/intuition.h>
#include <dos/dos.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/locale.h>
#include <proto/keymap.h>
#include <proto/graphics.h>
#include <proto/layers.h>
#include <proto/utility.h>
#include <proto/rexxsyslib.h>

#include "amigasupport.h"
#include "compiler.h"
#include "support_indep.h"

#ifdef __AMIGAOS4__
extern ULONG hookEntry();
#else
#ifdef __MORPHOS__
#define hookEntry HookEntry
#endif
#endif

struct Library *OpenLibraryInterface(STRPTR name, int version, void *interface_ptr);
void CloseLibraryInterface(struct Library *lib, void *interface);

static ASM void Hookfunc_Date_Write(REG(a0,struct Hook *j), REG(a2, void *object), REG(a1, ULONG c))
{
	char *data = (char*)j->h_Data;
	*data++ = c;
	j->h_Data = data;
}

void SecondsToString( char *buf, unsigned int seconds)
{
	struct DateStamp ds;
	extern struct Locale *DefaultLocale;

	ds.ds_Days = seconds / 60 / 60 / 24;
	ds.ds_Minute = (seconds / 60) % (60 * 24);
	ds.ds_Tick = (seconds % 60) * 50;

	if (LocaleBase && DefaultLocale)
	{
		struct Hook date_hook;

		date_hook.h_Data = buf;
#if defined(__AMIGAOS4__) || defined(__MORPHOS__)
		date_hook.h_Entry = (HOOKFUNC)hookEntry;
		date_hook.h_SubEntry = (HOOKFUNC)Hookfunc_Date_Write;
#else
		date_hook.h_Entry = (HOOKFUNC)Hookfunc_Date_Write;
#endif

		FormatDate(DefaultLocale, DefaultLocale->loc_ShortDateFormat, &ds, &date_hook);

		buf = buf + strlen(buf);
		*buf++ = ' ';

		date_hook.h_Data = buf;
		FormatDate(DefaultLocale, DefaultLocale->loc_TimeFormat, &ds, &date_hook);
	} else
	{
		/* dos stuff should follow here */
		*buf = 0;
	}
}

void SecondsToStringLong( char *buf, unsigned int seconds)
{
	struct DateStamp ds;
	extern struct Locale *DefaultLocale;

	ds.ds_Days = seconds / 60 / 60 / 24;
	ds.ds_Minute = (seconds / 60) % (60 * 24);
	ds.ds_Tick = (seconds % 60) * 50;

	if (LocaleBase && DefaultLocale)
	{
		struct Hook date_hook;

		date_hook.h_Data = buf;
#if defined(__AMIGAOS4__) || defined(__MORPHOS__)
		date_hook.h_Entry = (HOOKFUNC)hookEntry;
		date_hook.h_SubEntry = (HOOKFUNC)Hookfunc_Date_Write;
#else
		date_hook.h_Entry = (HOOKFUNC)Hookfunc_Date_Write;
#endif

		FormatDate(DefaultLocale, DefaultLocale->loc_DateFormat, &ds, &date_hook);

		buf = buf + strlen(buf);
		*buf++ = ' ';

		date_hook.h_Data = buf;
		FormatDate(DefaultLocale, DefaultLocale->loc_TimeFormat, &ds, &date_hook);
	} else
	{
		/* dos stuff should follow here */
		*buf = 0;
	}
}

void SecondsToDateString( char *buf, unsigned int seconds)
{
	struct DateStamp ds;
	extern struct Locale *DefaultLocale;

	ds.ds_Days = seconds / 60 / 60 / 24;
	ds.ds_Minute = (seconds / 60) % (60 * 24);
	ds.ds_Tick = (seconds % 60) * 50;

	if (LocaleBase && DefaultLocale)
	{
		struct Hook date_hook;

#if defined(__AMIGAOS4__) || defined(__MORPHOS__)
		date_hook.h_Entry = (HOOKFUNC)hookEntry;
		date_hook.h_SubEntry = (HOOKFUNC)Hookfunc_Date_Write;
#else
		date_hook.h_Entry = (HOOKFUNC)Hookfunc_Date_Write;
#endif
		date_hook.h_Data = buf;
		FormatDate(DefaultLocale, DefaultLocale->loc_ShortDateFormat, &ds, &date_hook);
	} else
	{
		/* dos stuff should follow here */
		*buf = 0;
	}
}

void SecondsToTimeString( char *buf, unsigned int seconds)
{
	struct DateStamp ds;
	extern struct Locale *DefaultLocale;

	ds.ds_Days = seconds / 60 / 60 / 24;
	ds.ds_Minute = (seconds / 60) % (60 * 24);
	ds.ds_Tick = (seconds % 60) * 50;

	if (LocaleBase && DefaultLocale)
	{
		struct Hook date_hook;
#if defined(__AMIGAOS4__) || defined(__MORPHOS__)
		date_hook.h_Entry = (HOOKFUNC)hookEntry;
		date_hook.h_SubEntry = (HOOKFUNC)Hookfunc_Date_Write;
#else
		date_hook.h_Entry = (HOOKFUNC)Hookfunc_Date_Write;
#endif
		date_hook.h_Data = buf;
		FormatDate(DefaultLocale, DefaultLocale->loc_TimeFormat, &ds, &date_hook);
	} else
	{
		/* dos stuff should follow here */
		*buf = 0;
	}
}


/* duplicates the string, allocated with AllocVec() */
STRPTR StrCopy(const STRPTR str)
{
	STRPTR dest;
	LONG len;
	if (!str) return NULL;

	len = strlen(str);

	if ((dest = (STRPTR)AllocVec(len+1,0)))
	{
		strcpy(dest,str);
	}
	return dest;
}

/* Converts a Rawkey to a vanillakey */
ULONG ConvertKey(struct IntuiMessage *imsg)
{
   struct InputEvent event;
   UBYTE code = 0;
   event.ie_NextEvent    = NULL;
   event.ie_Class        = IECLASS_RAWKEY;
   event.ie_SubClass     = 0;
   event.ie_Code         = imsg->Code;
   event.ie_Qualifier    = imsg->Qualifier;
   event.ie_EventAddress = (APTR *) *((ULONG *)imsg->IAddress);
   MapRawKey(&event, &code, 1, NULL);
   return code;
}

/* Returns the lock of a name (Allocated with AllocVec()) */
STRPTR NameOfLock( BPTR lock )
{
	STRPTR n;
	BOOL again;
	ULONG bufSize = 127;
	if( !lock ) return NULL;

	do
	{
		again = FALSE;
		if((n = (STRPTR)AllocVec(bufSize, 0x10000 )))
		{
			if( NameFromLock( lock, n, bufSize-1 ) == DOSFALSE )
			{
				if( IoErr() == ERROR_LINE_TOO_LONG )
				{
					bufSize += 127;
					again = TRUE;
				}
				FreeVec(n);
				n = NULL;
			}
		}
	}	while(again);

	return n;
}


/* A BltBitMaskPort() replacement which blits masks for interleaved bitmaps correctly */
struct LayerHookMsg
{
	struct Layer *layer;
	struct Rectangle bounds;
	LONG offsetx;
	LONG offsety;
};

struct BltMaskHook
{
  struct Hook hook;
  struct BitMap maskBitMap;
  struct BitMap *srcBitMap;
  LONG srcx,srcy;
  LONG destx,desty;
};

VOID MyBltMaskBitMap(CONST struct BitMap *srcBitMap, LONG xSrc, LONG ySrc, struct BitMap *destBitMap, LONG xDest, LONG yDest, LONG xSize, LONG ySize, struct BitMap *maskBitMap)
{
  BltBitMap(srcBitMap,xSrc,ySrc,destBitMap, xDest, yDest, xSize, ySize, 0x99,~0,NULL);
  BltBitMap(maskBitMap,xSrc,ySrc,destBitMap, xDest, yDest, xSize, ySize, 0xe2,~0,NULL);
  BltBitMap(srcBitMap,xSrc,ySrc,destBitMap, xDest, yDest, xSize, ySize, 0x99,~0,NULL);
}

STATIC ASM SAVEDS VOID HookFunc_BltMask(REG(a0,struct Hook *hook), REG(a2,struct RastPort *rp), REG(a1,struct LayerHookMsg *msg))
{
  struct BltMaskHook *h = (struct BltMaskHook*)hook;

  LONG width = msg->bounds.MaxX - msg->bounds.MinX+1;
  LONG height = msg->bounds.MaxY - msg->bounds.MinY+1;
  LONG offsetx = h->srcx + msg->offsetx - h->destx;
  LONG offsety = h->srcy + msg->offsety - h->desty;

  MyBltMaskBitMap( h->srcBitMap, offsetx, offsety, rp->BitMap, msg->bounds.MinX, msg->bounds.MinY, width, height, &h->maskBitMap);
}

VOID MyBltMaskBitMapRastPort( struct BitMap *srcBitMap, LONG xSrc, LONG ySrc, struct RastPort *destRP, LONG xDest, LONG yDest, LONG xSize, LONG ySize, ULONG minterm, APTR bltMask)
{
	if (GetBitMapAttr(srcBitMap,BMA_FLAGS)&BMF_INTERLEAVED)
	{
		LONG src_depth = GetBitMapAttr(srcBitMap,BMA_DEPTH);
		struct Rectangle rect;
		struct BltMaskHook hook;
		
		/* Define the destination rectangle in the rastport */
		rect.MinX = xDest;
		rect.MinY = yDest;
		rect.MaxX = xDest + xSize - 1;
		rect.MaxY = yDest + ySize - 1;
		
		/* Initialize the hook */
#if defined(__AMIGAOS4__) || defined(__MORPHOS__)
		hook.hook.h_Entry = (HOOKFUNC)hookEntry;
		hook.hook.h_SubEntry = (HOOKFUNC)HookFunc_BltMask;
#else
		hook.hook.h_Entry = (HOOKFUNC)HookFunc_BltMask;
#endif
		hook.srcBitMap = srcBitMap;
		hook.srcx = xSrc;
		hook.srcy = ySrc;
		hook.destx = xDest;
		hook.desty = yDest;

		/* Initialize a bitmap where all plane pointers points to the mask */
		InitBitMap(&hook.maskBitMap,src_depth,GetBitMapAttr(srcBitMap,BMA_WIDTH),GetBitMapAttr(srcBitMap,BMA_HEIGHT));
		while (src_depth)
			hook.maskBitMap.Planes[--src_depth] = bltMask;

		/* Blit onto the Rastport */
		DoHookClipRects(&hook.hook,destRP,&rect);
	} else
	{
		BltMaskBitMapRastPort(srcBitMap, xSrc, ySrc, destRP, xDest, yDest, xSize, ySize, minterm, bltMask);
	}
}

LONG GetControlChar(char *label)
{
	char *buf = strchr(label,'_');
	if (buf) return ToLower(*(buf+1));
	return 0;
}

/* Free all resources allocated in ParseTemplate */
VOID FreeTemplate(APTR m)
{
	ULONG *mem = (ULONG*)m;
	if(mem)
	{
		if(mem[2]) FreeArgs((struct RDArgs*)mem[2]);
		if(mem[1]) FreeVec((APTR)mem[1]);
		if(mem[0]) FreeDosObject(DOS_RDARGS,(APTR)mem[0]);
	}
	FreeVec(mem);
}

/* Parse a line for a template. Result should be freed via
   FreeTemplate() */
APTR ParseTemplate(STRPTR temp, STRPTR line, APTR results)
{
	ULONG *mem = (ULONG*)AllocVec(12,0);
	if(mem)
	{
		struct RDArgs *rdargs = (struct RDArgs*)AllocDosObject(DOS_RDARGS,NULL);
		if((mem[0] = (ULONG)rdargs))
		{
			LONG len = mystrlen(line)+2;
			STRPTR buf = (STRPTR)AllocVec(len,0);

			if((mem[1] = (ULONG)buf))
			{
				struct RDArgs *rd;

				if (line) strcpy(buf,line);
				buf[len-2]=10;
				buf[len-1]=0;
	
				rdargs->RDA_Buffer = NULL;
				rdargs->RDA_Source.CS_Buffer = buf;
				rdargs->RDA_Source.CS_Length = strlen(buf);
				rdargs->RDA_Source.CS_CurChr = 0;
		
				rd = ReadArgs(temp,(LONG*)results, rdargs );
				if((mem[2] = (ULONG)rd))
				{
					return mem;
				}
			}
		}
	}
	FreeTemplate(mem);
	return NULL;
}

LONG SendRexxCommand(STRPTR port, STRPTR Cmd, STRPTR Result, LONG ResultSize)
{
	struct MsgPort *RexxPort;
	struct MsgPort *ReplyPort = &((struct Process *)FindTask(NULL))->pr_MsgPort;
	struct Library *RexxSysBase;
#ifdef __AMIGAOS4__
	struct RexxSysIFace *IRexxSys;
#else
	void *IRexxSys;
#endif
	int rc;

	*Result = '\0';

	if ((RexxSysBase = OpenLibraryInterface("rexxsyslib.library", 44L, &IRexxSys)))
	{
		Forbid();

		if ((RexxPort = FindPort(port)))
		{
			struct RexxMsg *rexxMsg, *Answer;

			if ((rexxMsg = CreateRexxMsg(ReplyPort, NULL, NULL)))
			{
				if ((rexxMsg->rm_Args[0] = CreateArgstring(Cmd, strlen(Cmd))))
				{
					rexxMsg->rm_Action = RXCOMM | RXFF_RESULT;

					PutMsg(RexxPort, &rexxMsg->rm_Node);
					Permit();

					do
					{
						WaitPort(ReplyPort);
						Answer = (struct RexxMsg *)GetMsg(ReplyPort);
					} while (Answer == NULL);

					if ((rc = (Answer->rm_Result1 == RETURN_OK)))
					{
						if (Answer->rm_Result2)
						{
							strncpy(Result, (STRPTR)Answer->rm_Result2, ResultSize);
							DeleteArgstring((UBYTE *)Answer->rm_Result2);
						}
					}

					DeleteArgstring((UBYTE *)ARG0(Answer));
					DeleteRexxMsg(Answer);

					CloseLibraryInterface(RexxSysBase,IRexxSys);
					return rc;
				} else
				{
					DeleteRexxMsg(rexxMsg);
				}
			}
		}

		Permit();

		CloseLibraryInterface(RexxSysBase,IRexxSys);
	}

	return 0;
}

