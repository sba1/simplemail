/**
 * @file imap_unittest.c
 */

#include "imap.h"
#include "simplemail.h"

int main(int argc, char **argv)
{
	if (!simplemail_init())
		return 20;

	simplemail_deinit();
	return 0;
}
