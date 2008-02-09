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
** messageviewclass.h
*/

#ifndef SM__MAILTEXTVIEW_H
#define SM__MAILTEXTVIEW_H

struct mail;

IMPORT struct MUI_CustomClass *CL_MessageView;
#define MessageViewObject (Object*)MyNewObject(CL_MessageView->mcc_Class, NULL
/*#define MessageViewObject BOOPSIOBJMACRO_START(CL_MessageView->mcc_Class)*/
#define MUIM_MessageView_DisplayMail				(TAG_USER | 0x30140101)
/*#define MUIM_MessageView_Changed						(TAG_USER | 0x30140102)*/

struct MUIP_MessageView_DisplayMail {ULONG MethodID; struct mail_info *mail; char *folder_path;};
/*#define MUIA_MessageView_xxx (TAG_USER | 0x30140001)*/ /* I.. type */

int create_messageview_class(void);
void delete_messageview_class(void);

#endif  /* SM__MESSAGEVIEWCLASS_H */
