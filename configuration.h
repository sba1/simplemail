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
#ifndef SM__CONFIGURATION_H
#define SM__CONFIGURATION_H

struct config
{
	char *realname;
	char *email;
	char *smtp_server;
	char *smtp_domain;
	char *pop_server;
	char *pop_login;
	char *pop_password;
};

struct user
{
	char *name; /* name of the user */
	char *directory; /* the directory where all data is saved */

	char *config_filename; /* path to the the configuration */

	struct config config;
};

int load_config(void);
void save_config(void);

extern struct user user; /* the current user */

#endif

