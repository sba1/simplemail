/**
 * @file
 */

#include "gadgets.h"

#include <stdlib.h>
#include <string.h>


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

/******************************************************************************/

static const char *simple_text_render(void *l)
{
	return ((struct simple_text_label*)l)->text;
}

static void simple_text_free(void *l)
{
	free(((struct simple_text_label*)l)->text);
}

static void simple_text_display(struct gadget *g, WINDOW *win)
{
	struct simple_text_label *l = (struct simple_text_label *)g;
	const char *txt = l->tl.render(l);
	const char *endl;
	int oy = 0;
	int h = l->tl.g.r.h;

	while ((endl = mystrchrnul(txt, '\n')) != txt && oy < h)
	{
		size_t txt_len = endl - txt;
		int i;

		mvwaddnstr(win, l->tl.g.r.y + oy, l->tl.g.r.x, txt, endl - txt);

		for (i = txt_len; i < l->tl.g.r.w; i++)
		{
			mvwaddnstr(win, l->tl.g.r.y + oy, i, " ", 1);
		}
		txt = endl;
		oy++;
	}
}

static void group_display(struct gadget *gad, WINDOW *win)
{
	struct group *gr = (struct group *)gad;
	struct gadget *child = (struct gadget *)list_first(&gr->l);
	while (child)
	{
		child->display(child, win);
		child = (struct gadget *)node_next(&child->n);
	}
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

void gadgets_init_group(struct group *g)
{
	list_init(&g->l);
	g->g.display = group_display;
}

/******************************************************************************/

void gadgets_add(struct group *gr, struct gadget *gad)
{
	list_insert_tail(&gr->l, &gad->n);
}

/******************************************************************************/

void gadgets_remove(struct gadget *gad)
{
	node_remove(&gad->n);
}

/******************************************************************************/

void gadgets_init_simple_text_label(struct simple_text_label *l, int x, int y, int w, const char *text)
{
	char *buf = (char*)malloc(strlen(text) + 1);
	strcpy(buf, text);
	gadgets_set_extend(&l->tl, x, y, w, 1);
	l->text = buf;
	l->tl.g.display = simple_text_display;
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
	v->tl.tl.g.display = simple_text_display;
	v->tl.tl.render = simple_text_render;
	v->tl.tl.free = simple_text_free;
}


/*******************************************************************************/

void gadgets_display(WINDOW *win, struct gadget *g)
{
	g->display(g, win);
}
