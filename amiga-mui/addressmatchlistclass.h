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
 * @file addressmatchlistclass.h
 */

#ifndef SM__ADDRESSMATCHLISTCLASS_H
#define SM__ADDRESSMATCHLISTCLASS_H

/* AddressMatchListClass */
IMPORT struct MUI_CustomClass *CL_AddressMatchList;
#define AddressMatchListObject (Object*)MyNewObject(CL_AddressMatchList->mcc_Class, NULL

#define MUIM_AddressMatchList_Refresh			(TAG_USER | 0x30170101)

struct MUIP_AddressMatchList_Refresh {ULONG MethodID; char *pattern;};

/* The type of the entries */
struct address_match_entry
{
	int is_group;
	union
	{
		struct addressbook_group *group;
		struct addressbook_entry_new *entry;
	} o;
};

/**
 * Create the address match list custom class.
 *
 * @return 0 on failure, 1 on success
 */
int create_addressmatchlist_class(void);

/**
 * Delete the address match list custom class.
 */
void delete_addressmatchlist_class(void);

#endif
