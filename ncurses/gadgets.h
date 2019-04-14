#ifndef SIMPLEMAIL_NCURSES_GADGETS_H
#define SIMPLEMAIL_NCURSES_GADGETS_H

#include "lists.h"

#include <ncurses.h>

/**
 * Base for all text labels.
 */
struct text_label
{
	struct node node;
	int x;
	int y;

	int w;
	int h;

	const char * (*render)(void *l);
	void (*free)(void *l);
};

/**
 * A simple text label.
 */
struct simple_text_label
{
	struct text_label tl;
	char *text;
};

struct text_view
{
	struct simple_text_label tl;
};

void gadgets_set_extend(struct text_label *l, int x, int y, int w, int h);
void gadgets_init_simple_text_label(struct simple_text_label *l, int x, int y, int w, const char *text);
void gadgets_init_text_view(struct text_view *v, int x, int y, int w, int h, const char *text);
void gadgets_display(WINDOW *win, struct text_label *l);

#endif
