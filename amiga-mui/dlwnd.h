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
** dlwnd.h
*/

#ifndef SM__DLWND_H
#define SM__DLWND_H

int dl_window_open(void);
void dl_window_close(void);
void dl_set_title(char *);
void dl_set_status(char *);
void dl_init_gauge_mail(int);
void dl_set_gauge_mail(int);
void dl_init_gauge_byte(int);
void dl_set_gauge_byte(int);
int dl_checkabort(void);
void dl_insert_mail(int mno, int msize);

#endif