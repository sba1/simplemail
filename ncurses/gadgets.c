/**
 * @file
 */

#include "gadgets.h"

#include <stdlib.h>
#include <string.h>

/******************************************************************************/

static const char *simple_text_render(void *l)
{
	return ((struct simple_text_label*)l)->text;
}

static void simple_text_free(void *l)
{
	free(((struct simple_text_label*)l)->text);
}

/******************************************************************************/

void gadgets_init_simple_text_label(struct simple_text_label *l, int x, int y, const char *text)
{
	char *buf = (char*)malloc(strlen(text) + 1);
	strcpy(buf, text);
	l->tl.x = x;
	l->tl.y = y;
	l->text = buf;
	l->tl.render = simple_text_render;
	l->tl.free = simple_text_free;
}

/*******************************************************************************/

void gadgets_display(WINDOW *win, struct text_label *l)
{
	mwvaddstr(win, l->y, l->x, l->render(l));
}
