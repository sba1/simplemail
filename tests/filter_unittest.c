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

/* @Test */
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
