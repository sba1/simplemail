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

	/** Handle the given input, return 1, if the input was handled */
	int (*input)(struct gadget *g, int value);
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
 * The root of all windows.
 */
struct window
{
	struct group g;
	struct gadget *active;
};

/**
 * Base for all text labels.
 */
struct text_label
{
	struct gadget g;
	int xoffset;
	int yoffset;

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

/**
 * Set the extend of the gagdet.
 *
 * @param g the gadget
 * @param x the x coordinate
 * @param y the y coordinate
 * @param w the width
 * @param h the height
 */
void gadgets_set_extend(struct gadget *g, int x, int y, int w, int h);

/**
 * Initialize the given group to later hold a set of gadgets.
 *
 * @param g the group to be initialized
 */
void gadgets_init_group(struct group *g);

/**
 * Add the given gadget to be member of the group-
 *
 * @param gr the group to which the gad should be added
 * @param gad the gad that should be added to the group
 */
void gadgets_add(struct group *gr, struct gadget *gad);

/**
 * Remove a previously added gadget from its group.
 *
 * @param gad the previously added gadget to remove.
 */
void gadgets_remove(struct gadget *gad);

/**
 * Initialize the simple text label with some static text.
 *
 * @param l the label to be initialized
 * @param text the text to use. The text will be copied internally.
 */
void gadgets_init_simple_text_label(struct simple_text_label *l, const char *text);

/**
 * Initialize the given text view with the given text.
 *
 * @param v the value to be initialized
 * @param text the text to use. A copy will be made.
 */
void gadgets_init_text_view(struct text_view *v, const char *text);

/**
 * Display the given gadget onto the given window.
 *
 * @param win the window where to display the gadget.
 * @param g the gadget to display.
 */
void gadgets_display(WINDOW *win, struct gadget *g);

/**
 * Initializes the window.
 *
 * @param win the window to initialize.
 */
void windows_init(struct window *win);

#endif
