#ifndef SIMPLEMAIL_NCURSES_GADGETS_H
#define SIMPLEMAIL_NCURSES_GADGETS_H

#include "lists.h"
#include "string_lists.h"

#include <ncurses.h>

struct rect
{
	int x, y, w, h;
};

/** Indicates that an redraw update is needed */
#define GADF_REDRAW_UPDATE (1<<0)

struct window;

struct gadget
{
	struct node n;
	struct rect r;

	unsigned int flags;

	/** Display the given gadget */
	void (*display)(struct gadget *g, struct window *win);

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

/* Special valus for key listeners ch */
#define GADS_KEY_NONE -1
#define GADS_KEY_UP -2
#define GADS_KEY_DOWN -3
#define GADS_KEY_RIGHT -4
#define GADS_KEY_LEFT -5
#define GADS_KEY_DELETE -6
#define GADG_KEY_BACKSPACE -7

struct key_listener
{
	struct node n;
	int ch;

	/** A short description about this action */
	const char *short_description;

	void (*callback)(void);
};

typedef struct list key_listeners_t;

/**
 * The a single window.
 */
struct window
{
	struct group g;
	struct gadget *active;

	/** set to 1, if no input shall be forwared to the window */
	int no_input;

	/** The screen to which the window is currently associated */
	struct screen *scr;

	/** All window specific listeners */
	key_listeners_t key_listeners;
};

/**
 * The root of all windows.
 */
struct screen
{
	/** List of all windows attached to the screen */
	struct list windows;

	/** The window that gets key events first */
	struct window *active;

	/** Global list of listeners that are invoked on a screen resize */
	struct list resize_listeners;

	/** Global list of key listeners */
	key_listeners_t key_listeners;

	/** Called if key listener list has changed */
	void (*keys_changed)(struct screen *scr);

	/** Width of the screen */
	int w;

	/** Height of the screen */
	int h;

	/** Function to put a string on the screen */
	void (*puts)(struct screen *scr, int x, int y, const char *text, int len);

	/** Function to put a string on the screen in cursor mode */
	void (*put_cursor)(struct screen *scr, int x, int y, const char *text, int len);

	/* Specific to the ncurses renderer */
	WINDOW *handle;

	/* Specific to the in-memory renderer */
	char *buf;
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

struct text_edit_model
{
	struct string_list line_list;
};

struct line_node
{
	struct node n;

	/** The string_node that produces the line */
	struct string_node *s;

	/** The first position within the string node */
	int pos;
};

/** A simple text edit field */
struct text_edit
{
	struct gadget g;
	int xoffset;
	int yoffset;

	struct text_edit_model model;

	struct list line_list;
	int vruler_width;

	/* Cursor x and y position */
	int cx, cy;

	/**
	 * @return whether the given position is editable.
	 */
	int (*editable)(struct text_edit *, int ch, int x, int y, void *udata);
	void *editable_udata;
};


/** A simple list of elements. Only a subset of them is usually shown. */
struct listview
{
	struct gadget g;

	/** The current active one or -1 if there is none */
	int active;

	/** Total number of rows */
	int rows;

	/** Render element with index pos into buf */
	void (*render)(int pos, char *buf, int bufsize);
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

/******************************************************************************/

extern struct screen gui_screen;

/******************************************************************************/

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
 * Set the label of the given label gadget.

 * @param l the label to be updated
 * @param text the text to use. The text will be copied internally.
 */
void gadgets_set_label_text(struct simple_text_label *l, const char *text);

/**
 * Initialize the given text view with the given text.
 *
 * @param v the value to be initialized
 * @param text the text to use. A copy will be made.
 */
void gadgets_init_text_view(struct text_view *v, const char *text);

/**
 * Initializes the given text edit gadget.
 *
 * @param e the text edit gadget to be initialized.
 */
void gadgets_init_text_edit(struct text_edit *e);

/**
 * @return the text edit contents allocated via malloc(). Free with free when
 *  no longer needed.
 */
char *gadgets_get_text_edit_contents(const struct text_edit *e);


/**
 * Set the full contents of the text editor.
 *
 * @param e the text edit gadget to be modified
 * @param contents the contents to be set
 */
void gadgets_set_text_edit_contents(struct text_edit *e, const char *contents);

/**
 * Initialize the list view.
 *
 * @param v the value to be initialized
 * @param render callback to render item pos
 */
void gadgets_init_listview(struct listview *v, void (*render)(int pos, char *buf, int bufsize));

/**
 * Initializes the window.
 *
 * @param win the window to initialize.
 */
void windows_init(struct window *win);

/**
 * Display the content of the entire window.
 *
 * @param wnd the window to display
 * @param scr the screen on which to render
 */
void windows_display(struct window *wnd, struct screen *scr);

/**
 * Make the given gadget the active one in context of the window. This
 * gadget will receive all unrouted events.
 *
 * @param wnd where g belongs to.
 * @param g the gadget to be activated.
 */
void windows_activate_gadget(struct window *wnd, struct gadget *g);

/**
 * Add window-scoped key listener to the window.
 *
 * @param win
 * @param l
 * @param ch
 * @param short_description
 * @param callback
 */
void windows_add_key_listener(struct window *win, struct key_listener *l, int ch, const char *short_description, void (*callback)(void));

/**
 * Remove the given key listeners.
 *
 * @param win the window to which the listener is attached.
 * @param l the key listener to be removed.
 */
void windows_remove_key_listener(struct window *win, struct key_listener *l);

/**
 * Invoke the key listener listening to the given ch.
 *
 * @param win the win that contains all listeners.
 * @param ch the character value
 *
 * @return 1, if any key listener was successfully invoked
 */
int window_invoke_key_listener(struct window *win, int ch);

/**
 * Initialize the screen.
 *
 * @param scr the screen to be initialized.
 */
void screen_init(struct screen *scr);

/**
 * Initialize the screen with plain memory as front end.
 *
 * @param scr the screen to initialue
 * @param w the width
 * @param h the height
 */
void screen_init_in_memory(struct screen *scr, int w, int h);


/**
 * Handle screen events.
 *
 * @param scr the screen to handle.
 * @return 1 if loop should be left.
 */
int screen_handle(struct screen *scr);

/**
 * Add the given window to the screen.

 * @param scr the screen to which to add
 * @param wnd the window to add
 */
void screen_add_window(struct screen *scr, struct window *wnd);

/**
 * Remove the given window from the given screen. If the window was
 * active before, no window will be active after this call returned.
 *
 * @param scr the screen from which to remove
 * @param wnd the window to be removed
 */
void screen_remove_window(struct screen *scr, struct window *wnd);

/**
 * @return whether the window is associated to the screen.
 */
int screen_has_window(struct screen *scr, struct window *wnd);

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

/**
 * Add screen-scoped key listener to the screen.
 *
 * @param scr
 * @param l
 * @param ch
 * @param short_description
 * @param callback
 */
void screen_add_key_listener(struct screen *scr, struct key_listener *l, int ch, const char *short_description, void (*callback)(void));

/**
 * Remove the given key listeners.
 *
 * @param scr the screen to which the listener is attached.
 * @param l the key listeners to be removed.
 */
void screen_remove_key_listener(struct screen *src, struct key_listener *l);

/**
 * Invoke the key listener listening to the given ch.
 *
 * @param scr the scr that contains all listeners.
 * @param ch the character value
 */
void screen_invoke_key_listener(struct screen *scr, int ch);

/**
 * Fill in the given buf with a description line of the current active key
 * listeners.
 *
 * @param scr
 * @param buf
 * @param bufsize
 */
void screen_key_description_line(struct screen *scr, char *buf, size_t bufsize);


#endif
