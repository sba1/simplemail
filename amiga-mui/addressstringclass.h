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
 * @file addressstringclass.h
 */

#ifndef SM__ADDRESSSTRING_HPP
#define SM__ADDRESSSTRING_HPP

/* AddressStringClass */
IMPORT struct MUI_CustomClass *CL_AddressString;
#define AddressStringObject (Object*)MyNewObject(CL_AddressString->mcc_Class, NULL

#define MUIM_AddressString_Complete   (TAG_USER | 0x30010101)
#define MUIM_AddressString_UpdateList (TAG_USER | 0x30010102)
struct  MUIP_AddressString_Complete   {ULONG MethodID; char *text;};
struct  MUIP_AddressString_UpdateList {ULONG MethodID;};

/* MatchWindowClass */
#define MUIA_MatchWindow_Entries (TAG_USER | 0x30020001)
#define MUIA_MatchWindow_String  (TAG_USER | 0x30020002)
#define MUIA_MatchWindow_ActiveString (TAG_USER | 0x30020003)

#define MUIM_MatchWindow_Up      (TAG_USER | 0x30020101)
#define MUIM_MatchWindow_Down    (TAG_USER | 0x30020102)
#define MUIM_MatchWindow_Refresh (TAG_USER | 0x30020103)

struct  MUIP_MatchWindow_Refresh {ULONG MethodID; char *pattern;};

/**
 * Create the address string custom class.
 *
 * @return 0 on failure, 1 on success
 */
int create_addressstring_class(void);

/**
 * Delete the address string list custom class.
 */
void delete_addressstring_class(void);

#endif
