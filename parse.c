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
** parse.c
*/

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "codecs.h"
#include "parse.h"

static char *parse_encoded_word(char *encoded_word, char **pbuf);

/**************************************************************************
 Support function: Appends a string to another, the result string is
 malloced. Should be placed otherwhere
**************************************************************************/
char *strdupcat(const char *string1, const char *string2)
{
	char *buf = (char*)malloc(strlen(string1) + strlen(string2) + 1);
	if (buf)
	{
		strcpy(buf,string1);
		strcat(buf,string2);
	}
	return buf;
}

/**************************************************************************
 Like strdup() but you can limit the chars. A 0 byte is guaranted.
 The string is allocated via malloc(). Should be placed otherwhere
**************************************************************************/
char *strndup(const char *str1, int n)
{
	char *dest = (char*)malloc(n+1);
	if (dest)
	{
		strncpy(dest,str1,n);
		dest[n]=0;
	}
	return dest;
}

/**************************************************************************
 Adds a string (str1) to a given string src. The first string is
 deallocated (maybe NULL). (A real string library would be better)
 The string is allocated via malloc(). Should be placed otherwhere
**************************************************************************/
char *stradd(char *src, const char *str1)
{
	int len;
	char *dest;
	if (src) len = strlen(src);
	else len = 0;

	if (str1) len += strlen(str1);

	if ((dest = (char*)malloc(len+1)))
	{
		if (src) strcpy(dest,src);
		else dest[0] = 0;
		if (str1) strcat(dest,str1);
	}
	if (src) free(src);
	return dest;
}

/**************************************************************************
 is the char a rfc822 special
**************************************************************************/
static int isspecial(char c)
{
	if (c == '(' || c == ')' || c == '<' || c == '>' || c == '<' ||
		  c == '@' || c == ',' || c == ';' || c == ':' || c == '\\' ||
			c == '"' || c == '.' || c == '[' || c == ']') return 1;

	return 0;
}

/**************************************************************************
 is the char a mime special?
**************************************************************************/
static int ismimespecial(char c)
{
	if (c == '(' || c == ')' || c == '<' || c == '>' || c == '@' ||
		  c == ',' || c == ';' || c == ':' || c == '\\' ||
			c == '"' || c == '[' || c == ']' || c == '?' ||
			c == '=') return 1;

	return 0;
}

/**************************************************************************
 is the char a e special (encoded words)?
**************************************************************************/
static int isespecial(char c)
{
	if ( c == '(' || c == ')' || c == '<' || c == '>' || c == '@' ||
			 c == ',' || c == ';' || c == ':' || c =='"' || c == '/' ||
			 c == '[' || c == ']' || c == '?' || c =='.' || c == '=') return 1;
	return 0;
}

/**************************************************************************
 Needs the string quotation marks?
**************************************************************************/
int needs_quotation(char *str)
{
	char c;
	while ((c = *str++))
	{
		if (isspecial(c)) return 1;
	}
	return 0;
}

/**************************************************************************
 Needs the string quotation marks?
**************************************************************************/
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

/**************************************************************************
 Skip spaces (also comments)
**************************************************************************/
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
	return buf;
}

/**************************************************************************
 atom        =  1*<any CHAR except specials, SPACE and CTLs> 
**************************************************************************/
static char *parse_atom(const char *atom, char **pbuf)
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

/**************************************************************************
 quoted-string = <"> *(qtext/quoted-pair) <"> 
**************************************************************************/
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
				strncpy(buf,quoted_string_start,len); /* the escapes should be removed */
				buf[len]=0;
				*pbuf = buf;
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

/**************************************************************************
 word        =  atom / quoted-string (encoded_word)
**************************************************************************/
static char *parse_word(char *word, char **pbuf)
{
	char *ret;

	ret = parse_encoded_word(word,pbuf);
	if (!ret) ret = parse_quoted_string(word,pbuf);
	if (!ret) ret = parse_atom(word,pbuf);
	return ret;
}

/**************************************************************************
 word        =  atom / quoted-string (encoded_word)
**************************************************************************/
static char *parse_word_new(char *word, char **pbuf, int *quoted)
{
	char *ret;

	if ((ret = parse_encoded_word(word,pbuf)))
	{
		*quoted = 1;
		return ret;
	}
	*quoted = 0;
	if (!ret) ret = parse_quoted_string(word,pbuf);
	if (!ret) ret = parse_atom(word,pbuf);
	return ret;
}

/**************************************************************************
 local-part  =  word *("." word)
**************************************************************************/
static char *parse_local_part(char *local_part, char **pbuf)
{
	char *buf;
	char *ret = parse_word(local_part,&buf);
	char *ret_save;
	if (!ret) return NULL;

	ret_save = ret;
	while ((*ret++ == '.'))
	{
		char *new_word;
		if ((ret = parse_word(ret,&new_word)))
		{
			char *new_buf = strdupcat(buf,".");
			free(buf);
			if (!new_buf) return NULL;

			buf = strdupcat(new_buf,new_word);
			free(new_buf);
			if (!buf) return NULL;

			ret_save = ret;
		} else break;
	}

	*pbuf = buf;
	return ret_save;
}

/**************************************************************************
 sub-domain  =  domain-ref / domain-literal 
 domain-ref = atom
 domain-literal actually not implemented
**************************************************************************/
static char *parse_sub_domain(char *sub_domain, char **pbuf)
{
	return parse_atom(sub_domain,pbuf);
}

/**************************************************************************
 domain      =  sub-domain *("." sub-domain) 
**************************************************************************/
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
			if (!new_buf) return NULL;

			buf = strdupcat(new_buf,new_sub_domain);
			free(new_buf);
			if (!buf) return NULL;

			ret_save = ret;
		} else break;
	}

	*pbuf = buf;
	return ret_save;
}

/**************************************************************************
 addr-spec   =  local-part "@" domain 
**************************************************************************/
char *parse_addr_spec(char *addr_spec, char **pbuf)
{
	char *buf,*buf2;
	char *local_part, *domain;
	char *ret = parse_local_part(addr_spec,&local_part);

	if (!ret) return NULL;

	ret = skip_spaces(ret); /* not needed according rfc */

	if (!(*ret++ == '@'))
	{
		free(local_part);
		return NULL;
	}

	ret = parse_domain(ret,&domain);
	if (!ret)
	{
		free(local_part);
		return NULL;
	}

	buf2 = strdupcat(local_part,"@");
	free(local_part);

	if (!buf2)
	{
		free(domain);
		return NULL;
	}

	buf = strdupcat(buf2,domain);
	free(buf2);
	free(domain);
	if (!buf) return NULL;
	*pbuf = buf;
	return ret;
}

/**************************************************************************
 phrase      =  1*word 
**************************************************************************/
static char *parse_phrase(char *phrase, char **pbuf)
{
	int q;
	char *buf;
	char *ret = parse_word_new(phrase,&buf,&q);
	char *ret_save;
	if (!ret) return NULL;

	ret_save = ret;
	while (ret)
	{
		char *buf2;
		int q2;
		ret = parse_word_new(ret_save,&buf2,&q2);
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

/**************************************************************************
 mailbox = addr-spec | phrase route-addr
 route_addr = "<" [route] addr-spec ">" 
**************************************************************************/
char *parse_mailbox(char *mailbox, struct mailbox *mb)
{
	char *ret;

	memset(mb,0,sizeof(struct mailbox));

	if ((ret = parse_addr_spec(mailbox,&mb->addr_spec)))
	{
		/* the phrase can now be placed in the brackets */
		char *buf = ret;
		while (isspace(*buf)) buf++;
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
						parse_phrase(temp_str,&mb->phrase);
						free(temp_str);
					}
					ret = buf;
					break;
				}
			}
		}
		return ret;
	}

	ret = parse_phrase(mailbox,&mb->phrase);
	if (!ret) return NULL;

	ret = skip_spaces(ret);

	if (*(ret++) != '<')
	{
		free(mb->phrase);
		return NULL;
	}
	ret = parse_addr_spec(ret, &mb->addr_spec);
	if (!ret)
	{
		free(mb->phrase);
		return NULL;
	}
	if (*(ret++) != '>')
	{
		free(mb->phrase);
		free(mb->addr_spec);
		return NULL;
	}
	return ret;
}

/**************************************************************************
 group = phrase ":" [#mailbox] ";" 
**************************************************************************/
static char *parse_group(char *group, struct parse_address *dest)
{
	char *ret = parse_phrase(group,&dest->group_name);
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

/**************************************************************************
 Parses an address
 Returns the name or the first e-mail address
 address     =  mailbox / group
 (actually 1#mailbox / group which is not correct)
**************************************************************************/
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

/**************************************************************************
 Frees all memory allocated in parse_address
**************************************************************************/
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

/**************************************************************************
 text        =  <any CHAR, including bare    ; => atoms, specials, 
                 CR & bare LF, but NOT       ;  comments and 
                 including CRLF>             ;  quoted-strings are NOT recognized

 text_string = *(encoded-word/text)
**************************************************************************/
void parse_text_string(char *text, char **pbuf)
{
	int len = strlen(text);
	char *text_end = text + len;
	char *buf = (char*)malloc(len+1);
	char *buf_ptr;

	if (!buf) return;
	buf_ptr = buf;

	while (text < text_end)
	{
		char *word;
		char *new_text = parse_encoded_word(text, &word);
		if (new_text)
		{
			strcpy(buf_ptr,word);
			buf_ptr += strlen(word);
			free(word);
			text = new_text;
		} else
		{
			*buf_ptr++ = *text++;
		}
	}

  *buf_ptr = 0;
	*pbuf = buf;
}

/**************************************************************************
 In Mime support
 token  :=  1*<any (ASCII) CHAR except SPACE, CTLs, or tspecials> 
**************************************************************************/
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

/**************************************************************************
 In Mime support
 value := token / quoted-string 
**************************************************************************/
char *parse_value(char *value, char **pbuf)
{
	char *ret;
	if ((ret = parse_token(value,pbuf))) return ret;
	return parse_quoted_string(value,pbuf);
}

/**************************************************************************
 In Mime support
 parameter := attribute "=" value 
 attribute := token
**************************************************************************/
char *parse_parameter(char *parameter, struct parse_parameter *dest)
{
	char *attr;
	char *value;
	char *ret = parse_token(parameter,&attr);
  if (!ret) return NULL;
  if (*ret++ == '=')
  {
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


/**************************************************************************
 token  :=  1*<any (ASCII) CHAR except SPACE, CTLs, or especials> 
**************************************************************************/
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


/**************************************************************************
 encoded-word = "=?" charset "?" encoding "?" encoded-text "?="
**************************************************************************/
static char *parse_encoded_word(char *encoded_word, char **pbuf)
{
	char *ret,*encoding_start;
	char *charset;
	char *encoding;

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

	if (!stricmp(encoding,"b"))
	{
		unsigned int len;
		*pbuf = decode_base64(encoding_start, ret - encoding_start, &len);
	} else
	{
		if (!stricmp(encoding,"q"))
		{
			unsigned int len;
			*pbuf = decode_quoted_printable(encoding_start, ret - encoding_start, &len,1);
		} else
		{
			free(charset);
			free(encoding);
			return NULL;
		}
	}

	free(charset);
	free(encoding);

	if (!*pbuf) return NULL;

	return ret + 2;
}
