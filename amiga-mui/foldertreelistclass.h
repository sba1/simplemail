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
** foldertreelistclass.h
*/

#ifndef SM__FOLDERTREELISTCLASS_H
#define SM__FOLDERTREELISTCLASS_H

IMPORT struct MUI_CustomClass *CL_FolderTreelist;
#define FolderTreelistObject (Object*)NewObject(CL_FolderTreelist->mcc_Class, NULL

#define MUIA_FolderTreelist_MailDrop (TAG_USER+0x31000001) /* (struct folder *) */
#define MUIA_FolderTreelist_OrderChanged (TAG_USER+0x31000002) /* BOOL */

int create_foldertreelist_class(void);
void delete_foldertreelist_class(void);


#endif
