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
void statuswnd_set_title_utf8(char *title);
void statuswnd_init_gauge(int maximal);
void statuswnd_set_gauge(int value);
void statuswnd_set_gauge_text(char *text);
void statuswnd_set_status(char *text);
void statuswnd_set_head(char *text);
void statuswnd_mail_list_clear(void);
void statuswnd_mail_list_freeze(void);
void statuswnd_mail_list_thaw(void);
void statuswnd_mail_list_insert(int mno, int mflags, int msize);
void statuswnd_mail_list_set_flags(int mno, int mflags);
void statuswnd_mail_list_set_info(int mno, char *from, char *subject, char *date);
int statuswnd_mail_list_get_flags(int mno);
int statuswnd_wait(void);
int statuswnd_more_statistics(void);
int statuswnd_is_opened(void);

#endif
