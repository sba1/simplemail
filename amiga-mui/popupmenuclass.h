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

/**
 * @file popupmenuclass.h
 */

#ifndef SM__POPUPMENUCLASS_H
#define SM__POPUPMENUCLASS_H

IMPORT struct MUI_CustomClass *CL_Popupmenu;
#define PopupmenuObject (Object*)MyNewObject(CL_Popupmenu->mcc_Class, NULL

/**
 * Create the popup menu custom class.
 *
 * @return 0 on failure, 1 on success
 */
int create_popupmenu_class(void);

/**
 * Delete the popup menu custom class.
 */
void delete_popupmenu_class(void);

#define MUIA_Popupmenu_Selected     (TAG_USER | 0x300C0001) /* Notify */
#define MUIA_Popupmenu_SelectedData (TAG_USER | 0x300C0002)

#define MUIM_Popupmenu_AddEntry     (TAG_USER | 0x300C0101)
#define MUIM_Popupmenu_Clear        (TAG_USER | 0x300C0102)
struct  MUIP_Popupmenu_AddEntry     { ULONG MethodID; STRPTR Entry; APTR UData;};
struct  MUIP_Popupmenu_Clear        { ULONG MethodID;};

#endif
