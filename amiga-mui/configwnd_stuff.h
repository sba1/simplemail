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
** configwnd_stuff.h
*/

char **array_of_addresses_from_texteditor(Object *editor, int page, int *error_ptr, Object *config_wnd, Object *config_list);

IMPORT struct MUI_CustomClass *CL_Sizes;
#define SizesObject (Object*)MyNewObject(CL_Sizes->mcc_Class, NULL

int create_sizes_class(void);
void delete_sizes_class(void);
int value2size(int val);
int size2value(int val);
