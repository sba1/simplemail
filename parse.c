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
 * @file parse.c
 */

#include "parse.h"

#include <ctype.h>
#include <stdlib.h>

#include "codecs.h"
#include "support_indep.h"

static char *parse_encoded_word(char *encoded_word, char **pbuf, char **pcharset);

/**
 * Is the char a rfc822 special.
 *
 * @param c the character to check.
 * @return whether c is a rfc822 special character
 */
static int isspecial(char c)
{
	if (c == '(' || c == ')' || c == '<' || c == '>' || c == '<' ||
		  c == '@' || c == ',' || c == ';' || c == ':' || c == '\\' ||
			c == '"' || c == '.' || c == '[' || c == ']') return 1;

	return 0;
}

/**
 * Is the given character a MIME special?
 *
 * @param c the character to check
 * @return if c is a MIME special 1, else 0
 */
static int ismimespecial(char c)
{
	if (c == '(' || c == ')' || c == '<' || c == '>' || c == '@' ||
		  c == ',' || c == ';' || c == ':' || c == '\\' ||
			c == '"' || c == '[' || c == ']' || c == '?' ||
			c == '=') return 1;

	return 0;
}

/**
 * Is the given character an encoded word special?
 *
 * @param c the character to check
 * @return if c is an encoded special 1, else 0
 */
static int isespecial(char c)
{
	if ( c == '(' || c == ')' || c == '<' || c == '>' || c == '@' ||
			 c == ',' || c == ';' || c == ':' || c =='"' || c == '/' ||
			 c == '[' || c == ']' || c == '?' || c =='.' || c == '=') return 1;
	return 0;
}

/*****************************************************************************/

int needs_quotation(char *str)
{
	char c;
	while ((c = *str++))
	{
		if (isspecial(c)) return 1;
	}
	return 0;
}

/*****************************************************************************/

int needs_quotation_len(char *str, int len)
{
	char c;
	while ((c = *str++) && len)
	{
		if (isspecial(c)) return 1;
		len--;
	}
	return 0;
}


/**
 * Skip spaces (and comments) for the given string.
 *
 * @param buf pointing to spaces that should be skipped
 * @return the pointer to the first non-space character
 */
static char *skip_spaces(const char *buf)
{
	unsigned char c;
	int brackets = 0;
	while ((c = *buf))
	{
		if (!c) break;
		if (c=='(') brackets++;
		else if (c == ')' && brackets) brackets--;
		else if (!isspace(c) && !brackets) break;
		buf++;
	}
	return (char*)buf;
}

/**
 * Parse a string as atom.
 *
 * @verbatim
 * atom        =  1*<any CHAR except specials, SPACE and CTLs>
 * @endverbatim
 *
 * @param atom pointer to the characters to parse as atom
 * @param pbuf pointer where the parsed atom is stored
 * @return NULL for a parse error or the pointer to the next unparsed character
 */
static char *parse_atom(char *atom, char **pbuf)
{
	const char *atom_start;
	char *buf;
	unsigned char c;
	int len;

	atom_start = atom = skip_spaces(atom); /* skip spaces */

	while ((c = *atom))
	{
		if (isspace(c) || isspecial(c) || iscntrl(c)) break;
		atom++; /* should check c better */
	}

	len = atom - atom_start;
	if (!len) return NULL;

	if ((buf = malloc(len+1)))
	{
		strncpy(buf,atom_start,len);
		buf[len] = 0;
	}

	*pbuf = buf;
	return atom;
}

/**
 * Parse a string as a quoted string
 *
 * @verbatim
 * quoted-string = <"> *(qtext/quoted-pair) <">
 * @endverbatim
 *
 * @param quoted_string pointer to the characters to parse as atom
 * @param pbuf pointer where the parsed quoted string is stored
 * @return NULL for a parse error or the pointer to the next unparsed character
 */
static char *parse_quoted_string(char *quoted_string, char **pbuf)
{
	char *quoted_string_start;
	unsigned char c;
	int escape = 0;

	quoted_string = skip_spaces(quoted_string);
	if (*quoted_string != '"') return NULL;

	quoted_string_start = ++quoted_string;

	while ((c = *quoted_string))
	{
		if (c == '"' && !escape)
		{
			int len = quoted_string - quoted_string_start;
			char *buf = (char*)malloc(len+1);
			if (buf)
			{
				char *src = quoted_string_start;
				char *dest = buf;
				int i = 0;

				/* filter the escape character */
				for (i=0;i<len;i++)
				{
					unsigned char c = *src;

					if (c == '\\' && !escape) escape = 1;
					else escape = 0;

					if (!escape) *dest++ = c;

					src++;
				}
				*dest = 0;

				*pbuf = buf;
#if 0 /* removed, because it makes trouble with the expand function */
				*pbuf = utf8create(buf,NULL);
				free(buf);
#endif
				quoted_string++;

				return quoted_string; /* the '"' sign */
			}
			return NULL;
		}

		if (c == '\\' && !escape) escape = 1;
		else
		{
			escape = 0;
		}

		quoted_string++;
	}
	return NULL;
}

/**
 * Parse a string a (simple) word.
 *
 * @verbatim
 * word        =  atom / quoted-string
 * @endverbatim
 *
 * @param word pointer to the buffer that contains a word
 * @param pbuf where the word is stored
 * @return
 */
static char *parse_word_simple(char *word, char **pbuf)
{
	char *ret;

	ret = parse_quoted_string(word,pbuf);
	if (!ret) ret = parse_atom(word,pbuf);
	return ret;
}

/**
 * Parses an encoded word and converts it to utf8.
 *
 * @param word pointer to the buffer that contains a word
 * @param pbuf where the word is stored
 * @return NULL for an error or the pointer to the next unparsed character
 */
static char *parse_encoded_word_utf8(char *word, utf8 **pbuf)
{
  char *ret;
	char *charset;
	char *buf;

	if ((ret = parse_encoded_word(word,&buf,&charset)))
	{
		utf8 *newbuf = utf8create(buf,charset);

		if (newbuf) *pbuf = newbuf;
		else ret = NULL;

		free(buf);
		free(charset);
	}
	return ret;
}

/**
 * Parse a word as UTF8.
 *
 * @verbatim
 * word        =  atom / quoted-string (encoded_word)
 * @endverbatim
 *
 * @param word the word to be parsed
 * @param pbuf where the word is stored
 * @param quoted where the information is stored if it was a quoted word
 * @return NULL for an error or the pointer to the next unparsed character
 */
static char *parse_word_new_utf8(char *word, utf8 **pbuf, int *quoted)
{
	char *ret;

	if ((ret = parse_encoded_word_utf8(word,pbuf)))
	{
		*quoted = 1;
		return ret;
	}

	*quoted = 0;

	if (!ret)
	{
		if ((ret = parse_quoted_string(word,(char**)pbuf)))
		{
			utf8 *newbuf;
			if ((parse_encoded_word_utf8(*pbuf,&newbuf)))
			{
				*quoted = 1;
				free(*pbuf);
				*pbuf = newbuf;
			}
		}
	}
	if (!ret) ret = parse_atom(word,(char**)pbuf);
	return ret;
}

/**
 * Parse the local part of an address.
 *
 * @verbatim
 * local-part  =  word *("." word)
 * @endverbatim
 *
 * @param local_part the local_part to be parsed
 * @param pbuf where the local part is stored
 * @return NULL for an error or the pointer to the next unparsed character
 */
static char *parse_local_part(char *local_part, char **pbuf)
{
	char *buf;
	char *ret = parse_word_simple(local_part,&buf);
	char *ret_save;
	if (!ret) return NULL;

	ret_save = ret;
	while ((*ret++ == '.'))
	{
		char *new_word;
		if ((ret = parse_word_simple(ret,&new_word)))
		{
			char *new_buf = strdupcat(buf,".");
			free(buf);
			if (!new_buf) return NULL;

			buf = strdupcat(new_buf,new_word);
			free(new_buf);
			free(new_word);
			if (!buf) return NULL;

			ret_save = ret;
		} else break;
	}

	*pbuf = buf;
	return ret_save;
}

/**
 * Parse a sub domain string.
 *
 * @verbatim
 * sub-domain  =  domain-ref / domain-literal
 * domain-ref = atom
 * domain-literal actually not implemented
 * @endverbatim
 *
 * @param sub_domain the sub domain to be parsed
 * @param pbuf where the sub domain is stored
 * @return NULL for an error or the pointer to the next unparsed character
 */
static char *parse_sub_domain(char *sub_domain, char **pbuf)
{
	char *ret, *ref;

	if ((ret = parse_atom(sub_domain,&ref)))
	{
		/* IDN support */
		if (!strncmp(ref,"xn--",4))
		{
			utf8 *utf8 = punycodetoutf8(ref+4,strlen(ref+4));
			free(ref);
			*pbuf = utf8;
		} else *pbuf = ref;
	}
	return ret;
}

/**
 * Parse a domain string.
 *
 * @verbatim
 * domain      =  sub-domain *("." sub-domain)
 * @endverbatim
 *
 * @param domain the domain to be parsed
 * @param pbuf where the domain is stored
 * @return NULL for an error or the pointer to the next unparsed character
 */
static char *parse_domain(char *domain, char **pbuf)
{
	char *buf;
	char *ret = parse_sub_domain(domain,&buf);
	char *ret_save;
	if (!ret) return NULL;

	ret_save = ret;
	while (*ret++ == '.')
	{
		char *new_sub_domain;
		if ((ret = parse_sub_domain(ret,&new_sub_domain)))
		{
			char *new_buf = strdupcat(buf,".");
			free(buf);
			if (!new_buf)
			{
				free(new_sub_domain);
				return NULL;
			}

			buf = strdupcat(new_buf,new_sub_domain);
			free(new_sub_domain);
			free(new_buf);
			if (!buf) return NULL;

			ret_save = ret;
		} else break;
	}

	*pbuf = buf;
	return ret_save;
}

/*****************************************************************************/

char *parse_addr_spec(char *addr_spec, char **pbuf)
{
	char *local_part, *domain = NULL;
	char *ret;

	string addr_str;

	if (!(ret = parse_local_part(addr_spec,&local_part)))
		return NULL;

	if (!string_initialize(&addr_str,100))
		goto out;

	ret = skip_spaces(ret); /* not needed according rfc */

	if (!(*ret++ == '@'))
		goto out;

	if (!(ret = parse_domain(ret,&domain)))
		goto out;

	if (!string_append(&addr_str,local_part)) goto out;
	if (!(string_append(&addr_str,"@"))) goto out;
	if (!(string_append(&addr_str,domain))) goto out;

	free(domain);
	free(local_part);

	*pbuf = addr_str.str;
	return ret;
out:
	free(addr_str.str);
	free(domain);
	free(local_part);
	return NULL;
}

/**
 * Parse a string as phrase.
 *
 * @verbatim
 * phrase      =  1*word
 * @endverbatim
 *
 * @param phrase pointer to the characters to parse as phrase
 * @param pbuf pointer where the parsed phrase is stored
 * @return NULL for a parse error or the pointer to the next unparsed character
 */
static char *parse_phrase(char *phrase, utf8 **pbuf)
{
	utf8 *buf;
	int q;
	char *ret = parse_word_new_utf8(phrase,&buf,&q);
	char *ret_save;
	if (!ret) return NULL;

	ret_save = ret;
	while (ret)
	{
		utf8 *buf2;
		int q2;
		ret = parse_word_new_utf8(ret_save,&buf2,&q2);
		if (ret)
		{
			char *buf3 = strdupcat(buf,(q && q2)?"":" ");
			free(buf);
			if (!buf3)
			{
				free(buf2);
				return NULL;
			}
			buf = strdupcat(buf3,buf2);
			free(buf3);
			free(buf2);
			if (!buf) return NULL;
			q = q2;
			ret_save = ret;
		}
	}
	*pbuf = buf;
	return ret_save;
}

/*****************************************************************************/

char *parse_mailbox(char *mailbox, struct mailbox *mbox)
{
	char *ret;
	struct mailbox mb;

	if (!mailbox) return NULL;

	memset(&mb,0,sizeof(struct mailbox));

	if ((ret = parse_addr_spec(mailbox, &mb.addr_spec)))
	{
		/* the phrase can now be placed in the brackets */
		char *buf = ret;

		/* don't use skip_spaces() because it skips comments */
		while (isspace((unsigned char)(*buf))) buf++;

		if (*buf == '(')
		{
			char *comment_start = ++buf;

			while (*buf)
			{
				if (*buf++ == ')')
				{
					char *temp_str = (char*)malloc(buf - comment_start);
					if (temp_str)
					{
						strncpy(temp_str,comment_start,buf - comment_start - 1);
						temp_str[buf - comment_start - 1] = 0;
						parse_phrase(temp_str,&mb.phrase);
						free(temp_str);
					}
					ret = buf;
					break;
				}
			}
		}
	} else
	{
		ret = parse_phrase(mailbox,&mb.phrase);
		if (!ret) ret = mailbox; /* for empty phrases */

		ret = skip_spaces(ret);

		if (*(ret++) != '<') goto bailout;
		if (!(ret = parse_addr_spec(ret, &mb.addr_spec))) goto bailout;
		if (*(ret++) != '>') goto bailout;
	}

	*mbox = mb;
	return ret;

bailout:
	free(mb.phrase);
	free(mb.addr_spec);
	return NULL;
}

/**
 * Parse a string as a mailbox group
 *
 * @verbatim
 * group = phrase ":" [#mailbox] ";"
 * @endverbatim
 *
 * @param group pointer to the characters to parse as phrase
 * @param dest pointer where the parsed address is stored
 * @return NULL for a parse error or the pointer to the next unparsed character
 */
static char *parse_group(char *group, struct parse_address *dest)
{
	char *ret = parse_phrase(group,(utf8**)&dest->group_name);
	if (!ret) return NULL;
	ret = skip_spaces(ret);

	if (*(ret++) != ':')
	{
		free(dest->group_name);
		return NULL;
	}

	while (ret)
	{
		struct mailbox mb;
		struct mailbox *nmb;
		while (isspace(*ret)) ret++;

		if (*ret == ';')
		{
			ret++;
			break;
		}
		while (*ret == ',') ret++;

		ret = parse_mailbox(ret, &mb);
		if (!ret) return NULL; /* memory must be freed! */

		if ((nmb = (struct mailbox*)malloc(sizeof(struct mailbox))))
		{
			*nmb = mb;
			list_insert_tail(&dest->mailbox_list,&nmb->node);
		}
	}
	return ret;
}

/*****************************************************************************/

char *parse_address(char *address, struct parse_address *dest)
{
	char *retval;
	memset(dest,0,sizeof(struct parse_address));
	list_init(&dest->mailbox_list);

	retval = parse_group(address, dest);
	if (!retval)
	{
		struct mailbox mb;

		retval = address;

		while ((retval = parse_mailbox(retval, &mb)))
		{
			struct mailbox *nmb;
			if ((nmb = (struct mailbox*)malloc(sizeof(struct mailbox))))
			{
				dest->group_name = NULL;
				*nmb = mb;
				list_insert_tail(&dest->mailbox_list,&nmb->node);
			} else return NULL;

			retval = skip_spaces(retval);

			if (*retval == ',')
			{
				retval++;
			} else break;
		}
	}
	return retval;
}

/*****************************************************************************/

void free_address(struct parse_address *addr)
{
	struct mailbox *mb;
	if (addr->group_name) free(addr->group_name);
	while ((mb = (struct mailbox*)list_remove_tail(&addr->mailbox_list)))
	{
		if (mb->phrase) free(mb->phrase);
		if (mb->addr_spec) free(mb->addr_spec);
		free(mb);
	}
}

/*****************************************************************************/

void parse_text_string(char *text, utf8 **pbuf)
{
	int len = strlen(text);
	int buf_allocated = len + 1;
	char *text_end = text + len;
	char *buf = (char*)malloc(len+1);
	char *buf_ptr;
	int enc = 0;

	if (!buf) return;
	buf_ptr = buf;

	while (text < text_end)
	{
		char *word;
		char *charset;
		char *new_text;

		if ((*text != ' ' || enc) && (new_text = parse_encoded_word(text, &word, &charset)))
		{
			utf8 *word_utf8 = utf8create(word,charset);
			int word_utf8_size = utf8size(word_utf8);

			if (word_utf8_size)
			{
				int old_pos = buf_ptr - buf;
				if (old_pos + word_utf8_size + 1 >= buf_allocated)
				{
					if ((buf = realloc(buf,old_pos + word_utf8_size + 1 + 8)))
					{
						buf_ptr = buf + old_pos;
						buf_allocated = old_pos + word_utf8_size + 1 + 8;
					} else
					{
						*pbuf = NULL;
						return;
					}
				}

				if (buf)
				{
					utf8cpy(buf_ptr, word_utf8);
					buf_ptr += word_utf8_size;
				}
				free(word_utf8);
			}

			free(word);
			free(charset);
			text = new_text;
			enc = 1;
		} else
		{
			int old_pos = buf_ptr - buf;
			unsigned char c = *text;

			if (old_pos + 2 + 1 >= buf_allocated)
			{
				if ((buf = realloc(buf,old_pos + 2 + 1 + 8)))
				{
					buf_ptr = buf + old_pos;
					buf_allocated = old_pos + 2 + 1 + 8;
				} else
				{
					*pbuf = NULL;
					return;
				}
			}

			if (c < 128)
			{
				*buf_ptr++ = c;
			} else
			{
				char *temp = utf8create_len(text,NULL,1);
				if (temp)
				{
					*buf_ptr++ = temp[0];
					*buf_ptr++ = temp[1];
				}
				free(temp);
			}
			text++;
			enc = 0;
		}
	}

	/* The last byte guaranted to be allocated */
  *buf_ptr = 0;
	*pbuf = buf;
}


/*****************************************************************************/

int is_token(char *token)
{
	int i;
	unsigned char *utoken = (unsigned char*)token;

	for (i=0;token[i];i++)
	{
		if (isspace(utoken[i]) || ismimespecial(utoken[i]) || iscntrl(utoken[i]))
			return 0;
	}
	return 1;
}

/*****************************************************************************/

char *parse_token(char *token, char **pbuf)
{
	const char *token_start = token;
	char *buf;
	unsigned char c;
	int len;

	while ((c = *token))
	{
		if (isspace(c) || ismimespecial(c) || iscntrl(c)) break;
		token++; /* should check c better */
	}

	len = token - token_start;
	if (!len) return NULL;

	if ((buf = malloc(len+1)))
	{
		strncpy(buf,token_start,len);
		buf[len] = 0;
	}

	while ((c = *token))
	{
		if (!isspace(c)) break;
		token++;
	}

	*pbuf = buf;
	return token;
}

/*****************************************************************************/

char *parse_value(char *value, char **pbuf)
{
	char *ret;
	if ((ret = parse_token(value,pbuf))) return ret;
	return parse_quoted_string(value,pbuf);
}

/*****************************************************************************/

char *parse_parameter(char *parameter, struct parse_parameter *dest)
{
	char *attr;
	char *value;
	char *ret = parse_token(parameter,&attr);
  if (!ret) return NULL;
  ret = skip_spaces(ret); /* This doesn't conform to the RFC */
  if (*ret++ == '=')
  {
	ret = skip_spaces(ret);  /* This doesn't conform to the RFC */
  	if ((ret = parse_value(ret,&value)))
  	{
  		dest->attribute = attr;
  		dest->value = value;
  		return ret;
  	}
  }
  free(attr);
  return NULL;
}

/**
 * Parse a string as token.
 *
 * @verbatim
 * token  :=  1*<any (ASCII) CHAR except SPACE, CTLs, or especials>
 * @endverbatim
 *
 * @param token the pointer to the characters to parse as token
 * @param pbuf will hold the buffer that contains the token only. The string
 *  shall be freed via free() when no longer in use.
 * @return pointer to the first character that is not a token or NULL if the
 *  string was not token
 */
char *parse_etoken(char *token, char **pbuf)
{
	const char *token_start = token;
	char *buf;
	unsigned char c;
	int len;

	while ((c = *token))
	{
		if (isspace(c) || isespecial(c) || iscntrl(c)) break;
		token++; /* should check c better */
	}

	len = token - token_start;
	if (!len) return NULL;

	if ((buf = malloc(len+1)))
	{
		strncpy(buf,token_start,len);
		buf[len] = 0;
	}

	while ((c = *token))
	{
		if (!isspace(c)) break;
		token++;
	}

	*pbuf = buf;
	return token;
}


/**
 * Parse an encoded word
 *
 * @verbatim
 * encoded-word = "=?" charset "?" encoding "?" encoded-text "?="
 * @endverbatim
 *
 * @param encoded_word the pointer to the encoded word to be parsed
 * @param pbuf where the encoded word is stored
 * @param pcharset the charset or NULL
 * @return NULL for a parse error or the pointer to the next unparsed character
 */
static char *parse_encoded_word(char *encoded_word, char **pbuf, char **pcharset)
{
	char *ret,*encoding_start;
	char *charset;
	char *encoding;
	unsigned int len;

	ret = skip_spaces(encoded_word);
	if (*ret != '=') return NULL;
	if (*(++ret) != '?') return NULL;

	if (!(ret = parse_etoken(ret+1,&charset))) return NULL;
	if (*ret!='?')
	{
		free(charset);
		return NULL;
	}

	if (!(ret = parse_etoken(ret+1,&encoding)))
	{
		free(charset);
		return NULL;
	}
	if (*ret!='?')
	{
		free(charset);
		free(encoding);
		return NULL;
	}

	encoding_start = ret + 1;
	if (!(ret = strstr(ret + 1,"?=")))
	{
		free(charset);
		free(encoding);
		return NULL;
	}

	len = (unsigned int)-1;

	if (!mystricmp(encoding,"b"))
	{
		*pbuf = decode_base64((unsigned char*)encoding_start, ret - encoding_start, &len);
	} else
	{
		if (!mystricmp(encoding,"q"))
		{
			*pbuf = decode_quoted_printable((unsigned char*)encoding_start, ret - encoding_start, &len,1);
		} else
		{
			free(charset);
			free(encoding);
			return NULL;
		}
	}

	*pcharset = charset;
	free(encoding);

	/* filter out the Escape sequence as this is not allowed anywhy */
	if (*pbuf)
	{
		int i;
		for (i=0;i<len;i++)
			if ((*pbuf)[i]==27) (*pbuf)[i]=' ';

		return ret + 2;
	}
	return NULL;
}

/*****************************************************************************/

char *parse_date(char *buf, int *pday,int *pmonth,int *pyear,int *phour,int *pmin,int *psec, int *pgmt)
{
	int day, month, year, hour, min, sec, gmt;
	unsigned char *date;
	if (!buf) return NULL;
	date = (unsigned char*)strstr(buf,",");
	if (!date) date = (unsigned char*)buf;
	else date++;

	while (isspace(*date)) date++;
	day = atoi((char*)date);
	while (isdigit(*date)) date++;
	while (isspace(*date)) date++;
	if (isdigit(*date))
	{
		month = atoi((char*)date);
		if (month < 1 || month > 12) return NULL;
		while (isdigit(*date)) date++;
	}
	else
	{
		if (!mystrnicmp((char*)date,"jan",3)) month = 1; /* Not ANSI C */
		else if (!mystrnicmp((char*)date,"feb",3)) month = 2;
		else if (!mystrnicmp((char*)date,"mar",3)) month = 3;
		else if (!mystrnicmp((char*)date,"apr",3)) month = 4;
		else if (!mystrnicmp((char*)date,"may",3)) month = 5;
		else if (!mystrnicmp((char*)date,"jun",3)) month = 6;
		else if (!mystrnicmp((char*)date,"jul",3)) month = 7;
		else if (!mystrnicmp((char*)date,"aug",3)) month = 8;
		else if (!mystrnicmp((char*)date,"sep",3)) month = 9;
		else if (!mystrnicmp((char*)date,"oct",3)) month = 10;
		else if (!mystrnicmp((char*)date,"nov",3)) month = 11;
		else if (!mystrnicmp((char*)date,"dec",3)) month = 12;
		else return NULL;
		while (isalpha(*date)) date++;
	}

	while (isspace(*date)) date++;
	year = atoi((char*)date);
	if (year < 78) year += 2000;
	else if (year < 200) year += 1900;

	while (isdigit(*date)) date++;
	while (isspace(*date)) date++;
	hour = atoi((char*)date);
	if (hour < 100)
	{
		while (isdigit(*date)) date++;
		while (!isdigit(*date)) date++;
		min = atoi((char*)date);
		while (isdigit(*date)) date++;
		while (!isdigit(*date)) date++;
		sec = atoi((char*)date);
	} else /* like examples in rfc 822 */
	{
		min = hour % 100;
		hour = hour / 100;
		sec = 0;
	}
	while (isdigit(*date)) date++;
	while (isspace(*date)) date++;
	gmt = atoi((char*)date);
	gmt = (gmt % 100) + (gmt / 100)*60;
	while (isdigit(*date)) date++;

	if (pday) *pday = day;
	if (pmonth) *pmonth = month;
	if (pyear) *pyear = year;
	if (phour) *phour = hour;
	if (pmin) *pmin = min;
	if (psec) *psec = sec;
	if (pgmt) *pgmt = gmt;

	return (char*)date;
}


