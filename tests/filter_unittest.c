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
	struct filter_rule *fr;

	char *subjects[]={
			"[simplemail] test1",
			"[simplemail] test2",
			"Re: [simplemail] test2",
			"[simplemail] Re: test2"
	};

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

/* @Test */
void test_filter_rule_create_from_common_sorted_recipients(void)
{
	struct filter *f;
	struct filter_rule *fr;
	char *a1[] = {"abcdef@eeee","bcdef@eeee", NULL};
	char *a2[] = {"bcdef@eeee","zzzz@eeee", NULL};
	char **addresses[] = {a1,a2};

	fr = filter_rule_create_from_common_sorted_recipients(addresses,2);
	CU_ASSERT(fr != NULL);
	CU_ASSERT(fr->type == RULE_RCPT_MATCH);
	CU_ASSERT(fr->u.rcpt.rcpt[0] != NULL);
	CU_ASSERT(strcmp(fr->u.rcpt.rcpt[0],"bcdef@eeee")==0);

	f = filter_create();
	CU_ASSERT(f != NULL);

	filter_add_rule(f,fr);
	filter_dispose(f);
}
