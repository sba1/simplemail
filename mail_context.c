/**
 * @file
 */

#include "mail_context.h"

#include <stdlib.h>

/**
 * Create a single mail context.
 *
 * @return the pointer to a mail context
 */
mail_context *mail_context_create(void)
{
	mail_context *c = (mail_context *)malloc(sizeof(*c));
	if (!c)
	{
		return NULL;
	}
	if (!(c->sp = string_pool_create()))
	{
		free(c);
		return NULL;
	}
	return c;
}

/**
 * Free the given mail context.
 *
 * @param c the context to be freed
 */
void mail_context_free(mail_context *c)
{
	if (!c)
	{
		return;
	}

	string_pool_delete(c->sp);
	free(c);
}
