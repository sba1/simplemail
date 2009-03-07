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

#ifdef HAVE_STUBS_C
#include "stubs.c"
#endif

unsigned int sm_get_seconds(int day, int month, int year)
{
        struct tm tm;
        time_t t;

        if (year < 1978) return 0;

        tm.tm_sec = 0;
        tm.tm_min = 0;
        tm.tm_hour = 0;
        tm.tm_mday = day;
        tm.tm_mon = month - 1;
        tm.tm_year = year - 1900;
        tm.tm_isdst = -1;

        t = mktime(&tm);

        tm.tm_sec = 0;
        tm.tm_min = 0;
        tm.tm_hour = 0;
        tm.tm_mday = 1;
        tm.tm_mon = 0;
        tm.tm_year = 78;
        tm.tm_isdst = 0;
        t -= mktime(&tm);
        return (unsigned int)t;
}

int sm_get_gmt_offset(void)
{
	struct timezone tz;
	struct timeval tv;

	gettimeofday(&tv,&tz);
	return tz.tz_minuteswest;
}

/*************************************************************/

static unsigned char *filename = "test.eml";


int init_suite1(void)
{
	return 0;
}

int clean_suite1(void)
{
	return 0;
}

void test_MAIL_INFO_CREATE_FROM_FILE(void)
{
	struct mail_info *m;

	m = mail_info_create_from_file(filename);

	CU_ASSERT(m != NULL);

	mail_info_free(m);
}

void test_MAIL_COMPOSE_NEW(void)
{
}

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
	if ((NULL == CU_add_test(pSuite, "test of mail_info_complete_create_from_file()", test_MAIL_INFO_CREATE_FROM_FILE)) ||
	    (NULL == CU_add_test(pSuite, "test of mail_compose_new()", test_MAIL_COMPOSE_NEW)))
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

