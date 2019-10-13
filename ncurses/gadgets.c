/**
 * @file
 */

#include "gadgets.h"
#include "support_indep.h"
#include "text_edit_helper.h"

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

static const style_t normal_style;

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
		/* Advance by one more, but not if this was the end */
		txt = endl + !!(*endl);
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
			win->scr->puts(win->scr, dx + x, dy + y + oy, txt, endl - txt, normal_style);
		} else
		{
			txt_len = 0;
		}

		for (i = txt_len; i < w; i++)
		{
			win->scr->puts(win->scr, dx + i, dy + y + oy, " ", 1, normal_style);
		}
		txt = endl + !!(*endl);
		oy++;
	}

	/* Clear remaining */
	while (oy < h)
	{
		int i;

		for (i = 0; i < g->r.w; i++)
		{
			win->scr->puts(win->scr, dx + x + i, dy + y + oy, " ", 1, normal_style);
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
	memset(g, 0, sizeof(*g));

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
	memset(l, 0, sizeof(*l));

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

	memset(v, 0, sizeof(*v));

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

static struct line *line_first(const struct line_list *list)
{
	return (struct line *)list_first(&list->l);
}

static struct line *line_next(struct line *line)
{
	return (struct line *)node_next(&line->n);
}

static struct line *line_find(const struct line_list *list, int index)
{
	return (struct line *)list_find(&list->l, index);
}

static struct line *line_remove_tail(struct line_list *list)
{
	return (struct line *)list_remove_tail(&list->l);
}

static void line_insert_after(struct line_list *l, struct line *new, struct line *pred)
{
	list_insert(&l->l, &new->n, &pred->n);
}

static struct line *line_insert_tail(struct line_list *list, const char *str)
{
	struct line *line;
	if (!(line = (struct line *)malloc(sizeof(*line))))
	{
		return NULL;
	}
	memset(line, 0, sizeof(*line));
	list_init(&line->styles);
	if (!(line->contents = (char *)malloc(strlen(str) + 1)))
	{
		free(line);
		return NULL;
	}
	strcpy(line->contents, str);
	list_insert_tail(&list->l, &line->n);
	return line;
}

static struct line *line_insert_tail_len(struct line_list *list, const char *str, int len)
{
	struct line *l;

	if (!(l = line_insert_tail(list, "")))
	{
		return NULL;
	}
	free(l->contents);
	if (!(l->contents = (char *)malloc(len + 1)))
	{
		free(l);
		return NULL;
	}
	strncpy(l->contents, str, len);
	l->contents[len] = 0;
	return l;
}

static int line_len(const struct line *line)
{
	return strlen(line->contents);
}

static void line_clear(struct line_list *list)
{
	struct line *node;
	while ((node = line_remove_tail(list)))
	{
		struct style_node *sn;

		while ((sn = (struct style_node *)list_remove_tail(&node->styles)))
		{
			free(sn);
		}
		free(node->contents);
		free(node);
	}
}

static void line_exchange(struct line_list *a, struct line_list *b)
{
	struct line_list t;

	t = *a;
	*a = *b;
	*b = t;
}

/**
 * Insert new formatted line starting at pos into the given list.
 */
static struct formatted_line_node *text_edit_formatted_line_list_insert_tail(struct list *list, struct line *line, int pos)
{
	struct formatted_line_node *n;

	if (!(n = (struct formatted_line_node *)malloc(sizeof(*n))))
	{
		return NULL;
	}
	memset(n, 0, sizeof(*n));
	n->pos = pos;
	n->l = line;

	list_insert_tail(list, &n->n);
	return n;
}

/**
 * Clean the line (view) list.
 */
static void text_edit_clean_formatted_line_list(struct text_edit *e)
{
	struct formatted_line_node *ln;

	while ((ln = (struct formatted_line_node *)list_remove_tail(&e->formatted_line_list)))
	{
		free(ln);
	}
}

/******************************************************************************/

struct text_edit_wrap
{
	int *pos;
	int bps;
};

/******************************************************************************/

/**
 * A simple callback that stores all positions.
 */
static void text_edit_wrap_callback(int num_breakpoints, int bp, int pos, void *udata)
{
	struct text_edit_wrap *wrap;

	wrap = (struct text_edit_wrap *)udata;

	wrap->bps = num_breakpoints;
	if (!wrap->pos)
	{
		wrap->pos = (int *)malloc(sizeof(wrap->pos[0]) * num_breakpoints);
	}

	if (wrap->pos)
	{
		wrap->pos[bp] = pos;
	}
}

/**
 * Formats the model according to the current view properties, i.e., populates
 * the gadgets line list that is used for layout purposes.
 */
static void text_edit_format(struct text_edit *e)
{
	struct text_edit_model *m = &e->model;
	struct text_edit_wrap wrap = {};
	struct line *line;

	int gw = e->g.r.w;
	int lines = 0;
	int vruler_width;
	int vw;

	/* Start from scratch */
	text_edit_clean_formatted_line_list(e);

	/* count lines first */
	line = line_first(&m->line_list);
	while (line)
	{
		lines++;
		line = line_next(line);
	}

	if (e->display_line_numbers)
	{
		vruler_width = 1;
		while (lines)
		{
			vruler_width++;
			lines /= 10;
		}
	} else
	{
		vruler_width = 0;
	}

	vw = gw - vruler_width;
	e->vruler_width = vruler_width;

	line = line_first(&m->line_list);
	while (line)
	{
		int bps;

		wrap.bps = -1;

		if ((bps = wrap_line_nicely_cb(line->contents, vw, text_edit_wrap_callback, &wrap)) >= 0)
		{
			int bp; /* breakpoint */

			for (bp = 0; bp <= bps; bp++)
			{
				text_edit_formatted_line_list_insert_tail(&e->formatted_line_list, line, bp?wrap.pos[bp-1] + 1:0);
			}
		} else
		{
			text_edit_formatted_line_list_insert_tail(&e->formatted_line_list, line, 0);
		}

		line = line_next(line);
	}
}

/******************************************************************************/

int text_edit_input(struct gadget *g, int value)
{
	/* The following code is not optimized yet */
	struct text_edit *e = (struct text_edit *)g;
	struct text_edit_model *m = &e->model;
	struct line *line;

	while (!(line = line_find(&m->line_list, e->cy)))
	{
		if (!line_insert_tail(&m->line_list, ""))
		{
			/* Reject input in failure case (this is probably not a good idea) */
			return 0;
		}
	}

	if (e->editable && (value >= 32 || value == '\n' || value == GADS_KEY_DELETE || value == GADG_KEY_BACKSPACE))
	{
		if (!e->editable(e, value, e->cx, e->cy, e->editable_udata))
		{
			return 0;
		}
	}

	if (value >= 32)
	{
		char *new_string;
		int s_len;

		s_len = line_len(line);
		if (e->cx > s_len) e->cx = s_len;
		if (!(new_string = malloc(s_len + 2)))
		{
			return 0;
		}
		strncpy(new_string, line->contents, e->cx);
		strcpy(&new_string[e->cx + 1], &line->contents[e->cx]);
		new_string[e->cx++] = value;
		free(line->contents);
		line->contents = new_string;
		g->flags |= GADF_REDRAW_UPDATE;
		return 1;
	}

	if (value == '\n')
	{
		struct line *new_node;

		if (!(new_node = line_insert_tail(&m->line_list, &line->contents[e->cx])))
		{
			/* Reject input in failure case (this is probably not a good idea) */
			return 0;
		}
		line->contents[e->cx] = 0;
		e->cx = 0;
		e->cy++;

		/* Remove tail again (we just misused it) and insert at the proper pos */
		line_remove_tail(&m->line_list);
		line_insert_after(&m->line_list, new_node, line);
		return 1;
	}

	if (value == GADS_KEY_DELETE || value == GADG_KEY_BACKSPACE)
	{
		int s_len;

		s_len = line_len(line);

		if (value == GADG_KEY_BACKSPACE)
		{
			if (e->cx > 0)
			{
				e->cx--;
			} else
			{
				return 1;
			}
		}

		if (e->cx < s_len)
		{
			memmove(&line->contents[e->cx], &line->contents[e->cx + 1], strlen(&line->contents[e->cx + 1]) + 1);
		} else
		{
			e->cx = s_len;
		}
		return 1;
	}

	switch (value)
	{
	case GADS_KEY_UP:
		if (e->cy > 0)
		{
			e->cy--;
		}
		break;

	case GADS_KEY_DOWN:
		if (e->cy + 1 < list_length(&m->line_list.l))
		{
			e->cy++;
		}
		break;

	case GADS_KEY_RIGHT:
		if (e->cx < line_len(line))
		{
			e->cx++;
		} else
		{
			if (e->cy + 1 < list_length(&m->line_list.l))
			{
				e->cx = 0;
				e->cy++;
			}
		}
		break;

	case GADS_KEY_LEFT:
		if (e->cx)
		{
			e->cx--;
		} else if (e->cy > 0)
		{
			struct line *l;

			e->cy--;
			l = line_find(&e->model.line_list, e->cy);
			e->cx = line_len(l);
		}
		break;

	case GADS_KEY_HOME:
		e->cx = 0;
		break;

	case GADS_KEY_END:
		{
			struct line *l;
			if ((l = line_find(&e->model.line_list, e->cy)))
			{
				e->cx = line_len(l);
			}
		}
		break;

	default:
		return 0;
	}
	g->flags |= GADF_REDRAW_UPDATE;
	return 1;
}

/******************************************************************************/

static void text_edit_display(struct gadget *g, struct window *win)
{
	struct text_edit *e = (struct text_edit *)g;

	int cx = e->cx;
	int cy = e->cy;
	int wx = win->g.g.r.x;
	int wy = win->g.g.r.y;
	int gx = e->g.r.x;
	int gy = e->g.r.y;
	int gw = e->g.r.w;
	int vw = gw - e->vruler_width;
	int gh = e->g.r.h;
	int line = 0, nline = 0;
	int y = 0;

	struct formatted_line_node *l, *nl;

	text_edit_format(e);

	l = (struct formatted_line_node *)list_first(&e->formatted_line_list);
	while (l && y < gh)
	{
		char lbuf[20];
		int x;
		int mx; /* max x that bears a true character */
		int sl;

		if (e->vruler_width)
		{
			snprintf(lbuf, sizeof(lbuf), "%*d  ", e->vruler_width - 1, line);
			win->scr->puts(win->scr, wx + gx, y + wy + gy, lbuf, strlen(lbuf), normal_style);
		}

		sl = line_len(l->l);

		nl = (struct formatted_line_node *)node_next(&l->n);

		/* Determine maximum x in this line */
		if (nl && nl->l == l->l)
		{
			/* Next displayed line is same as this, consider the breakpoint */
			mx = nl->pos - 1;
		} else
		{
			/* Next displayed line is a different one than this, consider line length */
			mx = sl;

			/* Also increment line number for the next iteration */
			nline = line + 1;
		}

		for (x = 0; x < vw; x++)
		{
			const char *c; /* character to be displayed next */
			void (*puts)(struct screen *scr, int x, int y, const char *text, int len, style_t style);

			if (l->pos + x < mx)
			{
				c = &l->l->contents[l->pos + x];
			} else
			{
				c = " ";
			}

			/* If this is the character below the cursor, enable cursor mode.
			 * We also deal here with the cursor being outside the line length.
			 */
			if ((l->pos + x == cx || (l->pos + x == sl && e->cx > sl)) && l->pos + x <= mx && line == cy)
			{
				puts = win->scr->put_cursor;
			} else
			{
				puts = win->scr->puts;
			}

			puts(win->scr, x + wx + gx + e->vruler_width, y + wy + gy, c, 1, normal_style);
		}

		line = nline;
		l = nl;
		y++;
	}
}

/******************************************************************************/

void gadgets_init_text_edit(struct text_edit *e)
{
	struct text_edit_model *m = &e->model;
	struct line *l;

	memset(e, 0, sizeof(*e));

	gadgets_init(&e->g);
	list_init(&m->line_list.l);
	list_init(&e->formatted_line_list);

	/* Insert first, empty line */
	l = line_insert_tail(&m->line_list, "");
	text_edit_formatted_line_list_insert_tail(&e->formatted_line_list, l, 0);

	e->g.input = text_edit_input;
	e->g.display = text_edit_display;
}

/******************************************************************************/

void gadgets_set_text_edit_contents(struct text_edit *e, const char *txt)
{
	struct text_edit_model *m = &e->model;
	struct line_list l;
	const char *endl;

	list_init(&l.l);

	while ((endl = mystrchrnul(txt, '\n')) != txt)
	{
		line_insert_tail_len(&l, txt, endl - txt);
		txt = endl + !!*endl;
	}

	line_clear(&m->line_list);
	line_exchange(&l, &m->line_list);

	e->g.flags |= GADF_REDRAW_UPDATE;
}

/******************************************************************************/

char *gadgets_get_text_edit_contents(const struct text_edit *e)
{
	const struct text_edit_model *m = &e->model;
	struct line *n;
	char *str, *buf;
	int l;

	/* Determine length first */
	l = 0;
	n = line_first(&m->line_list);
	while (n)
	{
		l += line_len(n) + 1; /* plus newline */
		n = line_next(n);
	}

	if (!(str = (char *)malloc(l + 1))) /* plus null byte */
	{
		return NULL;
	}

	buf = str;
	n = line_first(&m->line_list);
	while (n)
	{
		strcpy(buf, n->contents);
		buf += line_len(n);
		*buf++ = '\n';
		n = line_next(n);
	}
	*buf = 0;

	return str;
}

/******************************************************************************/

int gadgets_get_text_edit_number_of_lines(const struct text_edit *e)
{
	return list_length(&e->model.line_list.l);
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
		win->scr->puts(win->scr, dx, dy + y, buf, buf_len, normal_style);
		for (i = buf_len; i < g->r.w; i++)
		{
			win->scr->puts(win->scr, dx + i, dy + y, " ", 1, normal_style);
		}
	}

	/* Clear remaining space */
	for (; y < g->r.h; y++)
	{
		int i;

		for (i = 0; i < g->r.w; i++)
		{
			win->scr->puts(win->scr, dx + i, dy + y, " ", 1, normal_style);
		}
	}
}

/******************************************************************************/

void gadgets_init_listview(struct listview *v, void (*render)(int pos, char *buf, int bufsize))
{
	memset(v, 0, sizeof(*v));

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
	int x, y;
	int ox = wnd->g.g.r.x;
	int oy = wnd->g.g.r.y;

	/* Clear the window's area before anything else is drawn onto it */
	for (y = oy; y < oy + wnd->g.g.r.h; y++)
	{
		for (x = ox; x < ox + wnd->g.g.r.w; x++)
		{
			scr->puts(scr, x, y, " ", 1, normal_style);
		}
	}

	gadgets_display(wnd, &wnd->g.g);
	if (!wnd->no_input)
	{
		scr->active = wnd;

		/* Update key description since another window was activated */
		if (scr->keys_changed)
		{
			scr->keys_changed(scr);
		}
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

static void screen_in_memory_puts(struct screen *scr, int x, int y, const char *txt, int len, style_t style)
{
	int i;

	if (y < 0 || y >= scr->h)
	{
		return;
	}

	if (x < 0)
	{
		len += x;
		if (len < 0)
		{
			return;
		}
		txt -= x;
		x = 0;
	}
	for (i = x; i < MIN(len, scr->w); i++)
	{
		scr->buf[i + scr->w * y] = txt[i-x];
	}
}

/*******************************************************************************/

static void screen_in_memory_put_cursor(struct screen *scr, int x, int y, const char *txt, int len, style_t style)
{
	/* No special support for this for now */
	screen_in_memory_puts(scr, x, y, txt, len, style);
}

/*******************************************************************************/

static void screen_ncurses_puts(struct screen *scr, int x, int y, const char *txt, int len, style_t style)
{
	if (style.underline)
	{
		wattron(scr->handle, A_UNDERLINE);
	}

	mvwaddnstr(scr->handle, y, x, txt, len);

	if (style.underline)
	{
		wattroff(scr->handle, A_UNDERLINE);
	}
}

/*******************************************************************************/

static void screen_ncurses_put_cursor(struct screen *scr, int x, int y, const char *txt, int len, style_t style)
{
	wattron(scr->handle, A_REVERSE);
	mvwaddnstr(scr->handle, y, x, txt, len);
	wattroff(scr->handle, A_REVERSE);
}

/*******************************************************************************/

static void screen_init_base(struct screen *scr)
{
	memset(scr, 0, sizeof(*scr));
	list_init(&scr->windows);
	list_init(&scr->resize_listeners);
	list_init(&scr->key_listeners);
}

/*******************************************************************************/

static int ncurses_initialized;

/*******************************************************************************/

void screen_init(struct screen *scr)
{
	if (!ncurses_initialized)
	{
		initscr();
		noecho();
		curs_set(0);

		ncurses_initialized = 1;
	}

	screen_init_base(scr);

	{
		int w, h;
		getmaxyx(stdscr, h, w);
		scr->w = w;
		scr->h = h;
		scr->handle = newwin(h, w, 0, 0);
		nodelay(scr->handle, TRUE);
		keypad(scr->handle, TRUE);
		scr->puts = screen_ncurses_puts;
		scr->put_cursor = screen_ncurses_put_cursor;
	}
}

/*******************************************************************************/

void screen_init_in_memory(struct screen *scr, int w, int h)
{
	screen_init_base(scr);

	scr->w = w;
	scr->h = h;

	if (!(scr->buf = malloc(w * h)))
	{
		fprintf(stderr, "Not enough memory (%s:%d)", __FILE__, __LINE__);
		exit(1);
	}

	scr->puts = screen_in_memory_puts;
	scr->put_cursor = screen_in_memory_put_cursor;
}

/*******************************************************************************/

int screen_handle(struct screen *scr)
{
	int ch;
	while ((ch = wgetch(scr->handle)) != 'q')
	{
		if (ch == KEY_RESIZE)
		{
			screen_invoke_resize_listener(scr);
			continue;
		} else if (ch >= 0x100)
		{
			switch (ch)
			{
			case KEY_DOWN:
				ch = GADS_KEY_DOWN;
				break;
			case KEY_UP:
				ch = GADS_KEY_UP;
				break;
			case KEY_LEFT:
				ch = GADS_KEY_LEFT;
				break;
			case KEY_RIGHT:
				ch = GADS_KEY_RIGHT;
				break;
			case KEY_BACKSPACE:
				ch = GADG_KEY_BACKSPACE;
				break;
			case KEY_DC:
				ch = GADS_KEY_DELETE;
				break;
			case KEY_HOME:
				ch = GADS_KEY_HOME;
				break;
			case KEY_END:
				ch = GADS_KEY_END;
				break;

			default:
				ch = GADS_KEY_NONE;
				break;
			}
		} else if (ch <= 0x1f && ch != '\n')
		{
			/* Try to map ctrl keys to proper char first */
			if (screen_invoke_key_listener(scr, ch+64))
			{
				continue;
			}
		}

		if (ch == GADS_KEY_NONE)
		{
			return 0;
		}

		if (scr->active)
		{
			struct gadget *g;

			/* Try active gadget first */
			if ((g = scr->active->active))
			{
				if (g->input)
				{
					if (g->input(g, ch))
					{
						/* Redisplay the entire window */
						/* TODO: Obviously, this can be optimized */
						windows_display(scr->active, scr);
						continue;
					}
				}
			}

			/* Now invoke possible window-related listeners */
			if (window_invoke_key_listener(scr->active, ch))
			{
				continue;
			}
		}
		screen_invoke_key_listener(scr, ch);
	}

	return ch == 'q';
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

int screen_invoke_key_listener(struct screen *scr, int ch)
{
	struct key_listener *l;
	int called = 0;

	l = (struct key_listener *)list_first(&scr->key_listeners);
	while (l)
	{
		struct key_listener *n;

		n = (struct key_listener *)node_next(&l->n);
		if (l->ch == ch)
		{
			l->callback();
			called = 1;
		}

		l = n;
	}
	return called;
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
