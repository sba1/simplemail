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
 * @file text2html.c
 *
 * @todo rewrite this
 */

#include "text2html.h"

#include <stdlib.h>
#include <string.h>

#include "codesets.h"
#include "configuration.h"
#include "debug.h"
#include "parse.h"
#include "support_indep.h"

#include "support.h"

#define SIZE_URI 256

static const char legalchars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz@_?+-,.~/%&=:*#";

struct smily
{
	const char *ascii;
	const char *gfx;
};

const static struct smily smily[] =
{
	{":)","smily_smile"},
	{":-)","smily_smile"},
	{"(-:","smily_smile"},
	{":|","smily_neutral"},
	{":-|","smily_neutral"},
	{":(","smily_sad"},
	{":-(","smily_sad"},
	{")-:","smily_sad"},
	{";-)","smily_winky"},
	{";)","smily_winky"},
	{":-/","smily_undecided"},
	{":/","smily_undecided"},
	{":-D","smily_laugh"},
	{":D","smily_laugh"},
	{":-P","smily_nyah"},
	{":P","smily_nyah"},
	{":-O","smily_uhoh"},
	{":-I","smily_neutral"},
	{"8-)","smily_cool"},
	{"%-)","smily_weird"},
	{":*)","smily_crazy"},
	{":'-(","smily_cry"},
	{":*","smily_kiss"},
	{":-e","smily_angry"}
};

static int write_unicode(utf8 *src, string *str)
{
	int len;
	char buf[16];

	len = strlen(src);

	while (len > 0)
	{
		unsigned char c;

		c = *src;

		if (c > 127)
		{
			unsigned int iso = 0;
			int advance = utf8tochar(src, &iso, user.config.default_codeset);
			if (iso == 0) iso = '_';

			sm_snprintf(buf,sizeof(buf),"&#%d;",iso);
			string_append(str,buf);

			len -= advance;
			src += advance;
		} else
		{
			string_append_part(str,(char*)&c,1);
			src++;
			len--;
		}
	}
	return 1;
}

static int write_uri(unsigned char **buffer_ptr, int *buffer_len_ptr, string *str)
{
	char uri[SIZE_URI];
	unsigned char *buffer = *buffer_ptr;
	int buffer_len = *buffer_len_ptr;
	int i;

	for (i = 0; buffer_len && *buffer && (strchr(legalchars, *buffer) || *buffer > 127) && i < SIZE_URI-1; i++)
	{
		uri[i] = *buffer++;
		buffer_len--;
	}

	/* The is probably the end of a sentence */
	if (i > 0 && strchr(".?!", uri[i-1])) --i;
	uri[i] = 0;

	if (i)
	{
		string_append(str,"<A HREF=\"");
		string_append(str,uri);
		string_append(str,"\"");
		if (!user.config.read_link_underlined) string_append(str," STYLE=\"TEXT-DECORATION: none\"");
		string_append(str,">");
		write_unicode(uri,str);
		string_append(str,"</A>");
	}

	*buffer_ptr = buffer;
	*buffer_len_ptr = buffer_len;

	return 1;
}

/*****************************************************************************/

char *text2html(unsigned char *buffer, int buffer_len, int flags, char *fonttag)
{
	unsigned char *saved_buffer = buffer;
	string str;

	if (string_initialize(&str,1024))
	{
		char buf[512];
		int last_color = 0; /* the color of the current line */
		int eval_color = 2; /* recheck the color */
		int initial_color = 1;
		int level = 0;
		int line = 0; /* the type of the line */

		if (flags & TEXT2HTML_BODY_TAG)
		{
			sm_snprintf(buf,sizeof(buf),"<BODY BGCOLOR=\"#%06x\" TEXT=\"#%06x\" LINK=\"#%06x\">",user.config.read_background,user.config.read_text,user.config.read_link);
			string_append(&str,buf);
		}
		string_append(&str,fonttag); /* accepts NULL pointer */

		/* check for >0, otherwise we'll be in an endless loop if buffer_len becomes <0 */
		while (buffer_len > 0)
		{
			if (eval_color)
			{
				int new_level = 0;
				int buffer2_len = buffer_len;
				unsigned char *buffer2 = buffer;
				int new_color = 0;

				/* Determine the citation level. Afterwards, buffer2 will point to the end of the citation symbols. */
				while (buffer2_len)
				{
					unsigned char c = *buffer2;
					if (c == '>')
					{
						new_level++;
						if (new_color == 1) new_color = 2;
						else new_color = 1;
					} else
					{
						if (c != ' ') break;
						if (c == ' ' && !new_level) break;
					}
					buffer2_len--;
					buffer2++;
				}

				if (user.config.read_graphical_quote_bar)
				{
					/* When graphical quote bar is enabled we skip all quotation symbols */
					buffer = buffer2;
					buffer_len = buffer2_len;

					if (level != new_level)
					{
						const char *begin_quote_string = "<TABLE BGCOLOR=\"#%06x\" WIDTH=\"100%%\" STYLE=\"border-left: 3px solid #%06x; border-right: 3px solid #%06x;\"><TD><FONT COLOR=\"#%06x\">";
						const char *end_quote_string = "</FONT></TD></TABLE>";

						/* If new level is larger */
						for (;level < new_level; level++)
						{
							unsigned int color = level%2?user.config.read_quoted:user.config.read_old_quoted;
							sm_snprintf(buf,sizeof(buf),begin_quote_string,user.config.read_quoted_background,color,color,color);
							string_append(&str,buf);
						}

						/* If new level is lower */
						for (;level > new_level; level--)
							string_append(&str,end_quote_string);
					}
				} else
				{
					if (last_color != new_color)
					{
						const char *begin_quote_string = "<FONT COLOR=\"#%x\">";
						const char *end_quote_string = "</FONT>";

						if (!initial_color) string_append(&str,end_quote_string);
						if (new_color == 1)
						{
							sm_snprintf(buf,sizeof(buf),begin_quote_string,user.config.read_quoted);
							string_append(&str,buf);
						}
						else if (new_color == 2)
						{
							sm_snprintf(buf,sizeof(buf),begin_quote_string,user.config.read_old_quoted);
							string_append(&str,buf);
						}
						last_color = new_color;
						if (new_color) initial_color = 0;
						else initial_color = 1;
					}
				}
				eval_color = 0;
			}

			if (!mystrnicmp("http:",(char*)buffer,5)) write_uri(&buffer, &buffer_len, &str);
			else if (!mystrnicmp("mailto:",(char*)buffer,7)) write_uri(&buffer, &buffer_len, &str);
			else if (!mystrnicmp("ftp:",(char*)buffer,4)) write_uri(&buffer, &buffer_len, &str);
			else if (!mystrnicmp("https:",(char*)buffer,6)) write_uri(&buffer, &buffer_len, &str);
			else
			{
				unsigned char c;

				c = *buffer;

				if ((c == '@') && (buffer > saved_buffer))
				{
					/* A @ has been encountered, check if this belongs to an email adresse by traversing back
           * within the string */

					unsigned char *buffer2 = buffer - 1;
					unsigned char *buffer3;
					unsigned char c2;
					int buffer2_len = buffer_len + 1;
					char *address;

					while ((c2 = *buffer2) && buffer2 > saved_buffer)
					{
						static const char noaliaschars[] = {
							" ()<>@,;:\\\"[]\n\r"};

						if (strchr(noaliaschars,c2))
						{
							buffer2++;
							break;
						}
						buffer2_len++;
						buffer2--;
					}

					if ((buffer3 = (unsigned char*)parse_addr_spec((char*)buffer2, &address)))
					{
						int email_len;

						/* crop the string to the beginning of the email address */
						string_crop(&str,0,str.len - (buffer - buffer2));

						buffer_len += buffer - buffer2;
						buffer -= buffer - buffer2;
						email_len = buffer3 - buffer;
						buffer_len -= email_len;

						buffer = buffer3;

						sm_snprintf(buf,sizeof(buf),"<A HREF=\"mailto:%s\"%s>",address, user.config.read_link_underlined?"":" STYLE=\"TEXT-DECORATION: none\"");
						string_append(&str,buf);
						write_unicode(address,&str);
						string_append(&str,"</A>");
						free(address);
						continue;
					}
				}

		  	if (user.config.read_smilies)
		  	{
		  		unsigned int i;
		  		int smily_used = 0;
			  	/* No look into the smily table, this is slow and needs to be improved */
		  		for (i=0;i<sizeof(smily)/sizeof(struct smily);i++)
		  		{
		  			if (!strncmp(smily[i].ascii,(char*)buffer,strlen(smily[i].ascii)))
		  			{
		  				buffer += strlen(smily[i].ascii);
		  				buffer_len -= strlen(smily[i].ascii);
		  				sm_snprintf(buf,sizeof(buf),"<IMG SRC=\"PROGDIR:Images/%s\" VALIGN=\"middle\" ALT=\"%s\">",smily[i].gfx,smily[i].ascii);
		  				string_append(&str,buf);
		  				smily_used = 1;
		  			}
		  		}
		  		if (smily_used) continue;
		  	}

				if (!strncmp("\n<sb>",(char*)buffer,5))
				{
					if (line) string_append(&str,"<BR></TD><TD WIDTH=\"50%\"><HR></TD></TR></TABLE>");
					line = 1;
					buffer += 5;
					buffer_len -= 5;

					string_append(&str,"<TABLE WIDTH=\"100%\" BORDER=\"0\"><TR><TD VALIGN=\"middle\" WIDTH=\"50%\"><HR></TD><TD>");
					continue;
				}

				if (!strncmp("\n<tsb>",(char*)buffer,6))
				{
					if (line) string_append(&str,"<BR></TD><TD WIDTH=\"50%\"><HR></TD></TR></TABLE>");
					line = 2;
					buffer += 6;
					buffer_len -= 6;

					string_append(&str,"<TABLE WIDTH=\"100%\" BORDER=\"0\"><TR><TD VALIGN=\"middle\" WIDTH=\"50%\"><HR></TD><TD>");
					continue;
				}

				if (c < 128)
				{
					buffer++;
					buffer_len--;
					if (c== '<') string_append(&str,"&lt;");
					else if (c== '>') string_append(&str,"&gt;");
					else if (c== '&') string_append(&str,"&amp;");
					else if (c == 10)
					{
						eval_color = 1;
						string_append(&str,"<BR>\n");
						if (line)
						{
							string_append(&str,"</TD><TD WIDTH=\"50%\"><HR></TD></TR></TABLE>");
							line = 0;
						}
					} else
					{
						if (c == 32) {
							if (*buffer == 32 || (flags & TEXT2HTML_NOWRAP)) string_append(&str,"&nbsp;");
							else string_append(&str," ");
						} else {
						  if (c)
						  {
						  	string_append_part(&str,(char*)&c,1);
						  }
						}
					}
				} else
				{
					unsigned int unicode;
					int len = 0;
					/* check if it really could be a utf8 char */
					if (utf8islegal((utf8*)buffer, (utf8*)(buffer+buffer_len)))
					{
						len = utf8tochar((utf8*)buffer, &unicode, user.config.default_codeset);
					}
					if ((len == 0) || (len > buffer_len))
					{
						/* something wrong with that utf8 sequence */
						unicode = '?';
						len = 1;
					}
					buffer_len -= len;
					buffer += len;

					if (unicode == 0) unicode = '_';
					sm_snprintf(buf,sizeof(buf),"&#%d;",unicode);
					string_append(&str,buf);
				}
			}
		}

		if (fonttag) string_append(&str,"</FONT>");

		if (flags & TEXT2HTML_ENDBODY_TAG) string_append(&str,"</BODY>");

		SM_DEBUGF(20,("%s\n",str.str));

		return str.str;
	}
	return NULL;
}


