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
** $Id$
*/

#ifndef SM__SUPPORT_H
#define SM__SUPPORT_H

int sm_makedir(char *path);
unsigned int sm_get_seconds(int day, int month, int year);
int sm_get_gmt_offset(void);
int sm_add_part(char *drawer, const char *filename, int buf_size);
char *sm_file_part(char *filename);
char *sm_path_part(char *filename);
int sm_request(char *title, char *text, char *gadgets, ...);

void tell(char *str);
void tell_from_subtask(char *str);

int mystricmp(const char *str1, const char *str2);
int mystrnicmp(const char *str1, const char *str2, int n);
char *mystristr(const char *str1, const char *str2);
unsigned int mystrlen(const char *str);
char *mystrdup(const char *str);
char *mystrndup(const char *str, int len);

#endif
