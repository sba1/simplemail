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
** configuration.h
*/

#ifndef SM__CONFIGURATION_H
#define SM__CONFIGURATION_H

#ifndef SM__LISTS_H
#include "lists.h"
#endif

#ifndef SM__ACOUNT_H
#include "account.h"
#endif

struct pop3_server;

struct config
{
	int dst;

	int receive_preselection; /* 0 no selection, 1 size selection, 2 full selection */
	int receive_size; /* the size in kb */
	int receive_autocheck; /* 0 no auto check */

	/* list of all accounts */
	struct list account_list;

	/* list of all signatures */
	struct list signature_list;

	/* list of all phrases */
	struct list phrase_list;

	int signatures_use;

	char *read_fixedfont;
	char *read_propfont;

	/* Colors */
	int read_background;
	int read_text;
	int read_quoted;
	int read_old_quoted;
	int read_link;
	
	/* Boolean */
	int read_wordwrap;
	int read_link_underlined;
	int read_smilies;

	/* list of the filters */
	struct list filter_list; 

	/* array of "internet connectable" e-mail addresses */
	char **internet_emails;
};

struct user
{
	char *name; /* name of the user */
	char *directory; /* the directory where all data is saved */

	char *config_filename; /* path to the the configuration */
	char *filter_filename; /* path to the separate filter config file */
	char *signature_filename; /* path to the sparate signature config file */

	struct config config;
};

int load_config(void);
void save_config(void);
void save_filter(void);

void clear_config_accounts(void);
void insert_config_account(struct account *account);

struct signature *find_config_signature_by_name(char *name);
void clear_config_signatures(void);
void insert_config_signature(struct signature *signature);

void clear_config_phrases(void);
void insert_config_phrase(struct phrase *phrase);

extern struct user user; /* the current user */

#endif

