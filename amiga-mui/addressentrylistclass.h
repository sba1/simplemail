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
** addressentrylistclass.h
*/

#ifndef SM__ADDRESSENTRYLISTCLASS_H
#define SM__ADDRESSENTRYLISTCLASS_H

/* AddressEntryListClass */
IMPORT struct MUI_CustomClass *CL_AddressEntryList;
#define AddressEntryListObject (Object*)MyNewObject(CL_AddressEntryList->mcc_Class, NULL

#define MUIA_AddressEntryList_Type				(TAG_USER | 0x30160000) /* i.. ULONG */
#define MUIA_AddressEntryList_GroupName		(TAG_USER | 0x30160001) /* .sg STRPTR */

#define MUIM_AddressEntryList_Refresh			(TAG_USER | 0x30160101)
#define MUIM_AddressEntryList_Store				(TAG_USER | 0x30160102)

struct MUIP_AddressEntryList_Refresh {ULONG MethodID;};

/* Data for MUIA_AddressEntryList_Type */
#define MUIV_AddressEntryList_Type_Addressbook	0
#define MUIV_AddressEntryList_Type_Main					1

int create_addressentrylist_class(void);
void delete_addressentrylist_class(void);

#endif
