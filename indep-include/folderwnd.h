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

#ifndef SM__FOLDERWND_H
#define SM__FOLDERWND_H

struct folder;

void folder_edit(struct folder *f);
void folder_fill_lists(struct list *list, struct list *sub_folder_list);

struct folder *folder_get_changed_folder(void);
char *folder_get_changed_name(void);
char *folder_get_changed_path(void);
int folder_get_changed_type(void);
char *folder_get_changed_defto(void);
char *folder_get_changed_deffrom(void);
char *folder_get_changed_defreplyto(void);
char *folder_get_changed_defsignature(void);
int folder_get_changed_primary_sort(void);
int folder_get_changed_secondary_sort(void);

void folder_edit_new_path(char *init_path);
void folder_refresh_signature_cycle(void);

#endif

