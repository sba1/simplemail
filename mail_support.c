/**
 * @file some utility functions useful when creating mails.
 */

#include <ctype.h>
#include <string.h>

#include "configuration.h"
#include "support_indep.h"

/*
 * Index into the table below with the first byte of a UTF-8 sequence to
 * get the number of trailing bytes that are supposed to follow it.
 */

static const char trailingBytesForUTF8[256] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3,4,4,4,4,5,5,5,5
};

/**
 * Like strncpy() but for mail headers, returns the length of the string.
 *
 * @param dest
 * @param src
 * @param n
 * @return
 */
int mailncpy(char *dest, const char *src, int n)
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
		if (c==10 || c == 13 || c == 27) continue;
		if (c=='\t') c=' ';
		len++;
		*dest_ptr++ = c;
	}

  return len;
}

/**
 * @brief Determines the length (in bytes) from now until a wordend.
 *
 * @param buf defines the input which must be UTF8
 * @return
 */
static int word_length(const char *buf)
{
	unsigned char c;
	int len = 0;

  /* if word length counting starts on a space we count every space to the the word */
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
		buf += trailingBytesForUTF8[c] + 1;
	}
	return len;
}

/**
 * Copies to quoting chars in to the buffer. len is the length of the
 * buffer to avoid overflow.
 *
 * @param buf
 * @param len
 * @param text
 */
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

/**
 * Quotes the given text. The preferences entry
 * user.config.write_reply_citeemptyl is respected.
 *
 * @param src
 * @param len
 * @return
 */
char *quote_text(char *src, int len)
{
	static char temp_buf[128];
	int temp_len = 0;
	string cited;

	if (string_initialize(&cited,len))
	{
		int newline = 1;
		int wrapped = 0; /* needed for user.config.write_reply_quote */
		int line_len = 0;

		if (user.config.write_reply_quote)
		{
			quoting_chars(temp_buf,sizeof(temp_buf),src);
			temp_len = strlen(temp_buf);
		}

		while (len)
		{
			unsigned char c = *src;
			int char_len; /* utf8 characters are not always one byte in size */

			if (c==13)
			{
				src++;
				len--;
				continue;
			}

			if (c==10)
			{
				src++;
				len--;

				/* This actually means "wrap before quoting" */
				if (user.config.write_reply_quote)
				{
					quoting_chars(temp_buf,sizeof(temp_buf),src);

					if (temp_len == strlen(temp_buf) && wrapped)
					{
						/* the text has been wrapped previously and the quoting chars
						   are the same like the previous line, so the following text
							 probably belongs to the same paragraph */

						len -= temp_len; /* skip the quoting chars */
						src += temp_len;
						wrapped = 0;

						/* add a space to if this was the first quoting */
						if (!temp_len)
						{
							string_append_char(&cited,' ');
							line_len++;
						}
						continue;
					}
					temp_len = strlen(temp_buf);
					wrapped = 0;
				}

				/* If newline is already true this is a empty line */
				if (newline && user.config.write_reply_citeemptyl)
					string_append_char(&cited,'>');
				string_append_char(&cited,'\n');
				newline = 1;

				line_len = 0;

				/* Strip the signature */
				if (len >= 4 && user.config.write_reply_stripsig && !strncmp(src,"-- \n",4))
					break;

				continue;
			}

			if (newline)
			{
				if (user.config.write_wrap)
				{
					if (!strlen(temp_buf)) { string_append(&cited,"> "); line_len+=2;}
					else {string_append_char(&cited,'>'); line_len++;}
				} else
				{
					if (c=='>') { string_append_char(&cited,'>'); line_len++;}
					else { string_append(&cited,"> "); line_len+=2;}
				}
				newline = 0;
			}

			/* This actually means "wrap before quoting" */
			if (user.config.write_reply_quote)
			{
				if (isspace(c) && line_len + word_length(src) >= user.config.write_wrap)
				{
					src++;
					string_append(&cited,"\n>");
					string_append(&cited,temp_buf);
					string_append_char(&cited,' ');
					line_len=strlen(temp_buf)+2;
					wrapped = 1;		/* indicates that a word has been wrapped manually */
					continue;
				}
			}

			/* write out next character, it's an utf8 one */
			char_len = trailingBytesForUTF8[c];
			len -= char_len + 1;
			if (len < 0) break; /* input not valid */
			do
			{
				c = *src++; /* small overhead because for the first time we already have c */
				string_append_char(&cited,c);
			} while (char_len--);
			line_len++;
		}

		return cited.str;
	}
	return NULL;
}
