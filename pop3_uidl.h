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
 * @file
 */

#ifndef SM__POP3_UIDL_H
#define SM__POP3_UIDL_H

struct pop3_server;

/**
 * @brief the uidl contents.
 *
 * @note TODO: Use a better data structure
 */
struct uidl
{
	char *filename; /**< the filename of the uidl file */
	int num_entries; /**< number of entries */
	struct uidl_entry *entries; /**< these are the entries */
};

/**
 * @brief Initialize a given uidl
 *
 * @param uidl the uidl instance to be initialized
 * @param server the server in question
 * @param folder_directory base directory of the folders
 */
void uidl_init(struct uidl *uidl, struct pop3_server *server, char *folder_directory);

/**
 * Opens the uidl file if if it exists.
 *
 * @param uidl the uidl as initialized by uidl_init().
 * @param server the server for the uidl.
 * @return whether successful or not.
 */
int uidl_open(struct uidl *uidl, struct pop3_server *server);

/**
 * Tests if a uidl is inside the uidl file.
 *
 * @param uidl the uidl.
 * @param to_check the uidl to check
 * @return whether the uidl to_check is contained in the uidl.
 */
int uidl_test(struct uidl *uidl, char *to_check);

/**
 * Remove no longer used uidls in the uidl file. That means uidls which are
 * not on the server, are removed from the uidl file.
 *
 * @param uidl the uidl to synchronize
 * @param num_dl_mails number of entries within the dl_mails array.
 * @param dl_mails the dl_mails containing all uidls on the server.
 */
void uidl_remove_unused(struct uidl *uidl, int num_remote_uidls, char **remote_uidls);

/**
 * Add the uidl to the uidl file. Writes this into the file. Its directly
 * written to the correct place. The uidl->entries array is not expanded.
 *
 * @param uidl the uidl file
 * @param new_uidl the uidl that should be added
 */

void uidl_add(struct uidl *uidl, const char *new_uidl);

#endif
