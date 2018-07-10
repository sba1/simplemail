/**
 * @file imap_helper.c
 */

#include "imap_helper.h"

#include <ctype.h>
#include <stdlib.h>

/******************************************************************************/

char *imap_get_result(char *src, char *dest, int dest_size)
{
	char c;
	char delim = 0;

	dest_size--;

	dest[0] = 0;
	if (!src) return NULL;

	while ((c = *src))
	{
		if (!isspace((unsigned char)c))
			break;
		src++;
	}

	if (c)
	{
		int i = 0;
		int b = 0;
		int incr_delim = 0;

		if (c == '(') { incr_delim = c; delim = ')';}
		else if (c== '"') delim = '"';
		else if (c== '[') delim = ']';

		if (delim)
		{
			src++;
			b++;
		}

		while ((c = *src))
		{
			if (c == incr_delim)
			{
				b++;
			} else
			if (c == delim && !(--b))
			{
				src++;
				break;
			}

			if (!delim)
			{
				if (isspace((unsigned char)c)) break;
			}

			if (i<dest_size)
				dest[i++] = c;
			src++;
		}
		dest[i] = 0;
		return src;
	}
	return NULL;
}

/******************************************************************************/
