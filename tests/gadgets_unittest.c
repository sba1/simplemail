#include "ncurses/gadgets.h"

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
}
