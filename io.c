/*
** io.c
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "io.h"

void tell(char *str)
{
	printf("SimpleMail: %s\n", str);
}

void missing_lib(char *name, int ver)
{
	char *str;
	static const char tmpl[] = "Can\'t open %s V%d!";
	int len;
	
	len = (strlen(tmpl) - 4) + (strlen(name) + 5) + 1;
	
	str = malloc(len);
	if(str != NULL)
	{
		sprintf(str, tmpl, name, ver);
		tell(str);
		free(str);
	}
}