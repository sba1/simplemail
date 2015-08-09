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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include <CUnit/Basic.h>

#include "codesets.h"
#include "mail.h"
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

static unsigned char *simple_mail_with_attachment_filename = "attachment.eml";

/* @Test */
void test_simple_mail_info_with_attachemnt(void)
{
	struct mail_info *m;

	m = mail_info_create_from_file(simple_mail_with_attachment_filename);
	CU_ASSERT(m != NULL);

	CU_ASSERT(m->flags & MAIL_FLAGS_ATTACH);

	mail_info_free(m);
}

/*************************************************************/

/* @Test */
void test_simple_mail_complete_with_attachemnt(void)
{
	int rc;
	struct mail_complete *m, *m1, *m2, *m3;
	void *m1_data, *m2_data;
	int m1_data_len, m2_data_len;
	string s;

	m = mail_complete_create_from_file(simple_mail_with_attachment_filename);
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

	comp.from = "Sebastian Bauer <mail@sebastianbauer.info";
	comp.subject = "Test Subject";
	comp.to = "Sebastian Bauer <mail@sebastianbauer.info";

	fh = fopen("written.eml","wb");
	CU_ASSERT(fh != NULL);
	private_mail_compose_write(fh, &comp);
	fclose(fh);
}

/*************************************************************/

/* @Test */
void test_mail_compose_new_with_attachment(void)
{
	int success;

	FILE *fh;
	struct composed_mail comp = {0};
	struct composed_mail comp_attachment1 = {0};
	struct composed_mail comp_attachment2 = {0};

	success = codesets_init();
	CU_ASSERT(success != 0);

	comp.from = "Sebastian Bauer <mail@sebastianbauer.info";
	comp.subject = "Mail with simple attachment";
	comp.to = "Sebastian Bauer <mail@sebastianbauer.info";
	comp.content_type = "multipart/mixed";

	comp_attachment1.content_type = "text/plain";
	comp_attachment1.text = "Test";
	composed_mail_add(&comp, &comp_attachment1);

	comp_attachment2.content_type = "application/octet-stream";
	composed_mail_add(&comp, &comp_attachment2);

	fh = fopen("written-with-attachment.eml","wb");
	CU_ASSERT(fh != NULL);
	private_mail_compose_write(fh, &comp);
	fclose(fh);

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


/*************************************************************/

/* @Test */
void test_mail_create_for(void)
{
	struct mail_complete *m;

	m = mail_create_for("abcd@zzzzuuuu.qq.qq","test@abcd.deg.def",NULL,NULL,NULL);
	CU_ASSERT(m != NULL);
	CU_ASSERT(m->info != NULL);

	CU_ASSERT(strcmp(m->info->from_addr,"abcd@zzzzuuuu.qq.qq") == 0);
	CU_ASSERT(strcmp(m->info->to_addr,"test@abcd.deg.def") == 0);

	mail_complete_free(m);
}
