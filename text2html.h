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
 * @file text2html.h
 */

#ifndef SM__TEXT2HTML_H
#define SM__TEXT2HTML_H

/**
 * Copy the given email text to a suitable html representation.
 *
 * @param buffer the buffer that contains the text to be converted.
 * @param buffer_len the number of bytes that needs to be converted
 * @param flags some flags
 * @param fonttag a string that is used for the font tag
 * @return the html representation.
 */
char *text2html(unsigned char *buffer, int buffer_len, int flags, char *fonttag);

#define TEXT2HTML_BODY_TAG				(1 << 0)
#define TEXT2HTML_ENDBODY_TAG		(1 << 1)
#define TEXT2HTML_FIXED_FONT      (1 << 2)
#define TEXT2HTML_NOWRAP					(1 << 3)
#define TEXT2HTML_BOLDSTYLE       (1 << 4)
#define TEXT2HTML_ITALICSTYLE     (1 << 5)

#endif
