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
 * @file http.h
 */

#ifndef SM__HTTP_H
#define SM__HTTP_H

/**
 * Stores the photo of the given email into the given path.
 *
 * @param path the path where the photo shall be stored
 * @param email the email address for which the photo shall be downloaded.
 * @return 1 on success, 0 otherwise.
 *
 * @note this is obsolete
 */
int http_download_photo(char *path, char *email);

/**
 * Download the contents of the given URI into the given buffer.
 *
 * @param uri the URI that points to the resource to be downloaded.
 * @param buf pointer to variable where the pointer of the buffer is stored
 *  that contains the contents of the resource.
 * @param buf_len pointer to a variable to which the length of the data that has
 *  been downloaded will be stored.
 * @return 1 on success, 0 otherwise.
 */
int http_download(char *uri, void **buf, int *buf_len);

#endif

