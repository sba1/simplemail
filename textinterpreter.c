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

#include <string.h>
#include <stdlib.h>

#include "textinterpreter.h"

static int strcrlen(char *src)
{
	char c;
	int len = 0;
	while((c = *src))
	{
		if (c == 10 || c ==13) break;
		len++;
		src++;
	}

	return 0;
}

struct interpreted_contents *interpret_contents_create_plain(char *buf, int len)
{
/*
	char *buf_end = buf + len;

	memset(cont,0,sizeof(struct interpreted_contents));
	
	while (buf < buf_end)
	{
		int len;

		len = strcrlen(buf);
		buf += len;

		if (line_end)
		{
			
		}
	}*/
}

void interpret_contents_free(struct interpreted_contents *cont)
{
/*
	int i;
	for (i=0;i<cont->num_parts;i++)
		if (cont->parts_array[i]) free(cont->parts_array[i]);

	free(cont->parts_array);
	free(cont);*/
}
