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
** iconclass.h
*/

#ifndef SM__ICONCLASS_H
#define SM__ICONCLASS_H

/* the objects of the listview */
IMPORT struct MUI_CustomClass *CL_Icon;
#define IconObject (Object*)MyNewObject(CL_Icon->mcc_Class, NULL

#define MUIA_Icon_MimeType (TAG_USER+0x45687a) /* (STRPTR) IS. */
#define MUIA_Icon_MimeSubType (TAG_USER+0x45687b) /* (STRPTR) IS. */
#define MUIA_Icon_DoubleClick (TAG_USER+0x45687c) /* (BOOL) ...N */
#define MUIA_Icon_Buffer (TAG_USER+0x35687d)      /* (void*) I... */
#define MUIA_Icon_BufferLen (TAG_USER+0x35687e)   /* (ULONG) I... */
#define MUIA_Icon_DropPath (TAG_USER+0x3568f) /* (STRPTR) ..G */

int create_icon_class(void);
void delete_icon_class(void);

#endif
