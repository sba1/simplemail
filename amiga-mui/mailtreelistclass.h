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

#define MUIA_MailTreelist_Active           (TAG_USER | 0x300F0001)
#define MUIA_MailTreelist_DoubleClick      (TAG_USER | 0x300F0002)
#define MUIA_MailTreelist_FolderType       (TAG_USER | 0x300F0003)
#define MUIA_MailTreelist_Private		       (TAG_USER | 0x300F0004)
#define MUIA_MailTreelist_First						 (TAG_USER | 0x300F0005)

#define MUIM_MailTreelist_Clear            (TAG_USER | 0x300F0101)
#define MUIM_MailTreelist_Freeze           (TAG_USER | 0x300F0102)
#define MUIM_MailTreelist_GetFirstSelected (TAG_USER | 0x300F0103)
#define MUIM_MailTreelist_GetNextSelected  (TAG_USER | 0x300F0104)
#define MUIM_MailTreelist_InsertMail       (TAG_USER | 0x300F0105)
#define MUIM_MailTreelist_RefreshMail      (TAG_USER | 0x300F0106)
#define MUIM_MailTreelist_RefreshSelected  (TAG_USER | 0x300F0107)
#define MUIM_MailTreelist_RemoveMail       (TAG_USER | 0x300F0108)
#define MUIM_MailTreelist_RemoveSelected   (TAG_USER | 0x300F0109)
#define MUIM_MailTreelist_ReplaceMail      (TAG_USER | 0x300F010A)
#define MUIM_MailTreelist_SetFolderMails   (TAG_USER | 0x300F010B)
#define MUIM_MailTreelist_Thaw             (TAG_USER | 0x300F010C)
struct  MUIP_MailTreelist_GetFirstSelected { ULONG MethodID; void *handle;};
struct  MUIP_MailTreelist_GetNextSelected  { ULONG MethodID; void *handle;};
struct  MUIP_MailTreelist_InsertMail       { ULONG MethodID; struct mail_info *m; int after;};
struct  MUIP_MailTreelist_RefreshMail      { ULONG MethodID; struct mail *m;};
struct  MUIP_MailTreelist_RemoveMail       { ULONG MethodID; struct mail *m;};
struct  MUIP_MailTreelist_ReplaceMail      { ULONG MethodID; struct mail *oldmail; struct mail *newmail;};
struct  MUIP_MailTreelist_SetFolderMails   { ULONG MethodID; struct folder *f;};

Object *MakeMailTreelist(ULONG userid, Object **list);

int create_mailtreelist_class(void);
void delete_mailtreelist_class(void);

/* OBSOLETE, to be removed soon */
#define MUIA_MailTree_Active           (TAG_USER | 0x300F0001)
#define MUIA_MailTree_DoubleClick      (TAG_USER | 0x300F0002)

#define MUIM_MailTree_Clear            (TAG_USER | 0x300F0101)
#define MUIM_MailTree_Freeze           (TAG_USER | 0x300F0102)
#define MUIM_MailTree_GetFirstSelected (TAG_USER | 0x300F0103)
#define MUIM_MailTree_GetNextSelected  (TAG_USER | 0x300F0104)
#define MUIM_MailTree_InsertMail       (TAG_USER | 0x300F0105)
#define MUIM_MailTree_RefreshMail      (TAG_USER | 0x300F0106)
#define MUIM_MailTree_RefreshSelected  (TAG_USER | 0x300F0107)
#define MUIM_MailTree_RemoveMail       (TAG_USER | 0x300F0108)
#define MUIM_MailTree_RemoveSelected   (TAG_USER | 0x300F0109)
#define MUIM_MailTree_ReplaceMail      (TAG_USER | 0x300F010A)
#define MUIM_MailTree_SetFolderMails   (TAG_USER | 0x300F010B)
#define MUIM_MailTree_Thaw             (TAG_USER | 0x300F010C)
struct  MUIP_MailTree_GetFirstSelected { ULONG MethodID; void *handle;};
struct  MUIP_MailTree_GetNextSelected  { ULONG MethodID; void *handle;};
struct  MUIP_MailTree_InsertMail       { ULONG MethodID; struct mail *m; int after;};
struct  MUIP_MailTree_RefreshMail      { ULONG MethodID; struct mail *m;};
struct  MUIP_MailTree_RemoveMail       { ULONG MethodID; struct mail *m;};
struct  MUIP_MailTree_ReplaceMail      { ULONG MethodID; struct mail *oldmail; struct mail *newmail;};
struct  MUIP_MailTree_SetFolderMails   { ULONG MethodID; struct folder *f;};

#endif
