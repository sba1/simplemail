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
** mail.c
*/

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "account.h"
#include "addressbook.h"
#include "codecs.h"
#include "configuration.h"
#include "folder.h" /* for mail_compose_new() */
#include "mail.h"
#include "parse.h"
#include "phrase.h"
#include "signature.h"
#include "simplemail.h" /* for the callbacks() */
#include "SimpleMail_rev.h"
#include "support.h"
#include "support_indep.h"
#include "trans.h" /* for mail_upload_single() */

/* porototypes */
static char *mail_find_content_parameter_value(struct mail *mail, char *attribute);
/*static struct header *mail_find_header(struct mail *mail, char *name);*/

/* the mime preample used in mime multipart messages */
const static char mime_preample[] = 
{
	"Warning: This is a message in MIME format. Your mail reader does not\n"
	"support MIME. Some parts of this message will be readable as plain text.\n"
	"To see the rest, you will need to upgrade your mail reader. Following are\n"
	"some URLs where you can find MIME-capable mail programs for common\n"
	"platforms:\n"
	"\n"
	"  Amiga............: SimpleMail   http://simplemail.sourceforge.net/\n"
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
	while(n && isspace((unsigned char)(*src)))
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
 Determines the length from now until a wordend
**************************************************************************/
static int word_length(const char *buf)
{
	unsigned char c;
	int len = 0;

	while ((c = *buf))
	{
		if (isspace(c))
		{
			if (c == 10 || c == 13) return 0;
			len++;
		} else break;
		buf++;
	}


	while ((c = *buf))
	{
		if (isspace(c) || c == 0) break;
		len++;
		buf++;
	}
	return len;
}

/**************************************************************************
 Copies to quoting chars in to the buffer. len is the length of the
 buffer to avoid overflow
**************************************************************************/
static void quoting_chars(char *buf, int len, char *text)
{
	unsigned char c;
	int new_color = 0;
	int i=0;
	int last_bracket = 0;
	while ((c = *text++) && i<len-1)
	{
		if (c == '>')
		{
			last_bracket = i+1;
			if (new_color == 1) new_color = 2;
			else new_color = 1;
		} else
		{
			if (c==10) break;
			if ((new_color == 1 || new_color == 2) && c != ' ') break;
			if (c==' ' && new_color == 0) break;
		}
		buf[i++] = c;
	}
	buf[last_bracket]=0;
}

/**************************************************************************
 Cites a text
**************************************************************************/
static char *quote_text(char *src, int len)
{
	FILE *fh = tmpfile();
	static char temp_buf[128];
	char *cited_buf = NULL;
	

	if (fh)
	{
		int cited_len;
		int newline = 1;
		int line_len = 0;

		if (user.config.write_reply_quote)
			quoting_chars(temp_buf,sizeof(temp_buf),src);

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

				if (user.config.write_reply_quote)
					quoting_chars(temp_buf,sizeof(temp_buf),src);

				line_len = 0;

				continue;
			}

			if (newline)
			{
				if (c=='>') { fputc('>',fh); line_len++;}
				else { fputs("> ",fh); line_len+=2;}
				newline = 0;
			}

			if (user.config.write_reply_quote)
			{
				if (isspace(c) && line_len + word_length(src) >= user.config.write_wrap)
				{
					src++;
					fputs("\n>",fh);
					fputs(temp_buf,fh);
					fputc(' ',fh);
					line_len=strlen(temp_buf)+2;
					continue;
				}
			}

			fputc(c,fh);

			line_len++;

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
int mail_add_header(struct mail *mail, char *name, int name_len, char *contents, int contents_len)
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
	return 0;
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
 1 is returned, else 0 (error handling not supported yet, but it's safe).

 This function could now be replaced by a line version, since we now have
 tcp_readln()
**************************************************************************/
int mail_scan_buffer(struct mail_scan *ms, char *mail_buf, int size)
{
	unsigned char c;
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
					mail->text_begin = ms->position+((c==13)?2:1);
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
				else
				{
					mode = 0;
				}
			}
		}

		buf++;
		ms->position++;
	}

	/* if we are here the buffersize was too small */
	{
		if (/*name_start && !name_size &&*/mode == 1) name_size = buf - name_start;
		if (contents_start && !contents_size && (mode == 3 || mode ==4 || mode == 0)) contents_size = buf - contents_start;

		if (!mail_scan_buffer_save_line(ms,name_start,name_size,contents_start,contents_size))
			return 0;

		ms->mode = mode;
	}

	return 1;
}

/**************************************************************************
 Find an compound object of a multipart/related mail (RFC2387)
 (eighter by Content-ID or Content-Location). NULL if object. m is a mail
 in the multipart/related mail.
**************************************************************************/
struct mail *mail_find_compound_object(struct mail *m, char *id)
{
	int content_id = !mystrnicmp("cid:",id,4);
	if (content_id)
	{
		unsigned char c;
		id += 4;
		while ((c=*id))
		{
			if (!isspace(c)) break;
			id++;
		}
	}

	while ((m = m->parent_mail))
	{
		if (!mystricmp(m->content_type,"multipart") && !mystricmp(m->content_subtype,"related"))
		{
			int i;
			for (i=0;i<m->num_multiparts;i++)
			{
				if (content_id)
				{
					if (!mystricmp(id,m->multipart_array[i]->content_id)) return m->multipart_array[i];
				}
			}
			return NULL;
		}
	}

	return NULL;
}

/**************************************************************************
 returns the first mail with the given mime type/subtype
 (recursive). Return NULL if it doesn't exists.
**************************************************************************/
struct mail *mail_find_content_type(struct mail *m, char *type, char *subtype)
{
	int i;
	if (!mystricmp(m->content_type, type) && !mystricmp(m->content_subtype,subtype))
		return m;

	for (i=0;i < m->num_multiparts; i++)
	{
		struct mail *rm = mail_find_content_type(m->multipart_array[i],type,subtype);
		if (rm) return rm;
	}

	return NULL;
}

/**************************************************************************
 Finds the initial mail which should be displayed. This is always the
 first non multipart mail. For multipart/alternative mails it returns the
 prefered one (depending on what the GUI prefers and how SimpleMail is
 configured).
**************************************************************************/
struct mail *mail_find_initial(struct mail *m)
{
	struct mail *pref = NULL;
	int alter = 0;
	int i = 0;

	while(m)
	{
		if (!mystricmp(m->content_type, "multipart"))
		{
			if (!mystricmp(m->content_subtype, "alternative"))
				alter = 1;
			i = 0;
			m = m->multipart_array[0];
		} else
		{
			if (!alter) return m;
			if (!pref) pref = m;
			else
			{
				/* Currently we prefer always text/plain which is on the beginning anywhy */
				if (!mystricmp(m->content_type, "text") && !mystricmp(m->content_subtype, "plain"))
					return m;
			}
			i++;
			if (i >= m->parent_mail->num_multiparts) return pref;
			m = m->parent_mail->multipart_array[i];
		}
	}
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
		
		sprintf(buf,"%02d%02d%04d%s.%03x",tm.tm_mday,tm.tm_mon,tm.tm_year,dummy,i);

		fp = fopen(buf, "r");
		if(fp != NULL)
		{
			fclose(fp);
		} else break;
	}
	
	rc = buf;
	
	return(rc);
}

/* a table with all filename extensions */
/* they are mapped 1 to 1 */
static char status_extensions[] =
{
	0,'0','1','2','3','4','5','6','7','8','9',
	'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z','!','$','-'
};

/**************************************************************************
 Returns a new filename for the mail which matches the given status.
 String is allocated with malloc
**************************************************************************/
char *mail_get_status_filename(char *oldfilename, int status_new)
{
	int len = strlen(oldfilename);
	char *filename = (char*)malloc(len+6);
	if (filename)
	{
		char *suffix;
		int new_suffix;

		strcpy(filename,oldfilename);
		suffix = strrchr(filename,'.');
		if (!suffix) suffix = filename + len;
		else
		{
			if (suffix[2])
			{
				/* the point is not the status point, so it must be added */
				suffix = filename + len;
			}
		}

		if (status_new < 0 || status_new >= 32)
		{
			*suffix = 0;
			return filename;
		}

		new_suffix = status_extensions[status_new];
		if (!new_suffix) *suffix = 0;
		else
		{
			*suffix++ = '.';
			*suffix++ = new_suffix;
			*suffix = 0;
		}
	}
	return filename;
}

/**************************************************************************
 Identifies the status of the mail (using the filename)
**************************************************************************/
void mail_identify_status(struct mail *m)
{
	char *suffix;
	int i;
	if (!m->filename) return;
	suffix = strrchr(m->filename,'.');
	if (!suffix || suffix[2])
	{
		m->status = MAIL_STATUS_UNREAD;
		return;
	}

  /* decode the status information */
	for (i=0;i<sizeof(status_extensions);i++)
	{
	  if (suffix[1] == status_extensions[i])
	  	m->status = i;
	}
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
					mail_identify_status(m);
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
 Creates a mail to be send to a given address (fills out the to field
 and the contents)
**************************************************************************/
struct mail *mail_create_for(char *to_str_unexpanded, char *subject)
{
	struct mail *mail;
	char *to_str;

	struct mailbox mb;
	memset(&mb,0,sizeof(mb));

	to_str = to_str_unexpanded?addressbook_get_expand_str(to_str_unexpanded):NULL;

	if (to_str) parse_mailbox(to_str,&mb);
	if ((mail = mail_create()))
	{
		char *mail_contents;
		struct phrase *phrase;

		/* this makes some things simpler */
		if (!(mail_contents = malloc(1)))
		{
			mail_free(mail);
			free(to_str);
			return NULL;
		}
		*mail_contents = 0;
		phrase = phrase_find_best(to_str);

		/* TODO: write a function for this! */
		if (to_str)
		{
			struct list *list = create_address_list(to_str);
			if (list)
			{
				char *to_header;

				to_header = encode_address_field("To",list);
				free_address_list(list);

				if (to_header)
				{
					mail_add_header(mail, "To", 2, to_header+4, strlen(to_header)-4);
					free(to_header);
				}
			}
		}

		/* TODO: write a function for this! */
		if (subject)
		{
			char *subject_header;
			if ((subject_header = encode_header_field("Subject",subject)))
			{
				mail_add_header(mail, "Subject", 7, subject_header+9, strlen(subject_header)-9);
				free(subject_header);
			}
		}

		if (mb.phrase)
		{
			if (phrase && phrase->write_welcome_repicient)
			{
				char *str = mail_create_string(phrase->write_welcome_repicient, NULL, mb.phrase, mb.addr_spec);
				if (str)
				{
					mail_contents = realloc(mail_contents,mystrlen(mail_contents)+strlen(str)+1+1);
					if (mail_contents) { strcat(mail_contents,str); strcat(mail_contents,"\n");}
					free(str);
				}
			}
		} else
		{
			if (phrase && phrase->write_welcome)
			{
				char *str = mail_create_string(phrase->write_welcome, NULL, NULL, NULL);
				if (str)
				{
					mail_contents = realloc(mail_contents,mystrlen(mail_contents)+strlen(str)+1+1);
					if (mail_contents) { strcat(mail_contents,str); strcat(mail_contents,"\n");}
					free(str);
				}
			}
		}

		if (phrase && phrase->write_closing)
		{
			char *str = mail_create_string(phrase->write_closing, NULL, NULL, NULL);
			if (str)
			{
				int mail_contents_len = mystrlen(mail_contents);
				if (mail_contents)
				{
					mail_contents = realloc(mail_contents,mail_contents_len+strlen(str)+1);
					if (mail_contents) strcat(mail_contents,str);
					free(str);
				} else mail_contents = str;
			}
		}

		mail->decoded_data = mail_contents;
		mail->decoded_len = mail_contents?strlen(mail_contents):0;
		mail_process_headers(mail);
	}
	free(to_str);
	return mail;
}

/**************************************************************************
 Creates a Reply to a given mail. That means change the contents of
 "From:" to "To:", change the subject, quote the first text passage
 and remove the attachments. The mail is proccessed. The given mail should
 be processed to.
**************************************************************************/
struct mail *mail_create_reply(struct mail *mail)
{
	struct mail *m = mail_create();
	if (m)
	{
		char *from = mail_find_header_contents(mail,"from");
		char *to;
		struct mail *text_mail;
		struct phrase *phrase = phrase_find_best(from);

		if (from)
		{
			struct list *alist;
			char *replyto = mail_find_header_contents(mail, "reply-to");
			struct mailbox from_addr;
			struct mailbox replyto_addr;
			int which_address = 1;

			if (replyto)
			{
				parse_mailbox(from, &from_addr);
				parse_mailbox(replyto,&replyto_addr);

				which_address = sm_request(NULL,
												"Sender address (From) is <%s>, but\n"
												"return address (Reply-To) is <%s>.\n"
												"Which address do you want to use?","_From|*_Reply-To|_Both|_Cancel",
												from_addr.addr_spec,replyto_addr.addr_spec);

				if (from_addr.phrase)  free(from_addr.phrase);
				if (from_addr.addr_spec) free(from_addr.addr_spec);
				if (replyto_addr.phrase)  free(replyto_addr.phrase);
				if (replyto_addr.addr_spec) free(replyto_addr.addr_spec);

				if (!which_address) return NULL;
			}

			if (which_address == 2) from = replyto;

			alist = create_address_list(from);
			if (alist)
			{
				char *to_header;

				if (which_address == 3)
					append_to_address_list(alist, replyto);

				to_header = encode_address_field("To",alist);
				free_address_list(alist);

				if (to_header)
				{
					mail_add_header(m, "To", 2, to_header+4, strlen(to_header)-4);
					free(to_header);
				}
			}
		}

		if ((to = mail_find_header_contents(mail,"to")))
		{
			struct account *ac = account_find_by_from(to);
			if (ac)
			{
				mail_add_header(m,"From", 4, ac->email,strlen(ac->email));
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
				while ((c = *src))
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

		if ((text_mail = mail_find_content_type(mail, "text", "plain")))
		{
			char *replied_text;
			char *welcome_text;

			if (phrase)
			{
				char *intro_text;
				int welcome_text_len;

				welcome_text = mail_create_string(phrase->reply_welcome,mail,NULL,NULL);
				welcome_text_len = mystrlen(welcome_text);
				intro_text = mail_create_string(phrase->reply_intro,mail,NULL,NULL);
				if ((welcome_text = realloc(welcome_text,welcome_text_len+mystrlen(intro_text)+3)))
				{
					if (!welcome_text_len) welcome_text[0] = 0;
					strcat(welcome_text,"\n");
					if (intro_text)
					{
						strcat(welcome_text,intro_text);
						strcat(welcome_text,"\n");
						free(intro_text);
					}
				}
			} else welcome_text = NULL;

			/* city the text and assign it to the mail, it's enough to set decoded_data */
			mail_decode(text_mail);

			if (text_mail->decoded_data) replied_text = quote_text(text_mail->decoded_data,text_mail->decoded_len);
			else replied_text = quote_text(text_mail->text + text_mail->text_begin, text_mail->text_len);

			if (replied_text)
			{
				if (welcome_text)
				{
					char *closing_text;
					char *buf;

					if (phrase) closing_text = mail_create_string(phrase->reply_close, mail, NULL,NULL);
					else closing_text = NULL;

					if ((buf = (char*)malloc(strlen(replied_text)+strlen(welcome_text)+mystrlen(closing_text)+8)))
					{
						strcpy(buf,welcome_text);
						strcat(buf,replied_text);
						if (closing_text)
						{
							strcat(buf,"\n");
							strcat(buf,closing_text);
							free(closing_text);
						}
						free(replied_text);
						replied_text = buf;
					}
				}

				m->decoded_data = replied_text;
				m->decoded_len = strlen(replied_text);
			}
			free(welcome_text);
		}

		if (mail->message_id) m->message_reply_id = mystrdup(mail->message_id);

		mail_process_headers(m);
	}
	return m;
}

/**************************************************************************
 Modifies the mail to a forwared one
**************************************************************************/
int mail_forward(struct mail *mail)
{
	char *subject = mail->subject;

	struct header *header = mail_find_header(mail,"to");

	if (header)
	{
		/* remove the to header */
		node_remove(&header->node);
		if (header->name) free(header->name);
		if (header->contents) free(header->contents);
		free(header);
	}

	if (mail->to) free(mail->to);
	mail->to = NULL;
	if (mail->message_id) free(mail->message_id);
	mail->message_id = NULL;
	if (mail->message_reply_id) free(mail->message_reply_id);
	mail->message_reply_id = NULL;

	if (subject)
	{
		char *new_subject;
		int len = strlen(subject);

		if ((new_subject = (char*)malloc(len+10)))
		{
			strcpy(new_subject,subject);
			strcat(new_subject," (fwd)");
			mail->subject = new_subject;
			free(subject);
		}
	}
	return 1;
}


/**************************************************************************
 Extract the name of a given address (and looks for matches in the
 addressbook). If more than one e-mail address is specified, *more_prt
 will be set to 1.
**************************************************************************/
static char *extract_name_from_address(char *addr, int *more_ptr)
{
	char *name = NULL;
	struct parse_address paddr;

	if (more_ptr) *more_ptr = 0;

	if ((name = parse_address(addr,&paddr)))
	{
		struct mailbox *first_addr = (struct mailbox*)list_first(&paddr.mailbox_list);
		if (first_addr)
		{
			if (first_addr->phrase) name = strdup(first_addr->phrase);
			else
			{
				if (first_addr->addr_spec)
				{
					if (!(name = mystrdup(addressbook_get_realname(first_addr->addr_spec))))
					{
						name = mystrdup(first_addr->addr_spec);
					}
				}
			}
			if (node_next(&first_addr->node))
			{
				if (more_ptr) *more_ptr = 1;
			}
		}
		free_address(&paddr);
	}

	if (!name) name = mystrdup(addr);
	return name;
}

/**************************************************************************
 Interprets the the already read headers. A return value of 0 means error
 TODO: Must use functions better since this function is really too big
**************************************************************************/
int mail_process_headers(struct mail *mail)
{
	char *buf;

	/* find out the date of the mail */
	if ((buf = mail_find_header_contents(mail,"date")))
	{
		/* syntax should be checked before! */
		int day,month,year,hour,min,sec;
		parse_date(buf,&day,&month,&year,&hour,&min,&sec);

		/* Time zone is missing */
		mail->seconds = sm_get_seconds(day,month,year) + (hour*60+min)*60 + sec;
	}

	buf = mail_find_header_contents(mail,"from");
	if (buf) mail->from = extract_name_from_address(buf,NULL);
	else mail->from = NULL;

	buf = mail_find_header_contents(mail,"to");
	if (buf)
	{
		int more;
		mail->to = extract_name_from_address(buf,&more);
		if (more) mail->flags |= MAIL_FLAGS_GROUP;
	} else mail->to = NULL;

	if ((buf = mail_find_header_contents(mail, "cc")))
	{
		mail->flags |= MAIL_FLAGS_GROUP;
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
								unsigned char c;

								/* Skip spaces */
								while ((c = *subtype))
								{
									if (!isspace(c)) break;
									subtype++;
								}

								if (!(subtype = parse_parameter(subtype, &dest)))
									break;

								if (!mystricmp(dest.attribute,"name"))
								{
									if (dest.attribute) free(dest.attribute);
									if (!mail->filename) mail->filename = dest.value;
									else
									{
										if (dest.value) free(dest.value);
									}
								} else
								{
									if ((new_param = (struct content_parameter *)malloc(sizeof(struct content_parameter))))
									{
										new_param->attribute = dest.attribute;
										new_param->value = dest.value;
										list_insert_tail(&mail->content_parameter_list,&new_param->node);
									} else break;
								}
							} else break;
						}
					}
				}
			}
		}
	}

	/* Content-ID */
	if ((buf = mail_find_header_contents(mail, "Content-ID")))
	{
		if (*buf++ == '<')
		{
			if (!(parse_addr_spec(buf,&mail->content_id)))
			{
				/* for the non rfc conform content-id's */
				char *buf2 = strrchr(buf,'>');
				if (buf2)
				{
					if ((mail->content_id = malloc(buf2-buf+1)))
					{
						strncpy(mail->content_id,buf,buf2-buf);
						mail->content_id[buf2-buf]=0;
					}
				}
			}
		}
	}

  /* Message ID's */
  if ((buf = mail_find_header_contents(mail, "Message-ID")))
  {
  	if (*buf++ == '<')
	  	parse_addr_spec(buf,&mail->message_id);
  }

	/* In-Reply To */
  if ((buf = mail_find_header_contents(mail, "In-Reply-To")))
  {
  	if (*buf++ == '<')
	  	parse_addr_spec(buf,&mail->message_reply_id);
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

	if (!mystricmp(mail->content_type, "multipart"))
	{
		mail->flags |= MAIL_FLAGS_ATTACH;
	}

  if ((buf = mail_find_header_contents(mail, "Importance")))
  {
		if (!mystricmp(buf,"high")) mail->flags |= MAIL_FLAGS_IMPORTANT;
  }

	/* if no filename is given set one */
	if (!mail->filename)
	{
		mail->filename = strdup("unnamed");
	}


/*
	if (!mystricmp(mail->content_type, "multipart") && !mystricmp(mail->content_subtype,"related"))
	{
		mail->multipart_related_type = mail_find_content_parameter_value(mail, "type");
		printf("%s\n",mail->multipart_related_type);
	}
*/

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
		if (!mystricmp(attribute,param->attribute)) return param->value;
		param = (struct content_parameter *)node_next(&param->node);
	}

	return NULL;
}

/**************************************************************************
 Reads the structrutre of a mail (uses ugly recursion)
**************************************************************************/
static int mail_read_structure(struct mail *mail)
{
	if (!mystricmp(mail->content_type,"multipart"))
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
/*					int related;*/ /* if is a related content subtype */

					if (*buf == 13) buf++;
					if (*buf == 10) buf++;

/*					related = !mystricmp(content_subtype, "related");*/

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
							new_mail->parent_mail = mail;
						}

						buf = end_part + strlen(search_str);
						if (*buf == 13) buf++;
						if (*buf == 10) buf++;
					}

/*
					if (mail->num_multiparts)
					{
						mail->multipart_related_root = mail->multipart_array[0];
					}
*/
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

	if (!mystricmp(mail->content_transfer_encoding,"base64"))
	{
		mail->decoded_data = decode_base64(mail->text + mail->text_begin, mail->text_len,&mail->decoded_len);
	} else if (!mystricmp(mail->content_transfer_encoding,"quoted-printable"))
	{
		mail->decoded_data = decode_quoted_printable(mail->text + mail->text_begin, mail->text_len,&mail->decoded_len,0);
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

	if (mail->html_header) free(mail->html_header);

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

	if (mail->message_id) free(mail->message_id);
	if (mail->message_reply_id) free(mail->message_reply_id);
	if (mail->content_type) free(mail->content_type);
	if (mail->content_subtype) free(mail->content_subtype);
	if (mail->content_id) free(mail->content_id);

	if (mail->decoded_data) free(mail->decoded_data);
	if (mail->filename && mail->text) free(mail->text);
	if (mail->filename) free(mail->filename);
	free(mail);
}

/**************************************************************************
 Looks for an header and returns it, otherwise NULL
**************************************************************************/
struct header *mail_find_header(struct mail *mail, char *name)
{
	struct header *header = (struct header*)list_first(&mail->header_list);

	while (header)
	{
		if (!mystricmp(header->name, name)) return header;
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
 Appends a address from a given address string to the list
**************************************************************************/
void append_to_address_list(struct list *list, char *str)
{
	struct list *append_list = create_address_list(str);
	if (append_list)
	{
		struct mailbox *mb;

		while ((mb = (struct mailbox*)list_remove_tail(append_list)))
		{
			list_insert_tail(list,&mb->node);
		}
		free(append_list);
	}
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
static int mail_compose_write_from(FILE *fp, char *from)
{
	int rc = 0;
	struct list *list = create_address_list(from);

	if (list)
	{
		char *from = encode_address_field("From", list);
		if (from)
		{
			if (fprintf(fp,"%s\n",from)>=0) rc = 1;
			free(from);
		}
		free_address_list(list);
	}
	return rc;
}

/**************************************************************************
 Writes out the reply header field
**************************************************************************/
static int mail_compose_write_reply(FILE *fp, char *reply)
{
	int rc = 0;
	struct list *list = create_address_list(reply);

	if (list)
	{
		char *from = encode_address_field("Reply-To", list);
		if (from)
		{
			if (fprintf(fp,"%s\n",from)>=0) rc = 1;
			free(from);
		}
		free_address_list(list);
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

		if (!mail_compose_write_from(fp,new_mail->from))
			return 0;

		if (new_mail->replyto && *new_mail->replyto)
			if (!mail_compose_write_reply(fp,new_mail->replyto))
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
			int offset = sm_get_gmt_offset();

			const char *mon_str[] = 
			{
				"Jan","Feb","Mar","Apr","May","Jun",
				"Jul","Aug","Sep","Oct","Nov","Dec"
			};

			fprintf(fp,"%s", subject);
			fprintf(fp,"X-Mailer: SimpleMail %d.%d (%s) E-Mail Client (c) 2000,2001 by Hynek Schlawack and Sebastian Bauer\n",VERSION,REVISION,"AmigaOS");

			time(&t);
			d = localtime(&t);

			fprintf(fp,"Date: %02d %s %4d %02d:%02d:%02d %+03d%02d\n",d->tm_mday,mon_str[d->tm_mon],d->tm_year + 1900,d->tm_hour,d->tm_min,d->tm_sec,offset/60+user.config.dst,offset%60);
		}

		if (new_mail->reply_message_id)
		{
			fprintf(fp,"In-Reply-To: <%s>\n",new_mail->reply_message_id);
		}
	}

	if ((cmail = (struct composed_mail *)list_first(&new_mail->list)))
	{
		/* mail is a multipart message */
		char *boundary = (char*)malloc(128);
		if (boundary)
		{
			fprintf(fp,"MIME-Version: 1.0\n");
			sprintf(boundary, "--==bound%x%lx----",(int)boundary,ftell(fp));
			fprintf(fp, "Content-Type: %s; boundary=\"%s\"\n", new_mail->content_type,boundary);
			fprintf(fp, "\n");
			if (new_mail->to) fprintf(fp, mime_preample);

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
		char *body_encoding = NULL;
		char *body = NULL;

		if (new_mail->text)
		{
			char *text;

			/* mail text */
			if (new_mail->to) body_encoding = "8bit"; /* mail has only one part which is a text, so it can be encoded in 8bit */

			if (user.config.write_wrap_type == 2)
			{
				text = mystrdup(new_mail->text);
				wrap_text(text,user.config.write_wrap);
			} else text = new_mail->text;

			if (text)
			{
				body = encode_body(text, strlen(text), new_mail->content_type, &body_len, &body_encoding);
				if (body_encoding && mystricmp(body_encoding,"7bit"))
				{
					if (new_mail->to) fprintf(fp,"MIME-Version: 1.0\n");
				  fprintf(fp,"Content-Type: text/plain; charset=ISO-8859-1\n");
				}
				if (user.config.write_wrap_type == 2) free(text);
			}
		} else
		{
			if (new_mail->filename)
			{
				FILE *fh;

				if (new_mail->to) fprintf(fp,"MIME-Version: 1.0\n");
				fprintf(fp,"Content-Type: %s\n",new_mail->content_type);
				fprintf(fp,"Content-Disposition: attachment; filename=%s\n",sm_file_part(new_mail->filename));

				if ((fh = fopen(new_mail->temporary_filename?new_mail->temporary_filename:new_mail->filename, "rb")))
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

		if (body_encoding && mystricmp(body_encoding,"7bit"))
			fprintf(fp,"Content-transfer-encoding: %s\n",body_encoding);

		fprintf(fp,"\n");
		fprintf(fp,"%s\n",body?body:"");

		if (body) free(body);
	}
	return 1;
}

/**************************************************************************
 Composes a new mail and write's it to the outgoing drawer.
 Returns 1 if succesful, else 0
**************************************************************************/
int mail_compose_new(struct composed_mail *new_mail, int hold)
{
	struct folder *outgoing;
	char path[256];
	char *new_name;
	int rc = 0;

	if (new_mail->mail_folder)
		outgoing = folder_find_by_name(new_mail->mail_folder);
	else outgoing = NULL;

	if (!outgoing) outgoing = folder_outgoing();
	if (!outgoing) return 0;

	if (!new_mail->from)
	{
		sm_request(NULL, "No valid from field specified. You must configure some Accounts\n"
										 "before creating new mails","Ok");
		return 0;
	}

	getcwd(path, sizeof(path));
	if (chdir(outgoing->path) == -1) return 0;

	if ((new_name = mail_get_new_name()))
	{
		FILE *fp;
		struct mail *mail; /* the mail after it has scanned */
		char *status_name; /* the prober status name */

		if ((status_name = mail_get_status_filename(new_name,hold?MAIL_STATUS_HOLD:MAIL_STATUS_WAITSEND)))
		{
			free(new_name);
			new_name = status_name;
		}

		if ((fp = fopen(new_name,"wb")))
		{
			mail_compose_write(fp, new_mail);
			rc = 1;
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

		if (hold == 2)
		{
			/* Mail should be send now! */
			mails_upload_signle(mail);
		}
	}

	chdir(path);

	return rc;
}

/**************************************************************************
 Creates an HTML File from a header. Currently only the most important
 headers were created.
**************************************************************************/
int mail_create_html_header(struct mail *mail)
{
	int rc = 0;
	FILE *fh;

	if (mail->html_header) return 1;

	if ((fh = tmpfile()))
	{
		int len;
		char *from = mail_find_header_contents(mail,"from");
		char *to = mail_find_header_contents(mail,"to");
		char *replyto = mail_find_header_contents(mail, "reply-to");
		char *cc = mail_find_header_contents(mail, "cc");
		char *portrait;
		char *style_text = user.config.read_link_underlined?"":" STYLE=\"TEXT-DECORATION: none\"";
		struct header *header;

		fprintf(fh,"<HTML><BODY BGCOLOR=\"#%06x\" TEXT=\"#%06x\" LINK=\"#%06x\">",user.config.read_background,user.config.read_text,user.config.read_link);

		if (from && (user.config.header_flags & (SHOW_HEADER_FROM | SHOW_HEADER_ALL)))
		{
			struct mailbox mb;
			parse_mailbox(from, &mb);

			if ((portrait = addressbook_get_portrait(mb.addr_spec)))
			{
				fprintf(fh,"<IMG SRC=\"file://localhost/%s\" ALIGN=RIGHT>",portrait);
			}

			fprintf(fh,"<STRONG>From:</STRONG> <A HREF=\"mailto:%s\"%s>",mb.addr_spec,style_text);

			if (mb.phrase)
			{
				fprintf(fh,"%s &lt;%s&gt;",mb.phrase,mb.addr_spec);
			} else
			{
				fputs(mb.addr_spec,fh);
			}

			if (mb.phrase)  free(mb.phrase);
			if (mb.addr_spec) free(mb.addr_spec);

			fputs("</A><BR>",fh);
		}

		if (to && (user.config.header_flags & (SHOW_HEADER_TO | SHOW_HEADER_ALL)))
		{
			struct parse_address p_addr;
			fputs("<STRONG>To:</STRONG> ",fh);
			if ((parse_address(to,&p_addr)))
			{
				struct mailbox *mb = (struct mailbox*)list_first(&p_addr.mailbox_list);
				while (mb)
				{
					fprintf(fh,"<A HREF=\"mailto:%s\"%s>",mb->addr_spec,style_text);
					if (mb->phrase) fprintf(fh,"%s &lt;%s&gt;",mb->phrase,mb->addr_spec);
					else fputs(mb->addr_spec,fh);
					fputs("</A>",fh);
					if ((mb = (struct mailbox*)node_next(&mb->node)))
					{
						fputs(", ",fh);
					}
				}
				free_address(&p_addr);
			}
			fputs("<BR>",fh);
		}

		if (cc && (user.config.header_flags & (SHOW_HEADER_CC | SHOW_HEADER_ALL)))
		{
			struct parse_address p_addr;
			fputs("<STRONG>Copies to:</STRONG> ",fh);
			if ((parse_address(cc,&p_addr)))
			{
				struct mailbox *mb = (struct mailbox*)list_first(&p_addr.mailbox_list);
				while (mb)
				{
					fprintf(fh,"<A HREF=\"mailto:%s\"%s>",mb->addr_spec,style_text);
					if (mb->phrase) fprintf(fh,"%s &lt;%s&gt;",mb->phrase,mb->addr_spec);
					else fputs(mb->addr_spec,fh);
					fputs("</A>",fh);
					if ((mb = (struct mailbox*)node_next(&mb->node)))
					{
						fputs(", ",fh);
					}
				}
				free_address(&p_addr);
			}
			fputs("<BR>",fh);
		}


		if (mail->subject && (user.config.header_flags & (SHOW_HEADER_SUBJECT|SHOW_HEADER_ALL))) fprintf(fh,"<STRONG>Subject:</STRONG> %s<BR>",mail->subject);
		if ((user.config.header_flags & (SHOW_HEADER_DATE | SHOW_HEADER_ALL))) fprintf(fh,"<STRONG>Date: </STRONG>%s<BR>",sm_get_date_long_str(mail->seconds));

		if (replyto && (user.config.header_flags & (SHOW_HEADER_REPLYTO | SHOW_HEADER_ALL)))
		{
			struct mailbox addr;
			parse_mailbox(replyto, &addr);
			fprintf(fh,"<STRONG>Replies To:</STRONG> <A HREF=\"mailto:%s\"%s>",addr.addr_spec,style_text);

			if (addr.phrase)
			{
				fprintf(fh,"%s &lt;%s&gt;",addr.phrase,addr.addr_spec);
			} else
			{
				fputs(addr.addr_spec,fh);
			}

			if (addr.phrase)  free(addr.phrase);
			if (addr.addr_spec) free(addr.addr_spec);

			fputs("</A><BR>",fh);
		}

		header = (struct header*)list_first(&mail->header_list);
		while (header)
		{
			if ((user.config.header_flags & SHOW_HEADER_ALL) || array_contains(user.config.header_array,header->name))
			{
				if (mystricmp(header->contents,"from") && mystricmp(header->contents,"to") &&
					  mystricmp(header->contents,"cc") && mystricmp(header->contents,"reply-to") &&
					  mystricmp(header->contents,"date") && mystricmp(header->contents,"subject"))
				{
					fprintf(fh,"<STRONG>%s:</STRONG> %s<BR>\n",header->name,header->contents);
				}
			}
			header = (struct header*)node_next(&header->node);
		}
		
		fputs("<BR CLEAR=ALL><HR>",fh);
		fputs("</BODY></HTML>",fh);

		if ((len = ftell(fh)))
		{
			fseek(fh,0,SEEK_SET);
			if ((mail->html_header = (char*)malloc(len+1)))
			{
				fread(mail->html_header,1,len,fh);
				mail->html_header[len]=0;
				rc = 1;
			}
		}
		fclose(fh);
	}
	return rc;
}

/**************************************************************************
 Returns the first name of a given person.
 Also (will somewhen) performs addressbook look up to determine the
 first name.
 Returns a static buffer.
**************************************************************************/
static char *get_first_name(char *realname, char *addr_spec)
{
	static char buf[256];
	if (realname)
	{
		char *sp;
		mystrlcpy(buf,realname,256);
		if ((sp = strchr(buf,' '))) *sp = 0;
	} else buf[0]=0;
	return buf;
}

/**************************************************************************
 Returns the last name of a given person.
 Also (will somewhen) performs addressbook look up to determine the
 first name.
 Returns a static buffer.
**************************************************************************/
static char *get_last_name(char *realname, char *addr_spec)
{
	static char buf[256];
	if (realname)
	{
		char *sp = strchr(realname,' ');
		if (sp)
		{
			sp++;
			mystrlcpy(buf,sp,256);
		}
	} else buf[0]=0;
	return buf;
}

/**************************************************************************
 Creates a string for a greeting/closing phrase. orig_mail, realname, addr_spec
 might be NULL
**************************************************************************/
char *mail_create_string(char *format, struct mail *orig_mail, char *realname,
												 char *addr_spec)
{
	char *str;

	if (!format) return NULL;
	if ((str = (char*)malloc(2048)))
	{
		char *src = format;
		char *dest = str;
		char c;

		while ((c = *src++))
		{
			if (c=='%')
			{
				if (*src == '%') { c = '%';src++;}
				else
				{
					if (*src == 0) continue;
					if (*src == 'a' && addr_spec)
					{
						strcpy(dest,addr_spec);
						dest += strlen(addr_spec);
					}
					if (*src == 'r' && realname)
					{
						char *first_name = get_first_name(realname,addr_spec); /* returns a static buffer and never fails */
						strcpy(dest,first_name);
						dest += strlen(first_name);
					}
					if (*src == 'v' && realname)
					{
						strcpy(dest,realname);
						dest += strlen(realname);
/*
						char *last_name = get_last_name(realname,addr_spec);
						strcpy(dest,last_name);
						dest += strlen(last_name);
*/
					}

					if (orig_mail)
					{
						char *from = mail_find_header_contents(orig_mail,"from");
						if (from)
						{
							struct mailbox mb;
							if (parse_mailbox(from,&mb))
							{
								if (*src == 'n' && mb.phrase)
								{
/*
									char *last_name = get_last_name(mb.phrase,mb.addr_spec);
									strcpy(dest,last_name);
									dest += strlen(last_name);
*/
									strcpy(dest,mb.phrase);
									dest += strlen(mb.phrase);
								}

								if (*src == 'f')
								{
									char *first_name = get_first_name(mb.phrase,mb.addr_spec);
									strcpy(dest,first_name);
									dest += strlen(first_name);
								}

								if (*src == 'e' && mb.addr_spec)
								{
									strcpy(dest,mb.addr_spec);
									dest += strlen(mb.addr_spec);
								}
								free(mb.phrase);
								free(mb.addr_spec);
							}
						}

						if (*src == 'd')
						{
							char *date = sm_get_date_str(orig_mail->seconds);
							strcpy(dest,date);
							dest += strlen(date);
						}
						if (*src == 't')
						{
							char *date = sm_get_time_str(orig_mail->seconds);
							strcpy(dest,date);
							dest += strlen(date);
						}
						if (*src == 's' && orig_mail->subject)
						{
							strcpy(dest,orig_mail->subject);
							dest += strlen(orig_mail->subject);
						}
					}

					src++;
					continue;
				}
			}

			if (c=='\\')
			{
				if (*src == '\\') { c = '\\';src++;}
				else if (*src == 'n') { c = '\n';src++;}
				else if (*src == 't') { c = '\t';src++;}
			}
			*dest++ = c;
		}
		*dest = 0;
	}
	return str;
}

/**************************************************************************
 Tests if the mail is allowed to download from internet
**************************************************************************/
int mail_allowed_to_download(struct mail *mail)
{
	char *from = mail_find_header_contents(mail,"from");
	struct mailbox mb;
	int rc = 0;

	if (parse_mailbox(from,&mb))
	{
		if (user.config.internet_emails)
		{
			int i;
			for (i=0;user.config.internet_emails[i];i++)
			{
				if (!mystricmp(user.config.internet_emails[i],mb.addr_spec))
				{
					rc = 1;
					break;
				}
			}
		}
		free(mb.addr_spec);
		free(mb.phrase);
	}
	return rc;
}
