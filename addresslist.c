/**
 * @file
 */

#include "addresslist.h"

#include "codecs.h"
#include "codesets.h"
#include "parse.h"
#include "support_indep.h"

#include <stdlib.h>
#include <string.h>

/**
 * Creates an address list from a given string (Note, that this is probably
 * misplaced in mail.c)
 *
 * @param str
 * @return
 */
struct list *create_address_list(char *str)
{
	struct list *list;
	if (!str) return NULL;

	if ((list = (struct list*)malloc(sizeof(struct list))))
	{
		struct parse_address addr;
		char *ret;

		list_init(list);

		if ((ret = parse_address(str,&addr)))
		{
			/* note mailbox is simliar to address, probably one is enough */
			struct mailbox *mb = (struct mailbox*)list_first(&addr.mailbox_list);
			while (mb)
			{
				struct address *new_addr = (struct address*)malloc(sizeof(struct address));
				if (new_addr)
				{
					new_addr->realname = mystrdup(mb->phrase);
					new_addr->email = mystrdup(mb->addr_spec);
					list_insert_tail(list,&new_addr->node);
				}
				mb = (struct mailbox*)node_next(&mb->node);
			}
			free_address(&addr);
		}
	}
	return list;
}

/**
 * Checks if the address list already contains an entry with the given
 * addr_spec.
 *
 * @param list
 * @param addr_spec
 * @return
 */
struct mailbox *find_addr_spec_in_address_list(struct list *list, char *addr_spec)
{
	struct mailbox *mb;

	mb = (struct mailbox *)list_first(list);
	while (mb)
	{
		if (!mystricmp(mb->addr_spec,addr_spec))
			return mb;
		mb = (struct mailbox*)node_next(&mb->node);
	}
	return NULL;
}

/**
 * Appends mail addresses in the given address string to the given address list.
 *
 * @param list
 * @param str
 */
void append_to_address_list(struct list *list, char *str)
{
	struct list *append_list = create_address_list(str);
	if (append_list)
	{
		struct mailbox *new_mb;

		while ((new_mb = (struct mailbox*)list_remove_tail(append_list)))
		{
			int add_it = !find_addr_spec_in_address_list(list,new_mb->addr_spec);
			if (add_it) list_insert_tail(list,&new_mb->node);
			else
			{
				free(new_mb->phrase);
				free(new_mb->addr_spec);
				free(new_mb);
			}
		}
		free(append_list);
	}
}

/**
 * Appends the given mailbox as an address book entry into the given list.
 *
 * @param list the list to which the address should be appeneded.
 * @param mb the mailbox
 */
void append_mailbox_to_address_list(struct list *list, struct mailbox *mb)
{
	struct mailbox *new_mb;
	if (find_addr_spec_in_address_list(list,mb->addr_spec)) return;
	new_mb = (struct mailbox*)malloc(sizeof(struct mailbox));
	if (new_mb)
	{
		new_mb->phrase = mystrdup(mb->phrase);
		new_mb->addr_spec = mystrdup(mb->addr_spec);
		list_insert_tail(list,&new_mb->node);
	}
}

/**
 * Removes an entry corresponding to the given email address from an address list.
 *
 * @param list the list from which the entry should be removed.
 * @param email the email address defining the entry to be removed.
 */
void remove_from_address_list(struct list *list, char *email)
{
	struct mailbox *mb = find_addr_spec_in_address_list(list, email);
	if (mb)
	{
		node_remove(&mb->node);
		free(mb->phrase);
		free(mb->addr_spec);
		free(mb);
	}
}

/**
 * Frees all memory allocated in create_address_list() for the given list.
 * The given list must not be used after that call.
 *
 * @param list the list to free.
 */
void free_address_list(struct list *list)
{
	struct address *address;
	while ((address = (struct address*)list_remove_tail(list)))
	{
		if (address->realname) free(address->realname);
		if (address->email) free(address->email);
		free(address);
	}
	free(list);
}

/**
 * Returns a string of all addresses but codeset-safe (means that punycode
 * is used for addresses which contains different char then the given
 * codeset provides)
 *
 * @param list
 * @param codeset
 * @return
 */
utf8 *get_addresses_from_list_safe(struct list *list, struct codeset *codeset)
{
	struct address *address = (struct address*)list_first(list);
	string str;

	if (!string_initialize(&str,200))
		return NULL;

	while (address)
	{
		struct address *nextaddress = (struct address*)node_next(&address->node);
		char *email_allocated = NULL;
		char *email;

		if (codeset && !codesets_unconvertable_chars(codeset,address->email,strlen(address->email)))
			email = address->email;
		else
		{
			email = email_allocated = encode_address_puny(address->email);
		}

		if (address->realname)
		{
			if (needs_quotation(address->realname))
			{
				string_append(&str,"\"");
				string_append(&str,address->realname);
				string_append(&str,"\"");
			} else string_append(&str,address->realname);

			string_append(&str," <");
			string_append(&str,email);
			string_append(&str,">");
		} else string_append(&str,email);

		if (nextaddress) string_append(&str,",");

		free(email_allocated);
		address = nextaddress;
	}
	return str.str;
}

