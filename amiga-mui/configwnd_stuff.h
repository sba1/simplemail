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
 * @file configwnd_stuff.h
 */

/**
 * Convert addresses from a texteditor object to an array.
 * This is principle the same like in addressbookwnd.c but uses
 * parse_mailbox.
 *
 * @param editor the text editor that contains the addresses
 * @param page the page that should be activated on an error on the config list.
 * @param error_ptr where to store the error on parse
 * @param config_wnd the configuration window
 * @param config_list the list of all pages.
 * @return the array of addresses that shall be freed with array_free()
 */
char **array_of_addresses_from_texteditor(Object *editor, int page, int *error_ptr, Object *config_wnd, Object *config_list);

IMPORT struct MUI_CustomClass *CL_Sizes;
#define SizesObject (Object*)MyNewObject(CL_Sizes->mcc_Class, NULL

/**
 * Create the sizes custom class.
 *
 * @return 0 on failure, 1 on success
 */
int create_sizes_class(void);

/**
 * Delete the size custom class.
 */
void delete_sizes_class(void);

/**
 * Given a value as in the size custom class, return the proper size.
 *
 * @param val the value as in the size custom class
 *
 * @return the proper size
 */
int value2size(int val);

/**
 * Given a poper size, return the value as in the site custom class.
 *
 * @param val the value to transform
 *
 * @return the transformed value.
 */
int size2value(int val);
