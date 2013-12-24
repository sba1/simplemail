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
#include "folder.h"
#include "support_indep.h"

/*************************************************************/

void *test_imap_timer_callback(void *data)
{
	thread_abort(thread_get_main());
	return NULL;
}

/* @Test */
void test_imap(void)
{
	struct account *ac;
	struct folder *f;

	debug_init();
	debug_set_level(25);

	config_set_user_profile_directory("imap-profile");

	CU_ASSERT(codesets_init() != 0);

	CU_ASSERT(init_threads() != 0);
	CU_ASSERT(progmon_init() != 0);

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
	ac->imap->passwd = mystrdup("zzz");
	CU_ASSERT(ac->imap->passwd != NULL);

	ac->imap->ssl = 1;
	ac->imap->port = 993;

	insert_config_account(ac);
	account_free(ac);

	folder_create_imap();

	f = folder_find_by_name("localhost");
	CU_ASSERT(f != NULL);

	imap_thread_connect(f);

	thread_wait(test_imap_timer_callback, NULL, 2000);

	del_folders();
	free_config();
	progmon_deinit();
	cleanup_threads();
	codesets_cleanup();
}

/*************************************************************/
