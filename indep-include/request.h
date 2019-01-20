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
#ifndef SM__REQUEST_H
#define SM__REQUEST_H

/**
 * Requests from the user a file and returns its full path.
 *
 * @param title
 * @param path
 * @param save
 * @param extension
 * @return
 */
char *sm_request_file(const char *title, const char *path, int save, const char *extension);

/**
 * Opens a requester. Returns 0 if the rightmost gadgets is pressed
 * otherwise the position of the gadget from left to right
 *
 * @param title
 * @param text
 * @param gadgets
 * @return
 */
int sm_request(const char *title, const char *text, const char *gadgets, ...);

/**
 * Opens a requester to enter a string.
 *
 * @param title
 * @param text
 * @param contents
 * @param secret
 * @return NULL on error, otherwise the string as entered by the user.
 */
char *sm_request_string(const char *title, const char *text, const char *contents, int secret);

/**
 * Opens a requester to enter a user id and a passwort. Returns 1 on
 * success, else 0. The strings are filled in a previously alloced
 * login and password buffer but not more than len bytes
 *
 * @param text
 * @param login
 * @param password
 * @param len
 * @return
 */
int sm_request_login(const char *text, char *login, char *password, int len);

/**
 * Returns a malloc()ed string of an pgp id as selected bythe user.
 *
 * @param text
 * @return the id or NULL for an error (or in case cancelation).
 */
char *sm_request_pgp_id(const char *text);

/**
 * Opens a requester for the user to select a folder.
 *
 * @param text
 * @param exculde
 * @return the folder or NULL for an error (or in case of cancelation)
 */
struct folder *sm_request_folder(const char *text, struct folder *exculde);

#endif
