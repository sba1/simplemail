/**
 * @file imap_unittest.c
 */

#include "debug.h"
#include "folder.h"
#include "gui_main.h"
#include "imap.h"
#include "progmon.h"
#include "simplemail.h"

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
