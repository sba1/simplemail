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
 * @file folderwnd.h
 */

#ifndef SM__FOLDERWND_H
#define SM__FOLDERWND_H

struct folder;

/**
 * Edit settings of the given folder.
 *
 * @param f folder to be edited.
 */
void folder_edit(struct folder *f);

/**
 * Update the GUI folder list according to the given lists.
 *
 * @param all_folders contains all known folders
 * @param sub_folders contains all subscribed folders
 */
void folder_fill_lists(struct remote_folder *all_folders, int num_all_folders, struct remote_folder *sub_folders, int num_sub_folders);

/**
 * Return the folder that is currently being edited.
 *
 * @return the folder that is currently being edited or NULL if no such folder
 *  exists
 */
struct folder *folder_get_changed_folder(void);

/**
 * Return the name of the changed folder.
 *
 * @return the name
 */
char *folder_get_changed_name(void);

/**
 * Return the path of the changed folder
 *
 * @return the path of the folder currently being edited
 */
char *folder_get_changed_path(void);

/**
 * Return the type of the folder currently being edited
 *
 * @return the folder type
 */
int folder_get_changed_type(void);

/**
 * Return the default to field of the folder currently being edited
 *
 * @return the settings of the default to field.
 */
char *folder_get_changed_defto(void);

/**
 * Return the default to field of the folder currently being edited
 *
 * @return the settings for the default from field
 */
char *folder_get_changed_deffrom(void);

/**
 * Return the default replay to field of the folder currently being edited
 *
 * @return the settings for the default reply to field
 */
char *folder_get_changed_defreplyto(void);

/**
 * Return the default signature of the folder currently being edited
 *
 * @return the settings for the default signature
 */
char *folder_get_changed_defsignature(void);

/**
 * Return the primary sort mode of the folder currently being edited
 *
 * @return the settings for the primary sort mode
 */
int folder_get_changed_primary_sort(void);

/**
 * Return the secondary sort mode of the folder currently being edited
 *
 * @return the settings for the secondary sort mode
 */
int folder_get_changed_secondary_sort(void);

/**
 * Return the settings for the imap download mode of folder currently being
 * edited.
 *
 * @return the imap download mode of the folder currently being edited
 */
int folder_get_imap_download(void);

/**
 * Edit a new folder with the given path.
 *
 * @param init_path the initial path of the new folder.
 */
void folder_edit_new_path(char *init_path);

/**
 * Refresh the signature cycle in accordance of the current configuration
 */
void folder_refresh_signature_cycle(void);

#endif

