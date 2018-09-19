/***************************************************************************
 SimpleMail - Copyright (C) 2000 Hynek Schlawack and Sebastian Bauer

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
***************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include <CUnit/Basic.h>

#include "codesets.h"
#include "mail.h"
#include "support.h"
#include "support_indep.h"

/*************************************************************/

/* @Test */
void test_mail_filenames_of_new_mails_are_unique(void)
{
	char dir[L_tmpnam];
	char oldpath[380];
	char *filenames[3];
	const int number_of_filenames = sizeof(filenames)/sizeof(filenames[0]);
	int i,j;

	CU_ASSERT(getcwd(oldpath,sizeof(oldpath)) != 0);

	tmpnam(dir);

	CU_ASSERT(sm_makedir(dir) == 1);
	CU_ASSERT(chdir(oldpath) == 0);

	for (i=0; i < number_of_filenames; i++)
	{
		FILE *f;
		filenames[i] = mail_get_new_name(MAIL_STATUS_UNREAD);
		CU_ASSERT(filenames[i] != NULL);
		f = fopen(filenames[i], "wb");
		fclose(f);
	}

	for (i=0; i < number_of_filenames; i++)
	{
		for (j=i+1; j < number_of_filenames; j++)
		{
			/* Strings should never match */
			CU_ASSERT_EQUAL(strcmp(filenames[i],filenames[j]) == 0, 0);
		}
	}

	for (i=0; i < number_of_filenames; i++)
	{
		remove(filenames[i]);
		free(filenames[i]);
	}

	chdir(oldpath);
	remove(dir);
}

/*************************************************************/

/* @Test
 * @File "test.eml"
 * {{{
 * From: Test <abc@def.ghi>
 * To:  xyz@localhost
 * Subject: Test Subject
 * X-Mailer: SimpleMail 0.38 (UNIX/GTK+) E-Mail Client (c) 2000-2011 by Hynek Schlawack and Sebastian Bauer
 * Date: 00 Jan 1900 00:00:00 +0000
 * Importance: low
 * X-SimpleMail-POP3: pop3.def.ghi
 *
 * }}}
 */
void test_mail_info_create_from_file(void)
{
	struct mail_info *m;

	m = mail_info_create_from_file(NULL, "test.eml");

	CU_ASSERT_PTR_NOT_NULL(m);
	CU_ASSERT_STRING_EQUAL(m->from_phrase, "Test");
	CU_ASSERT_STRING_EQUAL(m->from_addr, "abc@def.ghi");
	CU_ASSERT_PTR_NOT_NULL(m->to_list);
	CU_ASSERT_PTR_NULL(mail_get_to_phrase(m));
	CU_ASSERT_STRING_EQUAL(mail_get_to_addr(m), "xyz@localhost");
	CU_ASSERT_PTR_NULL(m->cc_list);
	CU_ASSERT_STRING_EQUAL(m->pop3_server.str, "pop3.def.ghi");
	CU_ASSERT_PTR_NULL(m->reply_addr);
	CU_ASSERT_STRING_EQUAL(m->subject, "Test Subject");
	CU_ASSERT_PTR_NULL(m->message_id);
	CU_ASSERT_PTR_NULL(m->message_reply_id);
	CU_ASSERT_EQUAL(m->seconds, 0);
	CU_ASSERT_EQUAL(m->received, 0);
	CU_ASSERT_PTR_NULL(m->excerpt);
	CU_ASSERT_STRING_EQUAL(m->filename, "test.eml");
	CU_ASSERT_EQUAL(m->reference_count, 0);
	CU_ASSERT_EQUAL(m->tflags & MAIL_TFLAGS_TO_BE_FREED, 0);
#if 0
	CU_ASSERT_PTR_NULL(m->sub_thread_mail);
#endif
	CU_ASSERT_PTR_NULL(m->next_thread_mail);

	mail_info_free(m);
}

/*************************************************************/

/* @Test
 * @File "test-only-from.eml"
 * {{{
 * From: Test <abc@def.ghi>
 * }}}
 */
void test_mail_info_create_from_file_only_from(void)
{
	struct mail_info *m;

	m = mail_info_create_from_file(NULL, "test-only-from.eml");

	CU_ASSERT_PTR_NOT_NULL(m);
	CU_ASSERT_STRING_EQUAL(m->from_phrase, "Test");
	CU_ASSERT_STRING_EQUAL(m->from_addr, "abc@def.ghi");

	mail_info_free(m);
}

/*************************************************************/

/* @Test
 */
void test_mail_info_create_from_file_with_common_context(void)
{
	struct mail_info *m;
	struct mail_info *m2;
	mail_context *mc;
	int pop_id;

	mc = mail_context_create();
	CU_ASSERT_PTR_NOT_NULL(mc);

	/* Read same mail twice to check for string sharing */
	m = mail_info_create_from_file(mc, "test.eml");
	m2 = mail_info_create_from_file(mc, "test.eml");
	pop_id = string_pool_get_id(mc->sp, "pop3.def.ghi");

	CU_ASSERT_PTR_NOT_NULL(m);
	CU_ASSERT_PTR_NOT_NULL(m2);
	CU_ASSERT_NOT_EQUAL(pop_id, -1);

	CU_ASSERT_STRING_EQUAL(m->from_phrase, "Test");
	CU_ASSERT_STRING_EQUAL(m->from_addr, "abc@def.ghi");
	CU_ASSERT_PTR_NOT_NULL(m->to_list);
	CU_ASSERT_PTR_NULL(mail_get_to_phrase(m));
	CU_ASSERT_STRING_EQUAL(mail_get_to_addr(m), "xyz@localhost");
	CU_ASSERT_PTR_NULL(m->cc_list);
	CU_ASSERT_NOT_EQUAL(m->tflags & MAIL_TFLAGS_POP3_ID, 0);
	CU_ASSERT_EQUAL(m->pop3_server.id, pop_id);
	CU_ASSERT_PTR_NULL(m->reply_addr);
	CU_ASSERT_STRING_EQUAL(m->subject, "Test Subject");
	CU_ASSERT_PTR_NULL(m->message_id);
	CU_ASSERT_PTR_NULL(m->message_reply_id);
	CU_ASSERT_EQUAL(m->seconds, 0);
	CU_ASSERT_EQUAL(m->received, 0);
	CU_ASSERT_PTR_NULL(m->excerpt);
	CU_ASSERT_STRING_EQUAL(m->filename, "test.eml");
	CU_ASSERT_EQUAL(m->reference_count, 0);
	CU_ASSERT_EQUAL(m->tflags & MAIL_TFLAGS_TO_BE_FREED, 0);
#if 0
	CU_ASSERT_PTR_NULL(m->sub_thread_mail);
#endif
	CU_ASSERT_PTR_NULL(m->next_thread_mail);

	mail_info_free(m);
	mail_info_free(m2);
	mail_context_free(mc);
}

/*************************************************************/

static unsigned char *simple_mail_with_attachment_filename = "attachment.eml";

/* @Test */
void test_simple_mail_info_with_attachment(void)
{
	struct mail_info *m;

	m = mail_info_create_from_file(NULL, simple_mail_with_attachment_filename);
	CU_ASSERT(m != NULL);

	CU_ASSERT(m->flags & MAIL_FLAGS_ATTACH);

	mail_info_free(m);
}

/*************************************************************/

/* @Test */
void test_simple_mail_complete_with_attachment(void)
{
	int rc;
	struct mail_complete *m, *m1, *m2, *m3;
	void *m1_data, *m2_data;
	int m1_data_len, m2_data_len;
	string s;

	m = mail_complete_create_from_file(NULL, simple_mail_with_attachment_filename);
	CU_ASSERT(m != NULL);
	CU_ASSERT_STRING_EQUAL(m->content_type, "multipart");
	CU_ASSERT_STRING_EQUAL(m->content_subtype, "mixed");

	mail_read_contents(".", m);

	m1 = mail_get_next(m);
	CU_ASSERT_PTR_NOT_NULL(m1);
	CU_ASSERT_STRING_EQUAL(m1->content_type, "text");
	CU_ASSERT_STRING_EQUAL(m1->content_subtype, "plain");
	mail_decoded_data(m1, &m1_data, &m1_data_len);
	if (!string_initialize(&s, 100))
		CU_ASSERT(0);
	string_append_part(&s, (char*)m1_data, m1_data_len);
	CU_ASSERT_STRING_EQUAL(s.str, "Here is an attachment\r\n");

	m2 = mail_get_next(m1);
	CU_ASSERT_PTR_NOT_NULL(m2);
	CU_ASSERT_STRING_EQUAL(m2->content_type, "text");
	CU_ASSERT_STRING_EQUAL(m2->content_subtype, "plain");
	mail_decoded_data(m2, &m2_data, &m2_data_len);
	string_crop(&s, 0, 0);
	string_append_part(&s, (char*)m2_data, m2_data_len);
	CU_ASSERT_STRING_EQUAL(s.str, "attachment\n");

	/* The m2 should have no next mail as it is the last */
	m3 = mail_get_next(m2);
	CU_ASSERT_PTR_NULL(m3);

	CU_ASSERT_PTR_EQUAL(mail_get_root(m), m);
	CU_ASSERT_PTR_NOT_EQUAL(m1, m);
	CU_ASSERT_PTR_NOT_EQUAL(m2, m);
	CU_ASSERT_PTR_NOT_EQUAL(m1, m2);

	mail_complete_free(m);
	free(s.str);
}

/*************************************************************/

/* @Test */
void test_mail_compose_new(void)
{
	FILE *fh;
	struct composed_mail comp;

	memset(&comp,0,sizeof(comp));

	comp.from = "Sebastian Bauer <mail@sebastianbauer.info>";
	comp.subject = "Test Subject";
	comp.to = "Sebastian Bauer <mail@sebastianbauer.info>";

	fh = fopen("written.eml","wb");
	CU_ASSERT(fh != NULL);
	private_mail_compose_write(fh, &comp);
	fclose(fh);
}

/*************************************************************/

/* @Test */
void test_mail_compose_new_wrong_address(void)
{
	FILE *fh;
	struct composed_mail comp;

	memset(&comp,0,sizeof(comp));

	comp.from = "Sebastian Bauer <mail@sebastianbauer.info";
	comp.subject = "Test Subject";
	comp.to = "Sebastian Bauer <mail@sebastianbauer.info";

	fh = fopen("written-wrong-address.eml","wb");
	CU_ASSERT(fh != NULL);
	private_mail_compose_write(fh, &comp);
	fclose(fh);
}

/*************************************************************/

static char *test_attchment1_text = "This mail contains an attachment";
static char *test_attchment2_text = "This is the attachment text";

static void test_write_mail_with_attachment(const char *filename)
{
	FILE *fh;
	struct composed_mail comp = {0};
	struct composed_mail comp_attachment1 = {0};
	struct composed_mail comp_attachment2 = {0};

	comp.from = "Sebastian Bauer <mail@sebastianbauer.info>";
	comp.subject = "Mail with simple attachment";
	comp.to = "Sebastian Bauer <mail@sebastianbauer.info>";
	comp.content_type = "multipart/mixed";

	comp_attachment1.content_type = "text/plain";
	comp_attachment1.text = test_attchment1_text;
	composed_mail_add(&comp, &comp_attachment1);

	comp_attachment2.content_type = "text/plain";
	comp_attachment2.text = test_attchment2_text;
	composed_mail_add(&comp, &comp_attachment2);

	fh = fopen(filename,"wb");
	CU_ASSERT(fh != NULL);
	private_mail_compose_write(fh, &comp);
	fclose(fh);
}

/* @Test */
void test_mail_compose_new_with_attachment(void)
{
	int success;

	success = codesets_init();
	CU_ASSERT(success != 0);

	test_write_mail_with_attachment("written-with-attachment.eml");

	codesets_cleanup();
}

/*************************************************************/

/* @Test */
void test_mail_compose_new_with_attachment_can_be_read_again(void)
{
	int success;

	struct mail_complete *m, *m1, *m2;
	void *m1_data = NULL, *m2_data = NULL;
	int m1_data_len, m2_data_len;

	success = codesets_init();
	CU_ASSERT(success != 0);

	test_write_mail_with_attachment("written-with-attachment2.eml");

	m = mail_complete_create_from_file(NULL, "written-with-attachment2.eml");
	CU_ASSERT(m!=NULL);

	CU_ASSERT(m->info->from_phrase != NULL);
	CU_ASSERT(m->info->from_addr != NULL);
	CU_ASSERT(mail_get_to_phrase(m->info) != NULL);
	CU_ASSERT(mail_get_to_addr(m->info) != NULL);

	CU_ASSERT_STRING_EQUAL(m->info->from_phrase, "Sebastian Bauer");
	CU_ASSERT_STRING_EQUAL(m->info->from_addr, "mail@sebastianbauer.info");
	CU_ASSERT_STRING_EQUAL(mail_get_to_phrase(m->info), "Sebastian Bauer");
	CU_ASSERT_STRING_EQUAL(mail_get_to_addr(m->info), "mail@sebastianbauer.info");

	mail_read_contents(".", m);

	CU_ASSERT_EQUAL(m->num_multiparts, 2);

	mail_decoded_data(m->multipart_array[0], &m1_data, &m1_data_len);
	mail_decoded_data(m->multipart_array[1], &m2_data, &m2_data_len);

	CU_ASSERT(m1_data != NULL);
	CU_ASSERT(m2_data != NULL);
	CU_ASSERT_STRING_EQUAL((char*)m1_data, test_attchment1_text);
	CU_ASSERT_STRING_EQUAL((char*)m2_data, test_attchment2_text);

	mail_complete_free(m);

	codesets_cleanup();
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

	mi = mail_info_create_from_file(NULL, "written2.eml");
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


/*************************************************************/

/* @Test */
void test_mail_create_for(void)
{
	struct mail_complete *m;

	m = mail_create_for("abcd@zzzzuuuu.qq.qq","test@abcd.deg.def",NULL,NULL,NULL);
	CU_ASSERT(m != NULL);
	CU_ASSERT(m->info != NULL);

	CU_ASSERT(strcmp(m->info->from_addr,"abcd@zzzzuuuu.qq.qq") == 0);
	CU_ASSERT(strcmp(mail_get_to_addr(m->info),"test@abcd.deg.def") == 0);

	mail_complete_free(m);
}
