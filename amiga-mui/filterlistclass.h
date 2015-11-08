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
 * @file filterlistclass.h
 */

#ifndef SM__FILTERLISTCLASS_H
#define SM__FILTERLISTCLASS_H

IMPORT struct MUI_CustomClass *CL_FilterList;
#define FilterListObject (Object*)MyNewObject(CL_FilterList->mcc_Class, NULL

/**
 * Create the filter list custom class.
 *
 * @return 0 on failure, 1 on success
 */
nt create_filterlist_class(void);

/**
 * Delete the filter list custom class.
 */
void delete_filterlist_class(void);

#endif
