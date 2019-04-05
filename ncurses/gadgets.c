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

void gadgets_init_simple_text_label(struct simple_text_label *l, int x, int y, int w, const char *text)
{
	char *buf = (char*)malloc(strlen(text) + 1);
	strcpy(buf, text);
	l->tl.x = x;
	l->tl.y = y;
	l->tl.w = w;
	l->tl.h = 1;
	l->text = buf;
	l->tl.render = simple_text_render;
	l->tl.free = simple_text_free;
}

/*******************************************************************************/

void gadgets_display(WINDOW *win, struct text_label *l)
{
	const char *txt = l->render(l);
	size_t txt_len = strlen(txt);
	size_t i;

	mvwaddstr(win, l->y, l->x, l->render(l));

	for (i = txt_len; i < l->w; i++)
	{
		mvwaddstr(win, l->y, i, " ");
	}
}
