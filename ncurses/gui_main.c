#include <stdlib.h>
#include <stdio.h>

#include "gui_main.h"

/*****************************************************************************/

int gui_parseargs(int argc, char *argv[])
{
	return 1;
}

/*****************************************************************************/

int main(int argc, char *argv[])
{
	if (!simplemail_init())
	{
		fprintf(stderr, "SimpleMail initialization failed!\n");
		return EXIT_FAILURE;
	}

	simplemail_deinit();

	return EXIT_SUCCESS;
}

