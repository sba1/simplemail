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
** text2html.c
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "text2html.h"

char *text2html(char *buffer, int buffer_len, int flags)
{
	char *html_buf;
	FILE *fh;

	if ((fh = tmpfile()))
	{
		int len;

		if (flags & TEXT2HTML_BODY_TAG) fputs("<BODY>",fh);
		if (flags & TEXT2HTML_FIXED_FONT) fputs("<TT>",fh);

		while (buffer_len)
		{
			unsigned char c = *buffer++;
			buffer_len--;
			if (c== '<') fputs("&lt;",fh);
			else if (c== '>') fputs("&gt;",fh);
			else if (c == 10) fputs("<BR>",fh);
			else
			{
				if (c == 32) {
					if (*buffer == 32 || flags & TEXT2HTML_NOWRAP) fputs("&nbsp;",fh);
					else fputc(32,fh);
				} else {
				  if (c) fputc(c,fh);
				}
			}
		}

		if (flags & TEXT2HTML_FIXED_FONT) fputs("</TT>",fh);
		if (flags & TEXT2HTML_ENDBODY_TAG) fputs("</BODY>",fh);
		if ((len = ftell(fh)))
		{
			fseek(fh,0,SEEK_SET);
			if ((html_buf = (char*)malloc(len+1)))
			{
				fread(html_buf,1,len,fh);
				html_buf[len]=0;
			}
		}
		fclose(fh);
	}
	return html_buf;
}
