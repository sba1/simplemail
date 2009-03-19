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
** sysprint.c
*/

#include "configuration.h"
#include "debug.h"
#include "smintl.h"
#include "support.h"
#include "sysprint.h"

#include <exec/types.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <stdlib.h>
#include <string.h>

int sysprint_print(PrintHandle *ph, char *txt, unsigned long len)
{
	int rc = 0;
	char *print_txt = utf8tostrcreate(txt, user.config.default_codeset);

	/*txt[len]=0;
	kprintf(txt);*/

	if (print_txt)
	{
		unsigned long print_len = strlen(print_txt);
		rc = (Write(ph->printer, print_txt, print_len) == print_len);
		free(print_txt);
	} else
	{
		rc = (Write(ph->printer, txt, len) == len);
	}

	if(rc == 0)
	{
		sm_request(NULL, _("Failed while sending data to printer!"), _("Okay"));
	}

	return rc;
}

PrintHandle * sysprint_prepare(void)
{
	PrintHandle *rc = NULL;

	rc = malloc(sizeof(PrintHandle));
	if(rc != NULL)
	{
		rc->printer = Open("PRT:", MODE_NEWFILE);
		if(rc->printer == 0)
		{
			free(rc);
			rc = NULL;
			sm_request(NULL, _("Can\'t access printer!"), _("Okay"));
		}
	}

	return rc;
}

void sysprint_cleanup(PrintHandle *ph)
{
	if(ph != NULL)
	{
		if(ph->printer)
		{
			Close(ph->printer);
		}
		free(ph);
	}
}
