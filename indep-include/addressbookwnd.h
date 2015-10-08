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
 * @file addressbookwnd.h
 */

#ifndef SM__ADDRESSBOOKWND_H
#define SM__ADDRESSBOOKWND_H

struct mail;
struct addressbook_entry_new;

/**
 * Opens the address book
 */
void addressbookwnd_open(void);

/**
 * Opens an address book and let the given entry be edited
 *
 * @param entry
 */
void addressbookwnd_create_entry(struct addressbook_entry_new *entry);

/**
 * Selects an address entry to the currently selected one.
 *
 * @param alias the alias of the entry to be selected
 * @return the number of the entry that has been selected
 *
 * @note if the alias couldn't not been found, this will return 0
 *  as well
 */
int addressbookwnd_set_active_alias(char *alias);

/**
 * Refreshes the address book
 */
void addressbookwnd_refresh(void);

#endif

