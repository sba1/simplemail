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
	int w = l->tl.g.r.w;
	int h = l->tl.g.r.h;
	int x = l->tl.g.r.x;
	int y = l->tl.g.r.y;
	int xoffset = l->tl.xoffset;
	int yoffset = l->tl.yoffset;

	/* Search for first text line to be displayed */
	while (yoffset && (endl = mystrchrnul(txt, '\n')) != txt)
	{
		txt = endl + 1;
		yoffset--;
	}

	while ((endl = mystrchrnul(txt, '\n')) != txt && oy < h)
	{
		size_t txt_len;
		int i;

		txt_len = endl - txt;

		if (txt_len < xoffset)
		{
			txt += xoffset;
			txt_len -= xoffset;
			mvwaddnstr(win, y + oy, x, txt, endl - txt);
		} else
		{
			txt_len = 0;
		}

		for (i = txt_len; i < w; i++)
		{
			mvwaddnstr(win, y + oy, i, " ", 1);
		}
		txt = endl + 1;
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

void gadgets_set_extend(struct gadget *g, int x, int y, int w, int h)
{
	g->r.x = x;
	g->r.y = y;
	g->r.w = w;
	g->r.h = h;
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

void gadgets_init_simple_text_label(struct simple_text_label *l, const char *text)
{
	char *buf = (char*)malloc(strlen(text) + 1);
	strcpy(buf, text);
	l->text = buf;
	l->tl.xoffset = l->tl.yoffset = 0;
	l->tl.g.display = simple_text_display;
	l->tl.render = simple_text_render;
	l->tl.free = simple_text_free;
}

/*******************************************************************************/

void gadgets_init_text_view(struct text_view *v, const char *text)
{
	char *buf = (char*)malloc(strlen(text) + 1);
	strcpy(buf, text);

	v->tl.text = buf;
	v->tl.tl.xoffset = v->tl.tl.yoffset = 0;
	v->tl.tl.g.display = simple_text_display;
	v->tl.tl.render = simple_text_render;
	v->tl.tl.free = simple_text_free;
}


/*******************************************************************************/

void gadgets_display(WINDOW *win, struct gadget *g)
{
	g->display(g, win);
}

/*******************************************************************************/

void windows_init(struct window *win)
{
	memset(win, 0, sizeof(*win));
	gadgets_init_group(&win->g);
}

