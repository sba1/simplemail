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
 * @file
 */
#ifndef SM__SUPPORT_H
#define SM__SUPPORT_H

#include "codesets.h"

/**
 * Creates a directory including all necessaries parent directories.
 * The call will be successful even if the directory exists.
 *
 * @param path defines the path of the directory to be created
 * @return 1 if the directory exists or have been created, otherwise 0.
 */
int sm_makedir(const char *path);

/**
 * Add a filename component to the given drawer string in place.
 *
 * @param drawer the drawer to which the filename is appended
 * @param filename the filename that is added to the drawer string
 * @param buf_size the size of the string
 * @return 1 on success, 0 on failure.
 */
int sm_add_part(char *drawer, const char *filename, int buf_size);

/**
 * Return the file component of a given path.
 *
 * @param filename from which to determine the file component.
 * @return the pointer to the file
 *
 * @note you should not use this function but instead call sm_file_part().
 */
char *sm_file_part_nonconst(char *filename);

#if __STDC_VERSION__ >= 201112L
static inline const char *sm_file_part_const(const char *filename)
{
	return (const char*)sm_file_part_nonconst((char*)filename);
}

#define sm_file_part(filename) _Generic((filename),\
		char *: sm_file_part_nonconst,\
		default:sm_file_part_const\
	)(filename)
#else
#define sm_file_part(filename) sm_file_part_nonconst(filename)
#endif

/**
 * Return the pointer to the character after the last path component.
 *
 * @param filename
 * @return the pointer to the last character after the last path component.
 * @note this will remove any const qualifier
 */
char *sm_path_part(char *filename);

/**
 * Shows an ASCII file on the screen.
 *
 * @param folder defines the folder where the document is located.
 * @param filename defines the filename of the document to be displayed,
 *  relative to the given folder to be displayed.
 */
void sm_show_ascii_file(char *folder, char *filename);
void sm_play_sound(char *filename);

char *sm_getenv(char *name);
void sm_setenv(char *name, char *value);
void sm_unsetenv(char *name);
int sm_system(char *command, char *output);

int sm_file_is_in_drawer(char *filename, char *path);
int sm_is_same_path(char *path1, char *path2);

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



