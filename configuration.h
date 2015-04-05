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

#ifndef SM__PHRASE_H
#include "phrase.h"
#endif

#ifndef SM__CODESETS_H
#include "codesets.h"
#endif

struct pop3_server;

#define SHOW_HEADER_NONE			0
#define SHOW_HEADER_FROM			(1<<0)
#define SHOW_HEADER_TO				(1<<1)
#define SHOW_HEADER_CC				(1<<2)
#define SHOW_HEADER_SUBJECT	(1<<3)
#define SHOW_HEADER_DATE			(1<<4)
#define SHOW_HEADER_REPLYTO	(1<<5)
#define SHOW_HEADER_ALL			(1<<31)

/**
 * @brief SimpleMail's main configuration structure
 */
struct config
{
	int from_disk; /* 1 if config has been loaded from disk */

	int dst;

	/**
	 * Indicates that the trash folder should be emptied
	 * on exit.
	 */
	int delete_deleted;

	int receive_preselection; /* 0 no selection, 1 size selection, 2 full selection */
	int receive_size; /* the size in kb */
	int receive_autocheck; /* 0 no auto check */
	int receive_autoifonline; /* before auto check, check if online */
	int receive_autoonstartup; /* check mails at startup */

	int receive_sound;
	char *receive_sound_file;

	int receive_arexx;
	char *receive_arexx_file;

	/* list of all accounts */
	struct list account_list;

	/* list of all signatures */
	struct list signature_list;

	/* list of all phrases */
	struct list phrase_list;

	int signatures_use;

	/* headers */
	unsigned int header_flags;
	char **header_array;

	int write_wrap;
	int write_wrap_type;
	int write_reply_quote;
	int write_reply_stripsig;
	int write_reply_citeemptyl;
	int write_forward_as_attachment;

	char *read_fixedfont;
	char *read_propfont;

	/* Colors */
	int read_background;
	int read_text;
	int read_quoted;
	int read_old_quoted;
	int read_quoted_background;
	int read_link;
	int read_header_background;

	/* Boolean */
	int read_wordwrap;
	int read_link_underlined;
	int read_smilies;
	int read_graphical_quote_bar;
	int readwnd_close_after_last;
	int readwnd_next_after_move;

	/* list of the filters */
	struct list filter_list;

	/* array of "internet connectable" e-mail addresses */
	char **internet_emails;

	/* spam */
	int spam_mark_moved; /* mark mails if they are moved to spam */
	int spam_auto_check;
	int spam_addrbook_is_white;
	char **spam_white_emails;
	char **spam_black_emails;

	/* charset */
	struct codeset *default_codeset; /* the user default codeset, might be NULL */

	/* appicon */
	char *appicon_label; /* the label to be shown with the appicon */
	int appicon_show;    /* when should the appicon be displayed */

	/* startup display folder */
	char *startup_folder_name; /* if NULL folder_incoming() will be shown */

	/* hidden */
	int set_all_stati;           /* expand the context menu of the maillisttree to set all stati */
	int min_classified_mails;    /* number of minimum spam/ham classification to invoke the auto spamcheck */
	int dont_show_shutdown_text; /* Do not show the text in the shutdown window */
	int dont_use_thebar_mcc;     /* Do not use TheBar, also if it's available */
	int dont_add_default_addresses; /* Do not add the default email addresses if they don't exist */
	int dont_jump_to_unread_mail;   /* Do not jump to the first unread mail if changing folders */
	int dont_use_aiss;              /* Do not use AISS icons, also if they're available */
	int dont_draw_alternating_rows;  /* Disable the rendering of alternating rows */
	int row_background;              /* Row color */
	int alt_row_background;          /* Color of alternative row */
	char *ssl_cypher_list;           /* The cypher list used for ssl connections */
};

struct user
{
	char *name; /* name of the user */
	char *directory; /* the directory where all data is saved */
	char *folder_directory; /* the directory where the folders should be saved */
	char *new_folder_directory; /* the new folder directory */

	char *config_filename; /* path to the the configuration */
	char *filter_filename; /* path to the separate filter config file */
	char *signature_filename; /* path to the sparate signature config file */
	char *taglines_filename; /* path to the taglines */

	struct config config;
};

int config_set_user_profile_directory(char *path);

int load_config(void);
void free_config(void);
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
