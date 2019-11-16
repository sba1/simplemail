/**
 * @file
 */

#ifndef SM__TEXT_EDIT_HELPER_H
#define SM__TEXT_EDIT_HELPER_H

#include "gadgets.h"

/**
 * @return the length of the line.
 */
int line_len(const struct line *line);

/**
 * @return whether the two given styles are equal.
 */
int style_equals(style_t s1, style_t s2);

/**
 * @return first style node of a line.
 */
struct style_node *line_style_first(struct line *l);

/**
 * @return successor to the style node.
 */
struct style_node *line_style_next(struct style_node *n);

/**
 * Return the style at the given position of the line.
 *
 * @param l the line for which the style shall be returned.
 * @param pos the position for which the style should be returned.
 * @return the style at pos
 */
style_t line_style_at(struct line *l, int pos);

/**
 * Insert the given line style at the given position.
 *
 * @return whether the style could be inserted.
 */
int line_style_insert(struct line *l, int pos, style_t s);

/**
 * Remove the line style information at the given position.
 * Line-styles after the to-be-removed one will be moved one
 * position to the left.
 */
void line_style_remove(struct line *l, int pos);

/**
 * Insert the given sequence into the line at the given position.
 *
 * @param l the line where to insert the sequence.
 * @param pos the character position where to insert the sequence.
 * @param seq the sequence which shall be inserted
 * @param s the style that the sequence should have
 * @return the new position of the cursor or -1 to indicate a failure.
 */
int line_insert_seq(struct line *l, int pos, char *seq, style_t s);

#endif
