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
** mailtreelistclass.h
*/

#ifndef SM__MAILTREELISTCLASS_H
#define SM__MAILTREELISTCLASS_H

/* the objects of the listview */
IMPORT struct MUI_CustomClass *CL_MailTreelist;
#define MailTreelistObject (Object*)MyNewObject(CL_MailTreelist->mcc_Class, NULL

/* Special User Data field */
#define MUIV_MailTreelist_UserData_Name (-1)

#define MUIA_MailTreelist_FolderType 	(TAG_USER+0x34567891)
#define MUIA_MailTree_Active						(TAG_USER+0x34567892)
#define MUIA_MailTree_DoubleClick			(TAG_USER+0x34567893)

#define MUIM_MailTree_Clear  					(TAG_USER+0x892892)
#define MUIM_MailTree_SetFolderMails 	(TAG_USER+0x892893)
#define MUIM_MailTree_Freeze						(TAG_USER+0x892894)
#define MUIM_MailTree_Thaw							(TAG_USER+0x892895)
#define MUIM_MailTree_RemoveSelected		(TAG_USER+0x892896)
#define MUIM_MailTree_GetFirstSelected	(TAG_USER+0x892897)
#define MUIM_MailTree_GetNextSelected	(TAG_USER+0x892898)
#define MUIM_MailTree_RefreshMail			(TAG_USER+0x892899)
#define MUIM_MailTree_InsertMail				(TAG_USER+0x89289a)
#define MUIM_MailTree_RemoveMail				(TAG_USER+0x89289b)
#define MUIM_MailTree_ReplaceMail			(TAG_USER+0x89289c)
#define MUIM_MailTree_RefreshSelected	(TAG_USER+0x89288d)

struct MUIP_MailTree_SetFolderMails { ULONG MethodID; struct folder *f;};
struct MUIP_MailTree_GetFirstSelected { ULONG MethodID; void *handle;};
struct MUIP_MailTree_GetNextSelected { ULONG MethodID; void *handle;};
struct MUIP_MailTree_RefreshMail { ULONG MethodID; struct mail *m;};
struct MUIP_MailTree_InsertMail { ULONG MethodID; struct mail *m; int after;};
struct MUIP_MailTree_RemoveMail { ULONG MethodID; struct mail *m;};
struct MUIP_MailTree_ReplaceMail { ULONG MethodID; struct mail *oldmail; struct mail *newmail;};

int create_mailtreelist_class(void);
void delete_mailtreelist_class(void);


#endif
