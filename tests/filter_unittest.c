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

#include "filter.h"

#ifdef HAVE_STUBS_C
#include "stubs.c"
#endif


int init_suite1(void)
{
	return 0;
}

int clean_suite1(void)
{
	return 0;
}

/********************************************************/

void test_filter_rule_create_from_strings(void)
{
	struct filter *f;

	char *subjects[]={
			"[simplemail] test1",
			"[simplemail] test2",
			"Re: [simplemail] test2",
			"[simplemail] Re: test2"
	};
	struct filter_rule *fr;

	fr = filter_rule_create_from_strings(subjects,4,RULE_SUBJECT_MATCH);

	CU_ASSERT(fr != NULL);
	CU_ASSERT(fr->type == RULE_SUBJECT_MATCH);
	CU_ASSERT(fr->u.subject.subject != NULL);
	CU_ASSERT(strcmp(fr->u.subject.subject[0],"[simplemail] ")==0);

	f = filter_create();
	CU_ASSERT(f != NULL);

	filter_add_rule(f,fr);

	filter_dispose(f);
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
	if ((NULL == CU_add_test(pSuite, "test of filter_new_from_mails()", test_filter_rule_create_from_strings)) ||
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

