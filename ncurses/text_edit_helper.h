/**
 * @file
 */

#ifndef SM__TEXT_EDIT_HELPER_H
#define SM__TEXT_EDIT_HELPER_H

#include "gadgets.h"

/**
 * Return the style at the given position of the line.
 *
 * @param l the line for which the style shall be returned.
 * @param pos the position for which the style should be returned.
 * @return the style at pos
 */
style_t line_style_at(struct line *l, int pos);

#endif
