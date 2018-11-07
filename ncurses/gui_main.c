#include <stdlib.h>
#include <stdio.h>

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

