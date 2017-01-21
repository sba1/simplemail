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
#include <stdint.h>
#include <time.h>
#include <sys/time.h>

#include <CUnit/Basic.h>

#include "codesets.h"

/*******************************************************/

/* @Test */
void test_isascii7(void)
{
	CU_ASSERT_NOT_EQUAL(isascii7("ascii7"),0);
	CU_ASSERT_NOT_EQUAL(isascii7(""),0);
	CU_ASSERT_EQUAL(isascii7("ö"),0);
}

/*******************************************************/

/* @Test */
void test_utf8len(void)
{
	CU_ASSERT_EQUAL(utf8len("ö"),1);
}

/*******************************************************/

/* @Test */
void test_utf8stricmp(void)
{
	CU_ASSERT_EQUAL(utf8stricmp("ößAF","ößaf"),0);
	CU_ASSERT_EQUAL(utf8stricmp("abcd","abCd"),0);
	CU_ASSERT(utf8stricmp("abcd","abc") > 0);
	CU_ASSERT(utf8stricmp("mßabcd","Nßabcd") < 0);
}

/*******************************************************/

/* @Test */
void test_utf8stricmp_len(void)
{
	CU_ASSERT_EQUAL(utf8stricmp_len("ößAF","ößaf",4),0);
	CU_ASSERT_EQUAL(utf8stricmp_len("abcd","abCd",4),0);
	CU_ASSERT(utf8stricmp_len("abcd","abc",3) == 0);
	CU_ASSERT(utf8stricmp_len("mßabcd","Nßabcd", 1) < 0);
}

/*******************************************************/

/* @Test */
void test_codeset(void)
{
	struct codeset *cs;

	CU_ASSERT(codesets_init() != 0);

	CU_ASSERT((cs = codesets_find(NULL)) != NULL);

	codesets_cleanup();
}
