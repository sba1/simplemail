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

#include <unistd.h>
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
#include "progmon.h"
#include "request.h"
#include "simplemail.h"
#include "status.h"
#include "support_indep.h"
#include "trans.h"

/*************************************************************/

void error_add_message(char *msg)
{
	printf("error: %s\n", msg?msg:"(nil)");
}

void main_refresh_window_title(unsigned int autocheck_seconds_start)
{
}

void main_set_progress(unsigned int max_work, unsigned int work)
{
}

void main_hide_progress(void)
{
}

void main_set_status_text(char *txt)
{
	printf("%s\n", txt?txt:"(nil)");
}

void progmonwnd_update(void)
{
}

void statuswnd_set_status(char *text)
{
	printf("%s\n", text?text:"(nil)");
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

void main_freeze_mail_list(void)
{
}

void main_thaw_mail_list(void)
{
}

void read_refresh_prevnext_button(struct folder *f)
{
}

void statuswnd_open(void)
{
}

void statuswnd_close(void)
{
}

void statuswnd_set_head(void)
{
}

void statuswnd_set_title(void)
{
}

void statuswnd_mail_list_clear(void)
{
}


void statuswnd_mail_list_freeze(void)
{
}

void statuswnd_mail_list_thaw(void)
{
}

void statuswnd_init_gauge(int maximal)
{
}

void statuswnd_set_gauge(int value)
{
}

void statuswnd_set_gauge_text(char *text)
{
}

int search_has_mails(void)
{
	return 0;
}

/*************************************************************/

static void mails_test_account_callback(account_tested_callback_success_t success)
{
}

/* @Test */
void test_pop3(void)
{
	struct list pop3_list;
	struct account *ac;
	struct pop3_server *pop3_server;
	struct pop3_dl_options dl_options = {0};

	char *profile_path;
	char *pwd;

	debug_init();
	debug_set_level(25);

	pwd = realpath(".", NULL);
	CU_ASSERT(pwd != NULL);

	profile_path = mycombinepath(pwd, "test-account-profile");
	CU_ASSERT(profile_path != NULL);

	config_set_user_profile_directory(profile_path);

	CU_ASSERT(codesets_init() != 0);

	CU_ASSERT(progmon_init() != 0);
	CU_ASSERT(init_threads() != 0);

	CU_ASSERT(load_config() != 0);
	CU_ASSERT(init_folders() != 0);

	ac = account_malloc();
	CU_ASSERT(ac != NULL);
	CU_ASSERT(ac->imap != NULL);
	CU_ASSERT(ac->pop != NULL);

	ac->recv_type = 0;
	if (ac->pop->name) free(ac->pop->name);
	ac->pop->name = mystrdup("localhost");
	CU_ASSERT(ac->pop->name != NULL);

	if (ac->pop->login) free(ac->pop->login);
	ac->pop->login = mystrdup("test");
	CU_ASSERT(ac->pop->login != NULL);

	if (ac->pop->passwd) free(ac->pop->passwd);
	ac->pop->passwd = mystrdup("test");
	CU_ASSERT(ac->pop->passwd != NULL);

	ac->pop->ssl = 0;
	ac->pop->port = 10110;

	insert_config_account(ac);

	mails_test_account(ac, mails_test_account_callback);
	sleep(3);

	account_free(ac);

	cleanup_threads();
	del_folders();
	free_config();
	progmon_deinit();
	codesets_cleanup();

	free(profile_path);
	free(pwd);
}

/*************************************************************/
