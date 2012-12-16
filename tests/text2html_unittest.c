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

#include "text2html.h"

/********************************************************/

/* @Test */
void test_text2html(void)
{
	char *buf = "Dear Mr. Test,\n\nOn xxxx abcdef@ghijklm wrote:\n> test\n> test\n\ntest :)\n\ntest";
	char *html;

	html = text2html((unsigned char *)buf,strlen(buf),0,NULL);

	printf("%s\n",html);
	CU_ASSERT(html != NULL);

	CU_ASSERT(strstr(html,"mailto:abcdef@ghijklm") != NULL);

	free(html);
}
