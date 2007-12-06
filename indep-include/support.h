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
** support.h
*/

#ifndef SM__SUPPORT_H
#define SM__SUPPORT_H

#include "codesets.h"

struct tm;

int sm_makedir(char *path);
unsigned int sm_get_seconds(int day, int month, int year);
int sm_get_gmt_offset(void);
unsigned int sm_get_current_seconds(void);
unsigned int sm_get_current_micros(void);
void sm_convert_seconds(unsigned int seconds, struct tm *tm);
int sm_add_part(char *drawer, const char *filename, int buf_size);
char *sm_file_part(char *filename);
char *sm_path_part(char *filename);
char *sm_request_file(char *title, char *path, int save, char *extension);
int sm_request(char *title, char *text, char *gadgets, ...);
char *sm_request_string(char *title, char *text, char *contents, int secret);
int sm_request_login(char *text, char *login, char *password, int len);
char *sm_request_pgp_id(char *text);
struct folder *sm_request_folder(char *text, struct folder *exculde);
void sm_show_ascii_file(char *folder, char *filename);
void sm_play_sound(char *filename);

char *sm_getenv(char *name);
void sm_setenv(char *name, char *value);
void sm_unsetenv(char *name);
int sm_system(char *command, char *output);
int sm_file_is_in_drawer(char *filename, char *path);

char *sm_get_date_long_str(unsigned int seconds);
char *sm_get_date_long_str_utf8(unsigned int seconds);
char *sm_get_date_str(unsigned int seconds);
char *sm_get_time_str(unsigned int seconds);

char *sm_parse_pattern(utf8 *utf8_str, int flags);
int sm_match_pattern(char *pat, utf8 *utf8_str, int flags);
#define SM_PATTERN_NOCASE (1L << 0) /* not case sensitive */
#define SM_PATTERN_SUBSTR (1L << 1) /* make it a substring search */
#define SM_PATTERN_ASCII7 (1L << 2) /* match string is ascii7, automatic detection */
#define SM_PATTERN_NOPATT (1L << 3) /* no patternmatching, just string compare */

int sm_snprintf(char *buf, int n, const char *fmt, ...);

void sm_put_on_serial_line(char *txt);

void tell_str(const char *str);
void tell_from_subtask(const char *str);

int pkcs7_decode(char *buf, int len, char **dest_ptr, int *len_ptr);

#endif



