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
** addressstringclass.h
*/

#ifndef SM__ADDRESSSTRING_HPP
#define SM__ADDRESSSTRING_HPP

IMPORT struct MUI_CustomClass *CL_AddressString;
#define AddressStringObject (Object*)NewObject(CL_AddressString->mcc_Class, NULL

#define MUIM_AddressString_Complete 0x676767
struct MUIP_AddressString_Complete {ULONG MethodID; char *text;};

int create_addressstring_class(void);
void delete_addressstring_class(void);

#endif
