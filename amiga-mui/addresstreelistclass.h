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
** addresstreelistclass.h
*/

#ifndef SM__ADDRESSTREELISTCLASS_H
#define SM__ADDRESSTREELISTCLASS_H

IMPORT struct MUI_CustomClass *CL_AddressTreelist;
#define AddressTreelistObject (Object*)(NewObject)(CL_AddressTreelist->mcc_Class, NULL

#define MUIA_AddressTreelist_InAddressbook	(TAG_USER+0x27878) /* BOOL */
#define MUIA_AddressTreelist_AsMatchList		(TAG_USER+0x27879) /* BOOL */

#define MUIM_AddressTreelist_Refresh (TAG_USER+0x267762)

struct MUIP_AddressTreelist_Refresh { ULONG MethodID; char *str ;};

int create_addresstreelist_class(void);
void delete_addresstreelist_class(void);


#endif
