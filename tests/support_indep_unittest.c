/* First Compile with
 *
 *  gcc mail_unittest.c -g ../*.c -I .. -I ../indep-include/ -I ../gtk/ -DNODEBUG -Dstricmp=strcasecmp -Dstrnicmp=strncasecmp -lcunit 2>error
 *
 * Then execute
 *  perl gen-stubs.pl <error >stubs.c
 *
 * Then compile with
 *  gcc mail_unittest.c -g ../*.c -I .. -I ../indep-include/ -I ../gtk/ -DNODEBUG  -DHAVE_STUBS_C -Dstricmp=strcasecmp -Dstrnicmp=strncasecmp -lcunit
 */

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
