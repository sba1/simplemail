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

struct filter;
struct folder;
struct mail_info;
struct addressbook_entry_new;
struct search_options;
struct list;
struct string_list;

int callback_read_active_mail(void);
void callback_save_active_mail(void);
int callback_read_mail(struct folder *f, struct mail_info *mail, int window);
int callback_delete_mails_silent(int permanent);
void callback_delete_mails(void);
int callback_delete_mail(struct mail_info *mail);
void callback_get_address(void);
int callback_open_message(char *message, int window);
void callback_new_mail(void);
void callback_reply_mails(char *folder_path, int num, struct mail_info **to_reply_array);
void callback_reply_selected_mails(void);
void callback_forward_mails(char *folder_path, int num, struct mail_info **to_forward_array);
void callback_forward_selected_mails(void);
void callback_change_mail(void);
void callback_show_raw(void);
void callback_create_sender_filter(void);
void callback_create_subject_filter(void);
void callback_create_recipient_filter(void);
int callback_move_mail_request(char *folder_path, struct mail_info *mail);
void callback_move_selected_mails(void);
void callback_check_selected_folder_for_spam(void);
void callback_move_spam_marked_mails(void);
void callback_add_spam_folder_to_statistics(void);
void callback_classify_selected_folder_as_ham(void);
void callback_mail_within_main_selected(void);
void callback_quick_filter_changed(void);
void callback_progmon_button_pressed(void);

void callback_fetch_mails(void);
void callback_check_single_account(int account_num);
void callback_send_mails(void);
void callback_search(void);
void callback_start_search(struct search_options *so);
void callback_stop_search(void);
void callback_filter(void);
void callback_new_folder(void);
void callback_new_folder_path(char *path, char *name);
void callback_new_group(void);
void callback_remove_folder(void);
void callback_edit_filter(void);
void callback_addressbook(void);
void callback_config(void);
void callback_folder_active(void);
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

void callback_write_mail_to(struct addressbook_entry_new *address);
int callback_write_mail_to_str(char *str, char *subject);
int callback_write_mail_to_str_with_body(char *str, char *subject, char *body);

int callback_failed_ssl_verification(char *server_name, char *reason, char *cert_summary, char *sha1_ascii, char *sha256_ascii);

void callback_imap_submit_folders(struct folder *f, struct string_list *list);
void callback_imap_get_folders(struct folder *f);
void callback_edit_folder(void);
void callback_change_folder_attrs(void);
void callback_reload_folder_order(void);
void callback_refresh_folders(void);

void callback_move_mail(struct mail_info *mail, struct folder *from_folder, struct folder *dest_folder);
void callback_maildrop(struct folder *dest_folder);
void callback_mails_mark(int mark);
void callback_mails_set_status(int status);
void callback_selected_mails_are_spam(void);
void callback_selected_mails_are_ham(void);
void callback_check_selected_mails_if_spam(void);

void callback_apply_folder(struct filter *filter);

/**
 * Tries to match the given mail with criterias of any remote filter.
 * Returns 1 if mail should be ignored otherwise 0.
 * (yes, this has to be extended in the future)
 *
 * @param mail the mail that is subjected to the comparision.
 * @return whether the mail the criterias of any filter.
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

int simplemail_get_mail_info_excerpt_lazy(struct mail_info *mail);

void callback_mail_changed(struct folder *folder, struct mail_info *oldmail, struct mail_info *newmail);

void callback_config_changed(void);

void callback_autocheck_reset(void);

void callback_select_mail(int num);

void callback_delete_all_indexfiles(void);
void callback_save_all_indexfiles(void);
void callback_rescan_folder(void);

int callback_import_addressbook(void);



void simplemail_update_progress_monitors(void);


int simplemail_init(void);
void simplemail_deinit(void);

/* the main entry point */
int simplemail_main(void);


#endif

