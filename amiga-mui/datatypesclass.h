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
** datatypesclass.h
*/

#ifndef SM__DATATYPESCLASS_H
#define SM__DATATYPESCLASS_H

/* the objects of the listview */
IMPORT struct MUI_CustomClass *CL_DataTypes;
#define DataTypesObject (Object*)NewObject(CL_DataTypes->mcc_Class, NULL

#define MUIA_DataTypes_FileName  (TAG_USER+0x45678) /* (char*) .S. */
#define MUIA_DataTypes_Buffer    (TAG_USER+0x45679) /* (void *) .S. */
#define MUIA_DataTypes_BufferLen (TAG_USER+0x4567a) /* (ULONG) .S. */
#define MUIA_DataTypes_SupportsPrint (TAG_USER+0x4567b) /* (BOOL) */

#define MUIM_DataTypes_Print (0x787878)
#define MUIM_DataTypes_PrintCompleted (0x787879)

int create_datatypes_class(void);
void delete_datatypes_class(void);

#endif
