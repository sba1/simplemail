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
** transwndclass.h
*/

#ifndef SM__TRANSWNDCLASS_H
#define SM__TRANSWNDCLASS_H

extern struct MUI_CustomClass *CL_transwnd;

#define transwndObject (Object *) NewObject(CL_transwnd->mcc_Class, NULL

int create_transwnd_class(void);
void delete_transwnd_class(void);

#define MUIA_transwnd_Status      (TAG_USER | (116456070 << 16) | 0x0002)
#define MUIA_transwnd_Gauge1_Str  (TAG_USER | (116456070 << 16) | 0x0003)
#define MUIA_transwnd_Gauge1_Max  (TAG_USER | (116456070 << 16) | 0x0004)
#define MUIA_transwnd_Gauge1_Val  (TAG_USER | (116456070 << 16) | 0x0005)
#define MUIA_transwnd_Gauge2_Str  (TAG_USER | (116456070 << 16) | 0x0006)
#define MUIA_transwnd_Gauge2_Max  (TAG_USER | (116456070 << 16) | 0x0007)
#define MUIA_transwnd_Gauge2_Val  (TAG_USER | (116456070 << 16) | 0x0008)
#define MUIA_transwnd_Aborted     (TAG_USER | (116456070 << 16) | 0x0009) /* N */

#define MUIM_transwnd_InsertMailSize  0x4545454
#define MUIM_transwnd_InsertMailInfo  0x4545455
#define MUIM_transwnd_GetMailFlags    0x4545456
#define MUIM_transwnd_Clear           0x4545457
#define MUIM_transwnd_Wait            0x4545458

struct MUIP_transwnd_InsertMailSize {
	ULONG MethodId;
	LONG Num;
	LONG Flags;
	LONG Size;
};

struct MUIP_transwnd_InsertMailInfo {
	ULONG MethodId;
	LONG Num;
	STRPTR From;
	STRPTR Subject;
	ULONG Seconds;
};

struct MUIP_transwnd_GetMailFlags {
	ULONG MethodID;
	LONG Num;
};

struct MUIP_transwnd_Wait {
	ULONG MethodID;
};

#endif