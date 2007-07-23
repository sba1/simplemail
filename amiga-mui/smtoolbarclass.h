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
** smtoolbarclass.h
**
** This class is for building a toolbar with MCC_TheBar or with our
** own PictureButtons.
**
*/

#ifndef SM__SMTOOLBARCLASS_H
#define SM__SMTOOLBARCLASS_H

#include <mui/TheBar_mcc.h>

IMPORT struct MUI_CustomClass *CL_SMToolbar;
#define SMToolbarObject (Object*)MyNewObject(CL_SMToolbar->mcc_Class, NULL

/* Attributes */
#define MUIA_SMToolbar_Buttons       (TAG_USER | 0x30180001) /* I... struct MUIS_SMToolbar_Button * */
#define MUIA_SMToolbar_InVGroup      (TAG_USER | 0x30180002) /* I... BOOL */
#define MUIA_SMToolbar_AddHVSpace    (TAG_USER | 0x30180003) /* I... BOOL */

/* Attributes for MUIM_SMToolbar_SetAttr and MUIM_SMToolbar_GetAttr */
#define MUIA_SMToolbar_Attr_Hide     (TAG_USER | 0x30180080 ) /* .SG. BOOL */
#define MUIA_SMToolbar_Attr_Disabled (TAG_USER | 0x30180081 ) /* .SG. BOOL */
#define MUIA_SMToolbar_Attr_Selected (TAG_USER | 0x30180082 ) /* .SG. BOOL */

/* Methods */
#define MUIM_SMToolbar_SetAttr   (TAG_USER | 0x30180101)
#define MUIM_SMToolbar_GetAttr   (TAG_USER | 0x30180102)
#define MUIM_SMToolbar_GetObject (TAG_USER | 0x30180103)
#define MUIM_SMToolbar_DoMethod  (TAG_USER | 0x30180104)

/* Methods structures */
struct  MUIP_SMToolbar_SetAttr   { ULONG MethodID; ULONG id; Tag attr; ULONG value; };
struct  MUIP_SMToolbar_GetAttr   { ULONG MethodID; ULONG id; Tag attr; ULONG *storage; };
struct  MUIP_SMToolbar_GetObject { ULONG MethodID; ULONG id; };
struct  MUIP_SMToolbar_DoMethod  { ULONG MethodID; ULONG id; ULONG FollowParams; /* ... */};

/* Button structure */
struct MUIS_SMToolbar_Button
{
	ULONG pos;
	ULONG id;
	ULONG flags;
	char *name;
	char *help;
	char *imagename;
};

/* buttonflags */
#define MUIV_SMToolbar_ButtonFlag_Toggle    1<<0
#define MUIV_SMToolbar_ButtonFlag_Disabled  1<<1
#define MUIV_SMToolbar_ButtonFlag_Selected  1<<2
#define MUIV_SMToolbar_ButtonFlag_Hide      1<<3

/* special pos values */
#define MUIV_SMToolbar_Space -1
#define MUIV_SMToolbar_End   -2

/* Values for the Strip PROGDIR:Images/images */
#define SMTOOLBAR_STRIP_ROWS   10
#define SMTOOLBAR_STRIP_COLS    7
#define SMTOOLBAR_STRIP_HSPACE  1
#define SMTOOLBAR_STRIP_VSPACE  1
#define PIC(x,y) (x+y*SMTOOLBAR_STRIP_COLS)

int create_smtoolbar_class(void);
void delete_smtoolbar_class(void);

#endif
