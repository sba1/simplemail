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

#include "parse.h"
#include "support_indep.h"
#include "text2html.h"

#define SIZE_URI 256

static const char legalchars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz@_?+-,.~/%&=:*#";

static int write_uri(char **buffer_ptr, int *buffer_len_ptr, FILE *fh)
{
	char uri[SIZE_URI];
	char *buffer = *buffer_ptr;
	int buffer_len = *buffer_len_ptr;
	int i;
	
	for (i = 0; buffer_len && *buffer && strchr(legalchars, *buffer) && i < SIZE_URI-1; i++)
	{
		uri[i] = *buffer++;
		buffer_len--;
	}

	/* The is probably the end of a sentence */
	if (strchr(".?!", uri[i-1])) --i;
	uri[i] = 0;

	if (i)
	{
		fprintf(fh,"<A HREF=\"%s\" STYLE=\"TEXT-DECORATION: none\">%s</A>",uri,uri);
	}

	*buffer_ptr = buffer;
	*buffer_len_ptr = buffer_len;

	return 1;
}

char *text2html(unsigned char *buffer, int buffer_len, int flags)
{
	char *html_buf;
	FILE *fh;

	if ((fh = tmpfile()))
	{
		int len;
		int last_color = 0; /* the color of the current line */
		int eval_color = 2; /* recheck the color */
		int initial_color = 1;

		if (flags & TEXT2HTML_BODY_TAG) fputs("<BODY>",fh);
		if (flags & TEXT2HTML_FIXED_FONT) fputs("<FONT SIZE=-1><TT>",fh);

		while (buffer_len)
		{
			if (eval_color)
			{
				int buffer2_len = buffer_len;
				char *buffer2 = buffer;
				int new_color = 0;

				while (buffer2_len)
				{
					unsigned char c = *buffer2++;
					buffer2_len--;
					if (c == '>')
					{
						if (new_color == 1) new_color = 2;
						else new_color = 1;
					} else
					{
						if (c==10) break;
						if ((new_color == 1 || new_color == 2) && c != ' ') break;
						if (c==' ' && new_color == 0) break;
					}
				}

				if (last_color != new_color)
				{
					if (!initial_color) fputs("</FONT>",fh);
					if (new_color == 1) fputs("<FONT COLOR=\"white\">",fh);
					else if (new_color == 2) fputs("<FONT COLOR=\"yellow\">",fh);
					last_color = new_color;
					if (new_color) initial_color = 0;
					else initial_color = 1;
				}

				eval_color = 0;
			}


			if (!mystrnicmp("http:",buffer,5)) write_uri(&buffer, &buffer_len, fh);
			else if (!mystrnicmp("mailto:",buffer,7)) write_uri(&buffer, &buffer_len, fh);
			else if (!mystrnicmp("ftp:",buffer,4)) write_uri(&buffer, &buffer_len, fh);
			else if (!mystrnicmp("https:",buffer,6)) write_uri(&buffer, &buffer_len, fh);
			else
			{
				unsigned char c;
				c = *buffer;

				if (c == '@')
				{
					/* For email addresses without mailto: */
					unsigned char *buffer2 = buffer - 1;
					unsigned char *buffer3;
					unsigned char c2;
					int buffer2_len = buffer_len + 1;
					char *address;

					while ((c2 = *buffer2))
					{
						static const char noaliaschars[] = {
							" ()<>@,;:\\\"[]"};

						if (strchr(noaliaschars,c2)) break;
						buffer2_len++;
						buffer2--;
					}

					if ((buffer3 = parse_addr_spec(buffer2, &address)))
					{
						int email_len;
						fseek(fh,buffer2 - buffer + 1,SEEK_CUR); /* @ sign has not been written yet */
						buffer_len += buffer - buffer2;
						buffer -= buffer - buffer2;
						email_len = buffer3 - buffer;
						buffer_len -= email_len;
						buffer = buffer3;
						fprintf(fh,"<A HREF=\"mailto:%s\">",address);
						fputs(address,fh);
						fputs("</A>",fh);
						free(address);
						continue;
					}
				}

				buffer++;
				buffer_len--;
				if (c== '<') fputs("&lt;",fh);
				else if (c== '>') fputs("&gt;",fh);
				else if (c == 10)
				{
					eval_color = 1;
					fputs("<BR>\n",fh);
				} else
				{
					if (c == 32) {
						if (*buffer == 32 || flags & TEXT2HTML_NOWRAP) fputs("&nbsp;",fh);
						else fputc(32,fh);
					} else {
					  if (c) fputc(c,fh);
					}
				}
			}
		}

		if (flags & TEXT2HTML_FIXED_FONT) fputs("</TT></FONT>",fh);
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
/*
		fh = fopen("ram:test.html","wb");
		fputs(html_buf,fh);
		fclose(fh);
*/
	}
	return html_buf;
}
