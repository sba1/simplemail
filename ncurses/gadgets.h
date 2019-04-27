#ifndef SIMPLEMAIL_NCURSES_GADGETS_H
#define SIMPLEMAIL_NCURSES_GADGETS_H

#include "lists.h"

#include <ncurses.h>

struct rect
{
	int x, y, w, h;
};

struct gadget
{
	struct node n;
	struct rect r;

	/** Display the given gadget */
	void (*display)(struct gadget *g, WINDOW *win);
};

/**
 * A list of gadgets.
 */
struct group
{
	struct gadget g;
	struct list l;
};

/**
 * Base for all text labels.
 */
struct text_label
{
	struct gadget g;

	const char * (*render)(void *l);
	void (*free)(void *l);
};

/**
 * A simple text label.
 */
struct simple_text_label
{
	struct text_label tl;

	/** Some text to be rendered. Owned by this */
	char *text;
};

struct text_view
{
	struct simple_text_label tl;
};

void gadgets_set_extend(struct text_label *l, int x, int y, int w, int h);
void gadgets_init_group(struct group *g);
void gadgets_add(struct group *gr, struct gadget *gad);
void gadgets_remove(struct gadget *gad);
void gadgets_init_simple_text_label(struct simple_text_label *l, int x, int y, int w, const char *text);
void gadgets_init_text_view(struct text_view *v, int x, int y, int w, int h, const char *text);
void gadgets_display(WINDOW *win, struct gadget *g);

#endif
