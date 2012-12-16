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
#include <stdint.h>
#include <time.h>
#include <sys/time.h>

#include <CUnit/Basic.h>

#include "configuration.h"
#include "smock.h"

/*******************************************************/

FILE *fopen(const char *fname, const char *mode)
{
	return (FILE*)mock(fname,mode);
}

/*******************************************************/

/* @Test */
void test_save_config_with_failing_fopen(void)
{
	user.config_filename = "test.cfg";
	mock_always_returns(fopen,0);
	save_config();
	mock_clean();
}
