#ifndef SM__MAIL_CONTEXT_H_
#define SM__MAIL_CONTEXT_H_

#ifndef SM__STRING_POOLS_H
#include "string_pools.h"
#endif

/**
 * The common mail context.
 */
typedef struct
{
	struct string_pool *sp;
} mail_context;


/**
 * Create a single mail context.
 *
 * @return the pointer to a mail context
 */
mail_context *mail_context_create(void);

/**
 * Free the given mail context.
 *
 * @param c the context to be freed
 */
void mail_context_free(mail_context *c);

#endif /* SM__MAIL_CONTEXT_H_ */
