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
** simplemail.h
*/

#ifndef SM__SIMPLEMAIL_H
#define SM__SIMPLEMAIL_H

struct filter;
struct folder;
struct mail_info;
struct addressbook_entry_new;
struct search_options;
struct list;

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
int callback_move_mail_request(char *folder_path, struct mail_info *mail);
void callback_move_selected_mails(void);
void callback_check_selected_folder_for_spam(void);
void callback_move_spam_marked_mails(void);
void callback_add_spam_folder_to_statistics(void);
void callback_classify_selected_folder_as_ham(void);
void callback_mail_within_main_selected(void);
void callback_quick_filter_changed(void);

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
void callback_import_mbox(int *in_folder_ptr);
void callback_import_dbx(int *in_folder_ptr);
void callback_export(void);

void callback_write_mail_to(struct addressbook_entry_new *address);
int callback_write_mail_to_str(char *str, char *subject);

void callback_imap_submit_folders(struct folder *f, struct list *list);
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
int callback_remote_filter_mail(struct mail_info *mail);

struct mail_info *callback_new_mail_to_folder(char *filename, struct folder *folder);
struct mail_info *callback_new_mail_to_folder_by_file(char *filename);
void callback_new_mail_arrived_filename(char *filename, int spam);
void callback_new_imap_mail_arrived(char *filename, char *user, char *server, char *path);
void callback_new_imap_mails_arrived(int num_filenames, char **filenames, char *user, char *server, char *path);
void callback_new_imap_uids(unsigned int uid_validity, unsigned int uid_next, char *user, char *server, char *path);
void callback_new_mail_written(struct mail_info *mail);
void callback_delete_mail_by_uid(char *user, char *server, char *path, unsigned int uid);
void callback_number_of_mails_downloaded(int num);
void callback_mail_has_been_sent(char *filename);
void callback_mail_has_not_been_sent(char *filename);
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

int simplemail_init(void);
void simplemail_deinit(void);

/* the main entry point */
int simplemail_main();


#endif

