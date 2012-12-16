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

#include "mail.h"
#include "support_indep.h"

/********************************************************/

/* @Test */
void test_longest_common_prefix(void)
{
	char *str[] =
	{
			"abcde",
			"abcdf",
			"abcf"
	};

	char *str2[] =
	{
			"abcd",
			"efgh",
			"ijkl"
	};

	CU_ASSERT(longest_common_prefix(str, 3) == 3);
	CU_ASSERT(longest_common_prefix(NULL, 0) == 0);
	CU_ASSERT(longest_common_prefix(str, 1) == 5);
	CU_ASSERT(longest_common_prefix(str2, 3) == 0);
}

/********************************************************/

/* @Test */
void test_longest_common_substring(void)
{
	const char *str[] =
	{
			"aaabbbcccdddeeefffaaa",
			"aaadddeeefffggg",
			"ddddddeeefffggg"
	};
	const char *str2[] =
	{
			"aaabbbcccdddxeeefffaaa",
			"aaadddeeefffggg",
			"ddddddeeefffggg"
	};
	const char *str3[] =
	{
			"abcdef",
			"gggggg",
			"hijkl"
	};
	const char *str4[]={
			"[simplemail] test1",
			"[simplemail] test2",
			"Re: [simplemail] test2",
			"[simplemail] Re: test2"};

	int len = 0;
	int pos_in_a = 0;

	longest_common_substring(str, 3, &pos_in_a, &len);
	CU_ASSERT(len == 9);
	CU_ASSERT(pos_in_a == 9);

	longest_common_substring(str2, 3, &pos_in_a, &len);
	CU_ASSERT(len == 6);
	CU_ASSERT(pos_in_a == 13);

	longest_common_substring(str3, 3, &pos_in_a, &len);
	CU_ASSERT(len == 0);

	longest_common_substring(str4, 4, &pos_in_a, &len);
	CU_ASSERT(len == 13);
	CU_ASSERT(pos_in_a == 0);
}


/********************************************************/

/* @Test */
void test_array_sort(void)
{
	char **a = NULL;

	array_sort_uft8(a);

	a = array_add_string(a,"zfc");
	CU_ASSERT(a != NULL);
	a = array_add_string(a,"abd");
	CU_ASSERT(a != NULL);
	a = array_add_string(a,"afc");
	CU_ASSERT(a != NULL);

	array_sort_uft8(a);

	CU_ASSERT(!strcmp(a[0],"abd"));
	CU_ASSERT(!strcmp(a[1],"afc"));
	CU_ASSERT(!strcmp(a[2],"zfc"));

	array_free(a);
}
