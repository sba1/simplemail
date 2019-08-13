/**
 * @file
 */

#include "gadgets.h"

#include <stdlib.h>
#include <string.h>

/*******************************************************************************/

#define MIN(a,b) (((a)<(b))?(a):(b))

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
static void gadgets_init(struct gadget *g)
{
	memset(g, 0, sizeof(*g));
}

/**
 * Display the given gadget onto the given window.
 *
 * @param win the window where to display the gadget.
 * @param g the gadget to display.
 */
static void gadgets_display(struct window *win, struct gadget *g)
{
	g->display(g, win);
	g->flags &= ~GADF_REDRAW_UPDATE;
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

static void simple_text_display(struct gadget *g, struct window *win)
{
	struct simple_text_label *l = (struct simple_text_label *)g;
	const char *txt = l->tl.render(l);
	const char *endl;
	int dx = win->g.g.r.x;
	int dy = win->g.g.r.y;
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
		int txt_len;
		int i;

		txt_len = endl - txt;

		if (txt_len > xoffset)
		{
			txt += xoffset;
			txt_len -= xoffset;
			win->scr->puts(win->scr, dx + x, dy + y + oy, txt, endl - txt);
		} else
		{
			txt_len = 0;
		}

		for (i = txt_len; i < w; i++)
		{
			win->scr->puts(win->scr, dx + i, dy + y + oy, " ", 1);
		}
		txt = endl + 1;
		oy++;
	}

	/* Clear remaining */
	while (oy < h)
	{
		int i;

		for (i = 0; i < g->r.w; i++)
		{
			win->scr->puts(win->scr, dx + x + i, dy + y + oy, " ", 1);
		}
		oy++;
	}
}

/**
 * Invoke display method of each group member.
 */
static void group_display(struct gadget *gad, struct window *win)
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

	g->flags |= GADF_REDRAW_UPDATE;
}

/******************************************************************************/

void gadgets_init_group(struct group *g)
{
	gadgets_init(&g->g);

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
	gadgets_init(&l->tl.g);

	l->tl.xoffset = l->tl.yoffset = 0;
	l->tl.g.display = simple_text_display;
	l->tl.render = simple_text_render;
	l->tl.free = simple_text_free;

	gadgets_set_label_text(l, text);
}

/*******************************************************************************/

void gadgets_set_label_text(struct simple_text_label *l, const char *text)
{
	char *buf;
	if (!(buf = (char*)malloc(strlen(text) + 1)))
		return;
	strcpy(buf, text);
	free(l->text);
	l->text = buf;
	l->tl.g.flags |= GADF_REDRAW_UPDATE;
}

/*******************************************************************************/

void gadgets_init_text_view(struct text_view *v, const char *text)
{
	char *buf;

	gadgets_init(&v->tl.tl.g);

	if (!(buf = (char*)malloc(strlen(text) + 1)))
		return;
	strcpy(buf, text);

	v->tl.text = buf;
	v->tl.tl.xoffset = v->tl.tl.yoffset = 0;
	v->tl.tl.g.display = simple_text_display;
	v->tl.tl.render = simple_text_render;
	v->tl.tl.free = simple_text_free;
}

/******************************************************************************/

int text_edit_input(struct gadget *g, int value)
{
	/* The following code is not optimized yet */
	struct text_edit *e = (struct text_edit *)g;
	struct string_node *s;

	while (!(s = string_list_find_by_index(&e->line_list, e->cy)))
	{
		if (!string_list_insert_tail_always(&e->line_list, ""))
		{
			return 0;
		}
	}

	if (value >= 32)
	{
		char *new_string;
		int s_len;

		s_len = strlen(s->string);
		if (e->cx > s_len) e->cx = s_len;
		if (!(new_string = malloc(s_len + 2)))
		{
			return 0;
		}
		strncpy(new_string, s->string, e->cx);
		strcpy(&new_string[e->cx + 1], &s->string[e->cx]);
		new_string[e->cx++] = value;
		free(s->string);
		s->string = new_string;
		g->flags |= GADF_REDRAW_UPDATE;
		return 1;
	}

	switch (value)
	{
	case GADS_KEY_RIGHT:
		if (e->cx < strlen(s->string))
		{
			e->cx++;
		}
		break;

	case GADS_KEY_LEFT:
		if (e->cx)
		{
			e->cx--;
		}
		break;

	default:
		return 0;
	}
	g->flags |= GADF_REDRAW_UPDATE;
	return 1;
}

/******************************************************************************/

void text_edit_display(struct gadget *g, struct window *win)
{
	struct text_edit *e = (struct text_edit *)g;
	struct string_node *s;

	int wx = win->g.g.r.x;
	int wy = win->g.g.r.y;
	int gx = e->g.r.x;
	int gy = e->g.r.y;
	int gw = e->g.r.w;
	int gh = e->g.r.h;
	int y = 0;

	s = string_list_first(&e->line_list);

	while (s)
	{
		int sl = strlen(s->string);
		for (int x = 0; x < gw; x++)
		{
			void (*puts)(struct screen *scr, int x, int y, const char *text, int len) = win->scr->puts;
			if (x == e->cx)
			{
				puts = win->scr->put_cursor;
			}
			puts(win->scr, x + wx + gx, y + wy + gy, x<sl?&s->string[x]:" ", 1);
		}
		s = string_node_next(s);
	}
}

/******************************************************************************/

void gadgets_init_text_edit(struct text_edit *e)
{
	memset(e, 0, sizeof(*e));

	gadgets_init(&e->g);
	string_list_init(&e->line_list);


	e->g.input = text_edit_input;
	e->g.display = text_edit_display;
}

/******************************************************************************/

/**
 * Callback called when displaying a list view.
 */
static void listview_display(struct gadget *g, struct window *win)
{
	struct listview *v = (struct listview *)g;
	int nelements = MIN(v->g.r.h, v->rows);
	int dx = win->g.g.r.x + g->r.x;
	int dy = win->g.g.r.y + g->r.y;
	int y;

	char buf[256];

	for (y = 0; y < nelements; y++)
	{
		int i;
		int buf_len;

		if (y == v->active)
		{
			buf[0] = '*';
		} else
		{
			buf[0] = ' ';
		}

		v->render(y, buf + 1, sizeof(buf) - 1);
		buf_len = strlen(buf);

		if (buf_len > g->r.w)
		{
			buf_len = g->r.w;
		}
		win->scr->puts(win->scr, dx, dy + y, buf, buf_len);
		for (i = buf_len; i < g->r.w; i++)
		{
			win->scr->puts(win->scr, dx + i, dy + y, " ", 1);
		}
	}

	/* Clear remaining space */
	for (; y < g->r.h; y++)
	{
		int i;

		for (i = 0; i < g->r.w; i++)
		{
			win->scr->puts(win->scr, dx + i, dy + y, " ", 1);
		}
	}
}

/******************************************************************************/

void gadgets_init_listview(struct listview *v, void (*render)(int pos, char *buf, int bufsize))
{
	gadgets_init(&v->g);
	v->active = -1;
	v->rows = 0;
	v->render = render;
	v->g.display = listview_display;
}

/*******************************************************************************/

void windows_init(struct window *win)
{
	memset(win, 0, sizeof(*win));
	gadgets_init_group(&win->g);
	list_init(&win->key_listeners);
}

/*******************************************************************************/

void windows_display(struct window *wnd, struct screen *scr)
{
	int ox = wnd->g.g.r.x;
	int oy = wnd->g.g.r.y;

	/* Clear the window's area before anything else is drawn onto it */
	for (int y = oy; y < oy + wnd->g.g.r.h; y++)
	{
		for (int x = ox; x < ox + wnd->g.g.r.w; x++)
		{
			scr->puts(scr, x, y, " ", 1);
		}
	}

	gadgets_display(wnd, &wnd->g.g);
	if (!wnd->no_input)
	{
		scr->active = wnd;

		/* Update key description since another window was activated */
		scr->keys_changed(scr);

	}
	wrefresh(wnd->scr->handle);
}

/*******************************************************************************/

void windows_activate_gadget(struct window *wnd, struct gadget *g)
{
	wnd->active = g;
}

/*******************************************************************************/

void windows_add_key_listener(struct window *win, struct key_listener *l, int ch, const char *short_description, void (*callback)(void))
{
	l->ch = ch;
	l->short_description = short_description;
	l->callback = callback;
	list_insert_tail(&win->key_listeners, &l->n);

	/* Update key command descriptions if window is the active one */
	if (win->scr->keys_changed && win->scr && win->scr->active == win)
	{
		win->scr->keys_changed(win->scr);
	}

}

/*******************************************************************************/

void windows_remove_key_listener(struct window *win, struct key_listener *l)
{
	node_remove(&l->n);
	if (win->scr->keys_changed)
	{
		win->scr->keys_changed(win->scr);
	}
}

/*******************************************************************************/

int window_invoke_key_listener(struct window *win, int ch)
{
	struct key_listener *l = (struct key_listener *)list_first(&win->key_listeners);
	while (l)
	{
		struct key_listener *n;

		n = (struct key_listener *)node_next(&l->n);
		if (l->ch == ch)
		{
			l->callback();
			return 1;
		}

		l = n;
	}
	return 0;
}

/*******************************************************************************/

static struct window *screen_find_next_active_candidate(struct screen *scr)
{
	struct window *w;

	for (w = (struct window *)list_last(&scr->windows); w; w = (struct window *)node_prev(&w->g.g.n))
	{
		if (!w->no_input)
		{
			break;
		}
	}
	return w;
}

/*******************************************************************************/

static void screen_ncurses_puts(struct screen *scr, int x, int y, const char *txt, int len)
{
	mvwaddnstr(scr->handle, y, x, txt, len);
}

/*******************************************************************************/

static void screen_ncurses_put_cursor(struct screen *scr, int x, int y, const char *txt, int len)
{
	wattron(scr->handle, A_REVERSE);
	mvwaddnstr(scr->handle, y, x, txt, len);
	wattroff(scr->handle, A_REVERSE);
}

/*******************************************************************************/

void screen_init(struct screen *scr)
{
	memset(scr, 0, sizeof(*scr));
	list_init(&scr->windows);
	list_init(&scr->resize_listeners);
	list_init(&scr->key_listeners);

	{
		int w, h;
		getmaxyx(stdscr, h, w);
		scr->w = w;
		scr->h = h;
		scr->handle = newwin(h, w, 0, 0);
		nodelay(scr->handle, TRUE);
		scr->puts = screen_ncurses_puts;
		scr->put_cursor = screen_ncurses_put_cursor;
	}
}

/*******************************************************************************/

void screen_add_window(struct screen *scr, struct window *wnd)
{
	list_insert_tail(&scr->windows, &wnd->g.g.n);
	wnd->scr = scr;
}

/*******************************************************************************/

void screen_remove_window(struct screen *scr, struct window *wnd)
{
	if (!screen_has_window(scr, wnd))
	{
		return;
	}

	node_remove(&wnd->g.g.n);
	if (scr->active == wnd)
	{
		if ((scr->active = screen_find_next_active_candidate(scr)))
		{
			windows_display(scr->active, &gui_screen);
		}
	}
	wnd->scr = NULL;

	/* Possibly new window, key set could have been changed */
	scr->keys_changed(scr);
}
/*******************************************************************************/

int screen_has_window(struct screen *scr, struct window *wnd)
{
	struct window *iter;

	iter = (struct window *)list_first(&scr->windows);
	while (iter)
	{
		if (iter == wnd)
		{
			return 1;
		}
		iter = (struct window*)node_next(&iter->g.g.n);
	}
	return 0;
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

/**
 * @return the number of written bytes excluding the possible 0 byte.
 */
static int keylisteners_description_line(key_listeners_t *listeners, char *buf, size_t bufsize)
{
	const char *space = "";
	struct key_listener *l;
	char *d;

	if (!bufsize)
	{
		return 0;
	}

	bufsize--;
	d = buf;

	l = (struct key_listener *)list_first(listeners);

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

	return d - buf;

}

/*******************************************************************************/

void screen_key_description_line(struct screen *scr, char *buf, size_t bufsize)
{
	if (!bufsize)
	{
		return;
	}

	if (scr->active)
	{
		int nbytes = keylisteners_description_line(&scr->active->key_listeners, buf, bufsize);
		buf += nbytes;
		bufsize -= nbytes;
	}
	keylisteners_description_line(&scr->key_listeners, buf, bufsize);
}
