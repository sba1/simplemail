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

#define MultiStringObject (Object*)(NewObject)(CL_MultiString->mcc_Class, NULL

#define MUIA_MultiString_ContentsArray		(TAG_USER+0x182c000) /* STRPTR * */

#define MUIM_MultiString_AddStringField (TAG_USER+0x677)

struct MUIP_MultiString_AddStringField { ULONG MethodID; const char *contents;};

extern struct MUI_CustomClass *CL_MultiString;

int create_multistring_class(void);
void delete_multistring_class(void);

#endif
