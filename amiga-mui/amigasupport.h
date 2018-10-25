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
** amigasupport.h
*/

#ifndef SM__AMIGASUPPORT_H
#define SM__AMIGASUPPORT_H

#include <dos/dos.h>

struct Library *OpenLibraryInterface(CONST_STRPTR name, int version, void *interface_ptr);
void CloseLibraryInterface(struct Library *lib, void *interface);

void SecondsToString( char *buf, unsigned int seconds);
void SecondsToStringLong( char *buf, unsigned int seconds);
void SecondsToDateString( char *buf, unsigned int seconds);
void SecondsToTimeString( char *buf, unsigned int seconds);
VOID OpenURL(STRPTR uri);
STRPTR StrCopy(const STRPTR str);
ULONG ConvertKey(struct IntuiMessage *imsg);
STRPTR NameOfLock( BPTR lock );
VOID MyBltMaskBitMapRastPort( struct BitMap *srcBitMap, LONG xSrc, LONG ySrc, struct RastPort *destRP, LONG xDest, LONG yDest, LONG xSize, LONG ySize, ULONG minterm, APTR bltMask );
LONG GetControlChar(const char *buf);
VOID FreeTemplate(APTR m);
APTR ParseTemplate(CONST_STRPTR temp, STRPTR line, APTR results);
LONG SendRexxCommand(STRPTR port, STRPTR Cmd, STRPTR Result, LONG ResultSize);


/* Compatibility wrapper as DeleteFile() was renamed to Delete()
 * in SDK 53.24
 */

#ifdef INLINE4_DOS_H
static inline BOOL DeleteFile(STRPTR name)
{
	return Delete(name);
}
#endif

#endif
