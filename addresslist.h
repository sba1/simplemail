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

struct list *create_address_list(char *str);
struct mailbox *find_addr_spec_in_address_list(struct list *list, char *addr_spec);
void append_to_address_list(struct list *list, char *str);
void append_mailbox_to_address_list(struct list *list, struct mailbox *mb);
void remove_from_address_list(struct list *list, char *email);
void free_address_list(struct list *list);
utf8 *get_addresses_from_list_safe(struct list *list, struct codeset *codeset);


#endif /* SM__ADDRESSLIST_H */
