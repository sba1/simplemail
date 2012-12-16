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

/**
 * @file imap_unittest.c
 */

#include "debug.h"
#include "folder.h"
#include "gui_main.h"
#include "imap.h"
#include "progmon.h"
#include "simplemail.h"
#include "support.h"

#include <proto/dos.h>


static int test_progmon(void *udata)
{
	struct progmon *pm;
	int i;

	char *name = (char*)udata;

	if (thread_get() != thread_get_main())
		thread_parent_task_can_contiue();

	if (!(pm = progmon_create()))
		return 0;

	pm->begin(pm,100,name);
	for (i=0;i<100;i++)
	{
		utf8 txt[20];
		sm_snprintf((char*)txt,sizeof(txt),"%d",i);

		pm->working_on(pm,txt);
		pm->work(pm,1);
		Delay(10);
	}
	pm->done(pm);

	progmon_delete(pm);

	return 0;
}

int main(int argc, char **argv)
{
	struct folder *f;

	if (!simplemail_init())
		return 20;

	debug_set_level(20);

	/* We assume that this folder is present and is a IMAP folder */
	f = folder_find_by_name("INBOX");
	if (f && f->imap_server)
	{
		thread_add("Test1", test_progmon, "Test1");
		thread_add("Test2", test_progmon, "Test2");

		gui_loop();

		imap_thread_connect(f);
		Delay(200);

		imap_thread_connect(f);
		Delay(200);
	} else
	{
		Printf("Folder INBOX not found\n");
	}

	simplemail_deinit();
	return 0;
}
