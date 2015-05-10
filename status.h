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

#ifndef SM__STATUS_H
#define SM__STATUS_H

void status_init(int type);
int status_open(void);
int status_open_notactivated(void);
void status_close(void);
void status_set_title(char *title);
void status_set_title_utf8(char *title);
void status_set_line(char *str);
void status_set_status(const char *str);
void status_set_head(char *head);
void status_set_connect_to_server(char *server);
void status_init_gauge_as_bytes(int maximal);
void status_set_gauge(int value);
void status_init_mail(int maximal);
void status_set_mail(int current, int current_size);
void status_mail_list_insert(int mno, int mflags, int msize);
void status_mail_list_set_flags(int mno, int mflags);
void status_mail_list_set_info(int mno, char *from, char *subject, char *date);
int status_mail_list_get_flags(int mno);
void status_mail_list_clear(void);
void status_mail_list_freeze(void);
void status_mail_list_thaw(void);
int status_wait(void);
int status_more_statistics(void);
int status_check_abort(void);
int status_skipped(void);

#endif
