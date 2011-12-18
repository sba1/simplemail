/**
 * @file imap_unittest.c
 */

#include "debug.h"
#include "folder.h"
#include "imap.h"
#include "progmon.h"
#include "simplemail.h"

#include <proto/dos.h>

static void test_progmon(void)
{
	struct progmon *pm;
	int i;

	if (!(pm = progmon_create()))
		return;

	pm->begin(pm,100,"Test progress");
	for (i=0;i<100;i++)
	{
		pm->work(pm,1);
		Delay(10);
	}
	pm->done(pm);

	progmon_delete(pm);
}

int main(int argc, char **argv)
{
	struct folder *f;

	if (!simplemail_init())
		return 20;

	/* We assume that this folder is present and is a IMAP folder */
	f = folder_find_by_name("INBOX");
	if (f && f->imap_server)
	{
		debug_set_level(20);

		test_progmon();

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
