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

struct folder;
struct mail;
struct addressbook_entry;

void callback_read_mail(void);
void callback_delete_mails(void);
int callback_delete_mail(struct mail *mail);
void callback_get_address(void);
void callback_new_mail(void);
void callback_reply_this_mail(char *folder_path, struct mail *to_reply);
void callback_reply_mail(void);
void callback_forward_this_mail(char *folder_path, struct mail *to_forward);
void callback_forward_mail(void);
void callback_change_mail(void);
void callback_fetch_mails(void);
void callback_check_single_account(int account_num);
void callback_send_mails(void);
void callback_filter(void);
void callback_new_folder(void);
void callback_new_folder_path(char *path, char *name);
void callback_new_group(void);
void callback_remove_folder(void);
void callback_edit_filter(void);
void callback_addressbook(void);
void callback_config(void);
void callback_folder_active(void);

void callback_write_mail_to(struct addressbook_entry *address);
void callback_write_mail_to_str(char *str, char *subject);

void callback_edit_folder(void);
void callback_change_folder_attrs(void);
void callback_reload_folder_order(void);

void callback_move_mail(struct mail *mail, struct folder *from_folder, struct folder *dest_folder);
void callback_maildrop(struct folder *dest_folder);
void callback_mails_mark(int mark);
void callback_mails_set_status(int status);

void callback_apply_folder(struct filter *filter);

void callback_new_mail_arrived_filename(char *filename);
void callback_new_mail_written(struct mail *mail);
void callback_mail_has_been_sent(char *filename);

void callback_mail_changed(struct folder *folder, struct mail *oldmail, struct mail *newmail);

void callback_config_changed(void);

void callback_autocheck_refresh(void);
void callback_timer(void);

#endif

