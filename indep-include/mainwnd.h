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
** mainwnd.h
*/

#ifndef SM__MAINWND_H
#define SM__MAINWND_H

struct folder;
struct mail;

int main_window_init(void);
int main_window_open(void);
void main_refresh_folders(void);
void main_refresh_folder(struct folder *folder);
void main_insert_mail(struct mail *mail);
void main_remove_mail(struct mail *mail);
void main_replace_mail(struct mail *oldmail, struct mail *newmail);
void main_refresh_mail(struct mail *m);
void main_clear_folder_mails(void);
void main_set_folder_mails(struct folder *folder);
struct folder *main_get_folder(void);
char *main_get_folder_drawer(void);
struct mail *main_get_active_mail(void);
char *main_get_mail_filename(void);

struct mail *main_get_mail_first_selected(void *handle);
struct mail *main_get_mail_next_selected(void *handle);
void main_remove_mails_selected(void);

void main_build_accounts(void);

#endif
