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
** $Id$
*/

#ifndef TEXTINTERPRETER_H
#define TEXTINTERPRETER_H

enum line_part_type
{
	LP_TYPE_TEXT,
	LP_TYPE_NEWLINE
};

struct line_part /* a part of a line */
{
	enum line_part_type type;
};

struct line_part_text
{
	enum line_part_type type;
	char *text;
	int text_len;
};

struct interpreted_contents
{
	struct line_part **parts_array;
	int parts_allocated;
	int num_parts;
};

struct interpreted_contents *interpret_contents_create_plain(char *buf, int len);
void interpret_contents_free(struct interpreted_contents *cont);

#endif
