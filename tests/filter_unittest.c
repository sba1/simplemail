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
