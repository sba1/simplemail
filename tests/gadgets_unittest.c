#include "ncurses/gadgets.h"

#include <stdlib.h>

/* @Test */
void test_text_edit_enter_simple_text_works(void)
{
	struct screen scr;
	struct text_edit te;
	char *contents;

	screen_init_in_memory(&scr, 200, 40);
	gadgets_init_text_edit(&te);

	contents = gadgets_get_text_edit_contents(&te);
	CU_ASSERT(contents != NULL);
	CU_ASSERT_STRING_EQUAL("\n", contents);
	free(contents);

	te.g.input(&te.g, 'h');
	te.g.input(&te.g, 'i');
	contents = gadgets_get_text_edit_contents(&te);
	CU_ASSERT(contents != NULL);
	CU_ASSERT_STRING_EQUAL("hi\n", contents);
	free(contents);

	te.g.input(&te.g, GADS_KEY_LEFT);
	te.g.input(&te.g, 'o');
	contents = gadgets_get_text_edit_contents(&te);
	CU_ASSERT(contents != NULL);
	CU_ASSERT_STRING_EQUAL("hoi\n", contents);
	free(contents);

	te.g.input(&te.g, '\n');
	contents = gadgets_get_text_edit_contents(&te);
	CU_ASSERT(contents != NULL);
	CU_ASSERT_STRING_EQUAL("ho\ni\n", contents);
	free(contents);
}
