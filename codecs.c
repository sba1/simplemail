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
** codecs.c
*/

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "codecs.h"
#include "lists.h"
#include "mail.h"
#include "parse.h"
#include "support_indep.h"

/* encoding table for the base64 coding */
static const char encoding_table[] =
{
	'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
	'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',
	'0','1','2','3','4','5','6','7','8','9','+','/'
};


#if 0

/**************************************************************************
 Gets the hexa digit if any (if not it returns 0). Otherwise
 the digit is in *val (only the lower 4 bits get overwritten)
**************************************************************************/
static int get_hexadigit(char c, int *pval)
{
	int val;

	if (c >= '0' && c <= '9') val = c - '0';
	else if (c >= 'A' && c <= 'F') val = c - 'A' + 10;
	else if (c >= 'a' && c <= 'f') val = c - 'a' + 10;
	else return 0;

	*pval = ((*pval) & 0xfffffff0) | val;
	return 1;
}

#endif

/**************************************************************************
 Decoding a given buffer using the base64 algorithm
**************************************************************************/
char *decode_base64(unsigned char *src, unsigned int len, unsigned int *ret_len)
{
   static const signed char decoding_table[128] = {
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
      52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,
      -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
      15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
      -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
      41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1
   };

   unsigned char *srcmax=src+len,*dest,*deststart;

   *ret_len=0;

   if(!(dest=malloc(len*3/4+1))) return NULL;
   deststart=dest;

   while (src + 3 < srcmax)
   {
      if(src[0] > 127) {src++; continue;};
      if(-1 == decoding_table[src[0]])
      {
         src++;
         continue;
      } else {
         unsigned char c1,c2,c3,c4;

         c1=decoding_table[src[0] & 0x7f];
         c2=decoding_table[src[1] & 0x7f];
         c3=decoding_table[src[2] & 0x7f];
         c4=decoding_table[src[3] & 0x7f];

         *dest++ = (c1 << 2) | (c2 >> 4);

         if (src[2] == '=') break;
         *dest++ = (c2 << 4) | (c3 >> 2);

         if (src[3] == '=') break;
         *dest++ = (c3 << 6) | c4;
         src += 4;
      }
   }
   *dest=0;

   *ret_len=dest-deststart;
   return realloc(deststart,*ret_len+1);
}

/**************************************************************************
 Decoding a given buffer using the quoted_printable algorithm.
 Set header to 1 if you want header decoding (underscore = space)
**************************************************************************/
#define IS_HEX(c)   ((((c) >= '0') && ((c) <= '9')) || \
                     (((c) >= 'A') && ((c) <= 'F')) || \
                     (((c) >= 'a') && ((c) <= 'f')))
#define FROM_HEX(c) ((c) - (((c) > '9') ? \
                           (((c) > 'F') ? 'a' - 10 : 'A' - 10 ) : '0'))
char *decode_quoted_printable(unsigned char *buf, unsigned int len, unsigned int *ret_len, int header)
{
   unsigned char *dest,*deststart;

   *ret_len=0;
   if(!len) return NULL;
   if(!(dest=malloc(len+1))) return NULL;
   deststart=dest;

   if(header)
   {
      while(len--)
      {
         unsigned char c=*buf++;

         if('=' == c)
         {
            unsigned char c2;

            c=buf[0];
            c2=buf[1];
            if (IS_HEX(c) && IS_HEX(c2))
            {
               c = (FROM_HEX(c) << 4) | FROM_HEX(c2);
               if(len) len--;
               if(len) len--;
               buf += 2;
            } else c = '='; /* should never happen */
         } else if('_' == c) c=' ';
         *dest++ = c;
      }
      *dest = 0;

      *ret_len=dest-deststart;
      return realloc(deststart,*ret_len+1);
   } else { /* body */
      unsigned char *text,*textstart;

      if(!(text=malloc(len+1))) {free(dest); return NULL;}
      memcpy(text,buf,len);
      text[len]=0;
      textstart=text;

      while(text)
      {
         unsigned char  c;
         unsigned char *lp = text;

         /* Remove trailing white space from end of line */
         {
            unsigned char *ep;

            /* Look for end of line */
            if ((ep = strchr(lp, '\n')))
            {
               text = ep + 1;
               ep--;
               if('\r' == *ep) ep--;
            } else {
               ep   = lp + strlen(lp) - 1;
               text = NULL;   /* End of text reached */
            }

            /* Remove trailing white space */
            while ((ep >= lp) && (((c = *ep) == ' ') || (c == '\t'))) ep--;

            /* Set string terminator */
            *(ep + 1) = '\0';
         }

         /* Scan line */
         do
         {
            switch (c = *lp++)
            {
               case '\0': /* Line end reached. Add line end (only while in text) */
                  if (text) *dest++ = '\n';
                  break;

               case '=':  /* Encoded character */
                  /* End of line reached? (-> soft line break!) */
                  if ((c = *lp))
                  {
                     unsigned char d = *(lp + 1);

                     /* Sanity check */
                     if (IS_HEX(c) && IS_HEX(d))
                     {
                        /* Decode two hex digit to 8-Bit characters */
                        *dest++ = (FROM_HEX(c) << 4) | FROM_HEX(d);

                        /* Move line pointer */
                        lp += 2;
                     } else {
                        /* The '=' was not followed by two hex digits. This is actually */
                        /* a violation of the standard. We put the '=' into the decoded */
                        /* text and continue decoding after the '»' character.          */
                        *dest++ = '=';
                     }
                  }
                  break;

               default:   /* Normal character */
                  *dest++ = c;
                  break;
            }
         } while (c);
      }
      /* Add string terminator */
      *dest=0;

      free(textstart);

      *ret_len=dest-deststart;
      return realloc(deststart,*ret_len+1);
   }
}

/*
unsigned char *strencoded(unsigned char *str)
{
	unsigned char c;
	while ((c=*str))
	{
		if (c > 127 || c == '=' || c == '?')
			return 
	}
}
*/

static int get_noencode_str(unsigned char *buf)
{
	int space = 0;
	unsigned char *buf_start = buf;
	unsigned char *word_start = buf;
	unsigned char c;

	while ((c=*buf))
	{
		if (isspace(c)) space = 1;
		else
		{
			if (space)
			{
				word_start = buf; /* a new word starts */
				space = 0;
			}
			if (c < 32 || c > 127 || (c == '=' && *(buf+1) == '?'))
			{
				return word_start - buf_start;
			}
		}
		buf++;
	}

	return buf - buf_start;
}

static int get_encode_str(unsigned char *buf)
{
	int space = 0;
	unsigned char *buf_start = buf;
	unsigned char *word_start = buf;
	unsigned char *word_end = NULL;
	unsigned char c;
	int toencode = 0;

	while ((c=*buf))
	{
		if (isspace(c))
		{
			if (!space)
			{
				if (!toencode) break;
				word_end = buf;
				toencode = 0;
			}
			space = 1;
		}
		else
		{
			if (space)
			{
				word_start = buf;
				space = 0;
			}
			if (c<32 || c >= 127 || (c == '=' && *(buf+1)=='?'))
			{
				toencode = 1;
			}
		}
		buf++;
	}

	if (!toencode)
	{
		if (!word_end) return 0;
		return word_end - buf_start;
	}

	return buf - buf_start;
}

/**************************************************************************
 Encodes a given header string (changed after 1.7)
 If structured is one the resulting string also may contains quotation
 marks
**************************************************************************/
static char *encode_header_str(char *toencode, int *line_len_ptr, int structured)
{
	int line_len = *line_len_ptr;
	int encoded_len = 0;
	int toencode_len = mystrlen(toencode);
	char *encoded = NULL;
	int max_line_len = 72; /* RFC 2047 says it can be 76 */
	FILE *fh;

	if (!toencode) return NULL;

	if ((fh = tmpfile()))
	{
		while (toencode_len)
		{
			int l;
			if ((l = get_noencode_str(toencode)))
			{
				int quotes = structured && needs_quotation_len(toencode,l);
				int space_add;
				if ((line_len + l + (quotes?2:0)) > max_line_len && line_len > 1)
				{
					fputs("\n ",fh);
					encoded_len += 2;
					line_len = 1;
				}

				if (toencode[l-1] == ' ' && quotes)
				{
					/* the last space should split the quoted string from the encoded word */
					l--;
					space_add = 1;
				} else space_add = 0;

				if (quotes) fputc('"',fh);
				fwrite(toencode,1,l,fh);
				toencode += l;
				toencode_len -= l;
				if (quotes) fputc('"',fh);
				if (space_add)
				{
					fputc(' ',fh);
					l++;
					toencode++;
					toencode_len--;
				}

				line_len += l + (quotes?2:0);
				encoded_len += l + (quotes?2:0);
				
/*				while (l)
				{
					if (line_len >= max_line_len)
					{
						fputs("\n ",fh);
						encoded_len += 2;
						line_len = 1;
					}
					fputc(*toencode,fh);
					l--;
					toencode_len--;
					toencode++;
					line_len++;
					encoded_len++;
				}*/
			}
			if ((l = get_encode_str(toencode)))
			{
				char buf[32];
				int buf_len;
				int line_start = 1;
				int have_written = 0;

				while (l)
				{
					unsigned char c = *toencode;

					if (line_start)
					{
						strcpy(buf,"=?iso-8859-1?q?");
						buf_len = sizeof("=?iso-8859-1?q?")-1;
						line_start = 0;
					} else buf_len = 0;

					if (c > 127 || c == '_' || c == '?' || c == '=')
					{
						sprintf(&buf[buf_len],"=%02X",c);
						buf_len += 3;
					} else
					{
						if (c==' ') c = '_';
						buf[buf_len++] = c;
					}

					if (buf_len + line_len > 70)
					{
						line_start = 1;
						line_len = 1;
						if (have_written)
						{
							fputs("?=\n ",fh);
							encoded_len += 4;
						} else
						{
							fputs("\n ",fh);
							encoded_len += 2;
						}
						continue;
					}

					have_written = 1;
					fwrite(buf,1,buf_len,fh);
					encoded_len += buf_len;
					line_len += buf_len;

					l--;
					toencode_len--;
					toencode++;
				}
				if (have_written)
				{
					fputs("?=",fh);
					encoded_len += 2;
				}
			}
		}

		if (encoded_len)
		{
	    fseek(fh,0,SEEK_SET);
			if ((encoded = (char*)malloc(encoded_len+1)))
			{
				fread(encoded,1,encoded_len,fh);
				encoded[encoded_len]=0;
			}
		}

		fclose(fh);
	}
	*line_len_ptr = line_len;
	return encoded;
}


/**************************************************************************
 Creates a unstructured encoded header field (includes all rules of the
 RFC 821 and RFC 2047)
 The string is allocated with malloc()
**************************************************************************/
char *encode_header_field(char *field_name, char *field_contents)
{
	char *header = NULL;
	int line_len;
	FILE *fh;

	if ((fh = tmpfile()))
	{
		char *encoded;
		int header_len;

		fprintf(fh, "%s: ",field_name);
		line_len = strlen(field_name) + 2;

		if ((encoded = encode_header_str(field_contents, &line_len,0)))
		{
			fprintf(fh, "%s", encoded);
			free(encoded);
		}
		fputc('\n',fh);

		if ((header_len = ftell(fh)))
		{
			fseek(fh,0,SEEK_SET);
			if ((header = (char*)malloc(header_len+1)))
			{
				fread(header,1,header_len,fh);
				header[header_len]=0;
			}
		}


		fclose(fh);
	}

	return header;
}

/**************************************************************************
 Creates a structured address encoded header field (includes all rules of the
 RFC 821 and RFC 2047). List is the list with all addresses.
 The string is allocated with malloc()
**************************************************************************/
char *encode_address_field(char *field_name, struct list *address_list)
{
	int field_len = strlen(field_name) + 2; /* including the ':' and the space */
	int header_len;
	int line_len;
	char *header = NULL;

	FILE *fh;

	if ((fh = tmpfile()))
	{
		struct address *address;

		fprintf(fh, "%s: ",field_name);
		line_len = field_len;

		address = (struct address*)list_first(address_list);
		while (address)
		{
			struct address *next_address = (struct address*)node_next(&address->node);
			char *text = encode_header_str(address->realname, &line_len,1);
			if (text)
			{
				fputs(text,fh);

				if (address->email)
				{
					int email_len = strlen(address->email) + 2;

					if (line_len + email_len + 1 + (next_address?1:0) > 72) /* <>, space and possible comma */
					{
						line_len = 1;
						fprintf(fh,"\n ");
					} else
					{
						fputc(' ',fh);
						line_len++;
					}

					fprintf(fh,"<%s>%s",address->email,next_address?",":"");
					line_len += email_len;
				}
				free(text);
			} else
			{
				int email_len = strlen(address->email);

				if (line_len + email_len + (next_address?1:0) > 72) /* <> and space */
				{
					line_len = 1;
					fprintf(fh,"\n ");
				}

				fprintf(fh,"%s%s",address->email,next_address?",":"");
				line_len += email_len;
			}

			address = next_address;
		}

		if ((header_len = ftell(fh)))
		{
	    fseek(fh,0,SEEK_SET);
			if ((header = (char*)malloc(header_len+1)))
			{
				fread(header,1,header_len,fh);
				header[header_len]=0;
			}
		}

		fclose(fh);
	}
	return header;
}

/**************************************************************************
 Encodes the given body quoted printable and writes it into fh
**************************************************************************/
static void encode_body_quoted(FILE *fh, unsigned char *buf, unsigned int len)
{
	int line_len = 0;
	while (len > 0)
	{
		unsigned char *next_str = buf;
		unsigned char c = *next_str;
		char digit_buf[16];
		int next_len = 1;

		if ((c < 33 || c == 61 || c > 126) && c != 10)
		{
			sprintf(digit_buf,"=%02X",c);
			next_str = digit_buf;
			next_len = 3;
		}

		if (line_len + next_len > 75)
		{
			if (c != 10) fprintf(fh,"=\n");
			else
			{
				/* don't soft break */
				fprintf(fh,"\n");
				next_len = 0;
			}
			line_len = 0;
		}

		if (next_len)
		{
			fwrite(next_str,1,next_len,fh);
			line_len += next_len;
		}

		if (c==10) line_len = 0;

		buf++;
		len--;
	}
}

/**************************************************************************
 Encodes the given body base64 and writes it into fh
**************************************************************************/
static void encode_body_base64(FILE *fh, unsigned char *buf, unsigned int len)
{
	int line_len = 0;
	char line_buf[100];
	while (len > 0)
	{
		if (len > 11)
		{
			/* could make problems with little endian */
			unsigned long todecode1 = *((unsigned long*)buf);
			unsigned long todecode2 = *(((unsigned long*)buf)+1);
			unsigned long todecode3 = *(((unsigned long*)buf)+2);
			buf += 12;
			len -= 12;

			if (line_len >= 76)
			{
				line_buf[line_len] = 0;
				fprintf(fh,"%s\n",line_buf);
				line_len = 0;
			}

			line_buf[line_len++] = encoding_table[(todecode1 & (0xfc000000)) >> 26];
			line_buf[line_len++] = encoding_table[(todecode1 & (0xfc000000 >> 6)) >> 20];
			line_buf[line_len++] = encoding_table[(todecode1 & (0xfc000000 >> 12)) >> 14];
			line_buf[line_len++] = encoding_table[(todecode1 & (0xfc000000 >> 18)) >> 8];

			todecode1 = (todecode1 << 24)|((todecode2 & 0xffff0000) >> 8);

			if (line_len >= 76)
			{
				line_buf[line_len] = 0;
				fprintf(fh,"%s\n",line_buf);
				line_len = 0;
			}

			line_buf[line_len++] = encoding_table[(todecode1 & (0xfc000000)) >> 26];
			line_buf[line_len++] = encoding_table[(todecode1 & (0xfc000000 >> 6)) >> 20];
			line_buf[line_len++] = encoding_table[(todecode1 & (0xfc000000 >> 12)) >> 14];
			line_buf[line_len++] = encoding_table[(todecode1 & (0xfc000000 >> 18)) >> 8];

			todecode1 = (todecode2 << 16) | ((todecode3 & 0xff000000)>>16);

			if (line_len >= 76)
			{
				line_buf[line_len] = 0;
				fprintf(fh,"%s\n",line_buf);
				line_len = 0;
			}

			line_buf[line_len++] = encoding_table[(todecode1 & (0xfc000000)) >> 26];
			line_buf[line_len++] = encoding_table[(todecode1 & (0xfc000000 >> 6)) >> 20];
			line_buf[line_len++] = encoding_table[(todecode1 & (0xfc000000 >> 12)) >> 14];
			line_buf[line_len++] = encoding_table[(todecode1 & (0xfc000000 >> 18)) >> 8];

			todecode1 = todecode3 << 8;

			if (line_len >= 76)
			{
				line_buf[line_len] = 0;
				fprintf(fh,"%s\n",line_buf);
				line_len = 0;
			}

			line_buf[line_len++] = encoding_table[(todecode1 & (0xfc000000)) >> 26];
			line_buf[line_len++] = encoding_table[(todecode1 & (0xfc000000 >> 6)) >> 20];
			line_buf[line_len++] = encoding_table[(todecode1 & (0xfc000000 >> 12)) >> 14];
			line_buf[line_len++] = encoding_table[(todecode1 & (0xfc000000 >> 18)) >> 8];
		} else
		{
			int c1,c2,c3;

			while (len > 0)
			{
				c1 = *buf++;
				len--;

				if (len) c2 = *buf++, len--;
				else c2 = -1;

				if (len) c3 = *buf++, len--;
				else c3 = -1;

				if (line_len >= 76)
				{
					line_buf[line_len] = 0;
					fprintf(fh,"%s\n",line_buf);
					line_len = 0;
				}

				if (c2 == -1)
				{
					line_buf[line_len++] = encoding_table[c1 >> 2];
					line_buf[line_len++] = encoding_table[((c1 & 0x3) << 4)];
					line_buf[line_len++] = '=';
					line_buf[line_len++] = '=';
				} else
				{
					if (c3 == -1)
					{
						line_buf[line_len++] = encoding_table[c1 >> 2];
						line_buf[line_len++] = encoding_table[((c1 & 0x3) << 4) | ((c2 & 0xf0)>>4)];
						line_buf[line_len++] = encoding_table[((c2 & 0xf) << 2)];
						line_buf[line_len++] = '=';
					} else
					{
						line_buf[line_len++] = encoding_table[c1 >> 2];
						line_buf[line_len++] = encoding_table[((c1 & 0x3) << 4) | ((c2 & 0xf0)>>4)];
						line_buf[line_len++] = encoding_table[((c2 & 0xf) << 2) | ((c3 & 0xc0)>>6)];
						line_buf[line_len++] = encoding_table[c3 & 0x3F];
					}
				}
			}
		}
	}

	if (line_len)
	{
		line_buf[line_len] = 0;
		fprintf(fh,"%s\n",line_buf);
		line_len = 0;
	}
}

/**************************************************************************
 Returns the best encoding of a given buffer. Should actually only be
 used for text parts.
**************************************************************************/
static char *get_best_encoding(unsigned char *buf, int len, char *max)
{
	int i,line_len=0,eight_bit=0;
	unsigned char c;

	for (i=0;i<len;i++)
	{
		c = *buf++;
		if (c=='\n')
		{
			/* if line is longer than 998 chars we must use qp */
			if (line_len > 998) return "quoted-printable";
			line_len = 0;
		}
		if (c > 127) eight_bit = 1;
		line_len++;
	}

  /* if no 8 bit chars we can use 7bit */
	if (!eight_bit) return "7bit";

	/* we have 8 bit chars, use 8bit if possible */
	if (!mystricmp(max,"8bit")) return "8bit";
	return "quoted-printable";
}

/**************************************************************************
 Encodes the given body. The encoded buffer is allocated with malloc(),
 the length is stored in *ret_len and the used transfer encoding in
 *encoding (MIME Content-Transfer-Encoding). The retured buffer is 0 byte
 terminated.
**************************************************************************/
char *encode_body(unsigned char *buf, unsigned int len, char *content_type, unsigned int *ret_len, char **encoding)
{
	char *body = NULL;
	FILE *fh;

	if ((fh = tmpfile()))
	{
		int body_len;

		if (!mystricmp(content_type,"text/plain"))
		{
			*encoding = get_best_encoding(buf,len,*encoding);
			if (!(mystricmp(*encoding,"8bit")) || !(mystricmp(*encoding,"7bit")))
			{
				/* TODO: This is a overhead, it's better to decide this outside of encode_body() */
				if ((body = malloc(len+1)))
				{
					memcpy(body,buf,len); /* faster then strncpy() */
					body[len]=0;
					*ret_len = len;
				}
				return body;
			} else
			{
				encode_body_quoted(fh,buf,len);
				*encoding = "quoted-printable";
			}
		} else
		{
			encode_body_base64(fh,buf,len);
			*encoding = "base64";
		}

		body_len = ftell(fh);
	  fseek(fh,0,SEEK_SET);
		if ((body = (char*)malloc(body_len+1)))
		{
			fread(body,1,body_len,fh);
			body[body_len] = 0;
			*ret_len = body_len;
		}
		fclose(fh);
	}
	return body;
}

/**************************************************************************
 Encodes an given string to the base64 format. The string is 0 terminated.
**************************************************************************/
char *encode_base64(unsigned char *buf, unsigned int len)
{
	unsigned int ret_len;
	char *encoding;
	return encode_body(buf,len,"base64",&ret_len,&encoding);
}

/**************************************************************************
 Identifies a given file and return it's mime type
**************************************************************************/
char *identify_file(char *fname)
{
	char *ctype = "application/octet-stream";
	FILE *fh;

	if ((fh = fopen(fname, "r")))
	{
		int len;
		static char buffer[1024], *ext;

		len = fread(buffer, 1, 1023, fh);
		buffer[len] = 0;
		fclose(fh);

		if ((ext = strrchr(fname, '.'))) ++ext;
		else ext = "--"; /* for rx */

		if (!mystricmp(ext, "htm") || !mystricmp(ext, "html"))
			return "text/html";
		else if (!mystrnicmp(buffer, "@database", 9) || !mystricmp(ext, "guide"))
			return "text/x-aguide";
		else if (!mystricmp(ext, "ps") || !mystricmp(ext, "eps"))
			return "application/postscript";
		else if (!mystricmp(ext, "rtf"))
			return "application/rtf";
		else if (!mystricmp(ext, "lha") || !strncmp(&buffer[2], "-lh5-", 5))
			return "application/x-lha";
		else if (!mystricmp(ext, "lzx") || !strncmp(buffer, "LZX", 3))
			return "application/x-lzx";
		else if (!mystricmp(ext, "zip"))
			return "application/x-zip";
		else if (!mystricmp(ext, "rexx") || !mystricmp(ext+strlen(ext)-2, "rx"))
			return "application/x-rexx";
		else if (!strncmp(&buffer[6], "JFIF", 4))
			return "image/jpeg";
		else if (!strncmp(buffer, "GIF8", 4))
			return "image/gif";
		else if (!mystrnicmp(ext, "png",4) || !strncmp(&buffer[1], "PNG", 3))
			return "image/png";
		else if (!mystrnicmp(ext, "tif",4))
			return "image/tiff";
		else if (!strncmp(buffer, "FORM", 4) && !strncmp(&buffer[8], "ILBM", 4))
			return "image/x-ilbm";
		else if (!mystricmp(ext, "au") || !mystricmp(ext, "snd"))
			return "audio/basic";
		else if (!strncmp(buffer, "FORM", 4) && !strncmp(&buffer[8], "8SVX", 4))
			return "audio/x-8svx";
		else if (!mystricmp(ext, "wav"))
			return "audio/x-wav";
		else if (!mystricmp(ext, "mpg") || !mystricmp(ext, "mpeg"))
			return "video/mpeg";
		else if (!mystricmp(ext, "qt") || !mystricmp(ext, "mov"))
			return "video/quicktime";
		else if (!strncmp(buffer, "FORM", 4) && !strncmp(&buffer[8], "ANIM", 4))
			return "video/x-anim";
		else if (!mystricmp(ext, "avi"))
			return "video/x-msvideo";
		else if (mystristr(buffer, "\nFrom:"))
			return "message/rfc822";
	}
	return ctype;
}





