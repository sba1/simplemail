/**
 * @file
 */

#include "gadgets.h"

#include <stdlib.h>
#include <string.h>


/*******************************************************************************/

/**
 * Like strchr() but returns the end of the string if the c could not be found.
 *
 * @param s the string to search through.
 * @param c the character to find (8-bit).
 *
 * @return a pointer that points to c within s or points to the null byte.
 */
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

/**
 * Initializes the base structure of the gadget.
 */
static void gagdets_init(struct gadget *g)
{
	memset(g, 0, sizeof(g));
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

		if (txt_len > xoffset)
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

/**
 * Invoke display method of each group member.
 */
static void group_display(struct gadget *gad, WINDOW *win)
{
	struct group *gr = (struct group *)gad;
	struct gadget *child = (struct gadget *)list_first(&gr->l);
	while (child)
	{
		gadgets_display(win, child);
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
	gagdets_init(&g->g);

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
	char *buf;

	gagdets_init(&l->tl.g);

	if (!(buf = (char*)malloc(strlen(text) + 1)))
		return;
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
	char *buf;

	gagdets_init(&v->tl.tl.g);

	if (!(buf = (char*)malloc(strlen(text) + 1)))
		return;
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
	list_init(&win->key_listeners);
}

/*******************************************************************************/

void windows_display(WINDOW *win, struct window *wnd)
{
	gadgets_display(win, &wnd->g.g);
}

/*******************************************************************************/

void screen_init(struct screen *scr)
{
	memset(scr, 0, sizeof(*scr));
	list_init(&scr->windows);
	list_init(&scr->resize_listeners);
	list_init(&scr->key_listeners);
}

/*******************************************************************************/

void screen_add_window(struct screen *scr, struct window *wnd)
{
	list_insert_tail(&scr->windows, &wnd->g.g.n);
}

/*******************************************************************************/

void screen_remove_window(struct screen *scr, struct window *wnd)
{
	node_remove(&wnd->g.g.n);
	if (scr->active == wnd)
	{
		scr->active = NULL;
	}
}

/*******************************************************************************/

void screen_add_resize_listener(struct screen *scr, struct screen_resize_listener *listener,
		void (*callback)(void *arg, int x, int y, int width, int height), void *udata)
{
	listener->callback = callback;
	list_insert_tail(&scr->resize_listeners, &listener->n);
}

/*******************************************************************************/

void screen_remove_resize_listener(struct screen_resize_listener *listener)
{
	node_remove(&listener->n);
}

/*******************************************************************************/

void screen_invoke_resize_listener(struct screen *scr)
{
	struct screen_resize_listener *l;
	int w, h;
	getmaxyx(stdscr, h, w);

	l = (struct screen_resize_listener *)list_first(&scr->resize_listeners);
	while (l)
	{
		l->callback(l->udata, 0, 0, w, h);
		l = (struct screen_resize_listener*)node_next(&l->n);
	}
}

/*******************************************************************************/

void screen_add_key_listener(struct screen *scr, struct key_listener *l, int ch, const char *short_description, void (*callback)(void))
{
	l->ch = ch;
	l->short_description = short_description;
	l->callback = callback;
	list_insert_tail(&scr->key_listeners, &l->n);
	if (scr->keys_changed)
	{
		scr->keys_changed(scr);
	}
}

/*******************************************************************************/

void screen_remove_key_listener(struct screen *scr, struct key_listener *l)
{
	node_remove(&l->n);
	if (scr->keys_changed)
	{
		scr->keys_changed(scr);
	}
}

/*******************************************************************************/

void screen_invoke_key_listener(struct screen *scr, int ch)
{
	struct key_listener *l = (struct key_listener *)list_first(&scr->key_listeners);
	while (l)
	{
		struct key_listener *n;

		n = (struct key_listener *)node_next(&l->n);
		if (l->ch == ch)
		{
			l->callback();
		}

		l = n;
	}
}

/*******************************************************************************/

void screen_key_description_line(struct screen *scr, char *buf, size_t bufsize)
{
	struct key_listener *l;
	const char *space = "";
	char *d;

	if (!bufsize)
	{
		return;
	}

	bufsize--;
	d = buf;
	l = (struct key_listener *)list_first(&scr->key_listeners);

	for (; l && bufsize > 1; l = (struct key_listener *)node_next(&l->n))
	{
		char tbuf[20];
		size_t tlen;

		if (l->ch == GADS_KEY_UP || l->ch == GADS_KEY_DOWN || !l->short_description)
		{
			continue;
		}

		snprintf(tbuf, sizeof(tbuf), "%s%c: %s", space, l->ch, l->short_description);

		/* Append tbuf but do not exceed bounds */
		tlen = strlen(tbuf);
		if (tlen > bufsize)
		{
			tlen = bufsize;
		}
		strncpy(d, tbuf, tlen);

		bufsize -= tlen;
		d += tlen;
		space = "  ";
	}
	*d = 0;
}
