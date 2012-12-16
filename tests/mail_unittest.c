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
