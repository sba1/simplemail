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
 * @file searchwnd.h
 */

#ifndef SM__SEARCHWND_H
#define SM__SEARCHWND_H

/**
 * Opens the search window displaying the folder name.
 *
 * @param foldername the folder name to be displayed
 */
void search_open(char *foldername);

/**
 *  Refreshes the folder list
 */
void search_refresh_folders(void);

/**
 * Clear the results.
 */
void search_clear_results(void);

/**
 * Add the mails of the given array as results to the search window.
 *
 * @param array the array
 * @param size number of elements to be added
 */
void search_add_result(struct mail_info **array, int size);

/**
 * Enable the possibility to search
 */
void search_enable_search(void);

/**
 * Disable the possibility to search
 */
void search_disable_search(void);

/**
 * Returns whether there are any mails.
 *
 * @return true if there are mails in the result list
 */
int search_has_mails(void);

/**
 * Remove the given mail from the results view.
 *
 * @param m mail to be removed
 */
void search_remove_mail(struct mail_info *m);

#endif

