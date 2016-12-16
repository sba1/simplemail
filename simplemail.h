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
 * @file simpemail.h
 */

#ifndef SM__SIMPLEMAIL_H
#define SM__SIMPLEMAIL_H

struct account;
struct filter;
struct folder;
struct mail_info;
struct addressbook_entry_new;
struct search_options;
struct list;
struct string_list;


/**
 * The currently selected mail should be viewed in a new window.
 *
 * @return the read window number or -1 for an error.
 */
int callback_read_active_mail(void);

/**
 * Save the currently activated mail to a user-selectable file.
 *
 */
void callback_save_active_mail(void);

/**
 * Display the given mail that is located in the given folder at the given window.
 *
 * @param f the folder that contains the mail
 * @param mail the mail whose contents should be displayed.
 * @param window the window or -1 if a free one should be taken
 * @return the window number (matches window unless window was set to -1)
 */
int callback_read_mail(struct folder *f, struct mail_info *mail, int window);

/**
 * The selected mails should be deleted. The user is not
 * bothered about the consequences.
 *
 * @param permanent 1 if mails should be deleted permanently (rather
 *     than moved to the dest drawer)
 *
 * @return 1 if mails were deleted. Otherwise 0 (e.g., if folder is in use).
 */
int callback_delete_mails_silent(int permanent);

/**
 * Deletes all selected mails.
 */
void callback_delete_mails(void);

/**
 * Deletes the selected mail. That is, move it to the standard deleted folder
 * unless the mails containing folder is the deleted folder, in which case the
 * mails is removed permanently. The view is updated.
 *
 * @param mail the mail that should be deleted.
 * @return 1 on success, 0 failure.
 */
int callback_delete_mail(struct mail_info *mail);

/**
 * Put the address of the currently selected mail into the address book.
 * Don't do anything if the address is already contained.
 */
void callback_get_address(void);

/**
 * Open the mail associated with the give filename.
 *
 * @param mail_filename defines the mail, which should be opened. May be
 *  NULL to indicate that the user is queried about the file name.
 * @param window, defines the window id, in which the mail should be
 *  openen. May be -1.
 * @return the window that actually was used for displaying the mail.
 */
int callback_open_message(char *message, int window);

/**
 * Open the compose mail view for editing a new mail with some defaults.
 */
void callback_new_mail(void);

/**
 * Open the compose mail view for editing a reply to the given mails.
 *
 * @param folder_path the directory where to deposit the new mail
 * @param num number of valid entries in to_reply_array
 * @param to_reply_array the mails that contains the mails to reply
 */
void callback_reply_mails(char *folder_path, int num, struct mail_info **to_reply_array);

/**
 * Open the compose mail view for editing a reply to the currently selected mails.
 */
void callback_reply_selected_mails(void);

/**
 * Open the compose mail view for editing a forward mail for the given mails.
 *
 * @param folder_path the directory where to deposit the new mail.
 * @param num number of valid entries in to_forward_array.
 * @param to_forward_array the mails that contains the mails to forward.
 */
void callback_forward_mails(char *folder_path, int num, struct mail_info **to_forward_array);

/**
 * Open the compose mail view for editing a forward mail for the currently selected
 * mails.
 */
void callback_forward_selected_mails(void);

/**
 * Open the compose mail view for the currently selected mail. It is supposed
 * to be changed directly.
 */
void callback_change_mail(void);

/**
 * Open a view for displaying the raw text of the mail.
 */
void callback_show_raw(void);

/**
 * Create a subject filter from the currently selected mails
 * and open the filter window.
 */
void callback_create_sender_filter(void);

/**
 * Create a subject filter from the currently selected mails
 * and open the filter window.
 */
void callback_create_subject_filter(void);

/**
 * Create a recipient filter from the currently selected mails
 * and open the filter window.
 */
void callback_create_recipient_filter(void);

/**
 * Move a single mail that is in the given folder into a user-selectable directory.
 *
 * @param folder_path the original path of the folder in which the mail is located.
 * @param mail the mail to move.
 * @return whether it worked (1) or not (0)
 */
int callback_move_mail_request(char *folder_path, struct mail_info *mail);

/**
 * Move all currently selected mails into a user-selectable directory.
 */
void callback_move_selected_mails(void);

/**
 * Process the current selected folder and mark all mails which are identified as spam
 */
void callback_check_selected_folder_for_spam(void);

/**
 * Move all mails marked as spam into the spam folder.
 */
void callback_move_spam_marked_mails(void);

/**
 * Adds all mails into the spam folder into the spam statistics.
 */
void callback_add_spam_folder_to_statistics(void);

/**
 * Classifies all mails within the selected folder as ham.
 */
void callback_classify_selected_folder_as_ham(void);

/**
 * A new mail has been selected in the main window. Update the view.
 */
void callback_mail_within_main_selected(void);

/**
 * The quick filter has been changed. Update the view.
 */
void callback_quick_filter_changed(void);

/**
 * The status button within the main window was clicked.
 */
void callback_progmon_button_pressed(void);

/**
 * Fetch mails of active accounts.
 */
void callback_fetch_mails(void);

/**
 * Fetch mails of a single account.
 *
 * @param account_num the account number as in the accounts list.
 */
void callback_check_single_account(int account_num);

/**
 * Send all mails that are scheduled to be sent.
 */
void callback_send_mails(void);

/**
 * The search button has been clicked. Open the search window.
 */
void callback_search(void);

/**
 * Start a new search with the given search options.
 *
 * @param so the search options.
 */
void callback_start_search(struct search_options *so);

/**
 * Stop the search process if there is an active one.
 */
void callback_stop_search(void);

/**
 * Start to apply the active filters on the folders.
 */
void callback_filter(void);

/**
 * Create a new folder
 */
void callback_new_folder(void);

/**
 * Create a new folder and update all views.
 *
 * @param path path of the newly to be created folder
 * @param name name if the newly to be created folder
 */
void callback_new_folder_path(char *path, char *name);

/**
 * Create a new group and update all views.
 */
void callback_new_group(void);

/**
 * Removed the currently selected folder and update all
 * views.
 */
void callback_remove_folder(void);

/**
 * Open the view for filter editing.
 */
void callback_edit_filter(void);

/**
 * Open the view for address book editing.
 */
void callback_addressbook(void);

/**
 * Open the view for config editing.
 */
void callback_config(void);

/**
 * Test the given account.
 *
 * @param ac account to be tested
 */
void callback_test_account(struct account *ac);

/**
 * Update the for a newly actived folder.
 */
void callback_folder_active(void);

/**
 * Return the amount of folders that use the given signature.
 *
 * @param def_signature the signatures for which the usage count shall be defined.
 * @return the usage count of the given signature.
 */
int callback_folder_count_signatures(char *def_signature);

/**
 * Import mails from a user-selectable mbox file into the standard incoming
 * folder.
 */
void callback_import_mbox(void);

/**
 * Import mails from a user-selectable dbx file (outlook express) into the
 * standard incoming folder.
 */
void callback_import_dbx(void);

/**
 * Export the mails of currently selected folder to a user-selectable mbox file.
 */
void callback_export(void);

/**
 * Open a compose window with a pre-filled recipient.

 * @param address the address of the recipient.
 */
void callback_write_mail_to(struct addressbook_entry_new *address);

/**
 * Open a compose window with a pre-filled recipient and subject.
 *
 * @param str the recipient address as a plain string.
 * @param subject the subject of the mail.
 * @return success (1) or not (0)
 */
int callback_write_mail_to_str(char *str, char *subject);

/**
 * Open a compose window with a pre-filled recipient, subject, and body.
 *
 * @param str the recipient address as a plain string.
 * @param subject the subject of the mail.
 * @param body the body of the mail
 * @return success (1) or not (0)
 */
int callback_write_mail_to_str_with_body(char *str, char *subject, char *body);

/**
 * Called on an SSL verification failure.
 *
 * @param server_name the name of the server as supplied by the user to which the ssl connection failed
 * @param reason the reason for the failure.
 * @param cert_summary the summary for the certificate.
 * @param sha1_ascii the sha1 of the failed certificate.
 * @param sha256_ascci the sha256 of the failed certificate. If first byte is a 0 byte then it is ignored.
 * @return 0 whether the any connection attempt should be aborted, any other value when
 *  the user accepts the risk.
 */
int callback_failed_ssl_verification(const char *server_name, const char *reason, const char *cert_summary, const char *sha1_ascii, const char *sha256_ascii);

/**
 * Submit the list of subscribed folders.
 *
 * @param f the folder that must be a root imap folder.
 * @param list the list of folders that should be subscribed.
 */
void callback_imap_submit_folders(struct folder *f, struct string_list *list);

/**
 * For a given imap root folder, receive all folders.
 *
 * @param f the folder that must be a root imap folder.
 */
void callback_imap_get_folders(struct folder *f);

/**
 * Edit folder settings of the currently selected folders.
 */
void callback_edit_folder(void);

/**
 * Apply the attributes of folder that was most recently changed in the folder window.
 */
void callback_change_folder_attrs(void);

/**
 * Reload the folders' order and refresh the view.
 */
void callback_reload_folder_order(void);

/**
 * Refresh the view of the folders.
 */
void callback_refresh_folders(void);

/**
 * A single mail should be moved from a folder to another folder.
 *
 * @param mail the mail in the source folder to be moved
 * @param from_folder the source folder
 * @param dest_folder the destination folder
 */
void callback_move_mail(struct mail_info *mail, struct folder *from_folder, struct folder *dest_folder);

/**
 * The currently selected mails have been dropped into the given folder. Do
 * what ever is appropriate.
 *
 * @param dest_folder the folder in which the mails have been dropped.
 */
void callback_maildrop(struct folder *dest_folder);

/**
 * Mark or unmark all selected mails.
 *
 * @param mark whether to mark or not.
 */
void callback_mails_mark(int mark);

/**
 * Set the status of all selected mails to a given one.
 *
 * @param status the status that the mails should have.
 */
void callback_mails_set_status(int status);

/**
 * Declare that the currently selected mails are spam.
 */
void callback_selected_mails_are_spam(void);

/**
 * Declare that the currently selected mails are ham.
 */
void callback_selected_mails_are_ham(void);

/**
 * Checks if the currently selected mails are spam.
 */
void callback_check_selected_mails_if_spam(void);

/**
 * Apply the given filter to the currently selected folder.
 *
 * @param filter defines the filter to be applied.
 */
void callback_apply_folder(struct filter *filter);

/**
 * Tries to match the given mail with criteria of any remote filter.
 * Returns 1 if mail should be ignored otherwise 0.
 * (yes, this has to be extended in the future)
 *
 * @param mail the mail that is subjected to the comparison.
 * @return whether the mail the criteria of any filter.
 */
int callback_remote_filter_mail(struct mail_info *mail);

/**
 * A new mail should be added to a given folder. If the filename points outside
 * the directory of the folder, the contents of the mail will be copied to the
 * folder under a newly generated name. Otherwise, the mail is just added to
 * the index.
 *
 * @param filename specifies the file to be added.
 * @param folder specifies the destination folder.
 * @return the mail_info of the newly mail.
 */
struct mail_info *callback_new_mail_to_folder(char *filename, struct folder *folder);

/**
 * A new mail should be added to the index. The actual folder is inferred from
 * the path component of the specified filename.
 *
 * @param filename the filename of the mail to be added.
 * @return the newly created mail_info.
 */
struct mail_info *callback_new_mail_to_folder_by_file(char *filename);

/**
 * @brief A new mail has been arrived within the incoming folder.
 *
 * A new mail has been arrived. In the current implementation the mail
 * is not immediately added to the viewer but collected and then added
 * to the viewer after a short period of time.
 *
 * @param filename defines the filename of the new mail
 * @param is_spam is the mail spam?
 *
 * @note FIXME: This functionality takes (mis)usage of next_thread_mail field
 *       FIXME: When SM is quit while mails are downloaded those mails get not presented to the user the next time.
 */
void callback_new_mail_arrived_filename(char *filename, int spam);

/**
 * @brief A new mail arrived into an imap folder
 *
 * @param filename the name of the filename.
 * @param user defines the user name of the login
 * @param server defines the name of the imap server
 * @param path defines the path on the imap server
 */
void callback_new_imap_mail_arrived(char *filename, char *user, char *server, char *path);

/**
 * New mails arrived into an imap folder
 *
 * @param num_filenames number of file names that are stored in filenames array
 * @param filenames an array of filenames.
 * @param user defines the user name of the login
 * @param server defines the name of the imap server
 * @param path defines the path on the imap server
 */
void callback_new_imap_mails_arrived(int num_filenames, char **filenames, char *user, char *server, char *path);

/**
 * Sets a new uid_valid and uid_next for the given imap folder.
 *
 * @param uid_validity
 * @param uid_next
 * @param user defines the user name of the login
 * @param server defines the name of the imap server
 * @param path defines the path on the imap server
 */
void callback_new_imap_uids(unsigned int uid_validity, unsigned int uid_next, char *user, char *server, char *path);

/**
 * The given amount of new mails have been just downloaded.
 *
 * @param num the number of new mails.
 */
void callback_number_of_mails_downloaded(int num);

/**
 * The given mail has just been writted/composed in the standard outgoing folder.
 * The mail is added to the index and the view is updated if needed.
 *
 * @param mail the mail that has just been written.
 */
void callback_new_mail_written(struct mail_info *mail);

/**
 * Delete an imap mail by uid.
 *
 * @param user of the imap account
 * @param server server name of the imap account
 * @param path path on the server
 * @param uid defines the uid of the mail to be deleted
 */
void callback_delete_mail_by_uid(char *user, char *server, char *path, unsigned int uid);

/**
 * Move a mail that has just been sent to the "Sent" drawer.
 *
 * @param filename specifies the name of the mail. This has to be present in the
 *  index with the folder being the standard outgoing folder.
 */
void callback_mail_has_been_sent(char *filename);

/**
 * A mail has NOT been sent, something went wrong, set ERROR status.
 *
 * @param filename specifies the name of the mail that has not been sent.
 */
void callback_mail_has_not_been_sent(char *filename);

/**
 * Add a new imap folder.
 *
 * @param user the login used to differentiate the folder from others.
 * @param server the server.
 * @param path the path on the server.
 */
void callback_add_imap_folder(char *user, char *server, char *path);

/**
 * Replace the oldmail in the folder with the newmail. The oldmail will be
 * deleted from the filesystem.
 *
 * @param folder specifies the folder in which the replacement took place.
 * @param oldmail the original mail that is going to be deleted.
 * @param newmail the mail that replaced the original mail
 */
void callback_mail_changed(struct folder *folder, struct mail_info *oldmail, struct mail_info *newmail);

/**
 * The global configuration has been changed. Update all views.
 */
void callback_config_changed(void);

/**
 * Resets the auto timer
 */
void callback_autocheck_reset(void);

/**
 * Update the view to select the given mail.
 *
 * @param num the number of the mail to be selected as in the current displayed
 *  order.
 */
void callback_select_mail(int num);

/**
 * Delete all indexfiles.
 */
void callback_delete_all_indexfiles(void);

/**
 * Save all indexfiles.
 */
void callback_save_all_indexfiles(void);

/**
 * Callback suitable for folder_rescan_asynch to indicate an issued folder
 * rescanning has been completed.
 *
 * @param folder_path the path folder that have been completed.
 * @param udata unused
 */
void callback_rescan_folder_completed(char *folder_path,  void *udata);

/**
 * Refresh the given folder (only the folder, not its contents)
 */

void callback_refresh_folder(struct folder *f);

/**
 * Rescan the currently selected folder.
 */
void callback_rescan_folder(void);

/**
 * Import an address book into SimpleMail.
 *
 * @return 1 for success, 0 otherwise.
 */
int callback_import_addressbook(void);

/**
 * Updates the progress monitor views.
 */
void simplemail_update_progress_monitors(void);

/**
 * Initializes SimpleMail.
 *
 * @return 1 on success
 */
int simplemail_init(void);

/**
 * Deinitializes SimpleMail.
 */
void simplemail_deinit(void);

/**
 * SimpleMail's entry point.
 *
 * @return 0 on success
 */
int simplemail_main(void);


#endif

