#ifndef SM__ADDRESSLIST_H
#define SM__ADDRESSLIST_H

#include "codesets.h"
#include "lists.h"

/* for nodes in an address list NOTE: misplaced in mail.h */
struct address
{
	struct node node;
	char *realname;
	char *email;
};

struct address_list
{
	struct list list;
};

struct mailbox;

struct address_list *address_list_create(char *str);
void address_list_append(struct address_list *list, char *str);
void address_list_append_mailbox(struct address_list *list, struct mailbox *mb);
void address_list_remove_by_mail(struct address_list *list, char *email);
void address_list_free(struct address_list *list);
utf8 *address_list_to_utf8_codeset_safe(struct address_list *list, struct codeset *codeset);


#endif /* SM__ADDRESSLIST_H */
