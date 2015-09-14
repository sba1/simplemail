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
 * @file dbx.h
 */

#ifndef SM__DBX_H
#define SM__DBX_H

struct folder;

/**
 * Import a given file which must be a dbx file to a given folder.
 *
 * @param folder the folder in which the mails should be imported. May be NULL
 *  in which case the process is handled like fetching mails.
 * @param filename defines the dbx file.
 * @return 1 if the process has started. This is an aynchrounous call.
 */
int dbx_import_to_folder(struct folder *folder, char *filename);

#endif
