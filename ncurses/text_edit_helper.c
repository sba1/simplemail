/**
 * @file helper for the text edit feature.
 */

#include "text_edit_helper.h"

#include <stdlib.h>

/******************************************************************************/

int line_len(const struct line *line)
{
	return strlen(line->contents);
}

/******************************************************************************/

int style_equals(style_t s1, style_t s2)
{
	return s1.underline == s2.underline && s1.bold == s2.bold;
}

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

/******************************************************************************/

int line_style_insert(struct line *l, int pos, style_t s)
{
	struct style_node *n;
	int cur = 0;

	n = line_style_first(l);
	while (n)
	{
		if (cur + n->len > pos)
		{
			struct style_node *n1;
			struct style_node *n2;

			/* Expand style if possible */
			if (style_equals(n->style, s))
			{
				n->len++;
				return 1;
			}

			/* Otherwise, allocate two new nodes. Insert n1 after n and n2 after that */
			if (!(n1 = malloc(sizeof(struct style_node))))
			{
				return 0;
			}

			if (!(n2 = malloc(sizeof(struct style_node))))
			{
				return 0;
			}
			n1->style = s;
			n1->len = 1;

			n2->style = n->style;
			n2->len = n->len - (pos - cur);
			n->len = pos - cur;

			list_insert(&l->styles, &n1->n, &n->n);
			list_insert(&l->styles, &n2->n, &n1->n);

			/* n may be empty now, remove it */
			if (!n->len)
			{
				node_remove(&n->n);
				free(n);
			}
			return 1;
		}

		cur += n->len;
		n = line_style_next(n);
	}

	if (!n)
	{
		if (!(n = malloc(sizeof(struct style_node))))
		{
			return 0;
		}
		n->len = 1;
		n->style = s;
		list_insert_tail(&l->styles, &n->n);
	}
	return 1;
}

/******************************************************************************/

void line_style_remove(struct line *l, int pos)
{
	struct style_node *n;
	int cur = 0;

	n = line_style_first(l);
	while (n)
	{
		if (cur + n->len > pos)
		{
			n->len--;
			if (!n->len)
			{
				node_remove(&n->n);
				free(n);
			}
			return;
		}

		cur += n->len;
		n = line_style_next(n);
	}
}

/*******************************************************************************/

int line_insert_seq(struct line *line, int pos, char *seq, style_t s)
{
	char *new_string;
	int s_len;

	s_len = line_len(line);
	if (pos > s_len) pos = s_len;
	if (!(new_string = malloc(s_len + 2)))
	{
		return -1;
	}
	strncpy(new_string, line->contents, pos);
	strcpy(&new_string[pos + 1], &line->contents[pos]);
	line_style_insert(line, pos, s);
	new_string[pos++] = seq[0];
	free(line->contents);
	line->contents = new_string;
	return pos;
}
