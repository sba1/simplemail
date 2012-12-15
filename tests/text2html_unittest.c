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

#include "text2html.h"

/********************************************************/

/* @Test */
void test_text2html(void)
{
	char *buf = "Dear Mr. Test,\n\nOn xxxx abcdef@ghijklm wrote:\n> test\n> test\n\ntest :)\n\ntest";
	char *html;

	html = text2html((unsigned char *)buf,strlen(buf),0,NULL);

	printf("%s\n",html);
	CU_ASSERT(html != NULL);

	CU_ASSERT(strstr(html,"mailto:abcdef@ghijklm") != NULL);

	free(html);
}
