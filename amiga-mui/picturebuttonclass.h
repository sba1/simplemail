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
** picturebuttonclass.h
*/

#ifndef SM__PICTUREBUTTONCLASS_H
#define SM__PICTUREBUTTONCLASS_H

#define PictureButtonObject (Object*)(NewObject)(CL_PictureButton->mcc_Class, NULL

#define MUIA_PictureButton_Filename			(TAG_USER+0x182b000) /* STRPTR */
#define MUIA_PictureButton_Label					(TAG_USER+0x182b001) /* STRPTR */
#define MUIA_PictureButton_FreeVert			(TAG_USER+0x182b002) /* BOOL */
#define MUIA_PictureButton_ShowLabel			(TAG_USER+0x182b003) /* BOOL */

extern struct MUI_CustomClass *CL_PictureButton;

int create_picturebutton_class(void);
void delete_picturebutton_class(void);

Object *MakePictureButton(char *label, char *filename);

#endif
