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
 * @file sysprint.h
 */

#ifndef SM__SYSPRINT_H
#define SM__SYSPRINT_H

typedef struct PrintHandle PrintHandle; /* Opaque */

/**
 * Open a printer handle.
 *
 * @return the printer handle.
 */
PrintHandle *sysprint_prepare(void);

/**
 * Cleanup everything related to the given print handle.
 *
 * @param ph handle to cleanup
 */
void sysprint_cleanup(PrintHandle *ph);

/**
 * Print the given text via the handle.
 *
 * @param ph the handle that is used to print
 * @param txt the text that is printed
 * @param len the length of the text to be printed
 * @return 0 on failure, 1 on success.
 */
int sysprint_print(PrintHandle *ph, char *txt, unsigned long len);

#endif
