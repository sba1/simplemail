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
 * @file pgplistclass.h
 */

#ifndef SM__PGPLISTCLASS_H
#define SM__PGPLISTCLASS_H

/* the objects of the listview */
IMPORT struct MUI_CustomClass *CL_PGPList;
#define PGPListObject (Object*)MyNewObject(CL_PGPList->mcc_Class, NULL

#define MUIM_PGPList_Refresh (TAG_USER | 0x300E0101)
struct  MUIP_PGPList_Refresh { ULONG MethodID;};

/**
 * Create the pgp list custom class.
 *
 * @return 0 on failure, 1 on success
 */
int create_pgplist_class(void);

/**
 * Delete the pgp list custom class.
 */
void delete_pgplist_class(void);

#endif
