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

/*
** $Id$
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