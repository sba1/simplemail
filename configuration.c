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
#include <ctype.h>

#include "configuration.h"
#include "support.h"

int read_line(FILE *fh, char *buf); /* in addressbook.c */

static char *get_config_item(char *buf, char *item)
{
	int len = strlen(item);
	if (!mystrnicmp(buf,item,len))
	{
		char c;
		buf += len;

		/* skip spaces */
		while ((c = *buf) && isspace(c)) buf++;
		if (*buf != '=') return NULL;
		buf++;
		/* skip spaces */
		while ((c = *buf) && isspace(c)) buf++;

		return buf;
	}
	return NULL;
}

struct user user;

void init_config(void)
{
	memset(&user,0,sizeof(struct user));
	user.directory = strdup("PROGDIR:");
}

int load_config(void)
{
	char *buf;

	init_config();

	if ((buf = malloc(512)))
	{
		FILE *fh;

		if (user.directory) strcpy(buf,user.directory);
		else buf[0] = 0;

		sm_add_part(buf,".config",512);

		if ((user.config_filename = strdup(buf)))
		{
			if ((fh = fopen(buf,"r")))
			{
				read_line(fh,buf);
				if (!strncmp("SMCO",buf,4))
				{
					while (read_line(fh,buf))
					{
						char *result;

						if ((result = get_config_item(buf,"EmailAddress")))
							user.config.email = strdup(result);
						if ((result = get_config_item(buf,"RealName")))
							user.config.realname = strdup(result);
						if ((result = get_config_item(buf,"POP00.Login")))
							user.config.pop_login = strdup(result);
						if ((result = get_config_item(buf,"POP00.Server")))
							user.config.pop_server = strdup(result);
						if ((result = get_config_item(buf,"POP00.Password")))
							user.config.pop_password = strdup(result);
						if ((result = get_config_item(buf,"POP00.Delete")))
							user.config.pop_delete = ((*result == 'Y') || (*result == 'y'))?1:0;
						if ((result = get_config_item(buf,"SMTP00.Server")))
							user.config.smtp_server = strdup(result);
						if ((result = get_config_item(buf,"SMTP00.Domain")))
							user.config.smtp_domain = strdup(result);
					}
				}

				fclose(fh);
			}
		}
		free(buf);
	}
	return 1;
}

void save_config(void)
{
	if (user.config_filename)
	{
		
	}
}

