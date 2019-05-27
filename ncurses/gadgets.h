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

typedef struct list key_listeners_t;

/**
 * The a single window.
 */
struct window
{
	struct group g;
	struct gadget *active;
	key_listeners_t key_listeners;
};

/**
 * The root of all windows.
 */
struct screen
{
	struct list windows;
	struct window *active;
	struct list resize_listeners;
	key_listeners_t key_listeners;
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
 * Resize listener
 */
struct screen_resize_listener
{
	struct node n;

	void *udata;
	void (*callback)(void *udata, int x, int y, int width, int height);
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

/**
 * Display the content of the window.
 *
 * @param wnd where to render
 * @param wnd the windows whose contents shall be displayed.
 */
void windows_display(WINDOW *win, struct window *wnd);

/**
 * Initialize the screen.
 *
 * @param scr the screen to be initialized.
 */
void screen_init(struct screen *scr);

/**
 * Add the given window to the screen.

 * @param scr the screen to which to add
 * @param wnd the window to add
 */
void screen_add_window(struct screen *scr, struct window *wnd);

/**
 * Add a listener that is invoked when the screen size is changed.
 *
 * @param listener the listener to add
 * @param callback the callback that is called
 * @param udata the udata is passed to the callback
 */
void screen_add_resize_listener(struct screen *scr,
	struct screen_resize_listener *listener,
	void (*callback)(void *arg, int x, int y, int width, int height), void *udata);

/**
 * Remove a screen's resize listener.
 *
 * @param listener the listener to be removed.
 */
void screen_remove_resize_listener(struct screen_resize_listener *listener);

/**
 * Invoke all added screen listeners.
 *
 * @param scr the screen for which to invoke the resize listeners.
 */
void screen_invoke_resize_listener(struct screen *scr);

#endif
