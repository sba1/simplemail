/**
 * @file helper for the text edit feature.
 */

#include "text_edit_helper.h"

/******************************************************************************/

struct style_node *line_style_first(struct line *l)
{
	return (struct style_node *)list_first(&l->styles);
}

/******************************************************************************/

struct style_node *line_style_next(struct style_node *n)
{
	return (struct style_node *)node_next(&n->n);
}

/******************************************************************************/

style_t line_style_at(struct line *l, int pos)
{
	struct style_node *n;
	style_t s = {};
	int cur = 0;

	n = line_style_first(l);
	while (n)
	{
		s = n->style;
		cur += n->len;

		if (cur > pos)
		{
			break;
		}

		n = line_style_next(n);
	}
	return s;
}

