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
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "addressbook.h"
#include "codecs.h"
#include "configuration.h"
#include "folder.h" /* for mail_compose_new() */
#include "mail.h"
#include "parse.h"
#include "simplemail.h" /* for the callbacks() */
#include "support.h"
#include "textinterpreter.h"

/* the mime preample used in mime multipart messages */
const static char mime_preample[] = 
{
	"Warning: This is a message in MIME format. Your mail reader does not\n"
	"support MIME. Some parts of this message will be readable as plain text.\n"
	"To see the rest, you will need to upgrade your mail reader. Following are\n"
	"some URLs where you can find MIME-capable mail programs for common\n"
	"platforms:\n"
	"\n"
	"  Amiga............: SimpleMail   http://www.simplemail.sourceforge.net/\n"
	"  Unix.............: Metamail     ftp://ftp.bellcore.com/nsb/\n"
	"  Windows/Macintosh: Eudora       http://www.qualcomm.com/\n"
	"\n"
	"General info about MIME can be found at:\n"
	"\n"
	"http://www.cis.ohio-state.edu/hypertext/faq/usenet/mail/mime-faq/top.html\n"
};


/**************************************************************************
 like strncpy() but for mail headers, returns the length of the string
**************************************************************************/
static int mailncpy(char *dest, const char *src, int n)
{
  int i;
  int len = 0;
  char c;
  char *dest_ptr = dest;

	/* skip spaces */
	while(n && isspace(*src))
	{
		src++;
		n--;
	}

  for (i=0;i<n;i++)
  {
    c = *src++;
    if (c==10 || c == 13) continue;
    if (c=='\t') c=' ';
    len++;
    *dest_ptr++ = c;
  }

  return len;
}

/**************************************************************************
 Cites a text
**************************************************************************/
static char *cite_text(char *src, int len)
{
	FILE *fh = tmpfile();
	char *cited_buf = NULL;

	if (fh)
	{
		int cited_len;
		int newline = 1;

		while (len)
		{
			char c = *src;

			if (c==13)
			{
				src++;
				len--;
				continue;
			}

			if (c==10)
			{
				fputc(10,fh);
				newline = 1;
				src++;
				len--;
				continue;
			}

			if (newline)
			{
				if (c=='>') fputc('>',fh);
				else fputs("> ",fh);
				newline = 0;
			}

			fputc(c,fh);

			src++;
			len--;
		}

		cited_len = ftell(fh);
	  fseek(fh,0,SEEK_SET);
		if ((cited_buf = (char*)malloc(cited_len+1)))
		{
			fread(cited_buf,1,cited_len,fh);
			cited_buf[cited_len] = 0;
		}
		fclose(fh);
	}

	return cited_buf;
}

/**************************************************************************
 Allocate an header and insert it into the header list
**************************************************************************/
static int mail_add_header(struct mail *mail, char *name, int name_len, char *contents, int contents_len)
{
	struct header *header;

	if ((header = (struct header*)malloc(sizeof(struct header))))
	{
		char *new_name = (char*)malloc(name_len+1);
		char *new_contents = (char*)malloc(contents_len+1);

		if (new_name && new_contents)
		{
			new_name[mailncpy(new_name,name,name_len)] = 0;
			new_contents[mailncpy(new_contents,contents,contents_len)] = 0;

			header->name = new_name;
			header->contents = new_contents;
			list_insert_tail(&mail->header_list,&header->node);

			return 1;
		}
		if (name) free(name);
		if (contents) free(contents);
		free(header);
	}
}

/**************************************************************************
 Prepares the mail scanning
**************************************************************************/
void mail_scan_buffer_start(struct mail_scan *ms, struct mail *mail)
{
	memset(ms,0,sizeof(struct mail_scan));
	ms->mail = mail;
}

/**************************************************************************
 Finish the mail scanning and free's all memory which has been allocated
**************************************************************************/
void mail_scan_buffer_end(struct mail_scan *ms)
{
	if (ms->line) free(ms->line);
}

/**************************************************************************
 save the current header line in line, sets name_size and contents_size
 return 0 if an error happened
**************************************************************************/
static int mail_scan_buffer_save_line(struct mail_scan *ms, char *name_start, int name_size, char *contents_start, int contents_size)
{
	if (name_size + contents_size) /* else nothing has changed */
	{
		char *line;
		if ((line = (char*)malloc(ms->name_size + ms->contents_size + name_size + contents_size)))
		{
			int pos = 0;
			if (ms->line)
			{
				/* restore the old line */
				pos = ms->name_size + ms->contents_size;
				strncpy(line,ms->line,pos);
				free(ms->line);
			}
			if (name_start)
			{
				strncpy(&line[pos],name_start,name_size);
				pos += name_size;
			}
			if (contents_start)
			{
				strncpy(&line[pos],contents_start,contents_size);
			}
			ms->line = line;
			ms->name_size += name_size;
			ms->contents_size += contents_size;
		} else return 0;
	}
	return 1;
}

/**************************************************************************
 scans a buffer and fill the given mail instance. If more info is needed
 1 is returned, else 0 (error handling not supported yet, but it's safe)
**************************************************************************/
int mail_scan_buffer(struct mail_scan *ms, char *mail_buf, int size)
{
	char c;
	char *name_start = NULL; /* start of the header */
	int name_size = 0; /* size of the header's name (without colon) */
	char *contents_start = NULL; /* start of the headers's contents */
	int contents_size = 0; /* size of the contents */
	int mode = ms->mode; /* 0 search name, 1 read name, 2 search contents, 3 read contents, 4 a LF is expected */
	char *buf = mail_buf;
	char *mail_buf_end = mail_buf + size; /* the end of the buffer */
	struct mail *mail = ms->mail;

	if (mode == 1) name_start = mail_buf;
	else if (mode == 3 || mode == 4) contents_start = mail_buf;

	while (buf < mail_buf_end)
	{
		c = *buf;
		if (mode == 4)
		{
			if (c != 10) return 0; /* the expected LF weren't there, so it's an error */
			else
			{
				mode = 0;
				ms->position++;
				buf++;
				continue;
			}
		}

		if (mode == 0)
		{
			if (c != '\t' && c != ' ')
			{
				if (name_size || ms->name_size)
				{
					if (contents_start && !contents_size) contents_size = buf - contents_start - 1;

					if (ms->line)
					{
						if (!mail_scan_buffer_save_line(ms,name_start,name_size,contents_start,contents_size))
							return 0;

						mail_add_header(mail, ms->line, ms->name_size, ms->line + ms->name_size, ms->contents_size);

						/* a previous call to this function saved a line */
						free(ms->line);
						ms->line = NULL;
						ms->name_size = ms->contents_size = 0;
					} else
					{
						/* no line has saved */
						mail_add_header(mail,name_start,name_size,contents_start,contents_size);
					}

					name_start = contents_start = NULL;
					name_size = contents_size = 0;
				}

				if (c==10 || c==13)
				{
					mail->text_begin = ms->position+1;
					mail->text_len = mail->size - mail->text_begin;
					return 0; /* all headers have been read */
				}

				name_start = buf;
				mode = 1;
			} else
			{
				mode = 3; /* the header continues on the next line */
			}
		}

		if (mode == 1 && c == ':')
		{
			name_size = buf - name_start;
			mode = 2;
		} else
		{
			if (mode == 2)
			{
				if (!isspace(c))
				{
					contents_start = buf;
					mode = 3;
				} else
				{
					if (c==10 || c == 13) /* the headers contents was empty */
					{
						mode = 3;
					}
				}
			}

			if (mode == 3 && (c == 10 || c == 13))
			{
				if (c==13) mode = 4; /* a LF (10) should follow now */
				else mode = 0;
			}
		}

		buf++;
		ms->position++;
	}

	/* if we are here the buffersize was too small */
	{
		if (/*name_start && !name_size &&*/mode == 1) name_size = buf - name_start;
		if (contents_start && !contents_size && (mode == 3 || mode ==4)) contents_size = buf - contents_start;

		if (!mail_scan_buffer_save_line(ms,name_start,name_size,contents_start,contents_size))
			return 0;

		ms->mode = mode;
	}

	return 1;
}

/**************************************************************************
 returns the first mail with the given mime type/subtype
 (recursive). Return NULL if it doesn't exists.
**************************************************************************/
static struct mail *mail_find_type(struct mail *m, char *type, char *subtype)
{
	int i;
	if (!mystricmp(m->content_type, type) && !mystricmp(m->content_subtype,subtype))
		return m;

	for (i=0;i < m->num_multiparts; i++)
	{
		struct mail *rm = mail_find_type(m->multipart_array[i],type,subtype);
		if (rm) return rm;
	}

	return NULL;
}

/**************************************************************************
 creates a mail, initialize it to deault values
**************************************************************************/
struct mail *mail_create(void)
{
	struct mail *m;

	if ((m = (struct mail*)malloc(sizeof(struct mail))))
	{
		memset(m,0,sizeof(struct mail));
		list_init(&m->content_parameter_list);
		list_init(&m->header_list); /* initialze the header_list */
	}
	return m;
}

/**************************************************************************
 scans a mail file and returns a filled (malloced) mail instance, NULL
 if an error happened.
**************************************************************************/
struct mail *mail_create_from_file(char *filename)
{
	struct mail *m;
	FILE *fh;

	if ((m = mail_create()))
	{
		if ((fh = fopen(filename,"rb")))
		{
			unsigned int size;
			char *buf;

			fseek(fh,0,SEEK_END);
			size = ftell(fh); /* get the size of the file */
			fseek(fh,0,SEEK_SET); /* seek to the beginning */
	
			if ((buf = (char*)malloc(2048))) /* a small buffer to test the the new functions */
			{
				if ((m->filename = strdup(filename))) /* Not ANSI C */
				{
					struct mail_scan ms;
					unsigned int bytes_read = 0;

					m->size = size;

					mail_scan_buffer_start(&ms,m);

					while ((bytes_read = fread(buf, 1, 2048/*buf_size*/, fh)))
					{
						if (!mail_scan_buffer(&ms,buf,bytes_read))
							break; /* we have enough */
					}

					mail_scan_buffer_end(&ms);
					mail_process_headers(m);
				}
				free(buf);
			}
	
			fclose(fh);
		} else
		{
			free(m);
			return NULL;
		}
	}
	return m;
}

/**************************************************************************
 Creates a Reply to a given mail. That means change the contents of
 "From:" to "To:", change the subject, cite the first text passage
 and remove the attachments. The mail is proccessed. The given mail should
 be processed to.

 Should use mail_compose_mail() to create the temporary mail
**************************************************************************/
struct mail *mail_create_reply(struct mail *mail)
{
	struct mail *m = mail_create();
	if (m)
	{
		char *from = mail_find_header_contents(mail,"from");
		struct mail *text_mail;

		if (from)
		{
			struct list *alist = create_address_list(from);
			if (alist)
			{
				char *to_header = encode_address_field("To",alist);
				free_address_list(alist);

				if (to_header)
				{
					mail_add_header(m, "To", 2, to_header+4, strlen(to_header)-4);
					free(to_header);
				}
			}
		}

		if (mail->subject)
		{
			char *new_subject = (char*)malloc(strlen(mail->subject)+8);
			if (new_subject)
			{
				char *subject_header;

				char *src = mail->subject;
				char *dest = new_subject;
				char c;
				int brackets = 0;
				int skip_spaces = 0;

				/* Add a Re: before the new subject */
				strcpy(dest,"Re: ");
				dest += 4;

				/* Copy the subject into a new buffer and filter all []'s and Re's */
				while (c = *src)
				{
					if (c == '[')
					{
						brackets++;
						src++;
						continue;
					} else
					{
						if (c == ']')
						{
							brackets--;
							skip_spaces = 1;
							src++;
							continue;
						}
					}

					if (!brackets)
					{
						if (!mystrnicmp("Re:",src,3))
						{
							src += 3;
							skip_spaces = 1;
							continue;
						}

						if (c != ' ' || !skip_spaces)
						{
							*dest++= c;
							skip_spaces=0;
						}
					}
					src++;
				}
				*dest = 0;

				if ((subject_header = encode_header_field("Subject",new_subject)))
				{
					mail_add_header(m, "Subject", 7, subject_header+9, strlen(subject_header)-9);
					free(subject_header);
				}

				free(new_subject);
			}
		}

		if ((text_mail = mail_find_type(mail, "text", "plain")))
		{
			char *replied_text;

			/* city the text and assign it to the mail, it's enough to set
         decoded_data */

			mail_decode(text_mail);

			if (mail->decoded_data) replied_text = cite_text(mail->decoded_data,mail->decoded_len);
			else replied_text = cite_text(mail->text + mail->text_begin, mail->text_len);

			if (replied_text)
			{
				m->decoded_data = replied_text;
				m->decoded_len = strlen(replied_text);
/*				free(replied_text);*/
			}
		}

		mail_process_headers(m);
	}
	return m;
}

/**************************************************************************
 Interprets the the already read headers. A return value of 0 means error
 TODO: Must use functions better since this function is really too big
**************************************************************************/
int mail_process_headers(struct mail *mail)
{
	char *buf;
	char *from;

	/* find out the date of the mail */
	if ((buf = mail_find_header_contents(mail,"date")))
	{
		/* syntax should be checked before! */
		int day,month,year,hour,min,sec;
		char *date = strstr(buf,",");
		if (!date) date = buf;
		else date++;

		while (isspace(*date)) date++;
		day = atoi(date);
		while (isdigit(*date)) date++;
		while (isspace(*date)) date++;
		if (!strnicmp(date,"jan",3)) month = 1; /* Not ANSI C */
		else if (!strnicmp(date,"feb",3)) month = 2;
		else if (!strnicmp(date,"mar",3)) month = 3;
		else if (!strnicmp(date,"apr",3)) month = 4;
		else if (!strnicmp(date,"may",3)) month = 5;
		else if (!strnicmp(date,"jun",3)) month = 6;
		else if (!strnicmp(date,"jul",3)) month = 7;
		else if (!strnicmp(date,"aug",3)) month = 8;
		else if (!strnicmp(date,"sep",3)) month = 9;
		else if (!strnicmp(date,"oct",3)) month = 10;
		else if (!strnicmp(date,"nov",3)) month = 11;
		else month = 12;
		date += 3;
		while (isspace(*date)) date++;
		year = atoi(date);
		if (year < 200) year += 1900;

		while (isdigit(*date)) date++;
		while (isspace(*date)) date++;
		hour = atoi(date);
		if (hour < 100)
		{
			while (isdigit(*date)) date++;
			while (!isdigit(*date)) date++;
			min = atoi(date);
			while (isdigit(*date)) date++;
			while (!isdigit(*date)) date++;
			sec = atoi(date);
		} else /* like examples in rfc 822 */
		{
			min = hour % 100;
			hour = hour / 100;
			sec = 0;
		}

		/* Time zone is missing */
		mail->seconds = sm_get_seconds(day,month,year) + (hour*60+min)*60 + sec;
	}

	from = mail_find_header_contents(mail,"from");
	if (from)
	{
		struct parse_address paddr;

		mail->author = NULL;

		if ((buf = parse_address(from,&paddr)))
		{
			struct mailbox *first_addr = (struct mailbox*)list_first(&paddr.mailbox_list);
			if (first_addr)
			{
				if (first_addr->phrase) mail->author = strdup(first_addr->phrase);
				else
				{
					if (first_addr->addr_spec)
					{
						if (!(mail->author = mystrdup(addressbook_get_realname(first_addr->addr_spec))))
						{
							mail->author = strdup(first_addr->addr_spec);
						}
					}
				}
			}
			free_address(&paddr);
		}

		if (!mail->author) mail->author = strdup(from);
	}

	if ((buf = mail_find_header_contents(mail,"subject")))
	{
		parse_text_string(buf,&mail->subject);
	}

	/* Check if mail is a mime mail */
	if ((buf = mail_find_header_contents(mail, "mime-version")))
	{
		int version;
		int revision;

		version = atoi(buf);
		while (isdigit(*buf)) buf++;
		revision = atoi(buf);

		mail->mime = (version << 16) | revision;
	} else mail->mime = 0;

	/* Check the content-type of the whole mail */
	if ((buf = mail_find_header_contents(mail, "content-type")))
	{
		/* content  :=   "Content-Type"  ":"  type  "/"  subtype  *(";" parameter) */

		char *subtype = strchr(buf,'/');
		if (subtype)
		{
			int len = subtype - buf;
			if (len)
			{
				if ((mail->content_type = malloc(len+1)))
				{
					subtype++;

					strncpy(mail->content_type,buf,len);
					mail->content_type[len]=0;

					if ((subtype = parse_token(subtype,&mail->content_subtype)))
					{
						while (1)
						{
							if (*subtype++ == ';')
							{
								struct content_parameter *new_param;
								struct parse_parameter dest;
								char c;

								/* Skip spaces */
								while ((c = *subtype))
								{
									if (!isspace(c)) break;
									subtype++;
								}

								if (!(subtype = parse_parameter(subtype, &dest)))
									break;

								if ((new_param = (struct content_parameter *)malloc(sizeof(struct content_parameter))))
								{
									new_param->attribute = dest.attribute;
									new_param->value = dest.value;
									list_insert_tail(&mail->content_parameter_list,&new_param->node);
								} else break;
							} else break;

						}
					}
				}
			}
		}
	}

	/* Check the Content-Disposition of the whole mail*/
	if ((buf = mail_find_header_contents(mail, "Content-Disposition")))
	{
		if (!mail->filename)
		{
			char *fn = mystristr(buf,"filename=");
			if (fn)
			{
				fn += sizeof("filename=")-1;
				parse_value(fn,&mail->filename);
			}
		}
	}

	if (!mail->content_type || !mail->content_subtype)
	{
		mail->content_type = strdup("text");
		mail->content_subtype = strdup("plain");
	}

	if ((buf = mail_find_header_contents(mail, "Content-transfer-encoding")))
	{
		mail->content_transfer_encoding = strdup(buf);
	}

	if (!mail->content_transfer_encoding)
	{
		mail->content_transfer_encoding = strdup("7bit");
	}

	return 1;
}

/**************************************************************************
 Looks for an parameter and returns the value, otherwise NULL
**************************************************************************/
static char *mail_find_content_parameter_value(struct mail *mail, char *attribute)
{
	struct content_parameter *param = (struct content_parameter*)list_first(&mail->content_parameter_list);

	while (param)
	{
		if (!stricmp(attribute,param->attribute)) return param->value;
		param = (struct content_parameter *)node_next(&param->node);
	}

	return NULL;
}

/**************************************************************************
 Reads the structrutre of a mail (uses ugly recursion)
**************************************************************************/
static int mail_read_structure(struct mail *mail)
{
	if (!stricmp(mail->content_type,"multipart"))
	{
		/* message is a multipart message */
		char *boundary = mail_find_content_parameter_value(mail,"boundary");
		if (boundary)
		{
			char *search_str = strdupcat("\n--"/*or "--"*/,boundary);
			if (search_str)
			{
				char *buf = mail->text + mail->text_begin;

				if ((buf = strstr(buf,search_str) + strlen(search_str)))
				{
					if (*buf == 13) buf++;
					if (*buf == 10) buf++;

					while (1)
					{
						struct mail *new_mail;
						char *end_part = strstr(buf,search_str);
						if (!end_part) break;

						if ((new_mail = mail_create()))
						{
							struct mail_scan ms;

							new_mail->size = end_part - buf;

							mail_scan_buffer_start(&ms,new_mail);
							mail_scan_buffer(&ms, buf, end_part - buf);
							mail_scan_buffer_end(&ms);
							mail_process_headers(new_mail);

							new_mail->text = mail->text;
							new_mail->text_begin += buf - mail->text;
							/* text_len is set by mail_scan_buffer */

							if (mail->num_multiparts == mail->multipart_allocated)
							{
								mail->multipart_allocated += 5;
								mail->multipart_array = realloc(mail->multipart_array,sizeof(struct mail*)*mail->multipart_allocated);
							}

							if (mail->multipart_array)
							{
								mail->multipart_array[mail->num_multiparts++] = new_mail;
							}
							mail_read_structure(new_mail); /* the recursion */
						}

						buf = end_part + strlen(search_str);
						if (*buf == 13) buf++;
						if (*buf == 10) buf++;
					}
				}
			}
		}
	}
	return 1;
}


/**************************************************************************
 Reads the contents of a mail
**************************************************************************/
void mail_read_contents(char *folder, struct mail *mail)
{
	char path[256];
	FILE *fp;

	getcwd(path, sizeof(path));
	if(chdir(folder) == -1) return;

	if ((fp = fopen(mail->filename,"rb")))
	{
		if ((mail->text = malloc(mail->size+1)))
		{
			fread(mail->text,1,mail->size,fp);
			mail->text[mail->size]=0;
		}

		mail_read_structure(mail);

		fclose(fp);
	}

	chdir(path);
}

/**************************************************************************
 Decodes the given mail
**************************************************************************/
void mail_decode(struct mail *mail)
{
	/* If mail is already decoded do nothing */
	if (mail->decoded_data) return;

	/* If no text is available return */
	if (!mail->text) return;

	if (!stricmp(mail->content_transfer_encoding,"base64"))
	{
		mail->decoded_data = decode_base64(mail->text + mail->text_begin, mail->text_len,&mail->decoded_len);
	} else if (!stricmp(mail->content_transfer_encoding,"quoted-printable"))
	{
		mail->decoded_data = decode_quoted_printable(mail->text + mail->text_begin, mail->text_len,&mail->decoded_len);
	}
}

/**************************************************************************
 Set some stuff of the mail (only useful if mail not created with
 mail_create_from_file())
**************************************************************************/
int mail_set_stuff(struct mail *mail, char *filename, unsigned int size)
{
	if (mail->filename) free(mail->filename);
	if ((mail->filename = strdup(filename)))
	{
		mail->size = size;
		return 1;
	}
	return 0;
}

/**************************************************************************
 frees all memory associated with a mail
**************************************************************************/
void mail_free(struct mail *mail)
{
	struct header *hdr;
	int i;

	if (!mail) return;

	while ((hdr = (struct header *)list_remove_tail(&mail->header_list)))
	{
		if (hdr->name) free(hdr->name);
		if (hdr->contents) free(hdr->contents);
		free(hdr);
	}

	for (i=0;i<mail->num_multiparts;i++)
	{
		mail_free(mail->multipart_array[i]); /* recursion */
	}

	if (mail->decoded_data) free(mail->decoded_data);
	if (mail->filename && mail->text) free(mail->text);
	if (mail->filename) free(mail->filename);
	free(mail);
}

/**************************************************************************
 Looks for an header and returns it, otherwise NULL
**************************************************************************/
static struct header *mail_find_header(struct mail *mail, char *name)
{
	struct header *header = (struct header*)list_first(&mail->header_list);

	while (header)
	{
		if (!stricmp(header->name, name)) return header;
		header = (struct header*)node_next(&header->node);
	}
	return NULL;
}

/**************************************************************************
 Looks for an header and returns the contents, otherwise NULL
**************************************************************************/
char *mail_find_header_contents(struct mail *mail, char *name)
{
	struct header *header = mail_find_header(mail,name);
	if (header) return header->contents;
	return NULL;
}

/**************************************************************************
 Converts a number to base 18 character sign
**************************************************************************/
static char get_char_18(int val)
{
	if (val >= 0 && val <= 9) return (char)('0' + val);
	return (char)('a' + val-10);
}

/**************************************************************************
 Returns a unique filename for a new mail
**************************************************************************/
char *mail_get_new_name(void)
{
	char *rc;
	long t;
	struct tm *d, tm;
	unsigned short day_secs;
	short i;
	char dummy[8]; 
	char *buf;
	
	buf = malloc(17);
	
	time(&t);
	d = localtime(&t);
	tm = *d;
	
	day_secs = (tm.tm_min * 60) + tm.tm_sec;
	dummy[4] = 0;
	dummy[3] = get_char_18(day_secs % 18);
	dummy[2] = get_char_18((day_secs / 18)%18);
	dummy[1] = get_char_18((day_secs / 18 / 18)%18);
	dummy[0] = get_char_18(day_secs / 18 / 18 / 18);
	
	for (i=0;;i++)
	{
		FILE *fp;
		
		sprintf(buf,"%02ld%02ld%04ld%s.%03lx",tm.tm_mday,tm.tm_mon,tm.tm_year,dummy,i);

		fp = fopen(buf, "r");
		if(fp != NULL)
		{
			fclose(fp);
		} else break;
	}
	
	rc = buf;
	
	return(rc);
}

/**************************************************************************
 Strips the LFs from a file. Not implemented.
**************************************************************************/
int mail_strip_lf(char *fn)
{
	int rc;
	
	rc = FALSE;
	
	return(rc);
}

/**************************************************************************
 Creates an address list from a given string (Note, that this is probably
 misplaced in mail.c)
**************************************************************************/
struct list *create_address_list(char *str)
{
	struct list *list = (struct list*)malloc(sizeof(struct list));
	if (list)
	{
		struct parse_address addr;
		char *ret;

		list_init(list);

		if ((ret = parse_address(str,&addr)))
		{
			/* note mailbox is simliar to address, probably one is enough */
			struct mailbox *mb = (struct mailbox*)list_first(&addr.mailbox_list);
			while (mb)
			{
				struct address *new_addr = (struct address*)malloc(sizeof(struct address));
				if (new_addr)
				{
					if (mb->phrase) new_addr->realname = strdup(mb->phrase);
					else new_addr->realname = NULL;
					if (mb->addr_spec) new_addr->email = strdup(mb->addr_spec);
					else new_addr->email = NULL;

					list_insert_tail(list,&new_addr->node);
				}
				mb = (struct mailbox*)node_next(&mb->node);
			}

			
			free_address(&addr);
		}
		
	}
	return list;
}

/**************************************************************************
 Frees all memory allocated in create_address_list()
**************************************************************************/
void free_address_list(struct list *list)
{
	struct address *address;
	while ((address = (struct address*)list_remove_tail(list)))
	{
		if (address->realname) free(address->realname);
		if (address->email) free(address->email);
	}
	free(list);
}

/**************************************************************************
 Initialized a composed mail instance
**************************************************************************/
void composed_mail_init(struct composed_mail *mail)
{
	memset(mail, 0, sizeof(struct composed_mail));
	list_init(&mail->list);
}

/**************************************************************************
 Writes out the from header field (it shouldn't be inlined because
 it uses some stack space
**************************************************************************/
int mail_compose_write_from(FILE *fp)
{
	struct list list;
	struct address address;
	char *from;
	int rc = 0;

	list_init(&list);
	memset(&address,0,sizeof(struct address));

	address.realname = user.config.realname;
	address.email = user.config.email;
	list_insert_tail(&list,&address.node);

	if ((from = encode_address_field("From", &list)))
	{
		if (fprintf(fp,"%s\n",from)>=0) rc = 1;
		free(from);
	}

	return rc;
}

/**************************************************************************
 Writes out the attachments into the body (uses recursion)
**************************************************************************/
static int mail_compose_write(FILE *fp, struct composed_mail *new_mail)
{
	struct composed_mail *cmail;

	if (new_mail->to)
	{
		char *subject;
		struct list *alist;

		if (!mail_compose_write_from(fp))
			return 0;

		if ((alist = create_address_list(new_mail->to)))
		{
			char *to = encode_address_field("To", alist);
			if (to)
			{
				fprintf(fp,"%s\n",to);
				free(to);
			}
			free_address_list(alist);
		}

		if ((subject = encode_header_field("Subject",new_mail->subject)))
		{
			time_t t;
			struct tm *d;

			const char *mon_str[] = 
			{
				"Jan","Feb","Mar","Apr","May","Jun",
				"Jul","Aug","Sep","Oct","Nov","Dec"
			};

			fprintf(fp,"%s", subject);
			fprintf(fp,"X-Mailer: %s\n", "SimpleMail - Mailer by Hynek Schlawack and Sebastian Bauer");
			fprintf(fp,"MIME-Version: 1.0\n");

			time(&t);
			d = localtime(&t);

			fprintf(fp,"Date: %02ld %s %4ld %02ld:%02ld:%02ld +0100\n",d->tm_mday,mon_str[d->tm_mon],d->tm_year + 1900,d->tm_hour,d->tm_min,d->tm_sec);
		}
	}

	if ((cmail = (struct composed_mail *)list_first(&new_mail->list)))
	{
		/* mail is a multipart message */
		char *boundary = (char*)malloc(128);
		if (boundary)
		{
			sprintf(boundary, "--==bound%lx%lx----",boundary,ftell(fp));
			fprintf(fp, "Content-Type: %s; boundary=\"%s\"\n", new_mail->content_type,boundary);
			fprintf(fp, "\n");
			fprintf(fp, mime_preample);

			while (cmail)
			{
				fprintf(fp, "--%s\n",boundary);
				mail_compose_write(fp,cmail);
				cmail = (struct composed_mail*)node_next(&cmail->node);
			}

			fprintf(fp, "--%s--\n",boundary);

			free(boundary);
		}
	} else
	{
		unsigned int body_len;
		char *body_encoding;
		char *body = NULL;

		if (new_mail->text)
		{
			/* mail text */
			body = encode_body(new_mail->text, strlen(new_mail->text), new_mail->content_type, &body_len, &body_encoding);
			fprintf(fp,"Content-Type: text/plain; charset=ISO-8859-1\n");
		} else
		{
			fprintf(fp,"Content-Type: %s\n",new_mail->content_type);
			if (new_mail->filename)
			{
				FILE *fh;

				fprintf(fp,"Content-Disposition: attachment; filename=%s\n",sm_file_part(new_mail->filename));

				if ((fh = fopen(new_mail->filename, "rb")))
				{
					int size;
					unsigned char *buf;

					fseek(fh,0,SEEK_END);
					size = ftell(fh);
					fseek(fh,0,SEEK_SET);

					if ((buf = (char*)malloc(size)))
					{
						fread(buf,1,size,fh);
						body = encode_body(buf, size, new_mail->content_type, &body_len, &body_encoding);
						free(buf);
					}
					fclose(fh);
				}
			}
		}

		fprintf(fp,"Content-transfer-encoding: %s\n",body_encoding);
		fprintf(fp,"\n");
		fprintf(fp,"%s\n",body?body:"");

		if (body) free(body);
	}
	return 1;
}

/**************************************************************************
 Composes a new mail and write's it to the outgoing drawer
**************************************************************************/
void mail_compose_new(struct composed_mail *new_mail)
{
	struct folder *outgoing;
	char path[256];
	char *new_name;

	if (new_mail->mail_folder)
		outgoing = folder_find_by_name(new_mail->mail_folder);
	else outgoing = NULL;

	if (!outgoing) outgoing = folder_outgoing();
	if (!outgoing) return;

	getcwd(path, sizeof(path));
	if (chdir(outgoing->path) == -1) return;

	if ((new_name = mail_get_new_name()))
	{
		FILE *fp;
		struct mail *mail; /* the mail after it has scanned */

		if ((fp = fopen(new_name,"wb")))
		{
			mail_compose_write(fp, new_mail);
			fclose(fp);
		}

		if ((mail = mail_create_from_file(new_name)))
		{
			struct mail *old_mail;

			if (new_mail->mail_filename) old_mail = folder_find_mail_by_filename(outgoing,new_mail->mail_filename);
			else old_mail = NULL;

			if (old_mail)
			{
				folder_replace_mail(outgoing, old_mail, mail);
				callback_mail_changed(outgoing, old_mail, mail);
				remove(old_mail->filename);
				free(old_mail);
			} else
			{
				callback_new_mail_written(mail);
			}
		}
	}

	chdir(path);
}
