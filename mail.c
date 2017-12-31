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
 * @file mail.c
 */

#include "mail.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "account.h"
#include "addressbook.h"
#include "addresslist.h"
#include "codecs.h"
#include "configuration.h"
#include "debug.h"
#include "folder.h" /* for mail_compose_new() */
#include "mail_headers.h"
#include "mail_support.h"
#include "parse.h"
#include "pgp.h"
#include "phrase.h"
#include "simplemail.h" /* for the callbacks() */
#include "smintl.h"
#include "support_indep.h"
#include "trans.h" /* for mail_upload_single() */

#include "request.h"
#include "support.h"
#include "timesupport.h"

#include "SimpleMail_rev.h"

/* prototypes */
static int mail_read_structure(struct mail_complete *mail);

/* the mime preample used in mime multipart messages */
static const char mime_preample[] =
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

static const char pgp_text[] =
{
 "Content-Type: application/pgp-encrypted\n"
 "\n"
 "Version: 1\n"
 "The following body part contains a PGP encrypted message. Either your\n"
 "mail reader doesn't support MIME/PGP as specified in RFC 2015, or\n"
 "the message was encrypted for someone else. To read the encrypted\n"
 "message, run the next body part through Pretty Good Privacy.\n"
};

/**
 * Add a new header with the given name/contents pair to the header list.
 *
 * @param mail the mail to which the header shall be added.
 * @param name the name of the header field
 * @param name_len the length of the name of the header field
 * @param contents the contents of the header field
 * @param contents_len the length
 * @return 0 on failure, 1 on success
 */
static int mail_complete_add_header(struct mail_complete *mail, char *name, int name_len,
									  char *contents, int contents_len)
{
	struct header *header;
	char *new_name;
	char *new_contents;

	if (!(header = (struct header*)malloc(sizeof(struct header))))
		return 0;

	new_name = (char*)malloc(name_len+1);
	new_contents = (char*)malloc(contents_len+1);

	if (new_name && new_contents)
	{
		new_name[mailncpy(new_name,name,name_len)] = 0;
		new_contents[mailncpy(new_contents,contents,contents_len)] = 0;

		header->name = new_name;
		header->contents = new_contents;
		list_insert_tail(&mail->header_list,&header->node);

		return 1;
	}
	free(new_name);
	free(new_contents);
	free(header);
	return 0;
}

/*****************************************************************************/

void mail_scan_buffer_start(struct mail_scan *ms, struct mail_complete *mail)
{
	memset(ms,0,sizeof(struct mail_scan));
	ms->mail = mail;
}

/*****************************************************************************/

void mail_scan_buffer_end(struct mail_scan *ms)
{
	free(ms->line);
}

/**
 * Save the current header line in line, sets name_size and contents_size
 * return 0 if an error happened
 *
 * @param ms
 * @param name_start
 * @param name_size
 * @param contents_start
 * @param contents_size
 * @return
 */
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

/*****************************************************************************/

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
	struct mail_complete *mail = ms->mail;

	if (mode == 1) name_start = mail_buf;
	else if (mode == 3 || mode == 4) contents_start = mail_buf;

	while (buf < mail_buf_end)
	{
		c = *buf;
		if (mode == 4)
		{
			if (c != 10) mode = 3; /* the expected LF weren't there, so it's an error, once we returned here, but now we ignore it */
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

						mail_complete_add_header(mail, ms->line, ms->name_size, ms->line + ms->name_size, ms->contents_size);

						/* a previous call to this function saved a line */
						free(ms->line);
						ms->line = NULL;
						ms->name_size = ms->contents_size = 0;
					} else
					{
						/* no line has saved */
						mail_complete_add_header(mail,name_start,name_size,contents_start,contents_size);
					}

					name_start = contents_start = NULL;
					name_size = contents_size = 0;
				}

				if (c==10 || c==13)
				{
					mail->text_begin = ms->position+((c==13)?2:1);
					mail->text_len = mail->info->size - mail->text_begin;
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
				contents_start = buf;
				if (!isspace(c))
				{
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

/*****************************************************************************/

struct mail_complete *mail_find_compound_object(struct mail_complete *m, char *id)
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

/*****************************************************************************/

struct mail_complete *mail_find_content_type(struct mail_complete *m, char *type, char *subtype)
{
	int i;
	if (!mystricmp(m->content_type, type) && !mystricmp(m->content_subtype,subtype))
		return m;

	for (i=0;i < m->num_multiparts; i++)
	{
		struct mail_complete *rm = mail_find_content_type(m->multipart_array[i],type,subtype);
		if (rm) return rm;
	}

	return NULL;
}

/*****************************************************************************/

struct mail_complete *mail_find_initial(struct mail_complete *m)
{
	struct mail_complete *pref = NULL;
	int alter = 0;
	int i = 0;

	while(m)
	{
		if (m->multipart_array)
		{
			if (!mystricmp(m->content_type, "multipart") &&
				  !mystricmp(m->content_subtype, "alternative")) alter = 1;
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

/*****************************************************************************/

struct mail_complete *mail_get_root(struct mail_complete *m)
{
	while (m->parent_mail) m = m->parent_mail;
	return m;
}

/*****************************************************************************/

struct mail_complete *mail_get_next(struct mail_complete *m)
{
	if (!m->multipart_array)
	{
		struct mail_complete *parent = m->parent_mail;

		if (!parent) return NULL;

		while (parent->multipart_array)
		{
			int i;
			for (i=0;i < parent->num_multiparts;i++)
			{
				if (parent->multipart_array[i] == m)
				{
					i++;
					if (i == parent->num_multiparts)
					{
						m = parent;
						parent = m->parent_mail;
						if (!parent) return NULL;
						break;
					}
					m = parent->multipart_array[i];
					while (m->multipart_array) m = m->multipart_array[0];
					return m;
				}
			}
		}
	} else
	{
		while (m->multipart_array) m = m->multipart_array[0];
	}

	return m;
}

/**
 * Converts a number to base 18 character sign.
 *
 * @param val
 * @return
 */
static char get_char_18(int val)
{
	if (val >= 0 && val <= 9) return (char)('0' + val);
	return (char)('a' + val-10);
}

/* a table with all filename extensions */
/* they are mapped 1 to 1 */
static char status_extensions[] =
{
	0,'0','1','2','3','4','5','6','7','8',			/* 10 */
	'9','a','b','c','d','e','f','g','h','i',		/* 20 */
	'j','k','l','m','n','o','p','q','r','s',		/* 30 */
	't','u','v','w','x','y','z','!','$','-'			/* 40 */
};

/*****************************************************************************/

char *mail_get_new_name(int status)
{
	struct tm tm;
	unsigned short day_secs;
	unsigned int secs;
	short i;
	char dummy[8];
	char status_buf[4];
	char buf[64];

	secs = sm_get_current_seconds();
	sm_convert_seconds(secs, &tm);

	day_secs = (tm.tm_min * 60) + tm.tm_sec;
	dummy[4] = 0;
	dummy[3] = get_char_18(day_secs % 18);
	dummy[2] = get_char_18((day_secs / 18)%18);
	dummy[1] = get_char_18((day_secs / 18 / 18)%18);
	dummy[0] = get_char_18(day_secs / 18 / 18 / 18);

	if (status == MAIL_STATUS_UNREAD) status_buf[0] = 0;
	else {
		status_buf[0] = '.';
		status_buf[1] = status_extensions[status&0x1f];
		status_buf[2] = 0;
	}

	for (i=0;;i++)
	{
		FILE *fp;

		sm_snprintf(buf,sizeof(buf),"%02d%02d%04d%s.%03x%s",tm.tm_mday,tm.tm_mon,tm.tm_year,dummy,i,status_buf);

		if ((fp = fopen(buf, "r"))) fclose(fp);
		else break;
	}

	return mystrdup(buf);
}

/*****************************************************************************/

int mail_is_marked_as_deleted_by_filename(const char *fn)
{
	return fn[0] == 'd' || fn[0] =='D';
}

/*****************************************************************************/

int mail_is_marked_as_deleted(struct mail_info *mail)
{
	return mail_is_marked_as_deleted_by_filename(mail->filename);
}

/*****************************************************************************/

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

		if (status_new < 0 || status_new >= 1<<(MAIL_STATUS_BITS + MAIL_STATUS_FLAG_BITS))
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

/*****************************************************************************/

void mail_identify_status(struct mail_info *m)
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

	/* decode the status information. TODO: Use at least binary search! */
	for (i=0;i<sizeof(status_extensions);i++)
	{
	  if (suffix[1] == status_extensions[i])
		m->status = i;
	}
}

/*****************************************************************************/

struct mail_info *mail_info_create(mail_context *mc)
{
	struct mail_info *m;

	if ((m = (struct mail_info*)malloc(sizeof(struct mail_info))))
	{
		memset(m,0,sizeof(*m));
		m->context = mc;
	}
	return m;
}

/*****************************************************************************/

struct mail_complete *mail_complete_create(mail_context *mc)
{
	struct mail_complete *m;

	if ((m = (struct mail_complete*)malloc(sizeof(struct mail_complete))))
	{
		memset(m,0,sizeof(*m));
		if ((m->info = mail_info_create(mc)))
		{
			list_init(&m->content_parameter_list);
			list_init(&m->header_list);
			return m;
		}
		mail_complete_free(m);
	}
	return NULL;
}

/*****************************************************************************/

int mail_read_header_list_if_empty(struct mail_complete *m)
{
	char *buf;
	FILE *fh;

	if (list_first(&m->header_list)) return 1;
	if (!m->info->filename) return 0;
	if (!(fh = fopen(m->info->filename,"rb"))) return 0;

	if ((buf = (char*)malloc(2048)))
	{
		struct mail_scan ms;
		unsigned int bytes_read = 0;

		mail_scan_buffer_start(&ms,m);

		while ((bytes_read = fread(buf, 1, 2048, fh)))
		{
			if (!mail_scan_buffer(&ms,buf,bytes_read))
					break; /* we have enough */
		}

		mail_scan_buffer_end(&ms);
	}
	free(buf);
	fclose(fh);
	return 1;
}

/*****************************************************************************/

struct mail_complete *mail_complete_create_from_file(mail_context *mc, char *filename)
{
	struct mail_complete *m;
	FILE *fh;

	if ((m = mail_complete_create(mc)))
	{
		unsigned int size = ~0;

		if ((fh = fopen(filename,"rb")))
		{
			char *buf;

			size = myfsize(fh); /* get the size of the file */

			if (size) /* empty mails are no mails */
			{
				if ((buf = (char*)malloc(2048))) /* a small buffer to test the the new functions */
				{
					if ((m->info->filename = mystrdup(filename))) /* Not ANSI C */
					{
						struct mail_scan ms;
						unsigned int bytes_read = 0;

						m->info->size = size;
						mail_scan_buffer_start(&ms,m);

						while ((bytes_read = fread(buf, 1, 2048/*buf_size*/, fh)))
						{
							if (!mail_scan_buffer(&ms,buf,bytes_read))
								break; /* we have enough */
						}

						mail_scan_buffer_end(&ms);
						mail_process_headers(m);
						mail_identify_status(m->info);
					}
					free(buf);
				}
			}

			fclose(fh);
		} else
		{
			mail_complete_free(m);
			return NULL;
		}

		if (!size)
		{
			/* we remove files with a size of 0 */
/*			remove(filename);*/
			mail_complete_free(m);
			return NULL;
		}
	}
	return m;
}

/*****************************************************************************/

struct mail_info *mail_info_create_from_file(mail_context *mc, char *filename)
{
	struct mail_complete *mail;

	if ((mail = mail_complete_create_from_file(mc, filename)))
	{
		struct mail_info *info;

		info = mail->info;
		mail->info = NULL;
		mail_complete_free(mail);
		return info;
	}
	return NULL;
}

/*****************************************************************************/

struct mail_complete *mail_create_for(char *from, char *to_str_unexpanded, char *replyto, char *subject, char *body)
{
	struct mail_complete *mail;
	char *to_str;

	struct mailbox mb;
	memset(&mb,0,sizeof(mb));

	to_str = to_str_unexpanded?addressbook_get_expanded(to_str_unexpanded):NULL;

	if (to_str) parse_mailbox(to_str,&mb);
	if ((mail = mail_complete_create(NULL)))
	{
		string contents_str;

		if (!(string_initialize(&contents_str,100)))
		{
			mail_complete_free(mail);
			free(to_str);
			return NULL;
		}

		if (from)
		{
			mail_complete_add_header(mail,"From",4,from,strlen(from));
		}

		if (replyto)
		{
			struct address_list *list = address_list_create(replyto);
			if (list)
			{
				struct address *addr = address_list_first(list);
				if (addr)
					mail_complete_add_header(mail,"ReplyTo",7,addr->email,strlen(addr->email));
				address_list_free(list);
			}
		}

		/* TODO: write a function for this! */
		if (to_str)
		{
			struct address_list *list = address_list_create(to_str);
			if (list)
			{
				char *to_header;

				to_header = encode_address_field_utf8("To",list);
				address_list_free(list);

				if (to_header)
				{
					mail_complete_add_header(mail, "To", 2, to_header+4, strlen(to_header)-4);
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
				mail_complete_add_header(mail, "Subject", 7, subject_header+9, strlen(subject_header)-9);
				free(subject_header);
			}
		}

		if (body)
		{
			string_append(&contents_str,body);
		} else
		{
			struct phrase *phrase;
			phrase = phrase_find_best(to_str);
			if (mb.phrase)
			{
				if (phrase && phrase->write_welcome_repicient)
				{
					char *str = mail_create_string(phrase->write_welcome_repicient, NULL, mb.phrase, mb.addr_spec);
					if (str)
					{
						string_append(&contents_str,str);
						string_append(&contents_str,"\n");
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
						string_append(&contents_str,str);
						string_append(&contents_str,"\n");
						free(str);
					}
				}
			}

			if (phrase && phrase->write_closing)
			{
				char *str = mail_create_string(phrase->write_closing, NULL, NULL, NULL);
				if (str)
				{
					string_append(&contents_str,str);
					free(str);
				}
			}
		}

		mail->decoded_data = contents_str.str;
		mail->decoded_len = contents_str.len;
		mail_process_headers(mail);
	}
	free(to_str);
	free(mb.addr_spec);
	free(mb.phrase);
	return mail;
}

/**
 * Creates a subject line for a mail that are replies to the given mails.
 *
 * @param num number of mails in the mail_array.
 * @param mail_array the array of mails.
 * @return the subject line that needs to be freed free() when no longer in
 *  use.
 */
static char *mail_create_replied_subject_line(int num, struct mail_complete **mail_array)
{
	struct mail_info *mail = mail_array[0]->info;
	char *new_subject = (char*)malloc(mystrlen(mail->subject)+8);
	if (new_subject)
	{
		char *src = mail->subject;
		char *dest = new_subject;
		char c;
		int brackets = 0;
		int skip_spaces = 0;

		/* Add a Re: before the new subject */
		strcpy(dest,"Re: ");
		dest += 4;

		/* Copy the subject into a new buffer and filter all []'s and Re's */
		if (src)
		{
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
		}
		*dest = 0;
	}
	return new_subject;
}

/*****************************************************************************/

struct mail_complete *mail_create_reply(int num, struct mail_complete **mail_array)
{
	struct mail_complete *m = mail_complete_create(NULL);
	if (m)
	{
		struct mail_complete *mail = mail_array[0];
		struct mail_complete *text_mail;
		char *from = mail_find_header_contents(mail, "from");
		char *to = mail_find_header_contents(mail, "to");
		char *cc = mail_find_header_contents(mail, "cc");
		struct phrase *phrase = phrase_find_best(from);
		struct account *ac = account_find_by_from(to);

		int i;

		/* first set the sender account for later use */
		if (!ac)
		{
			/* if not found in to, check cc header */
			ac = account_find_by_from(cc);
		}
		if (ac)
		{
			mail_complete_add_header(m,"From", 4, ac->email,strlen(ac->email));
		}

		if (from)
		{
			struct address_list *alist;
			char *replyto = mail_find_header_contents(mail, "reply-to");
			int which_address = 1;

			if (replyto)
			{
				struct mailbox from_addr;
				struct mailbox replyto_addr;

				if (parse_mailbox(from, &from_addr))
				{
					if (parse_mailbox(replyto,&replyto_addr))
					{
						if (mystricmp(from_addr.addr_spec,replyto_addr.addr_spec))
						{
							which_address = sm_request(NULL,
												_("Sender address (From) is <%s>, but\n"
												"return address (Reply-To) is <%s>.\n"
												"Which address do you want to use?"),
												_("_From|*_Reply-To|_Both|_Cancel"),
												from_addr.addr_spec,replyto_addr.addr_spec);
						}
						if (replyto_addr.phrase)  free(replyto_addr.phrase);
						if (replyto_addr.addr_spec) free(replyto_addr.addr_spec);
					}

					if (from_addr.phrase)  free(from_addr.phrase);
					if (from_addr.addr_spec) free(from_addr.addr_spec);
				}

				if (!which_address) return NULL;
			}

			if (which_address == 2) from = replyto;

			alist = address_list_create(from);
			if (alist)
			{
				int mult_count=0;
				char *to_header;
				char *cc_header = NULL;
				struct address_list *mult_list_to = address_list_create(to);
				struct address_list *mult_list_cc = address_list_create(cc);

				if (which_address == 3)
					address_list_append(alist, replyto);

				if (mult_list_to)
				{
					/* remove the sender account, if found in to */
					if (ac) address_list_remove_by_mail(mult_list_to, ac->email);
					/* remove the replyto (could be by mailinglist where To == ReplyTo */
					if (replyto) address_list_remove_by_mail(mult_list_to, replyto);
					mult_count += list_length(&mult_list_to->list);
				}

				if (mult_list_cc)
				{
					/* remove the sender account, if found in cc */
					if (ac) address_list_remove_by_mail(mult_list_cc, ac->email);
					mult_count += list_length(&mult_list_cc->list);
				}

				if (mult_count > 0)
				{
					int take_mult = sm_request(NULL,
				                          _("This e-mail has multiple recipients. Should it be answered to all recipients?"),
				                          _("*_Yes|_No"));
					if (take_mult)
					{
						if (mult_list_to)
						{
							if (list_length(&mult_list_to->list) > 0)
							{
								struct mailbox *mb = (struct mailbox*)list_first(&mult_list_to->list);
								while (mb)
								{
									address_list_append_mailbox(alist,mb);
									mb = (struct mailbox*)node_next(&mb->node);
								}
							}
							address_list_free(mult_list_to);
						}
						if (mult_list_cc)
						{
							if (list_length(&mult_list_cc->list) > 0)
							{
								cc_header = encode_address_field_utf8("Cc", mult_list_cc);
							}
							address_list_free(mult_list_cc);
						}
					}
				}

				to_header = encode_address_field_utf8("To",alist);
				address_list_free(alist);

				if (to_header)
				{
					mail_complete_add_header(m, "To", 2, to_header+4, strlen(to_header)-4);
					free(to_header);
				}
				if (cc_header)
				{
					mail_complete_add_header(m, "Cc", 2, cc_header+4, strlen(cc_header)-4);
					free(cc_header);
				}
			}
		}

		m->info->subject = mail_create_replied_subject_line(num,mail_array);

		if (phrase)
		{
			/* add the welcome phrase */
			char *welcome_text = mail_create_string(phrase->reply_welcome,mail->info,NULL,NULL);
			m->decoded_data = stradd(m->decoded_data,welcome_text);
			free(welcome_text);
		}

		for (i=0;i<num;i++)
		{
			if ((text_mail = mail_find_content_type(mail_array[i], "text", "plain")))
			{
				char *replied_text;
				void *data;
				int data_len;

				m->decoded_data = stradd(m->decoded_data,"\n");

				if (phrase)
				{
					/* add the intro phrase */
					char *intro_text = mail_create_string(phrase->reply_intro,mail_array[i]->info,NULL,NULL);
					m->decoded_data = stradd(m->decoded_data,intro_text);
					m->decoded_data = stradd(m->decoded_data,"\n");
					free(intro_text);
				}
				/* decode the text */
				mail_decode(text_mail);

				mail_decoded_data(text_mail,&data,&data_len);

				if (data && data_len)
				{
					if ((replied_text = quote_text((char*)data,data_len)))
					{
						m->decoded_data = stradd(m->decoded_data,replied_text);
						free(replied_text);
					}
				}
			}
		}

		if (phrase)
		{
			/* add the closing phrase */
			char *closing_text = mail_create_string(phrase->reply_close, mail->info, NULL,NULL);
			m->decoded_data = stradd(m->decoded_data,"\n");
			m->decoded_data = stradd(m->decoded_data,closing_text);
			free(closing_text);
		}

		m->decoded_len = mystrlen(m->decoded_data);

		if (mail->info->message_id) m->info->message_reply_id = mystrdup(mail->info->message_id);
		mail_process_headers(m);
	}
	return m;
}

/*****************************************************************************/

struct mail_complete *mail_create_forward(int num, char **filename_array)
{
	struct mail_complete *m;
	int i;

	if (num < 1) return NULL;

	if ((m = mail_complete_create(NULL)))
	{
		struct mail_complete *forward;

		if (!(forward = mail_complete_create_from_file(NULL, filename_array[0])))
		{
			mail_complete_free(forward);
			return NULL;
		}

		mail_read_contents("",forward);

		/* Forward */
		if (forward->info->subject)
		{
			char *new_subject = (char*)malloc(strlen(forward->info->subject)+8);
			if (new_subject)
			{
				char *subject_header;

				char *src = forward->info->subject;
				char *dest = new_subject;
				char c;

				/* Add a Fwd: before the new subject */
				strcpy(dest,"Fwd: ");
				dest += 5;

				while ((c = *src))
				{
					*dest++= c;
					src++;
				}
				*dest = 0;

				if ((subject_header = encode_header_field("Subject",new_subject)))
				{
					mail_complete_add_header(m, "Subject", 7, subject_header+9, strlen(subject_header)-9);
					free(subject_header);
				}

				free(new_subject);
			}
		}

		if (user.config.write_forward_as_attachment)
		{
			/* Mails should be forwarded as attachments */
			mail_complete_free(forward);

			for (i=0;i<num;i++)
			{
				char *filename = filename_array[i];
				FILE *fh = fopen(filename,"r");
				if (fh)
				{
					struct mail_complete *new_part;
					int size;

					size = myfsize(fh);

					if ((new_part = mail_complete_create(NULL)))
					{
						if ((new_part->decoded_data = malloc(size)))
						{
							fread(new_part->decoded_data,1,size,fh); /* Ignore possibilities of failure for now */

							new_part->decoded_len = size;
							new_part->content_name = mystrdup("mail");
							new_part->content_type = mystrdup("message");
							new_part->content_subtype = mystrdup("rfc822");
							new_part->parent_mail = m;

							/* Skip the first part because it's reserved for the text part */
							if (!m->num_multiparts) m->num_multiparts++;
							if (m->num_multiparts >= m->multipart_allocated)
							{
								m->multipart_allocated += 5;
								m->multipart_array = realloc(m->multipart_array,sizeof(struct mail*)*m->multipart_allocated);
							}

							m->multipart_array[m->num_multiparts] = new_part;
							m->num_multiparts++;
						}
					}
				}
			}

			m->content_type = mystrdup("multipart");
			m->content_subtype = mystrdup("mixed");

			if ((m->multipart_array[0] = mail_complete_create(NULL)))
			{
				m->multipart_array[0]->decoded_data = mystrdup("");
				m->multipart_array[0]->decoded_len = mystrlen(m->multipart_array[0]->decoded_data);
				m->multipart_array[0]->parent_mail = m;
			}
		} else
		{
			int i = 0;
			char *modified_text = NULL;

			/* Go through all mails */
			while (1)
			{
				struct mail_complete *text_mail, *mail_iter;

				if ((text_mail = mail_find_content_type(forward, "text", "plain")))
				{
					void *data;
					int data_len;

					char *fwd_text;
					char *from = mail_find_header_contents(forward,"from");
					struct phrase *phrase;

					phrase = phrase_find_best(from);

					if (phrase)
					{
						/* add the welcome phrase */
						char *welcome_text = mail_create_string(phrase->forward_initial,forward->info, NULL, NULL);
						if (welcome_text && *welcome_text)
						{
							modified_text = stradd(modified_text,welcome_text);
							modified_text = stradd(modified_text,"\n");
						}
						free(welcome_text);
					}

					/* decode the text */
					mail_decode(text_mail);
					mail_decoded_data(text_mail,&data,&data_len);

					if ((fwd_text = mystrndup((char*)data,data_len)))
					{
						char *sig;
						if ((sig = strstr(fwd_text,"\n-- \n")))
						{
							modified_text = strnadd(modified_text,fwd_text,sig - fwd_text + 3);
							modified_text = stradd(modified_text,sig + 4);
						} else modified_text = stradd(modified_text,fwd_text);
						free(fwd_text);
					}

					if (phrase)
					{
						/* add the closing phrase */
						char *closing_text = mail_create_string(phrase->forward_finish, forward->info, NULL,NULL);
						modified_text = stradd(modified_text,closing_text);
						free(closing_text);
					}
				}

				/* Attach all attachments into the new mail, note old MIME structure is destroyed */
				mail_iter = forward;
				do
				{
					/* Ignore multiparts parts and the part which has been found above */
					if (!mail_iter->num_multiparts && mail_iter != text_mail)
					{
						struct mail_complete *new_part = mail_complete_create(NULL);
						if (new_part)
						{
							int attach_len;
							void *attach_data;

							mail_decode(mail_iter);
							mail_decoded_data(mail_iter,&attach_data,&attach_len);

							if ((new_part->decoded_data = malloc(attach_len)))
							{
								memcpy(new_part->decoded_data,attach_data,attach_len);
								new_part->decoded_len = attach_len;
								new_part->content_name = mystrdup(mail_iter->content_name);
								new_part->content_type = mystrdup(mail_iter->content_type);
								new_part->content_subtype = mystrdup(mail_iter->content_subtype);
								new_part->parent_mail = m;

								if (m->num_multiparts == m->multipart_allocated)
								{
									m->multipart_allocated += 5;
									m->multipart_array = realloc(m->multipart_array,sizeof(struct mail*)*m->multipart_allocated);
								}

								/* Skip the first part because it's reserved for the text part */
								if (!m->num_multiparts) m->num_multiparts++;
								m->multipart_array[m->num_multiparts] = new_part;
								m->num_multiparts++;
							}
						}
					}
				} while ((mail_iter = mail_get_next(mail_iter)));

				mail_complete_free(forward);
				i++;
				if (i>=num) break;
				if (!(forward = mail_complete_create_from_file(NULL, filename_array[i]))) break;
				mail_read_contents("",forward);
			} /* while (1) */

			/* So we have not only a single part */
			if (m->num_multiparts)
			{
				if ((m->multipart_array[0] = mail_complete_create(NULL)))
				{
					m->multipart_array[0]->decoded_data = modified_text;
					m->multipart_array[0]->decoded_len = mystrlen(modified_text);
				}
				m->content_type = mystrdup("multipart");
				m->content_subtype = mystrdup("mixed");
			} else
			{
				m->decoded_data = modified_text;
				m->decoded_len = mystrlen(modified_text);
			}
		} /* as attachments? */
		mail_process_headers(m);
	}
	return m;
}

/*****************************************************************************/

int extract_name_from_address(char *addr, char **dest_phrase, char **dest_addr, int *more_ptr)
{
	struct parse_address paddr;

	if (more_ptr) *more_ptr = 0;
	if (dest_phrase) *dest_phrase = NULL;
	if (dest_addr) *dest_addr = NULL;
	if (!addr) return 0;

	if ((addr = parse_address(addr,&paddr)))
	{
		struct mailbox *first_addr = (struct mailbox*)list_first(&paddr.mailbox_list);
		if (first_addr)
		{
			if (first_addr->phrase)
			{
				if (dest_phrase) *dest_phrase = mystrdup(first_addr->phrase);
				if (dest_addr) *dest_addr = mystrdup(first_addr->addr_spec);
			} else
			{
				if (first_addr->addr_spec)
				{
					if (dest_phrase)
					{
						struct addressbook_entry_new *entry = addressbook_find_entry_by_address(first_addr->addr_spec);
						if (entry) *dest_phrase = mystrdup(entry->realname);
					}
					if (dest_addr) *dest_addr = mystrdup(first_addr->addr_spec);
				}
			}
			if (node_next(&first_addr->node))
			{
				if (more_ptr) *more_ptr = 1;
			}
		}
		free_address(&paddr);
	}

	if (dest_addr)
	{
		if (!*dest_addr) *dest_addr = mystrdup(addr);
	}
	return 1;
}

/*****************************************************************************/

char *mail_get_from_address(struct mail_info *mail)
{
	char *buf = malloc(mystrlen(mail->from_phrase) + mystrlen(mail->from_addr)+10);
	if (buf)
	{
		if (mail->from_phrase) sprintf(buf,"%s <%s>", mail->from_phrase, mail->from_addr);
		else strcpy(buf, mail->from_addr?mail->from_addr:(utf8*)"");
	}
	return buf;
}

/*****************************************************************************/

static struct address *mail_get_first_address(struct address_list *al)
{
	struct address *a;

	if (!al) return NULL;
	if (!(a = address_list_first(al)))
		return NULL;
	return a;
}

/*****************************************************************************/

utf8 *mail_get_to_phrase(const struct mail_info *mail)
{
	struct address *a;

	if ((a = mail_get_first_address(mail->to_list)))
		return a->realname;
	return NULL;
}

/*****************************************************************************/

utf8 *mail_get_to_addr(const struct mail_info *mail)
{
	struct address *a;

	if ((a = mail_get_first_address(mail->to_list)))
		return a->email;
	return NULL;
}

/*****************************************************************************/

char *mail_get_to_address(struct mail_info *mail)
{
	char *buf;
	utf8 *phrase = mail_get_to_phrase(mail);
	utf8 *addr = mail_get_to_addr(mail);

	if (!addr) addr = "";

	if ((buf = malloc(mystrlen(phrase) + mystrlen(addr)+10)))
	{
		if (phrase) sprintf(buf,"%s <%s>",phrase,addr);
		else strcpy(buf,addr);
	}
	return buf;
}

/*****************************************************************************/

char **mail_info_get_recipient_addresses(struct mail_info *mail)
{
	char **a;
	struct address *addr;

	a = NULL;

	addr = address_list_first(mail->to_list);
	while (addr)
	{
		if (!array_contains_utf8(a, addr->email))
			a = array_add_string(a,addr->email);
		addr = address_next(addr);
	}

	addr = address_list_first(mail->cc_list);
	while (addr)
	{
		if (!array_contains_utf8(a, addr->email))
			a = array_add_string(a,addr->email);
		addr = address_next(addr);
	}

	return a;
}

/*****************************************************************************/

char *mail_get_replyto_address(struct mail_info *mail)
{
	return mystrdup(mail->reply_addr);
}

/*****************************************************************************/

void mail_info_set_excerpt(struct mail_info *mail, utf8 *excerpt)
{
	if (mail->excerpt) free(mail->excerpt);
	mail->excerpt = excerpt;
}

/**
 * Does RFC 2184 stuff.
 *
 * @param list
 */
static void rebuild_parameter_list(struct list *list)
{
	struct content_parameter *param;

	/* pass one - decode the values into utf8 */
	param = (struct content_parameter*)list_first(list);
	while (param)
	{
		int attribute_len = strlen(param->attribute);
		if (param->attribute[attribute_len-1] == '*')
		{
			char *tcharset = param->value;
			char *tlang;
			char *tencoded;

			if ((tlang = strchr(tcharset,'\'')))
			{
				char *charset;
				if ((charset = (char*)malloc(tlang - tcharset + 1)))
				{
					strncpy(charset, tcharset, tlang - tcharset);
					charset[tlang - tcharset] = 0;

					tlang++;
					if ((tencoded = strchr(tlang,'\'')))
					{
						char *newval;
						tencoded++;
						if ((newval = (char*)malloc(strlen(tencoded)+1)))
						{
							char c;
							char *dest = newval;

							/* decode characters */
							while ((c = *tencoded++))
							{
								if (c != '%') *dest++ = c;
								else
								{
									char num[3];
									num[0] = *tencoded++;
									num[1] = *tencoded++;
									num[2] = 0;
									*dest++ = strtoul(num,NULL,16);
								}
							}
							*dest = 0;

							/* now convert into utf-8 */
							if (!mystricmp(charset,"utf-8"))
							{
								free(param->value);
								param->value = newval;
							} else
							{
								char *utf8 = utf8create(newval,charset);
								if (utf8)
								{
									free(param->value);
									free(newval);
									param->value = utf8;
								}
							}
						}
					}
					free(charset);
				}
			}
			param->attribute[attribute_len-1] =  0;
		}

		param = (struct content_parameter*)node_next(&param->node);
	}

	/* a second pass should follow which merges the lines */
}

/*****************************************************************************/

int mail_process_headers(struct mail_complete *mail)
{
	struct header *header = (struct header*)list_first(&mail->header_list);
	struct header *header_next;

	for (; header; header = header_next)
	{
		char *buf = header->contents;
		const struct header_entry *header_entry;
		char lowercase_header[MAX_WORD_LENGTH+1];
		int i;

		header_next = (struct header*)node_next(&header->node);

		/* Convert to lower case for now. TODO: Better do this when the hash
		 * is calculated (usually takes fewer lookups) and use a modified
		 * strcmp() function (we don't need to convert the string in the table
		 * as it is always lowercase)
		 */
		for (i=0; i < sizeof(lowercase_header); i++)
		{
			char c = header->name[i];
			if (c >= 65 && c <= 90) c = c | 0x20;
			lowercase_header[i] = c;
			if (!c) break;
		}
		if (i == sizeof(lowercase_header))
			continue;

		header_entry = mail_lookup_header(lowercase_header, i);
		if (!header_entry)
		{
			/* Skip headers we don't understand */
			continue;
		}

		switch (header_entry->id)
		{
			case HEADER_DATE:
			{
				/* syntax should be checked before! */
				int day,month,year,hour,min,sec,gmt;
				if ((parse_date(buf,&day,&month,&year,&hour,&min,&sec,&gmt)))
				{
					mail->info->seconds = sm_get_seconds(day,month,year) + (hour*60+min)*60 + sec - (gmt - sm_get_gmt_offset())*60;
				} else mail->info->seconds = 0;
			}
			break;

			case HEADER_FROM:
			{
				extract_name_from_address(buf,(char**)&mail->info->from_phrase,(char**)&mail->info->from_addr,NULL);
				/* for display optimization */
				if (isascii7(mail->info->from_phrase)) mail->info->flags |= MAIL_FLAGS_FROM_ASCII7;
				if (isascii7(mail->info->from_addr)) mail->info->flags |= MAIL_FLAGS_FROM_ADDR_ASCII7;
			}
			break;

			case HEADER_REPLY_TO:
			{
				extract_name_from_address(buf,NULL,&mail->info->reply_addr,NULL);
				if (isascii7(mail->info->reply_addr)) mail->info->flags |= MAIL_FLAGS_REPLYTO_ADDR_ASCII7;
			}
			break;

			case HEADER_TO:
			{
				/* TODO: We should query the address book for the realname -> addr correspondence */
				if ((mail->info->to_list = address_list_create(buf)))
				{
					if (address_list_length(mail->info->to_list) > 1)
						mail->info->flags |= MAIL_FLAGS_GROUP;
				}

				/* for display optimization */
				if (isascii7(mail_get_to_phrase(mail->info))) mail->info->flags |= MAIL_FLAGS_TO_ASCII7;
				if (isascii7(mail_get_to_addr(mail->info))) mail->info->flags |= MAIL_FLAGS_TO_ADDR_ASCII7;
			}
			break;

			case HEADER_CC:
			{
				mail->info->cc_list = address_list_create(buf);
				mail->info->flags |= MAIL_FLAGS_GROUP;
			}
			break;

			case HEADER_SUBJECT:
			{
				if (buf) parse_text_string(buf,&mail->info->subject);
				else mail->info->subject = mystrdup("");

				/* for display optimization */
				if (mail->info->subject && isascii7(mail->info->subject)) mail->info->flags |= MAIL_FLAGS_SUBJECT_ASCII7;
			}
			break;

			case HEADER_RECEIVED:
			{
				if ((buf = strchr(buf,';')))
				{
					int day,month,year,hour,min,sec,gmt;
					unsigned int new_recv;
					buf++;
					if ((parse_date(buf,&day,&month,&year,&hour,&min,&sec,&gmt)))
					{
						new_recv = sm_get_seconds(day,month,year) + (hour*60+min)*60 + sec - (gmt - sm_get_gmt_offset())*60;
						if (new_recv > mail->info->received) mail->info->received = new_recv;
					}
				}
			}
			break;

			case HEADER_MIME_VERSION:
			{
				int version;
				int revision;

				version = atoi(buf);
				while (isdigit((unsigned char)*buf)) buf++;
				revision = atoi(buf);

				mail->mime = (version << 16) | revision;
			}
			break;

			case HEADER_CONTENT_DISPOSITION:
			{
				/* Check the Content-Disposition of the whole mail */

				if (!mystrnicmp(header->contents,"inline",6))
					mail->content_inline = 1;

				if (!mail->content_name)
				{
					struct list parameter_list;
					char *param;

					char *fn = mystristr(buf,"filename=");
					if (fn)
					{
						fn += sizeof("filename=")-1;
						parse_value(fn,&mail->content_name);
					}

					list_init(&parameter_list);
					if ((param = mystristr(buf,";")))
					{
						while (1)
						{
							if (*param++ == ';')
							{
								struct content_parameter *new_param;
								struct parse_parameter dest;
								unsigned char c;

								/* Skip spaces */
								while ((c = *param))
								{
									if (!isspace(c)) break;
									param++;
								}

								if (!(param = parse_parameter(param, &dest)))
									break;

								if ((new_param = (struct content_parameter *)malloc(sizeof(struct content_parameter))))
								{
									new_param->attribute = dest.attribute;
									new_param->value = dest.value;
									list_insert_tail(&parameter_list,&new_param->node);
								} else break;
							} else break;
						}
					}
					rebuild_parameter_list(&parameter_list);

					{
						struct content_parameter *param = (struct content_parameter*)list_first(&parameter_list);
						while (param)
						{
							if (!mystricmp("filename",param->attribute))
							{
								/* Free memory that has been previously allocated */
								free(mail->content_name);
								mail->content_name = mystrdup(param->value);
								break;
							}
							param = (struct content_parameter *)node_next(&param->node);
						}

						while ((param = (struct content_parameter*)list_remove_tail(&parameter_list)))
						{
							free(param->attribute);
							free(param->value);
							free(param);
						}
					}
				}
			}
			break;

			case HEADER_CONTENT_TYPE:
			{
				/* content  := "Content-Type"  ":" type "/" subtype  *(";" parameter) */
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

										if (!mystricmp(dest.attribute,"charset"))
										{
											if (dest.attribute) free(dest.attribute);
											mail->content_charset = dest.value;
										} else
										{
											if (!mystricmp(dest.attribute,"name"))
											{
												if (dest.attribute) free(dest.attribute);
												if (!mail->content_name) mail->content_name = dest.value;
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
										}
									} else break;
								}
							}
						}
					}
				}
			}
			break;

			case HEADER_CONTENT_ID:
			{
				if (*buf == '<')
				{
					buf++;
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
				} else
				{
					/* for the non rfc conform content-id's */
					if ((mail->content_id = malloc(strlen(buf)+1)))
						strcpy(mail->content_id,buf);
				}
			}
			break;

			case HEADER_MESSAGE_ID:
			{
				if (*buf++ == '<')
					parse_addr_spec(buf,&mail->info->message_id);
			}
			break;

			case HEADER_IN_REPLY_TO:
			{
				if (*buf++ == '<')
					parse_addr_spec(buf,&mail->info->message_reply_id);
			}
			break;

			case HEADER_CONTENT_TRANSFER_ENCODING:
			{
				mail->content_transfer_encoding = mystrdup(buf);
			}
			break;

			case HEADER_CONTENT_DESCRIPTION:
			{
				parse_text_string(buf,&mail->content_description);
			}
			break;

			case HEADER_IMPORTANCE:
			{
				if (!mystricmp(buf,"high")) mail->info->flags |= MAIL_FLAGS_IMPORTANT;
			}
			break;

			case HEADER_X_PRIORITY:
			{
				/* check for High or Highest (Thunderbird compatibility) */
				if (mystristr(buf,"high")) mail->info->flags |= MAIL_FLAGS_IMPORTANT;
			}
			break;

			case HEADER_X_SIMPLEMAIL_POP3:
			{
				mail->info->pop3_server.str = mystrdup(buf);
			}
			break;

			case HEADER_X_SIMPLEMAIL_PARTIAL:
			{
				if (!mystricmp(buf,"yes")) mail->info->flags |= MAIL_FLAGS_PARTIAL;
			}
			break;
		}
	}

	if (!mail->info->to_list && !mail->info->cc_list)
		mail->info->flags |= MAIL_FLAGS_NORCPT;

	if (!mail->content_type || !mail->content_subtype)
	{
		mail->content_type = mystrdup("text");
		mail->content_subtype = mystrdup("plain");
	}

	if (!mail->content_transfer_encoding)
		mail->content_transfer_encoding = mystrdup("7bit");

	if (!mystricmp(mail->content_type, "multipart"))
	{
		mail->info->flags |= MAIL_FLAGS_ATTACH;
		if (!mystricmp(mail->content_subtype,"encrypted")) mail->info->flags |= MAIL_FLAGS_CRYPT;
		else if (!mystricmp(mail->content_subtype,"signed")) mail->info->flags |= MAIL_FLAGS_SIGNED;
	}

	if (!mystricmp(mail->content_type,"application") &&
		 (!mystricmp(mail->content_subtype, "x-pkcs7-mime") || !mystricmp(mail->content_subtype, "pkcs7-mime")))
	{
		mail->info->flags |= MAIL_FLAGS_CRYPT;
	}


	/* if no filename is given set one */
	if (!mail->content_name)
	{
		mail->content_name = mystrdup(mail->content_type);
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

/**
 * Looks for an content parameter and returns the value. If the parameter
 * is not contained, NULL is returned.
 *
 * @param mail
 * @param attribute
 * @return
 */
static char *mail_find_content_parameter_value(struct mail_complete *mail, char *attribute)
{
	struct content_parameter *param = (struct content_parameter*)list_first(&mail->content_parameter_list);

	while (param)
	{
		if (!mystricmp(attribute,param->attribute)) return param->value;
		param = (struct content_parameter *)node_next(&param->node);
	}

	return NULL;
}

/**
 * Decrypts a mail if it is encrypted
 *
 * @param mail
 */
static void mail_decrypt(struct mail_complete *mail)
{
	if (!mystricmp(mail->content_subtype,"encrypted"))
	{
		if (mail->num_multiparts==2 &&
				!mystricmp(mail_find_content_parameter_value(mail,"protocol"),"application/pgp-encrypted") &&
			  !mystricmp(mail->multipart_array[0]->content_type,"application") &&
			  !mystricmp(mail->multipart_array[0]->content_subtype,"pgp-encrypted"))
		{
			static char *saved_passphrase;
			char *env_passphrase = sm_getenv("PGPPASS");
			int keep_env = 0;

			struct mail_complete *encrypt_mail = mail->multipart_array[1];

			if (env_passphrase)
			{
				free(saved_passphrase);
				saved_passphrase = mystrdup(env_passphrase);
				keep_env = 1;
			}

			if (!saved_passphrase)
				saved_passphrase = sm_request_string(NULL,_("This mail is encrypted. Please enter your passphrase now"), "", 1);

			if (saved_passphrase)
			{
				FILE *fh;
				char tmpname[L_tmpnam+10];

				tmpnam(tmpname);
				strcat(tmpname,".asc");

				if (!keep_env)
					sm_setenv("PGPPASS",saved_passphrase);

				if ((fh=fopen(tmpname,"wb")))
				{
					char cmd_buf[256];
					int rc;
					fwrite(&encrypt_mail->text[encrypt_mail->text_begin],1,encrypt_mail->text_len,fh);
					fclose(fh);
					sprintf(cmd_buf,"%s +bat +f",tmpname);

					rc = pgp_operate(cmd_buf, NULL);
					remove(tmpname);

					if (!rc || rc==1)
					{
						tmpname[strlen(tmpname)-4]=0;
						if ((fh = fopen(tmpname,"rb")))
						{
							char *newtext;
							int size;
							fseek(fh,0,SEEK_END);
							size = ftell(fh);
							fseek(fh,0,SEEK_SET);

							if ((newtext = (char*)malloc(size)))
							{
								struct mail_complete *new_mail;
								int i;

								/* Read the decrypted data */
								fread(newtext,1,size,fh);

								/* There must be at least two mails allocated */
								for (i=0;i<mail->num_multiparts;i++)
									mail_complete_free(mail->multipart_array[i]);

								if ((new_mail = mail->multipart_array[0] = mail_complete_create(NULL)))
								{
									struct mail_scan ms;

									mail->num_multiparts = 1;
									mail->multipart_array[0] = new_mail;

									new_mail->info->size = size;

									mail_scan_buffer_start(&ms,new_mail);
									mail_scan_buffer(&ms, newtext, size);
									mail_scan_buffer_end(&ms);
									mail_process_headers(new_mail);

									new_mail->text = new_mail->extra_text = newtext;
									/* text_len and text_begin is set by mail_scan_buffer */

									mail_read_structure(new_mail); /* the recursion */
									new_mail->parent_mail = mail;
								} else
								{
									free(mail->multipart_array);
									mail->multipart_array = NULL;
									mail->num_multiparts = 0;
								}
							}
							fclose(fh);
						} else
						{
							free(saved_passphrase);
							saved_passphrase = NULL;
						}

						remove(tmpname);
					} else
					{
						sm_request(NULL,
							_("Decrypting failed! Eighter because the passphrase was incorrect\n"
							"or because the encryption is not yet supported by SimpleMail (only PGP 2.6.x supported for now)!"),
							_("Ok"));

						free(saved_passphrase);
						saved_passphrase = NULL;
					}
				}

				if (!keep_env)
					sm_unsetenv("PGPPASS");
			}
		} else
		{
			mail->decoded_data = mystrdup(_("Unsupported encryption"));
			mail->decoded_len = strlen(mail->decoded_data);
			if (mail->multipart_array)
			{
				mail_complete_free(mail->multipart_array[1]);
				mail_complete_free(mail->multipart_array[0]);
				free(mail->multipart_array);
				mail->multipart_array = NULL;
			}
			mail->num_multiparts = 0;
		}
	}
}

/**
 * Resolve the smime stuff.
 *
 * @param mail
 */
static void mail_resolve_smime(struct mail_complete *mail)
{
	if (!mystricmp(mail->content_type,"application") &&
		 (!mystricmp(mail->content_subtype, "x-pkcs7-mime") || !mystricmp(mail->content_subtype, "pkcs7-mime")))
	{
		void *data;
		int data_len;

		char *decoded_data;
		int decoded_data_len;

		struct mail_complete *new_mail;
		struct mail_scan ms;

		if (!(mail->multipart_array = (struct mail_complete**)malloc(sizeof(struct mail_complete*)*1))) return;
		if (!(new_mail = mail->multipart_array[0] = mail_complete_create(mail->info->context))) return;
		mail->multipart_allocated = mail->num_multiparts = 1;

		mail_decode(mail);
		mail_decoded_data(mail,&data,&data_len);

		if (pkcs7_decode((char*)data, data_len, &decoded_data, &decoded_data_len))
		{
			new_mail->info->size = decoded_data_len;
			new_mail->text = decoded_data;
			new_mail->extra_text = decoded_data;

			mail_scan_buffer_start(&ms,new_mail);
			mail_scan_buffer(&ms, (char*)decoded_data,decoded_data_len);
			mail_scan_buffer_end(&ms);
			mail_process_headers(new_mail);

			mail_read_structure(new_mail);

			/* so new_mail->text will not be freed */
			new_mail->parent_mail = mail;
		} else
		{
			free(mail->multipart_array);
			mail->multipart_array = NULL;
			mail->multipart_allocated = mail->num_multiparts = 0;
			mail_complete_free(new_mail);
		}
	}
}

/**
 * Recursively, read the structure of the given mail.
 *
 * @param mail the mail of which the structure should be read.
 * @return 0 on failure, otherwise some other value.
 */
static int mail_read_structure(struct mail_complete *mail)
{
	if (!mystricmp(mail->content_type,"multipart"))
	{
		/* message is a multipart message */
		char *boundary = mail_find_content_parameter_value(mail,"boundary");

		if (boundary)
		{
			char *search_str = strdupcat("--",boundary);
			if (search_str)
			{
				char *text = mail->text + mail->text_begin;
				char *buf = mail->text + mail->text_begin;

				if ((buf = strstr(buf,search_str)))
				{
					buf += strlen(search_str);
				}

				if (buf)
				{
					/* Skip line endings */
					if (*buf == '\r') buf++;
					if (*buf == '\n') buf++;

					while (1)
					{
						struct mail_complete *new_mail;
						char *end_part = strstr(buf,search_str);
						int skip = 0;
						if (!end_part) break;

						if (end_part - 2 >= text)
						{
							/* The CRLF normally belongs to the boundary tag
							 * but we also accept LFs only. But these are mandatory.
							 */
							if (end_part[-1] == '\n')
							{
								end_part--;
								skip++;
							}
							else break;
							if (end_part[-1] == '\r')
							{
								end_part--;
								skip++;
							}
						}

						if ((new_mail = mail_complete_create(NULL)))
						{
							struct mail_scan ms;

							new_mail->info->size = end_part - buf;

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

						buf = end_part + strlen(search_str) + skip;

						/* Skip line endings */
						if (*buf == '\r') buf++;
						if (*buf == '\n') buf++;
					}
				}
				free(search_str);
			}

			mail_decrypt(mail);
		}
	} else if (!mystricmp(mail->content_type,"message") && !mystricmp(mail->content_subtype,"rfc822"))
	{
		void *data;
		int data_len;

		struct mail_complete *new_mail;
		struct mail_scan ms;

		if (!(mail->multipart_array = malloc(sizeof(struct mail*)))) return 0;
		if (!(new_mail = mail->multipart_array[0] = mail_complete_create(NULL))) return 0;
		mail->multipart_allocated = mail->num_multiparts = 1;

		/* Decode the mail */
		mail_decode(mail);
		mail_decoded_data(mail,&data,&data_len);

		/* Must be set befor buffer functions! */
		new_mail->info->size = data_len;
		new_mail->text = (char*)data;
		/* text_begin and text_len will be set by buffer functions */

		mail_scan_buffer_start(&ms,new_mail);
		mail_scan_buffer(&ms, (char*)data,data_len);
		mail_scan_buffer_end(&ms);
		mail_process_headers(new_mail);

		mail_read_structure(new_mail);

		/* so new_mail->text will not be freed */
		new_mail->parent_mail = mail;
	} else mail_resolve_smime(mail);
	return 1;
}

/*****************************************************************************/

void mail_read_contents(char *folder, struct mail_complete *mail)
{
	char path[256];
	FILE *fp;

	if (folder)
	{
		getcwd(path, sizeof(path));
		if(chdir(folder) == -1) return;
	}

	if ((fp = fopen(mail->info->filename,"rb")))
	{
		if ((mail->text = malloc(mail->info->size+1)))
		{
			fread(mail->text,1,mail->info->size,fp);
			mail->text[mail->info->size]=0;
		}

		mail_read_structure(mail);

		fclose(fp);
	}

	if (folder) chdir(path);
}

/*****************************************************************************/

void mail_decoded_data(struct mail_complete *mail, void **decoded_data_ptr, int *decoded_data_len_ptr)
{
	mail_decode(mail);

	if (mail->decoded_data)
	{
		*decoded_data_ptr = mail->decoded_data;
		*decoded_data_len_ptr = mail->decoded_len;
	} else
	{
		if (mail->text)
		{
			*decoded_data_ptr = mail->text + mail->text_begin;
			*decoded_data_len_ptr = mail->text_len;
		} else
		{
			*decoded_data_ptr = 0;
			*decoded_data_len_ptr = 0;
		}
	}
}

/*****************************************************************************/

void mail_decode(struct mail_complete *mail)
{
	/* If mail is already decoded do nothing */
	if (mail->decoded_data) return;

	/* If no text is available return */
	if (!mail->text) return;

	if (!mystricmp(mail->content_transfer_encoding,"base64"))
	{
		unsigned int decoded_len = (unsigned int)-1;
		if ((mail->decoded_data = decode_base64((unsigned char*)(mail->text + mail->text_begin), mail->text_len,&decoded_len)))
			mail->decoded_len = decoded_len;
	} else if (!mystricmp(mail->content_transfer_encoding,"quoted-printable"))
	{
		unsigned int decoded_len = (unsigned int)-1;
		if ((mail->decoded_data = decode_quoted_printable((unsigned char*)(mail->text + mail->text_begin), mail->text_len,&decoded_len,0)))
			mail->decoded_len = decoded_len;
	}

	if (!mystricmp(mail->content_type,"text") && mystricmp(mail->content_subtype, "html"))
	{
		/* It's a text mail so convert it to utf-8 expect if it is already utf8 */
		if (mystricmp(mail->content_charset,"utf-8"))
		{
			if (mail->decoded_data)
			{
			  	char *new_data = utf8create_len(mail->decoded_data,mail->content_charset,mail->decoded_len);
				free(mail->decoded_data);
				mail->decoded_data = new_data;
				mail->decoded_len = mystrlen(new_data);
			} else
			{
				mail->decoded_data = utf8create_len(mail->text + mail->text_begin,mail->content_charset,mail->text_len);
				mail->decoded_len = mystrlen(mail->decoded_data);
			}
		}
	}
}

/*****************************************************************************/

void *mail_decode_bytes(struct mail_complete *mail, unsigned int *len_ptr)
{
	void *decoded = NULL;
	if (!mystricmp(mail->content_transfer_encoding,"base64"))
	{
		decoded = decode_base64((unsigned char*)(mail->text + mail->text_begin), mail->text_len,len_ptr);
	} else if (!mystricmp(mail->content_transfer_encoding,"quoted-printable"))
	{
		decoded = decode_quoted_printable((unsigned char*)(mail->text + mail->text_begin), mail->text_len,len_ptr,0);
	}
	return decoded;
}

/*****************************************************************************/

void mail_info_free(struct mail_info *info)
{
	if (!info) return;

	/* don't free anything, if there still other references of this mail */
	if (info->reference_count)
	{
		info->tflags |= MAIL_TFLAGS_TO_BE_FREED;
		return;
	}

	free(info->subject);
	free(info->from_phrase);
	free(info->from_addr);
	if (info->to_list) address_list_free(info->to_list);
	if (info->cc_list) address_list_free(info->cc_list);
	free(info->reply_addr);
	free(info->pop3_server.str);
	free(info->message_id);
	free(info->message_reply_id);
	free(info->filename);
	free(info->excerpt);
	free(info);
}

/*****************************************************************************/

void mail_complete_free(struct mail_complete *mail)
{
	struct header *hdr;
	struct content_parameter *cp;
	int i;

	if (!mail) return;

	if (mail->html_header) free(mail->html_header);

	while ((hdr = (struct header *)list_remove_tail(&mail->header_list)))
	{
		free(hdr->name);
		free(hdr->contents);
		free(hdr);
	}

	while ((cp = (struct content_parameter*)list_remove_tail(&mail->content_parameter_list)))
	{
		free(cp->attribute);
		free(cp->value);
		free(cp);
	}

	for (i=0;i<mail->num_multiparts;i++)
	{
		mail_complete_free(mail->multipart_array[i]); /* recursion */
	}
	free(mail->multipart_array);

	free(mail->extra_text);

	free(mail->content_description);
	free(mail->content_charset);
	free(mail->content_type);
	free(mail->content_subtype);
	free(mail->content_id);
	free(mail->content_name);
	free(mail->content_transfer_encoding);

	free(mail->decoded_data);

	/* TODO: Check if mail->text must be freed even if mail->info == NULL */
	if (mail->info && mail->info->filename && mail->text) free(mail->text);

	mail_info_free(mail->info);
	free(mail);
}

/*****************************************************************************/

void mail_reference(struct mail_info *mail)
{
	mail->reference_count++;
	SM_DEBUGF(20,("Increased reference count of mail %p to %ld\n",mail,mail->reference_count));
}

/*****************************************************************************/

void mail_dereference(struct mail_info *mail)
{
	if (mail->reference_count <= 0)
	{
		SM_DEBUGF(1,("Wrong reference count!\n"));
		return;
	}
	mail->reference_count--;
	SM_DEBUGF(20,("Decreased reference count of mail %p to %ld\n",mail,mail->reference_count));
	if (mail->tflags & MAIL_TFLAGS_TO_BE_FREED) mail_info_free(mail);
}

/*****************************************************************************/

struct header *mail_find_header(struct mail_complete *mail, char *name)
{
	struct header *header = (struct header*)list_first(&mail->header_list);

	while (header)
	{
		if (!mystricmp(header->name, name)) return header;
		header = (struct header*)node_next(&header->node);
	}
	return NULL;
}

/*****************************************************************************/

char *mail_find_header_contents(struct mail_complete *mail, char *name)
{
	struct header *header = mail_find_header(mail,name);
	if (header) return header->contents;
	return NULL;
}

/*****************************************************************************/

void composed_mail_init(struct composed_mail *mail)
{
	memset(mail, 0, sizeof(struct composed_mail));
	mail->importance = 1;  /* normal is default */
	list_init(&mail->list);
}

/*****************************************************************************/

void composed_mail_add(struct composed_mail *parent, struct composed_mail *mail)
{
	list_insert_tail(&parent->list, &mail->node);
}

/**
 * Writes out the a selected address header field correctly encoded.
 *
 * @param fp the destination file.
 * @param header_name the name of the field
 * @param header_contents the contents of the field.
 * @return failure (0), or success.
 */
static int mail_compose_write_addr_header(FILE *fp, char *header_name, char *header_contents)
{
	int rc = 0;
	struct address_list *list = address_list_create(header_contents);

	if (list)
	{
		char *hc = encode_address_field_utf8(header_name, list);
		if (hc)
		{
			if (fprintf(fp,"%s\n",hc)>=0) rc = 1;
			free(hc);
		}
		address_list_free(list);
	}
	return rc;
}

/**
 * Write to the given file all headers.
 *
 * @param fp the file to the headers are added.
 * @param new_mail the mail from which the headers are taken.
 */
static int mail_compose_write_headers(FILE *fp, struct composed_mail *new_mail)
{
	char *subject;

	if (!mail_compose_write_addr_header(fp,"From",new_mail->from))
		return 0;

	if (new_mail->to && *new_mail->to)
		if (!mail_compose_write_addr_header(fp,"To",new_mail->to))
			return 0;

	if (new_mail->replyto && *new_mail->replyto)
		if (!mail_compose_write_addr_header(fp,"Reply-To",new_mail->replyto))
			return 0;

	if (new_mail->cc && *new_mail->cc)
		if (!mail_compose_write_addr_header(fp,"CC",new_mail->cc))
			return 0;

	if (new_mail->bcc && *new_mail->bcc)
		if (!mail_compose_write_addr_header(fp,"Bcc",new_mail->bcc))
			return 0;

	if ((subject = encode_header_field_utf8("Subject",new_mail->subject)))
	{
		unsigned secs;
		struct tm d;
		int offset = sm_get_gmt_offset();

		static const char *mon_str[] =
		{
			"Jan","Feb","Mar","Apr","May","Jun",
			"Jul","Aug","Sep","Oct","Nov","Dec"
		};

		fputs(subject,fp);
		free(subject);
		fprintf(fp,"X-Mailer: SimpleMail %d.%d (%s) E-Mail Client (c) 2000-2015 by Hynek Schlawack and Sebastian Bauer\n",VERSION,REVISION,SM_OPERATINGSYSTEM);

		secs = sm_get_current_seconds();
		sm_convert_seconds(secs,&d);

		fprintf(fp,"Date: %02d %s %4d %02d:%02d:%02d %+03d%02d\n",d.tm_mday,mon_str[d.tm_mon],d.tm_year + 1900,d.tm_hour,d.tm_min,d.tm_sec,offset/60,offset%60);
	}

	if (new_mail->importance != 1)
	{
		/* only add importance if not set to normal */
		fputs("Importance: ", fp);
		if (new_mail->importance == 0) fputs("low\n", fp);
		else fputs("high\n", fp);
	}

	if (new_mail->reply_message_id)
	{
		fprintf(fp,"In-Reply-To: <%s>\n",new_mail->reply_message_id);
	}
	return 1;
}

/**
 * Returns a unique boundary id string.
 *
 * @param fp a file that maybe used to ensure the uniqueness.
 * @return the boundary string that shall be freed when no longer in use.
 */
static char *get_boundary_id(FILE *fp)
{
	char boundary[64];
	sm_snprintf(boundary, sizeof(boundary), "--==bound%x%lx----", rand(), ftell(fp));
	return mystrdup(boundary);
}

/**
 * Write the contents of a file encrypted to the fp.
 *
 * @param fp the file to which the encrypted data is writtten to.
 * @param new_mail the to-be-composed mail, which, among other things contain
 *  the recipients of the mail
 * @param ofh_name the file that contents should be encryped.
 * @return
 */
static int mail_write_encrypted(FILE *fp, struct composed_mail *new_mail, char *ofh_name)
{
	char *boundary;
	int rc = 1;

	if ((boundary = get_boundary_id(fp)))
	{
		struct address_list *tolist = address_list_create(new_mail->to);
		char *encrypted_name = malloc(L_tmpnam+1);
		char *id_name = malloc(L_tmpnam+1);
		char *cmd = malloc(2*L_tmpnam+300);

		if (cmd && encrypted_name && id_name && tolist)
		{
			struct address *addr;
			int sys_rc;
			FILE *id_fh;

			tmpnam(id_name);
			tmpnam(encrypted_name);

			if ((id_fh = fopen(id_name,"wb")))
			{
				addr = address_list_first(tolist);
				while (addr)
				{
					struct addressbook_entry_new *entry = addressbook_find_entry_by_address(addr->email);
					if (!entry || !entry->pgpid || !(*entry->pgpid))
					{
						char *pgpid;
						char *text = malloc(512);
						if (text)
						{
							sm_snprintf(text,512,_("Please select a key for %s <%s>"),addr->realname,addr->email);
						}
						pgpid = sm_request_pgp_id(text);
						free(text);
						if (pgpid)
						{
							fprintf(id_fh,"%s\n",pgpid);
							free(pgpid);
						} else
						{
							rc = 0;
							break;
						}
					}  else fprintf(id_fh,"%s\n",entry->pgpid);
					addr = address_next(addr);
				}
				fclose(id_fh);
			}

			if (rc)
			{
				fprintf(fp,"MIME-Version: 1.0\n");
				fprintf(fp,"Content-Type: multipart/encrypted; boundary=\"%s\";\n protocol=\"application/pgp-encrypted\"\n", boundary);
				fprintf(fp,"\n");
				fprintf(fp, "--%s\n",boundary);
				fputs(pgp_text,fp);
				fprintf(fp, "\n--%s\nContent-Type: application/octet-stream\n\n",boundary);
				sprintf(cmd, "-ea \"%s\" -@ \"%s\" -o \"%s\" +bat", ofh_name, id_name, encrypted_name);

				sys_rc = pgp_operate(cmd,NULL);

				if (!sys_rc)
				{
					char *buf = malloc(512);
					if (buf)
					{
						FILE *encrypted_fh = fopen(encrypted_name,"rb");
						if (encrypted_fh)
						{
							while (fgets(buf,512,encrypted_fh))
							{
								fputs(buf,fp);
							}
							fclose(encrypted_fh);
						}
						free(buf);
					}
				} else rc = 0;

				fprintf(fp, "\n--%s--\n",boundary);
			}
			remove(encrypted_name);
			remove(id_name);
		} else rc = 0;
		free(cmd);
		free(encrypted_name);
		free(id_name);
		if (tolist) address_list_free(tolist);
		free(boundary);
	} else rc = 0;
	return rc;
}

/**
 * Writes all the attachments into the given mail following several RFC.
 *
 * @param fp the already opened mail file that is appended by the attachments.
 * @param new_mail the structure describing the to-be-composed mail.
 * @return 0 on failure, all other values indicate success.
 *
 * @note recursion is used.
 */
static int mail_compose_write(FILE *fp, struct composed_mail *new_mail)
{
	struct composed_mail *cmail;
	int rc = 1;

	FILE *ofh = fp;
	char *ofh_name = NULL;

	if (new_mail->from) /* check is necessary to distinguish the root mail */
	{
		if (!(mail_compose_write_headers(fp,new_mail)))
			return 0;
	}

	if (new_mail->encrypt)
	{
		if ((ofh_name = malloc(L_tmpnam + 1)))
		{
			tmpnam(ofh_name);
			ofh = fopen(ofh_name,"wb");
		}
	}

	if ((cmail = (struct composed_mail *)list_first(&new_mail->list)))
	{
		/* mail is a multipart message */
		char *boundary;

		if ((boundary = get_boundary_id(fp)))
		{
			if (new_mail->encrypt)
			{
				fprintf(ofh,"MIME-Version: 1.0\n");
				fprintf(ofh,"Content-Type: message/rfc822\n\n");

				/* Only one header is required */
				rc = mail_compose_write_addr_header(ofh,"From",new_mail->from);
			}

			fprintf(ofh,"MIME-Version: 1.0\n");
			fprintf(ofh, "Content-Type: %s; boundary=\"%s\"\n", new_mail->content_type,boundary);

			/* Write the Content Description out */
			if (new_mail->content_description)
			{
				char *cd;
				if ((cd = encode_header_field_utf8("Content-Description",new_mail->content_description)))
				{
					fputs(cd,fp);
					free(cd);
				}
			}

			fprintf(ofh, "\n");
			if (new_mail->to) fputs(mime_preample,ofh);

			while (cmail)
			{
				fprintf(ofh, "\n--%s\n",boundary);
				if (!(mail_compose_write(ofh,cmail)))
				{
					rc = 0;
					break;
				}
				cmail = (struct composed_mail*)node_next(&cmail->node);
			}

			fprintf(ofh, "\n--%s--\n",boundary);

			free(boundary);
		}
	} else
	{
		unsigned int body_len;
		char *body_encoding = NULL;
		char *body = NULL;

		if (new_mail->text)
		{
			char *convtext;
			int converrors,unicode=0;
			int unconvtext_len = strlen(new_mail->text);
			struct codeset *best_codeset = codesets_find_best(new_mail->text, strlen(new_mail->text),&converrors);

			if (converrors)
			{
				int rc = sm_request(NULL, _("Converting this e-mail will cause the loss of some characters.\n"
																		"SimpleMail can store the mail as Unicode so this doesn't happen."),
																		_("Use Unicode|No Unicode"));
				unicode = rc;
			}

			if (!unicode)
			{
				if ((convtext = malloc(unconvtext_len+1)))
					utf8tostr(new_mail->text, convtext, unconvtext_len+1, best_codeset);
			} else convtext = mystrndup(new_mail->text,unconvtext_len);

			/* mail text */
			if (new_mail->to) body_encoding = "8bit"; /* mail has only one part which is a text, so it can be encoded in 8bit */

			if (user.config.write_wrap_type == 2)
				wrap_text(convtext,user.config.write_wrap);

			if (convtext)
			{
				body = encode_body((unsigned char*)convtext, strlen(convtext), new_mail->content_type, &body_len, &body_encoding);
				/* encode as mime only if body encoding is not 7bit or a content description was given */
				if ((body_encoding && mystricmp(body_encoding,"7bit")) || new_mail->content_description)
				{
					if (new_mail->to) fprintf(ofh,"MIME-Version: 1.0\n");
					fprintf(ofh,"Content-Type: text/plain; charset=%s\n",unicode?"utf-8":best_codeset->name);

					/* Write the Content Description out */
					if (new_mail->content_description && *new_mail->content_description)
					{
						char *cd;
						if ((cd = encode_header_field_utf8("Content-Description",new_mail->content_description)))
						{
							fputs(cd,fp);
							free(cd);
						}
					}
				}

				free(convtext);
			}
		} else
		{
			if (new_mail->filename)
			{
				FILE *fh;

				if (new_mail->to) fprintf(ofh,"MIME-Version: 1.0\n");
				fprintf(ofh,"Content-Type: %s\n",new_mail->content_type);
				fprintf(ofh,"Content-Disposition: attachment");

				if (new_mail->content_filename)
				{
					if (isascii7(new_mail->content_filename))
					{
						if (is_token(new_mail->content_filename))
							fprintf(ofh,"; filename=%s",new_mail->content_filename);
						else
							fprintf(ofh,"; filename=\"%s\"",new_mail->content_filename);
					} else
					{
						unsigned char c;
						unsigned char *buf = (unsigned char*)new_mail->content_filename;
						static const char pspecials[] = "'%* ()<>@,;:\\\"[]?=";

						fprintf(ofh,"; filename*=utf-8''");

						while((c = *buf++))
						{
							if (c > 127 || strchr(pspecials,c)) fprintf(ofh,"%%%02X",c);
							else fputc(c,ofh);
						}
					}
				}
				fprintf(ofh,"\n");

				/* Write the Content Description out */
				if (new_mail->content_description && *new_mail->content_description)
				{
					char *cd;
					if ((cd = encode_header_field_utf8("Content-Description",new_mail->content_description)))
					{
						fputs(cd,fp);
						free(cd);
					}
				}

				if ((fh = fopen(new_mail->temporary_filename?new_mail->temporary_filename:new_mail->filename, "rb")))
				{
					int size;
					unsigned char *buf;

					fseek(fh,0,SEEK_END);
					size = ftell(fh);
					fseek(fh,0,SEEK_SET);

					if ((buf = (unsigned char*)malloc(size)))
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
			fprintf(ofh,"Content-transfer-encoding: %s\n",body_encoding);

		fprintf(ofh,"\n");
		if (body)
		{
			fputs(body,ofh);
			free(body);
		}
	}

	if (new_mail->encrypt)
	{
		fclose(ofh);
		rc = mail_write_encrypted(fp, new_mail, ofh_name);
		remove(ofh_name);
	}

	return rc;
}

/*****************************************************************************/

int private_mail_compose_write(FILE *fp, struct composed_mail *new_mail)
{
	return mail_compose_write(fp, new_mail);
}

/*****************************************************************************/

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
		sm_request(NULL, _("No valid from field specified. You must configure some Accounts\n"
										 "before creating new mails"),_("Ok"));
		return 0;
	}

	if (!new_mail->to || (new_mail->to && !(*new_mail->to)))
	{
		/* mail has no recipient, so it automatically get the hold state */
		hold = 1;
	}

	if (hold != 1 && (!new_mail->subject || !new_mail->subject[0]))
	{
		if (sm_request(NULL,_("No subject was specified. It is not recommended to create mails\n"
		                  "without any subject. Do you want to add a subject line?"),
		                  _("Add|Don't add")))
			return 0;
	}

	getcwd(path, sizeof(path));
	if (chdir(outgoing->path) == -1) return 0;

	if ((new_name = mail_get_new_name(hold?MAIL_STATUS_HOLD:MAIL_STATUS_WAITSEND)))
	{
		FILE *fp;
		struct mail_info *mail; /* the mail after it has scanned */

		if ((fp = fopen(new_name,"wb")))
		{
			rc = mail_compose_write(fp, new_mail);
			fclose(fp);
			if (!rc)
				remove(new_name);
		}

		if ((mail = mail_info_create_from_file(NULL, new_name)))
		{
			struct mail_info *old_mail;

			if (new_mail->mail_filename) old_mail = folder_find_mail_by_filename(outgoing,new_mail->mail_filename);
			else old_mail = NULL;

			if (old_mail)
			{
				folder_replace_mail(outgoing, old_mail, mail);
				callback_mail_changed(outgoing, old_mail, mail);
				remove(old_mail->filename);
				mail_info_free(old_mail);
			} else
			{
				callback_new_mail_written(mail);
			}
		}

		if (hold == 2 && mail)
		{
			/* Mail should be send now! */
			mails_upload_single(mail);
		}
		free(new_name);
	}

	chdir(path);

	return rc;
}

/**
 * Write the given UTF8 string as proper HTML text.
 *
 * @param str defines the UTF8 string that should be processed.
 * @param fh defines the file that should be written to
 */
static void fputhtmlstr(char *str, FILE *fh)
{
	unsigned char c;
	while ((c = *str))
	{
		if (c < 128)
		{
			switch(c)
			{
				case '<': fputs("&lt;", fh); break;
				case '>': fputs("&gt;", fh); break;
				case '&': fputs("&amp;", fh); break;
				default:  fputc(c,fh);
			}
			str++;
		} else
		{
			unsigned int unicode;
/*			str = uft8toucs(str,&unicode);*/
			str += utf8tochar(str, &unicode, user.config.default_codeset);
			if (unicode == 0) unicode = '_';
			fprintf(fh,"&#%u;",unicode);
		}
	}
}

/*****************************************************************************/

int mail_create_html_header(struct mail_complete *mail, int all_headers)
{
	int rc = 0;

	FILE *fh;

	if (mail->html_header)
	{
		free(mail->html_header);
		mail->html_header = NULL;
	}

	all_headers = all_headers || (user.config.header_flags & SHOW_HEADER_ALL);

	if ((fh = tmpfile()))
	{
		int len;
		char *replyto = mail_find_header_contents(mail, "reply-to");
		char *style_text = user.config.read_link_underlined?"":" STYLE=\"TEXT-DECORATION: none\"";
		struct header *header;
		struct addressbook_entry_new *entry = addressbook_find_entry_by_address(mail->info->from_addr);

		fprintf(fh,"<HTML><BODY BGCOLOR=\"#%06x\" TEXT=\"#%06x\" LINK=\"#%06x\">",user.config.read_background,user.config.read_text,user.config.read_link);
		fprintf(fh,"<TABLE WIDTH=\"100%%\" BORDER=\"1\" CELLPADDING=\"0\" BGCOLOR=\"#%06x\"><TR><TD><TABLE>",user.config.read_header_background);


		if (mail->info->from_addr && (user.config.header_flags & (SHOW_HEADER_FROM | SHOW_HEADER_ALL)))
		{
			fputs("<TR>",fh);

			fputs("<TD>",fh);
			fprintf(fh,"<STRONG>%s:</STRONG>",_("From"));
			fputs("</TD>",fh);

			fputs("<TD>",fh);
			fprintf(fh,"<A HREF=\"mailto:%s\"%s>",mail->info->from_addr,style_text);

			if (mail->info->from_phrase)
			{
				fputhtmlstr(mail->info->from_phrase,fh);
				fputs(" &lt;",fh);
				fputhtmlstr(mail->info->from_addr,fh);
				fputs("&gt;",fh);
			} else
			{
				fputhtmlstr(mail->info->from_addr,fh);
			}

			fputs("</A></TD>",fh);

			if (entry && entry->portrait)
				fprintf(fh,"<TD><IMG SRC=\"file://localhost/%s\" ALIGN=RIGHT></TD>",entry->portrait);
			fputs("</TR>",fh);
		}

		if (mail->info->to_list && ((user.config.header_flags & (SHOW_HEADER_TO)) || all_headers))
		{
			struct address *addr;

			fputs("<TR>",fh);
			fputs("<TD>",fh);
			fprintf(fh,"<STRONG>%s:</STRONG> ",_("To"));
			fputs("</TD>",fh);

			fputs("<TD>",fh);
			addr = (struct address*)list_first(&mail->info->to_list->list);
			while (addr)
			{
				fprintf(fh,"<A HREF=\"mailto:%s\"%s>",addr->email,style_text);
				if (addr->realname)
				{
					fputhtmlstr(addr->realname,fh);
					fputs(" &lt;",fh);
					fputhtmlstr(addr->email,fh);
					fputs("&gt;",fh);
				} else fputhtmlstr(addr->email,fh);
				fputs("</A>",fh);

				if ((addr = (struct address*)node_next(&addr->node)))
					fputs(", ",fh);
			}
			fputs("</TD>",fh);
			fputs("</TR>",fh);
		}

		if (mail->info->cc_list && ((user.config.header_flags & (SHOW_HEADER_CC)) || all_headers))
		{
			struct address *addr;

			fputs("<TR>",fh);
			fputs("<TD>",fh);
			fprintf(fh,"<STRONG>%s:</STRONG> ",_("Copies to"));
			fputs("</TD>",fh);

			fputs("<TD>",fh);
			addr = (struct address*)list_first(&mail->info->cc_list->list);
			while (addr)
			{
				fprintf(fh,"<A HREF=\"mailto:%s\"%s>",addr->email,style_text);
				if (addr->realname)
				{
					fputhtmlstr(addr->realname,fh);
					fputs(" &lt;",fh);
					fputhtmlstr(addr->email,fh);
					fputs("&gt;",fh);
				} else fputhtmlstr(addr->email,fh);
				fputs("</A>",fh);

				if ((addr = (struct address*)node_next(&addr->node)))
					fputs(", ",fh);
			}
			fputs("</TD>",fh);
			fputs("</TR>",fh);
		}

		if (mail->info->subject && ((user.config.header_flags & (SHOW_HEADER_SUBJECT)) || all_headers))
		{
			fputs("<TR>",fh);
			fputs("<TD>",fh);
			fprintf(fh,"<STRONG>%s:</STRONG> ",_("Subject"));
			fputs("</TD>",fh);

			fputs("<TD>",fh);
			fputhtmlstr(mail->info->subject,fh);
			fputs("</TD>",fh);
			fputs("</TR>",fh);
		}
		if ((user.config.header_flags & (SHOW_HEADER_DATE)) || all_headers)
		{
			fputs("<TR>",fh);
			fputs("<TD>",fh);
			fprintf(fh,"<STRONG>%s:</STRONG> ",_("Date"));
			fputs("</TD>",fh);

			fputs("<TD>",fh);
			fputhtmlstr(sm_get_date_long_str_utf8(mail->info->seconds),fh);
			fputs("</TD>",fh);
			fputs("</TR>",fh);
		}

		if (replyto && ((user.config.header_flags & (SHOW_HEADER_REPLYTO)) || all_headers))
		{
			struct mailbox addr;
			if (parse_mailbox(replyto, &addr))
			{
				fputs("<TR>",fh);
				fputs("<TD>",fh);
				fprintf(fh,"<STRONG>%s:</STRONG>",_("Replies To"));
				fputs("</TD>",fh);

				fputs("<TD>",fh);
				fprintf(fh,"<A HREF=\"mailto:%s\"%s>",addr.addr_spec,style_text);
				if (addr.phrase)
				{
					fputhtmlstr(addr.phrase,fh);
					fputs(" &lt;",fh);
					fputhtmlstr(addr.addr_spec,fh);
					fputs("&gt;",fh);
				} else fputhtmlstr(addr.addr_spec,fh);

				if (addr.phrase)  free(addr.phrase);
				if (addr.addr_spec) free(addr.addr_spec);
				fputs("</A>",fh);

				fputs("</TD>",fh);
				fputs("</TR>",fh);
			}
		}

		header = (struct header*)list_first(&mail->header_list);
		while (header)
		{
			if (all_headers || array_contains(user.config.header_array,header->name))
			{
				if (mystricmp(header->contents,"from") && mystricmp(header->contents,"to") &&
					  mystricmp(header->contents,"cc") && mystricmp(header->contents,"reply-to") &&
					  mystricmp(header->contents,"date") && mystricmp(header->contents,"subject"))
				{
					fputs("<TR>",fh);
					fputs("<TD>",fh);
					fprintf(fh,"<STRONG>%s:</STRONG> ",header->name);
					fputs("</TD>",fh);
					fputs("<TD>",fh);
					fputhtmlstr(header->contents,fh);
					fputs("</TD>",fh);
					fputs("</TR>",fh);
				}
			}
			header = (struct header*)node_next(&header->node);
		}

		fputs("</TABLE></TD></TR></TABLE><BR>",fh);
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

/**
 * Determine the first name of a given person.
 *
 * @param realname the realname of the person
 * @param addr_spec the email address of the person.
 * @return the first name.
 *
 * @note The function assumes that the first name is really at the beginning.
 * @note The function may utilize the addressbook, but this isn't implemented
 *  yet.
 */
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

/*****************************************************************************/

char *mail_create_string(char *format, struct mail_info *orig_mail,
												char *realname, char *addr_spec)
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
						strcpy(dest,realname);
						dest += strlen(realname);
					}
					if (*src == 'v' && realname)
					{
						char *first_name = get_first_name(realname,addr_spec); /* returns a static buffer and never fails */
						strcpy(dest,first_name);
						dest += strlen(first_name);
					}

					if (orig_mail)
					{
						utf8 *from_phrase = orig_mail->from_phrase;
						char *from_addr = orig_mail->from_addr;

						if (*src == 'n' && from_phrase)
						{
							strcpy(dest,from_phrase);
							dest += strlen(from_phrase);
						}

						if (*src == 'f')
						{
							char *first_name = get_first_name(from_phrase,from_addr);
							strcpy(dest,first_name);
							dest += strlen(first_name);
						}

						if (*src == 'e' && from_addr)
						{
							strcpy(dest,from_addr);
							dest += strlen(from_addr);
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

/*****************************************************************************/

int mail_allowed_to_download(struct mail_info *mail)
{
	int rc = 0;

	if (user.config.internet_emails)
	{
		int i;
		for (i=0;user.config.internet_emails[i];i++)
		{
			if (!mystricmp(user.config.internet_emails[i],mail->from_addr))
			{
				rc = 1;
				break;
			}
		}
	}
	return rc;
}
