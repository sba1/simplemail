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
** multistringclass.h
*/

#ifndef SM__MULTISTRINGCLASS_H
#define SM__MULTISTRINGCLASS_H

/* MultiStringClass */
IMPORT struct MUI_CustomClass *CL_MultiString;
#define MultiStringObject (Object*)MyNewObject(CL_MultiString->mcc_Class, NULL

#define MUIA_MultiString_ContentsArray  (TAG_USER | 0x30080001) /* STRPTR * */

#define MUIM_MultiString_AddStringField (TAG_USER | 0x30080101)
struct  MUIP_MultiString_AddStringField { ULONG MethodID; const char *contents;};

/* SingeStringClass */
IMPORT struct MUI_CustomClass *CL_SingleString;
#define SingleStringObject (Object*)MyNewObject(CL_SingleString->mcc_Class, NULL

#define MUIA_SingleString_Event (TAG_USER | 0x30090001)

#define MUIV_SingleString_Event_CursorUp            1
#define MUIV_SingleString_Event_CursorDown          2
#define MUIV_SingleString_Event_CursorToPrevLine    3
#define MUIV_SingleString_Event_CursorToNextLine    4
#define MUIV_SingleString_Event_ContentsToPrevLine  5

int create_multistring_class(void);
void delete_multistring_class(void);

#endif
