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
** statuswnd.h
*/

#ifndef SM__STATUSWND_H
#define SM__STATUSWND_H

int statuswnd_open(int active);
void statuswnd_close(void);
void statuswnd_set_title(char *title);
void statuswnd_init_gauge(int maximal);
void statuswnd_set_gauge(int value);
void statuswnd_set_gauge_text(char *text);
void statuswnd_set_status(char *text);
void statuswnd_set_head(char *text);

/*
int dl_window_open(int active);
void dl_window_close(void);
void dl_set_status(char *);
void dl_connect_to_server(char *server);
void dl_init_gauge_mail(int);
void dl_set_gauge_mail(int);
void dl_init_mail_size_sum(int sum);
void dl_set_mail_size_sum(int sum);
void dl_init_gauge_byte(int);
void dl_set_gauge_byte(int);
int dl_checkabort(void);
void dl_insert_mail(int mno, int mflags, int msize);
void dl_insert_mail_info(int mno, char *from, char *subject, char *date);
int dl_get_mail_flags(int mno);
void dl_clear(void);
int dl_wait(void);
void dl_freeze_list(void);
void dl_thaw_list(void);
int dl_more_statistics(void);
*/

#endif