/**
 * gadgets_unittest - a simple test for the gadgets interface for SimpleMail.
 * Copyright (C) 2019 Sebastian Bauer
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ncurses/gadgets.h"

#include <stdlib.h>

/* @Test */
void test_text_edit_enter_simple_text_works(void)
{
	struct screen scr;
	struct text_edit te;
	char *contents;
	int i;

	screen_init_in_memory(&scr, 200, 40);
	gadgets_init_text_edit(&te);

	contents = gadgets_get_text_edit_contents(&te);
	CU_ASSERT(contents != NULL);
	CU_ASSERT_EQUAL(gadgets_get_text_edit_number_of_lines(&te), 1);
	CU_ASSERT_STRING_EQUAL("\n", contents);
	free(contents);

	te.g.input(&te.g, 'h');
	te.g.input(&te.g, 'i');
	contents = gadgets_get_text_edit_contents(&te);
	CU_ASSERT(contents != NULL);
	CU_ASSERT_EQUAL(gadgets_get_text_edit_number_of_lines(&te), 1);
	CU_ASSERT_STRING_EQUAL("hi\n", contents);
	free(contents);

	te.g.input(&te.g, GADS_KEY_LEFT);
	te.g.input(&te.g, 'o');
	contents = gadgets_get_text_edit_contents(&te);
	CU_ASSERT(contents != NULL);
	CU_ASSERT_EQUAL(gadgets_get_text_edit_number_of_lines(&te), 1);
	CU_ASSERT_STRING_EQUAL("hoi\n", contents);
	free(contents);

	te.g.input(&te.g, '\n');
	contents = gadgets_get_text_edit_contents(&te);
	CU_ASSERT(contents != NULL);
	CU_ASSERT_STRING_EQUAL("ho\ni\n", contents);
	CU_ASSERT_EQUAL(gadgets_get_text_edit_number_of_lines(&te), 2);
	CU_ASSERT_EQUAL(gadgets_get_text_edit_number_of_lines(&te), 2);
	free(contents);

	gadgets_set_text_edit_contents(&te, "Line 1\nLine 2");
	contents = gadgets_get_text_edit_contents(&te);
	CU_ASSERT(contents != NULL);
	CU_ASSERT_STRING_EQUAL("Line 1\nLine 2\n", contents);
	CU_ASSERT_EQUAL(gadgets_get_text_edit_number_of_lines(&te), 2);
	free(contents);

	/* Move below the limit */
	te.g.input(&te.g, GADS_KEY_DOWN);
	te.g.input(&te.g, GADS_KEY_DOWN);
	te.g.input(&te.g, GADS_KEY_DOWN);
	CU_ASSERT_EQUAL(gadgets_get_text_edit_number_of_lines(&te), 2);

	te.g.input(&te.g, GADS_KEY_HOME);
	CU_ASSERT_EQUAL(te.cx, 0);

	te.g.input(&te.g, GADS_KEY_END);
	CU_ASSERT_EQUAL(te.cx, 6);

	/* Move beyond the limit also via right keys */
	for (i = 0; i < 100; i++)
	{
		te.g.input(&te.g, GADS_KEY_RIGHT);
	}
	CU_ASSERT_EQUAL(gadgets_get_text_edit_number_of_lines(&te), 2);

	/* Empty line at the end */
	te.g.input(&te.g, '\n');
	CU_ASSERT_EQUAL(gadgets_get_text_edit_number_of_lines(&te), 3);

	/* "Reset" */
	gadgets_set_text_edit_contents(&te, "Line 1\nLine 2");
	gadgets_set_text_edit_cursor(&te, 0, 0);
	te.g.input(&te.g, GADS_KEY_DOWN);
	te.g.input(&te.g, GADG_KEY_BACKSPACE);
	CU_ASSERT_EQUAL(gadgets_get_text_edit_number_of_lines(&te), 1);

	/* "Reset" */
	gadgets_set_text_edit_contents(&te, "Line 1\nLine 2");
	gadgets_set_text_edit_cursor(&te, 0, 0);
	te.g.input(&te.g, GADS_KEY_DOWN);
	te.g.input(&te.g, GADS_KEY_LEFT);
	te.g.input(&te.g, GADS_KEY_DELETE);
	CU_ASSERT_EQUAL(gadgets_get_text_edit_number_of_lines(&te), 1);
}
