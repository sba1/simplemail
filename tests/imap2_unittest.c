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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include <CUnit/Basic.h>

#include "account.h"
#include "configuration.h"
#include "debug.h"
#include "folder.h"
#include "mail.h"
#include "simplemail.h"
#include "support_indep.h"

/*************************************************************/

int open_socket_lib(void)
{
	return 1;
}

void close_socket_lib(void)
{
}

void myclosesocket(int fd)
{
	close(fd);
}

long tcp_errno(void)
{
	return errno;
}

void main_set_progress(unsigned int max_work, unsigned int work)
{
}

void main_hide_progress(void)
{
}

void main_set_status_text(char *txt)
{
	printf("%s\n", txt);
}

void progmonwnd_update(void)
{
}

void statuswnd_set_status(char *text)
{
	printf("%s\n", text);
}

void main_refresh_folders(void)
{
}

void search_refresh_folders(void)
{
}

void config_refresh_folders(void)
{
}

void filter_update_folder_list(void)
{
}

struct folder *main_get_folder(void)
{
	return NULL;
}

void main_refresh_folder(struct folder *f)
{
}

void read_refresh_prevnext_button(struct folder *f)
{
}

/*************************************************************/

void *test_imap_timer_callback(void *data)
{
	static enum
	{
		SWITCH_TO_INBOX,
		ADD_NEW_MAIL,
		QUIT
	} state;

	SM_DEBUGF(20,("state = %d\n", state));
	switch (state)
	{
		case	SWITCH_TO_INBOX:
				{
					struct folder *f;

					f = folder_find_by_imap("test","localhost","INBOX");
					CU_ASSERT(f != NULL);

					imap_thread_connect(f);
				}
				state = ADD_NEW_MAIL;
				break;

		case	ADD_NEW_MAIL:
				{
					char *name = realpath("test.eml", NULL);
					struct folder *f;
					f = folder_find_by_imap("test","localhost","INBOX");
					CU_ASSERT(f != NULL);
					callback_new_mail_to_folder(name, f);
					free(name);
				}
				state = QUIT;
				break;

		case	QUIT:
				thread_abort(thread_get_main());
				break;
	}
	return NULL;
}

/* @Test */
void test_imap(void)
{
	struct account *ac;
	struct folder *f;

	char *imap_profile;

	debug_init();
	debug_set_level(25);

	imap_profile = realpath("imap-profile", NULL);
	CU_ASSERT(imap_profile != NULL);

	config_set_user_profile_directory(imap_profile);

	CU_ASSERT(codesets_init() != 0);

	CU_ASSERT(progmon_init() != 0);
	CU_ASSERT(init_threads() != 0);

	CU_ASSERT(load_config() != 0);
	CU_ASSERT(init_folders() != 0);

	ac = account_malloc();
	CU_ASSERT(ac != NULL);
	CU_ASSERT(ac->imap != NULL);

	ac->recv_type = 1;

	if (ac->imap->name) free(ac->imap->name);
	ac->imap->name = mystrdup("localhost");
	CU_ASSERT(ac->imap->name != NULL);

	if (ac->imap->login) free(ac->imap->login);
	ac->imap->login = mystrdup("test");
	CU_ASSERT(ac->imap->login != NULL);

	if (ac->imap->passwd) free(ac->imap->passwd);
	ac->imap->passwd = mystrdup("test");
	CU_ASSERT(ac->imap->passwd != NULL);

	ac->imap->ssl = 0;
	ac->imap->port = 10143;

	insert_config_account(ac);
	account_free(ac);

	folder_create_imap();

	f = folder_find_by_name("localhost");
	CU_ASSERT(f != NULL);

	/* Doing this twice is intended */
	imap_thread_connect(f);
	imap_thread_connect(f);

	thread_wait(test_imap_timer_callback, NULL, 2000);

	cleanup_threads();
	del_folders();
	free_config();
	progmon_deinit();
	codesets_cleanup();

	free(imap_profile);
}

/*************************************************************/
