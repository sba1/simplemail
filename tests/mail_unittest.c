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

/*************************************************************/

static unsigned char *filename = "test.eml";

/* @Test */
void test_mail_info_create_from_file(void)
{
	struct mail_info *m;

	m = mail_info_create_from_file(filename);

	CU_ASSERT(m != NULL);

	mail_info_free(m);
}

/*************************************************************/

/* @Test */
void test_mail_compose_new(void)
{
	FILE *fh;
	struct composed_mail comp;

	memset(&comp,0,sizeof(comp));

	comp.from = "Sebastian Bauer <mail@sebastianbauer.info";
	comp.subject = "Test Subject";
	comp.to = "Sebastian Bauer <mail@sebastianbauer.info";

	fh = fopen("written.eml","wb");
	CU_ASSERT(fh != NULL);
	private_mail_compose_write(fh, &comp);
	fclose(fh);

//	CU_ASSERT(mail_compose_new(&comp,0) != 0);
}

/*************************************************************/

/* @Test */
void test_mail_info_get_recipient_addresses(void)
{
	FILE *fh;
	struct mail_info *mi;
	struct composed_mail comp;
	char **recipients;

	memset(&comp,0,sizeof(comp));

	comp.from = "test <abcd@doo>";
	comp.subject = "Test Subject";
	comp.to = "test2 <abcd2@doo>, test3 <abcd@doo>";
	comp.cc = "abcd@doo";

	fh = fopen("written2.eml","wb");
	CU_ASSERT(fh != NULL);
	private_mail_compose_write(fh, &comp);
	fclose(fh);

	mi = mail_info_create_from_file("written2.eml");
	CU_ASSERT(mi != NULL);

	recipients = mail_info_get_recipient_addresses(mi);
	CU_ASSERT(recipients != NULL);

	array_sort_uft8(recipients);

	CU_ASSERT(array_length(recipients) == 2);
	CU_ASSERT(strcmp(recipients[0],"abcd2@doo") == 0);
	CU_ASSERT(strcmp(recipients[1],"abcd@doo") == 0);

	array_free(recipients);
	mail_info_free(mi);
}
