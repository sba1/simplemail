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

#include "configuration.h"
#include "folder.h"
#include "progmon.h"

#include "mainwnd.h"
#include "progmonwnd.h"

/*************************************************************/

void main_hide_progress(void)
{
}

void main_set_progress(unsigned int max_work, unsigned int work)
{
}

/*****************************************************************************/

void main_refresh_folder(struct folder *folder)
{
}

/*****************************************************************************/

void progmonwnd_update(int force)
{
}

/*************************************************************/

/* @Test */
void test_folder(void)
{
	config_set_user_profile_directory("test-profile");

	CU_ASSERT(load_config() != 0);
	CU_ASSERT(init_folders() != 0);

	del_folders();
	free_config();
}

/*************************************************************/

#define MANY_EMAILS_PROFILE "/tmp/sm-many-emails-profile"

static void test_folder_many_mails_rescan_completed(char *folder_path, void *udata)
{
	thread_abort(thread_get());
}

/* @Test */
void test_folder_many_mails(void)
{
	struct folder *f;
	char mail_filename[100];
	int i;

	system("rm -Rf " MANY_EMAILS_PROFILE);
	config_set_user_profile_directory(MANY_EMAILS_PROFILE);

	CU_ASSERT(progmon_init() != 0);
	CU_ASSERT(init_threads() != 0);
	CU_ASSERT(load_config() != 0);
	CU_ASSERT(codesets_init() != 0);
	CU_ASSERT(init_folders() != 0);

	for (i=0;i<5000;i++)
	{
		FILE *fp;
		struct composed_mail cm;
		char subject[100];

		snprintf(mail_filename, sizeof(mail_filename), MANY_EMAILS_PROFILE "/.folders/incoming/mail%06d", i);
		snprintf(subject, sizeof(subject), "Subject %d", i);
		composed_mail_init(&cm);

		CU_ASSERT((fp = fopen(mail_filename, "w")) != NULL);
		cm.from = "Sebastian Bauer <mail@sebastianbauer.info>";
		cm.subject = subject;
		cm.text = "Hello\n";
		private_mail_compose_write(fp, &cm);
		fclose(fp);
	}

	CU_ASSERT(folder_rescan_async(folder_incoming(), NULL, test_folder_many_mails_rescan_completed, NULL) != 0);
	thread_wait(NULL, NULL, NULL, 0);

	CU_ASSERT(folder_save_index(folder_incoming()) != 0);

	del_folders();
	codesets_cleanup();
	free_config();
	cleanup_threads();
	progmon_deinit();
}
