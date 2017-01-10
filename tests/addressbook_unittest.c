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

#include "addressbook.h"
#include "configuration.h"

/*****************************************************************************/

/* @Test */
void test_addressbook_simple(void)
{
	struct addressbook_entry_new new_ab = {0};
	struct addressbook_entry_new *ab;
	char *emails[] = {"abc@defgh.ijk", NULL};
	config_set_user_profile_directory("test-profile");

	CU_ASSERT_EQUAL(load_config(), 1);
	CU_ASSERT_EQUAL(init_addressbook(), 1);

	new_ab.alias = "cd";
	new_ab.realname = "AB CD";
	new_ab.email_array = emails;

	ab = addressbook_add_entry_duplicate(&new_ab);
	CU_ASSERT_PTR_NOT_NULL(ab);

	ab = addressbook_find_entry_by_address(emails[0]);
	CU_ASSERT_PTR_NOT_NULL(ab);
	/* Because it is a duplicate, the addresses should not match (only the contents) */
	CU_ASSERT(new_ab.realname != ab->realname);
	CU_ASSERT(new_ab.alias != ab->alias);
	CU_ASSERT_STRING_EQUAL(new_ab.realname, ab->realname);
	CU_ASSERT_STRING_EQUAL(new_ab.email_array[0], emails[0]);
	CU_ASSERT_STRING_EQUAL(new_ab.alias, ab->alias);

	ab = addressbook_find_entry_by_realname("AB CD");
	CU_ASSERT(new_ab.realname != ab->realname);
	CU_ASSERT(new_ab.alias != ab->alias);
	CU_ASSERT_STRING_EQUAL(new_ab.realname, ab->realname);
	CU_ASSERT_STRING_EQUAL(new_ab.email_array[0], emails[0]);
	CU_ASSERT_STRING_EQUAL(new_ab.alias, ab->alias);

	ab = addressbook_find_entry_by_alias("cd");
	CU_ASSERT(new_ab.realname != ab->realname);
	CU_ASSERT(new_ab.alias != ab->alias);
	CU_ASSERT_STRING_EQUAL(new_ab.realname, ab->realname);
	CU_ASSERT_STRING_EQUAL(new_ab.email_array[0], emails[0]);
	CU_ASSERT_STRING_EQUAL(new_ab.alias, ab->alias);

	cleanup_addressbook();
	free_config();
}

/*****************************************************************************/
