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

void gadgets_init_text_view(struct text_view *v, int x, int y, int w, int h, const char *text)
{
	char *buf = (char*)malloc(strlen(text) + 1);
	strcpy(buf, text);
	v->tl.tl.x = x;
	v->tl.tl.y = y;
	v->tl.tl.w = w;
	v->tl.tl.h = 1;
	v->tl.text = buf;
	v->tl.tl.render = simple_text_render;
	v->tl.tl.free = simple_text_free;
}

/*******************************************************************************/

static char *mystrchrnul(const char *s, int c)
{
	char *r = strchr(s, c);
	if (!r)
	{
		return (char*)&s[strlen(s)];
	}
	return r;
}

/*******************************************************************************/

void gadgets_display(WINDOW *win, struct text_label *l)
{
	const char *txt = l->render(l);
	const char *endl;

	while ((endl = mystrchrnul(txt, '\n')) != txt)
	{
		size_t txt_len = endl - txt;
		int i;

		mvwaddnstr(win, l->y, l->x, txt, txt - endl);

		for (i = txt_len; i < l->w; i++)
		{
			mvwaddnstr(win, l->y, i, " ", 1);
		}
		txt = endl;
	}
}
