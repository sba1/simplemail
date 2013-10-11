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
#include "filter.h"
#include "support_indep.h"

/*******************************************************/

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

/*******************************************************/

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

/*******************************************************/

#define NUM_MAILS 3

static struct mail_info *get_first_mail_info(void *handle, void *userdata)
{
	unsigned int *h = (unsigned int*)handle;
	struct mail_info **m;

	if (NUM_MAILS == 0)
		return NULL;

	m = (struct mail_info**)userdata;
	*h = 0;
	return m[*h];
}

static struct mail_info *get_next_mail_info(void *handle, void *userdata)
{
	unsigned int *h = (unsigned int*)handle;
	struct mail_info **m;

	if (*h == NUM_MAILS-1) return NULL;
	m = (struct mail_info**)userdata;
	*h = *h + 1;
	return m[*h];
}

/*******************************************************/

/* @Test */
void test_filter_rule_create_subject_rule_from_mail_iterator(void)
{
	struct filter_rule *fr;
	struct filter *f;

	struct mail_info *m[NUM_MAILS];
	int i;

	for (i=0;i<sizeof(m)/sizeof(*m);i++)
	{
		char buf[128];
		snprintf(buf,sizeof(buf),"%d%dTest%d%d\n",i,i,i,i);

		m[i] = mail_info_create();
		CU_ASSERT(m[i] != NULL);

		m[i]->subject = (utf8*)mystrdup(buf);
		CU_ASSERT(m[i]->subject != NULL);
	}

	f = filter_create();
	CU_ASSERT(f != NULL);

	fr = filter_rule_create_from_mail_iterator(FRCT_SUBJECT,-1,get_first_mail_info,get_next_mail_info,m);
	CU_ASSERT(fr != NULL);
	CU_ASSERT(!strcmp("Test",fr->u.subject.subject[0]));

	for (i=0;i<sizeof(m)/sizeof(*m);i++)
		mail_info_free(m[i]);

	filter_add_rule(f,fr);
	filter_dispose(f);
}

/*******************************************************/

/* @Test */
void test_filter_rule_create_recipient_rule_from_mail_iterator(void)
{
	struct filter_rule *fr;
	struct filter *f;

	struct mail_info *m[NUM_MAILS];
	int i;

	for (i=0;i<sizeof(m)/sizeof(*m);i++)
	{
		char buf[128];
		snprintf(buf,sizeof(buf),"test%d@abcdef.de,sba@zzzqqq.de\n",i);

		m[i] = mail_info_create();
		CU_ASSERT(m[i] != NULL);

		m[i]->to_list = create_address_list(buf);
		CU_ASSERT(m[i]->to_list != NULL);
	}

	f = filter_create();
	CU_ASSERT(f != NULL);

	fr = filter_rule_create_from_mail_iterator(FRCT_RECEPIENTS,-1,get_first_mail_info,get_next_mail_info,m);
	CU_ASSERT(fr != NULL);

	CU_ASSERT(strcmp("sba@zzzqqq.de",fr->u.rcpt.rcpt[0])==0);

	for (i=0;i<sizeof(m)/sizeof(*m);i++)
		mail_info_free(m[i]);

	filter_add_rule(f,fr);
	filter_dispose(f);
}

/*******************************************************/

/* @Test */
void test_filter_rule_create_from_rule_from_mail_iterator_all_match(void)
{
	struct filter_rule *fr;
	struct filter *f;

	struct mail_info *m[NUM_MAILS];
	int i;

	for (i=0;i<sizeof(m)/sizeof(*m);i++)
	{
		m[i] = mail_info_create();
		CU_ASSERT(m[i] != NULL);

		m[i]->from_addr = (utf8*)mystrdup("sba@zzzqqq.de");
		CU_ASSERT(m[i]->from_addr != NULL);
	}

	f = filter_create();
	CU_ASSERT(f != NULL);

	fr = filter_rule_create_from_mail_iterator(FRCT_FROM,-1,get_first_mail_info,get_next_mail_info,m);
	CU_ASSERT(fr != NULL);
	CU_ASSERT(fr->u.from.from[0] != NULL);
	CU_ASSERT(strcmp("sba@zzzqqq.de",fr->u.from.from[0])==0);

	for (i=0;i<sizeof(m)/sizeof(*m);i++)
		mail_info_free(m[i]);

	filter_add_rule(f,fr);
	filter_dispose(f);
}

/*******************************************************/

/* @Test */
void test_filter_rule_create_from_rule_from_mail_iterator_one_mismatch(void)
{
	struct filter_rule *fr;
	struct filter *f;

	struct mail_info *m[NUM_MAILS];
	int i;

	for (i=0;i<sizeof(m)/sizeof(*m);i++)
	{
		m[i] = mail_info_create();
		CU_ASSERT(m[i] != NULL);

		m[i]->from_addr = (utf8*)mystrdup(i==0?"sba@zzzqqqq.de":"sba@zzzqqq.de");
		CU_ASSERT(m[i]->from_addr != NULL);
	}

	f = filter_create();
	CU_ASSERT(f != NULL);

	fr = filter_rule_create_from_mail_iterator(FRCT_FROM,-1,get_first_mail_info,get_next_mail_info,m);
	CU_ASSERT(fr == NULL || fr->u.from.from[0] == NULL);

	for (i=0;i<sizeof(m)/sizeof(*m);i++)
		mail_info_free(m[i]);

	if (fr)
		filter_add_rule(f,fr);
	filter_dispose(f);
}
