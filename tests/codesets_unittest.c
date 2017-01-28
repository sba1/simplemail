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
void test_utf8tolower(void)
{
	char dest[7] = {0};

	CU_ASSERT_EQUAL(utf8tolower("Ö", dest), 2);
	CU_ASSERT_STRING_EQUAL(dest, "ö");
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

static int check_match_mask(const char *expected, match_mask_t *actual)
{
	int i;
	int l = strlen(expected);

	for (i=0; i < l; i++)
	{
		unsigned int mp = match_bitmask_pos(i);

		if (expected[i] == '1')
		{
			if (!(actual[mp] & match_bitmask(i)))
			{
				printf("expected 1 at %d (mp=%d, actual=%x)\n", i, mp, actual[mp]);
				return 0;
			}
		} else
		{
			if (actual[mp] & match_bitmask(i))
			{
				printf("expected 0 at %d\n", i);
				return 0;
			}
		}
	}
	return 1;
}

/* @Test */
void test_utf8match(void)
{
	const char *txt = "TextTextText";
	int txt_len = strlen(txt);
	match_mask_t m[txt_len/sizeof(match_mask_t)+1];

	memset(&m, 0, txt_len/sizeof(match_mask_t)+1);

	CU_ASSERT(utf8match(txt, "xe", 0, NULL) == 1);
	CU_ASSERT(utf8match(txt, "xe", 0, m) == 1);
	CU_ASSERT(check_match_mask("001001000000", m) == 1);

	CU_ASSERT(utf8match(txt, "tz", 0, NULL) == 0);
	CU_ASSERT(utf8match(txt, "tz", 0, m) == 0);

	CU_ASSERT(utf8match(txt, "TTT", 0, NULL) == 1);
	CU_ASSERT(utf8match(txt, "TTT", 0, m) == 1);
	CU_ASSERT(check_match_mask("100010001000", m) == 1);

	CU_ASSERT(utf8match(txt, "eee", 0, NULL) == 1);
	CU_ASSERT(utf8match(txt, "eee", 0, m) == 1);
	CU_ASSERT(check_match_mask("010001000100", m) == 1);

	CU_ASSERT(utf8match(txt, "eeee", 0, NULL) == 0);
	CU_ASSERT(utf8match(txt, "eeee", 0, m) == 0);

	CU_ASSERT(utf8match(txt, "eTx", 0, NULL) == 1);
	CU_ASSERT(utf8match(txt, "eTx", 0, m) == 1);
	CU_ASSERT(check_match_mask("010010100000", m) == 1);

	CU_ASSERT(utf8match(txt, "TTTT", 1, NULL) == 1);

	CU_ASSERT(utf8match("ö", "ö", 0, NULL) == 1);
	CU_ASSERT(utf8match("Ö", "Ö", 0, NULL) == 1);
	CU_ASSERT(utf8match("ö", "Ö", 0, NULL) == 0);
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
