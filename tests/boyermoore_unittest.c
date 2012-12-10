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

/********************************************************/

static const int positions[] = {1,7,12,18,24};
static int current_position_idx = 0;

int test_boyermoore_callback(char *x, unsigned int pos, void *user_data)
{
	CU_ASSERT(positions[current_position_idx++] == pos);
	return 1;
}

/* @Test */
void test_boyermoore(void)
{
	char *txt = "qhello2hellohello2hello2hello";
	char *pat = "hello";

	int rel_pos, pos;

	struct boyermoore_context *context;

	context = boyermoore_create_context(pat,strlen(pat));
	CU_ASSERT(context != NULL);

	boyermoore(context,txt,strlen(txt),test_boyermoore_callback,NULL);

	CU_ASSERT(current_position_idx == sizeof(positions)/sizeof(positions[0]));

	pos = 0;
	current_position_idx = 0;
	while ((rel_pos = boyermoore(context,txt+pos,strlen(txt+pos),NULL,NULL)) != -1)
	{
		pos += rel_pos;
		CU_ASSERT(positions[current_position_idx++] == pos);
		pos++;
	}
	boyermoore_delete_context(context);

	CU_ASSERT(current_position_idx == sizeof(positions)/sizeof(positions[0]));
}
