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
#include <stdio.h>

#include "codecs.h"
#include "lists.h"
#include "mail.h"
#include "support.h"

/* encoding table for the base64 coding */
static const char encoding_table[] =
{
	'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
	'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',
	'0','1','2','3','4','5','6','7','8','9','+','/'
};


/**************************************************************************
 Gets the hexa digit if any (if not it returns 0). Otherwise
 the digit is in *val (only the lower 4 bits get overwritten)
**************************************************************************/
static int get_hexadigit(char c, int *pval)
{
	int val;

	if (c >= '0' && c < '9') val = c - '0';
	else if (c >= 'A' && c <= 'F') val = c - 'A' + 10;
	else if (c >= 'a' && c <= 'f') val = c - 'a' + 10;
	else return 0;

	*pval = ((*pval) & 0xfffffff0) | val;
	return 1;
}

/**************************************************************************
 Decoding a given buffer using the base64 algorithm
**************************************************************************/
char *decode_base64(unsigned char *buf, unsigned int len, unsigned int *ret_len)
{
	char *decoded_buf = NULL;
	FILE *fh = tmpfile();

	if (fh)
	{
		unsigned char *buf_end;
		int decoded_len = 0;

		char decoding_table[256];

		unsigned int decode_buf[4];
		int i,decode_pos;

		memset(decoding_table,-1,sizeof(decoding_table));

		for (i=0;i<64;i++)
			decoding_table[(unsigned char)encoding_table[i]] = i;

		decode_pos = 0;

		buf_end = buf + len;
		while(buf < buf_end)
		{
			if (decoding_table[*buf] != -1)
			{
				decode_buf[decode_pos++] = decoding_table[*buf];

				if (decode_pos == 4)
				{
					unsigned int decoded;
					decoded = (decode_buf[0] << 26) | (decode_buf[1] << 20) | (decode_buf[2] << 14) | (decode_buf[3] << 8);
					fwrite(&decoded,1,3,fh);
					decoded_len+=3;
					decode_pos = 0;
				}
			} else
			{
				if (*buf == '=')
				{
					/* fill the rest with zeros */
					int output_len;
					unsigned int decoded;

					for (i=decode_pos;i<4;i++) decode_buf[i]=0;
					if ((*(buf+1))=='=') output_len = 1;
					else output_len = 2;
					decoded = (decode_buf[0] << 26) | (decode_buf[1] << 20) | (decode_buf[2] << 14) | (decode_buf[3] << 8);
					fwrite(&decoded,1,output_len,fh);
					decoded_len+=output_len;
					break;
				}
			}
			buf++;
		}

		if (decoded_len)
		{
	    fseek(fh,0,SEEK_SET);
			if ((decoded_buf = (char*)malloc(decoded_len+1)))
			{
				fread(decoded_buf,1,decoded_len,fh);
				decoded_buf[decoded_len]=0;
				*ret_len = decoded_len;
			}
		}

		fclose(fh);
	}
	return decoded_buf;
}

/**************************************************************************
 Decoding a given buffer using the quoted_printable algorithm
**************************************************************************/
char *decode_quoted_printable(unsigned char *buf, unsigned int len, unsigned int *ret_len)
{
	char *decoded_buf = NULL;

	FILE *fh = tmpfile();

	if (fh)
	{
		unsigned char *buf_end;
		int decoded_len = 0;

		buf_end = buf + len;
		while(buf < buf_end)
		{
			char c = *buf;
			if (c==13)
			{
				buf++;
				continue;
			}

			if (c=='=')
			{
				int new_c;

				buf++;

				if (get_hexadigit(*buf, &new_c))
				{
					new_c = new_c << 4;
					buf++;
					if (get_hexadigit(*buf, &new_c))
					{
						c = new_c;
					}
				} else
				{
					/* a soft line break (rule 5) */
					if (!(buf = strchr(buf,10))) break;
					buf++;
					continue;
				}
			}

			fputc(c,fh);
			decoded_len++;
			buf++;
		}

		if (decoded_len)
		{
	    fseek(fh,0,SEEK_SET);
			if ((decoded_buf = (char*)malloc(decoded_len+1)))
			{
				fread(decoded_buf,1,decoded_len,fh);
				decoded_buf[decoded_len]=0;
				*ret_len = decoded_len;
			}
		}

		fclose(fh);
	}
	return decoded_buf;
}

/**************************************************************************
 Encodes a given header string, it's correctly attached into the
 old_string
**************************************************************************/
char *encode_header_str(char *old_string, char *toencode, int *line_len_ptr)
{
	int line_len = *line_len_ptr;
	int encoded_len;
	int toencode_len = mystrlen(toencode);
	char *encoded = NULL;
	int max_line_len = 72; /* RFC 2047 says it can be 76 */
	FILE *fh;

	if ((fh = tmpfile()))
	{
		char buf[32];
		int quoted_encoding = 0;

		if (old_string)
		{
			fputs(old_string,fh);
			encoded_len = strlen(old_string);
		} else encoded_len = 0;

		while (toencode_len > 0)
		{
			int next_len = 1;
			char *next_str = toencode;
			unsigned char c = *toencode;

			if (c > 127 && !quoted_encoding)
			{
				/* put the encoded beginning */
				max_line_len = 70; /* so that "?=" can be put on the line */
				sprintf(buf,"=?iso-8859-1?q?=%02lX",c);
				next_str = buf;
				next_len = strlen(next_str);
			}

			if (quoted_encoding && (c > 127 || c == '_' || c == '?' || c == '=' || c == ' '))
			{
				sprintf(buf,"=%02lX",c);
				next_str = buf;
				next_len = strlen(next_str);
			}

			if (line_len + next_len > max_line_len)
			{
				if (quoted_encoding)
				{
					fprintf(fh, "?=\n ");
					encoded_len += 4;
				} else
				{
					fprintf(fh, "\n ");
					encoded_len += 2;
				}
				line_len = 1;
				quoted_encoding = 0;
				max_line_len = 72;
				continue;
			}

			if (c > 127 && !quoted_encoding)
			{
				/* set the flag now so it doesn't conflict with a line end */
				quoted_encoding = 1;
			}

			fwrite(next_str,1,next_len,fh);

			encoded_len += next_len;
			line_len += next_len;

			toencode_len--;
			toencode++;
		}

		if (quoted_encoding)
		{
			/* finish the encoding */
			fprintf(fh, "?=");
			encoded_len += 2;
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
 TODO: Use encode_header_str()
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

		if ((encoded = encode_header_str(NULL,field_contents, &line_len)))
		{
			fprintf(fh, "%s", encoded);
			free(encoded);
		}
		fputc('\n',fh);

		if (header_len = ftell(fh))
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
			char *text = encode_header_str(NULL, address->realname, &line_len);
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

		if (c < 33 || c == 61 || c > 126)
		{
			sprintf(digit_buf,"=%02lX",c);
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
 Encodes the given body. The encoded buffer is allocated with malloc(),
 the length is stored in *ret_len and the used transfer encoding in
 *encoding (MIME Content-Transfer-Encoding)
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
			encode_body_quoted(fh,buf,len);
			*encoding = "quoted-printable";
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
 Identifies a given file and return it's mime type
**************************************************************************/
char *identify_file(char *fname)
{
	char *ctype = "application/octet-stream";
	FILE *fh;

	if (fh = fopen(fname, "r"))
	{
		int i, len;
		static char buffer[1024], *ext;

		len = fread(buffer, 1, 1023, fh);
		buffer[len] = 0;
		fclose(fh);

		if (ext = strrchr(fname, '.')) ++ext;
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
			return "application/x.lzx";
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

