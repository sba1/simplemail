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

#ifndef CODECS_H
#define CODECS_H

char *decode_base64(unsigned char *buf, unsigned int len, unsigned int *ret_len);
char *decode_quoted_printable(unsigned char *buf, unsigned int len, unsigned int *ret_len, int header);
char *encode_header_field(char *field_name, char *field_contents);
char *encode_address_field(char *field_name, struct list *address_list);
char *encode_body(unsigned char *buf, unsigned int len, char *content_type, unsigned int *ret_len, char **encoding);
char *identify_file(char *fname);

#endif
