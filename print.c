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
** print.c
*/

#include <stdlib.h>

#include "print.h"
#include "mail.h"

#include "sysprint.h"

#define ANSI_NORMAL     "[0m"
#define ANSI_BOLD       "[1m"
#define ANSI_UNDER      "[4m"

/*
** print_mail - prints a given mail.
**
** These are the system-independent routines.
*/
int print_mail(struct mail *m)
{
	int rc = 0;
	char *text;
	PrintHandle *ph;
	unsigned long len;

	mail_decode(m);

	if(m->decoded_data)
	{
		text = m->decoded_data;
		len = m->decoded_len;
	}
	else
	{
		text = m->text+m->text_begin;
		len = m->text_len;
	}

	/* Just a very simple and basic routine so far. */
	ph = sysprint_prepare();
	if(ph != NULL)
	{
		if(sysprint_print(ph, text, len))
		{
			rc = 1; /* Mark success. */
		}
		sysprint_cleanup(ph);
	}

	return rc;
}
