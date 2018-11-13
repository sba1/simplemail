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
 * @file mainwnd.h
 */

#ifndef SM__MAINWND_H
#define SM__MAINWND_H

#ifndef SM__CODESETS_H
#include "codesets.h"
#endif

struct folder;
struct mail_info;

/**
 * Initialize the main window
 *
 * @return 1 for success, 0 for failure.
 */
int main_window_init(void);

/**
 * Deinitializes the main window.
 */
void main_window_deinit(void);

/**
 * Open the main window.
 *
 * @return 1 on success, 0 on failure.
 */
int main_window_open(void);

/**
 * Refreshes the folder list
 */
void main_refresh_folders(void);

/**
 * Refreshes a single folder entry.
 */
void main_refresh_folder(struct folder *folder);

/**
 * Inserts a new mail into the mail at the end
 *
 * @param mail the mail to be inserted.
 */
void main_insert_mail(struct mail_info *mail);

/**
 * Inserts a new mail into the listview after a given position
 *
 * @param mail the mail to be inserted
 * @param after the position
 */
void main_insert_mail_pos(struct mail_info *mail, int after);

/**
 * Remove a given mail from the listview
 *
 * @param mail the mail to be removed
 */
void main_remove_mail(struct mail_info *mail);

/**
 * Replaces a mail with a new mail.
 * This also activates the new mail, however it would be better if
 * this done by an extra call
 *
 * @param oldmail the mail to be replaced
 * @param newmail the mail that replaced oldmail
 */
void main_replace_mail(struct mail_info *oldmail, struct mail_info *newmail);

/**
 * Refresh a mail (if it status has changed fx)
 *
 * @param m the mail to be refreshed
 */
void main_refresh_mail(struct mail_info *m);

/**
 * Clears the folder list
 */
void main_clear_folder_mails(void);

/**
 * Updates the mail trees with the mails in the given folder
 *
 * @param folder
 */
void main_set_folder_mails(struct folder *folder);

/**
 * Activates a different folder
 *
 * @param folder the folder to be activated.
 */
void main_set_folder_active(struct folder *folder);

/**
 * Returns the current selected folder, NULL if no real folder
 * has been selected. It returns the true folder like it is
 * in the folder list
 *
 * @return the currently selected folder
 */
struct folder *main_get_folder(void);

/**
 * Returns the path of the current selected folder, or NULL if no
 * folder is selected.
 *
 * @return the path of the currently selected folder
 */
char *main_get_folder_drawer(void);

/**
 * Sets the active mail
 *
 * @param m the mail to be the active one
 */
void main_set_active_mail(struct mail_info *m);

/**
 * Returns the active mail.
 *
 * @return the active mail or NULL if no one is active.
 */
struct mail_info *main_get_active_mail(void);

/**
 * Returns the filename of the active mail, NULL if no thing is
 * selected
 *
 * @return the file name of the selected mail or NULL.
 */
char *main_get_mail_filename(void);

/**
 * Returns the contents of the quick filter (or NULL).
 *
 * @return the quick filter contents.
 */
utf8 *main_get_quick_filter_contents(void);

/**
 * Returns the first selected mail.
 *
 * @param handle points to a memory of size of at least an pointer or an integer.
 * @return the first selected mail or NULL if there is no mail selected.
 */
struct mail_info *main_get_mail_first_selected(void *handle);

/**
 * Returns the next selected mail.
 *
 * @param handle points to a memory of size of at least an pointer or an integer.
 * @return the next mail or NULL if there is no other mail selected.
 */
struct mail_info *main_get_mail_next_selected(void *handle);

/**
 * Remove all selected mails
 */
void main_remove_mails_selected(void);

/**
 * Refresh all selected mails
 */
void main_refresh_mails_selected(void);

/**
 * Refresh/build the check single account menu
 */
void main_build_accounts(void);

/**
 * Refresh/build the address book displayed in the window.
 */
void main_build_addressbook(void);

/**
 * Refresh/build the scripts menu.
 */
void main_build_scripts(void);

/**
 * Freeze the mail list
 */
void main_freeze_mail_list(void);

/**
 * Thaws the mail list
 */
void main_thaw_mail_list(void);

/**
 * Make the given mail active.
 *
 * @param mail number of mail to be activated
 */
void main_select_mail(int mail);

/**
 * Return the iconified state of the window
 *
 * @return 1 if the main window is iconified
 */
int main_is_iconified(void);

/**
 * Return whether the message view is displayed.
 *
 * @return 1 if the message view is displayed, 0 otherwise.
 */
int main_is_message_view_displayed(void);

/**
 * Sets an UTF-8 encoded status text.
 *
 * @param txt status text to display
 */
void main_set_status_text(char *txt);

/**
 * Sets the visual aspects of the global progress bar.
 *
 * @param max_work
 * @param work
 */
void main_set_progress(unsigned int max_work, unsigned int work);

/**
 * Hides the global progress bar.
 */
void main_hide_progress(void);

/**
 * Displays the given mail within the message view
 */
void main_display_active_mail(void);

/**
 * Refresh the title of the main window
 *
 * @param title the title to use
 */
void main_refresh_window_title(const char *title);

#endif
