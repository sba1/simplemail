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

void gadgets_set_extend(struct text_label *l, int x, int y, int w, int h)
{
	l->g.r.x = x;
	l->g.r.y = y;
	l->g.r.w = w;
	l->g.r.h = h;
}

/******************************************************************************/

void gadgets_init_simple_text_label(struct simple_text_label *l, int x, int y, int w, const char *text)
{
	char *buf = (char*)malloc(strlen(text) + 1);
	strcpy(buf, text);
	gadgets_set_extend(&l->tl, x, y, w, 1);
	l->text = buf;
	l->tl.render = simple_text_render;
	l->tl.free = simple_text_free;
}

/*******************************************************************************/

void gadgets_init_text_view(struct text_view *v, int x, int y, int w, int h, const char *text)
{
	char *buf = (char*)malloc(strlen(text) + 1);
	strcpy(buf, text);
	gadgets_set_extend(&v->tl.tl, x, y, w, h);

	v->tl.text = buf;
	v->tl.tl.render = simple_text_render;
	v->tl.tl.free = simple_text_free;
}

/*******************************************************************************/

static const char *mystrchrnul(const char *s, int c)
{
	const char *r = strchr(s, c);
	if (!r)
	{
		return &s[strlen(s)];
	}
	return r;
}

/*******************************************************************************/

void gadgets_display(WINDOW *win, struct text_label *l)
{
	const char *txt = l->render(l);
	const char *endl;
	int oy = 0;
	int h = l->g.r.h;

	while ((endl = mystrchrnul(txt, '\n')) != txt && oy < h)
	{
		size_t txt_len = endl - txt;
		int i;

		mvwaddnstr(win, l->g.r.y + oy, l->g.r.x, txt, endl - txt);

		for (i = txt_len; i < l->g.r.w; i++)
		{
			mvwaddnstr(win, l->g.r.y + oy, i, " ", 1);
		}
		txt = endl;
		oy++;
	}
}
