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

int init_suite1(void)
{
	return 0;
}

int clean_suite1(void)
{
	return 0;
}

/********************************************************/

void test_LONGEST_COMMON_PREFIX(void)
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

void test_LONGEST_COMMON_SUBSTRING(void)
{
	char *str[] =
	{
			"aaabbbcccdddeeefffaaa",
			"aaadddeeefffggg",
			"ddddddeeefffggg"
	};
	char *str2[] =
	{
			"aaabbbcccdddxeeefffaaa",
			"aaadddeeefffggg",
			"ddddddeeefffggg"
	};
	char *str3[] =
	{
			"abcdef",
			"gggggg",
			"hijkl"
	};
	char *str4[]={
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

int main()
{
	CU_pSuite pSuite = NULL;

	/* initialize the CUnit test registry */
	if (CUE_SUCCESS != CU_initialize_registry())
		return CU_get_error();

	/* add a suite to the registry */
	if (!(pSuite = CU_add_suite("Suite_1", init_suite1, clean_suite1)))
	{
		CU_cleanup_registry();
		return CU_get_error();
	}

	/* add the tests to the suite */
	if ((NULL == CU_add_test(pSuite, "test of longest_common_prefix()", test_LONGEST_COMMON_PREFIX)) ||
		(NULL == CU_add_test(pSuite, "test of longest_common_substring()", test_LONGEST_COMMON_SUBSTRING)) ||
	    0)
	{
		CU_cleanup_registry();
		return CU_get_error();
	}

	/* Run all tests using the CUnit Basic interface */
	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();
	CU_cleanup_registry();

	return CU_get_error();
}

