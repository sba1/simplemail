/**
 * @file
 */

#include <CUnit/Basic.h>

#include "imap_helper.h"

/******************************************************************************/

/* @Test */
void test_imap_get_result(void)
{
	char *res;
	char dest[200];
	int dest_size = sizeof(dest);

	res = imap_get_result(" ANSWER1 ANSWER2 (ANSWER3a ANSWER3b) \"ANSWER4\"",dest,dest_size);
	CU_ASSERT_STRING_EQUAL(dest, "ANSWER1");

	res = imap_get_result(res,dest,dest_size);
	CU_ASSERT_STRING_EQUAL(dest, "ANSWER2");

	res = imap_get_result(res,dest,dest_size);
	CU_ASSERT_STRING_EQUAL(dest, "ANSWER3a ANSWER3b");

	res = imap_get_result(res,dest,dest_size);
	CU_ASSERT_STRING_EQUAL(dest, "ANSWER4");
}

/******************************************************************************/
