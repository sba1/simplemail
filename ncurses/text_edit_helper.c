/**
 * @file helper for the text edit feature.
 */

#include "text_edit_helper.h"

style_t line_style_at(struct line *l, int pos)
{
	struct style_node *n;
	style_t s = {};
	int cur = 0;

	n = (struct style_node *)list_first(&l->styles);
	while (n)
	{
		s = n->style;
		cur += n->len;

		if (cur > pos)
		{
			break;
		}

		n = (struct style_node *)node_next(&n->n);
	}
	return s;
}

