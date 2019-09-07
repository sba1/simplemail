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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include <CUnit/Basic.h>

#include "mail.h"
#include "support_indep.h"

/********************************************************/

/* @Test */
void test_longest_common_prefix(void)
{
	char *str[] =
	{
			"abcde",
			"abcdf",
			"abcf"
	};

	char *str2[] =
	{
			"abcd",
			"efgh",
			"ijkl"
	};

	CU_ASSERT(longest_common_prefix(str, 3) == 3);
	CU_ASSERT(longest_common_prefix(NULL, 0) == 0);
	CU_ASSERT(longest_common_prefix(str, 1) == 5);
	CU_ASSERT(longest_common_prefix(str2, 3) == 0);
}

/********************************************************/

/* @Test */
void test_longest_common_substring(void)
{
	const char *str[] =
	{
			"aaabbbcccdddeeefffaaa",
			"aaadddeeefffggg",
			"ddddddeeefffggg"
	};
	const char *str2[] =
	{
			"aaabbbcccdddxeeefffaaa",
			"aaadddeeefffggg",
			"ddddddeeefffggg"
	};
	const char *str3[] =
	{
			"abcdef",
			"gggggg",
			"hijkl"
	};
	const char *str4[]={
			"[simplemail] test1",
			"[simplemail] test2",
			"Re: [simplemail] test2",
			"[simplemail] Re: test2"};

	int len = 0;
	int pos_in_a = 0;

	longest_common_substring(str, 3, &pos_in_a, &len);
	CU_ASSERT(len == 9);
	CU_ASSERT(pos_in_a == 9);

	longest_common_substring(str2, 3, &pos_in_a, &len);
	CU_ASSERT(len == 6);
	CU_ASSERT(pos_in_a == 13);

	longest_common_substring(str3, 3, &pos_in_a, &len);
	CU_ASSERT(len == 0);

	longest_common_substring(str4, 4, &pos_in_a, &len);
	CU_ASSERT(len == 13);
	CU_ASSERT(pos_in_a == 0);
}

/********************************************************/

/* @Test */
void test_wrap_text(void)
{
	char buf[128];

	strcpy(buf, "Word1, Word2, Word3, Word4, Word5, Word6");
	wrap_text(buf, 14);
	CU_ASSERT_STRING_EQUAL(buf, "Word1, Word2,\nWord3, Word4,\nWord5, Word6");

	strcpy(buf, "AAA BB CC DDDDD");
	wrap_text(buf, 7);
	CU_ASSERT_STRING_EQUAL(buf, "AAA BB\nCC\nDDDDD");
}

/********************************************************/

/**
 * Add one to the given bitstring.
 */
static void add_one(char *bs, int len)
{
	int i;

	for (i = len - 1; i >= 0; i--)
	{
		if (bs[i] == 0)
		{
			bs[i] = 1;
			return;
		}

		bs[i] = 0;
	}
}

/*#define DEBUG_WRAP_LINE_NICELY */

/*****************************************************************************/

/**
 * Determine the cost of the given breakpoint configuration.
 *
 * @param pos the position of the words to be wrapped
 * @param bp vector of breakpoint indicators
 * @param vlen the length of both vectors.
 * @param mlen the maximal allowed length of one line
 *
 * @return the costs
 */
static unsigned int breakpoint_costs(const int *pos, const char *bp, int vlen, int mlen)
{
	unsigned int cost = 0;

	/* initially, last breakpoint is before first word */
	int last_bp = 0;

	/* <= because the arrays carry one more element and we don't want
	 * to special case that yet */
	for (int j = 1; j < vlen; j++)
	{
		if (bp[j])
		{
			int line_len = pos[j] - pos[last_bp] - 1; /* don't count the trailing space */

			if (line_len > mlen)
			{
				cost = 1000000;
			} else
			{
				cost += (mlen - line_len) * (mlen - line_len);
			}
			/* Remember this break point as new last */
			last_bp = j;
		}
	}
	return cost;
}

/**
 * Naive version of wrap_line_nicely so that we are able
 * to compare results.
 */
static void wrap_line_nicely_naive(char *text, int border)
{
	unsigned char *buf = (unsigned char *)text;
	int min_cost;
	int words;
	int word;
	int bps;
	int len;
	int i,j;

	int *pos; /* position vector */
	char *bp; /* breakpoint vector */

	border--;

#ifdef DEBUG_WRAP_LINE_NICELY
	printf("INPUT\n%s\n", text);
#endif
	len = strlen(text);

	/* First, convert all new lines to spaces (just in case), and count number
	 * of words */
	bps = 0;
	for (i = 0; i < len; i++)
	{
		unsigned char c;

		c = buf[i];

		if (c == '\n')
		{
			buf[i] = ' ';
		}

		bps += !!isspace(buf[i]);
	}
	words = bps + 1;

	/* Allocate our vectors */
	if (!(pos = (int *)malloc((words + 1) * sizeof(pos[0]))))
	{
		return;
	}

	if (!(bp = (char *)malloc((words + 1) * sizeof(bp[0]))))
	{
		free(pos);
		return;
	}

	/* Fix start and end */
	pos[0] = -1; /* this "emulates" a space before the first word */
	bp[0] = 0; /* will be misused as an indicator to leave (if it is 1) */

	pos[words] = len;
	bp[words] = 1; /* this forces a breakpoint at the end of the line */

	word = 1;

	/* Fill vectors */
	for (i = 0; i < len; i++)
	{
		if (isspace(buf[i]))
		{
			pos[word] = i;
			bp[word] = 0;
			word++;
		}
	}

	/* Enumerate and evaluate. This is a naive implementation and not usable for
	 * even low number of words.
	 */
	min_cost = 1000000;
	while (!bp[0])
	{
		unsigned int cost;

#ifdef DEBUG_WRAP_LINE_NICELY
		printf("\n");
		for (int j = 1; j <= bps; j++)
		{
			printf("%d ", bp[j]);
		}
		printf("\n");
#endif

		cost = breakpoint_costs(pos, bp, bps + 2 /* head and tail */, border);

		if (cost < min_cost)
		{
#ifdef DEBUG_WRAP_LINE_NICELY
			printf("NEW MIN\n");
#endif
			min_cost = cost;

			/* Now set breakpoints corresponding to the current minimal config */
			for (int j = 1; j <= bps; j++)
			{
				text[pos[j]] = bp[j]?'\n':' ';
			}
		}

#ifdef DEBUG_WRAP_LINE_NICELY
		printf("cost: %d\n", cost);
#endif

		/* Next combination, note that if the index will become 1, we'll leave */
		add_one(bp, bps + 1);
	}

#ifdef DEBUG_WRAP_LINE_NICELY
	printf("OUTPUT\n%s\n", text);
#endif
	free(pos);
	free(bp);
}

static void wrap_line_nicely_dummy_cb(int num_breakpoints, int bp, int pos, void *udata)
{
}

/* @Test */
void test_wrap_line_nicely(void)
{
	char buf[256];
	int bps;

	bps = wrap_line_nicely_cb("AAAAAA", 8, wrap_line_nicely_dummy_cb, NULL);
	CU_ASSERT_EQUAL(bps, 0);

	bps = wrap_line_nicely_cb("AAAAAA BBBBBB", 8, wrap_line_nicely_dummy_cb, NULL);
	CU_ASSERT_EQUAL(bps, 1);

	strcpy(buf, "AAAAA BBBB");
	wrap_line_nicely(buf, 8);
	CU_ASSERT_STRING_EQUAL(buf, "AAAAA\nBBBB");

	strcpy(buf, "AAA BB CC DDDDD");
	wrap_line_nicely(buf, 7);
	CU_ASSERT_STRING_EQUAL(buf, "AAA\nBB CC\nDDDDD");

	strcpy(buf, "AAA BB C DDDDD E FFFFF G HH II JJ K LLLL MM N OOOO PPPPP QQ RR SS TT UUU VV WW");
	wrap_line_nicely(buf, 11);
	CU_ASSERT_STRING_EQUAL(buf,
		"AAA BB C\n"
		"DDDDD E\n"
		"FFFFF G\n"
		"HH II JJ\n"
		"K LLLL MM\n"
		"N OOOO\n"
		"PPPPP QQ\n"
		"RR SS TT\n"
		"UUU VV WW");
}

/********************************************************/

/* @Test */
void test_array_sort(void)
{
	char **a = NULL;

	array_sort_uft8(a);

	a = array_add_string(a,"zfc");
	CU_ASSERT(a != NULL);
	a = array_add_string(a,"abd");
	CU_ASSERT(a != NULL);
	a = array_add_string(a,"afc");
	CU_ASSERT(a != NULL);

	array_sort_uft8(a);

	CU_ASSERT(!strcmp(a[0],"abd"));
	CU_ASSERT(!strcmp(a[1],"afc"));
	CU_ASSERT(!strcmp(a[2],"zfc"));

	array_free(a);
}


/********************************************************/

/* @Test */
void test_mystrreplace(void)
{
	char *r;

	r = mystrreplace("abcdefgabcdefg","hij", "klm");
	CU_ASSERT(r != NULL);
	CU_ASSERT(!strcmp("abcdefgabcdefg",r));
	free(r);

	r = mystrreplace("abcdefgabcdefg","bcd", "q");
	CU_ASSERT(r != NULL);
	CU_ASSERT(!strcmp("aqefgaqefg",r));
	free(r);
}
