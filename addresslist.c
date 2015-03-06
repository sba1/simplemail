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
struct address_list *address_list_create(char *str)
{
	struct address_list *list;
	if (!str) return NULL;

	if ((list = (struct address_list*)malloc(sizeof(*list))))
	{
		struct parse_address addr;
		char *ret;

		list_init(&list->list);

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
					list_insert_tail(&list->list,&new_addr->node);
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
static struct address *address_list_find_by_addr(struct address_list *list, char *addr_spec)
{
	struct address *addr;

	addr = (struct address *)list_first(&list->list);
	while (addr)
	{
		if (!mystricmp(addr->email,addr_spec))
			return addr;
		addr = (struct address*)node_next(&addr->node);
	}
	return NULL;
}

/**
 * Appends mail addresses in the given address string to the given address list.
 *
 * @param list
 * @param str
 */
void address_list_append(struct address_list *list, char *str)
{
	struct address_list *append_list = address_list_create(str);
	if (append_list)
	{
		struct address *new_addr;

		while ((new_addr = (struct address*)list_remove_tail(&append_list->list)))
		{
			int add_it = !address_list_find_by_addr(list,new_addr->email);
			if (add_it) list_insert_tail(&list->list,&new_addr->node);
			else
			{
				free(new_addr->realname);
				free(new_addr->email);
				free(new_addr);
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
void address_list_append_mailbox(struct address_list *list, struct mailbox *mb)
{
	struct mailbox *new_mb;
	if (address_list_find_by_addr(list,mb->addr_spec)) return;
	new_mb = (struct mailbox*)malloc(sizeof(*new_mb));
	if (new_mb)
	{
		new_mb->phrase = mystrdup(mb->phrase);
		new_mb->addr_spec = mystrdup(mb->addr_spec);
		list_insert_tail(&list->list,&new_mb->node);
	}
}

/**
 * Removes an entry corresponding to the given email address from an address list.
 *
 * @param list the list from which the entry should be removed.
 * @param email the email address defining the entry to be removed.
 */
void address_list_remove_by_mail(struct address_list *list, char *email)
{
	struct address *mb = address_list_find_by_addr(list, email);
	if (mb)
	{
		node_remove(&mb->node);
		free(mb->realname);
		free(mb->email);
		free(mb);
	}
}

/**
 * Frees all memory allocated in create_address_list() for the given list.
 * The given list must not be used after that call.
 *
 * @param list the list to free.
 */
void address_list_free(struct address_list *list)
{
	struct address *address;
	while ((address = (struct address*)list_remove_tail(&list->list)))
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
utf8 *address_list_to_utf8_codeset_safe(struct address_list *list, struct codeset *codeset)
{
	struct address *address = (struct address*)list_first(&list->list);
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

/**
 * Returns the first address in the given address list.
 *
 * @param list
 * @return
 */
struct address *address_list_first(struct address_list *list)
{
	return (struct address*)list_first(&list->list);
}

/**
 * Returns the next address.
 *
 * @param address
 * @return
 */
struct address *address_next(struct address *address)
{
	return (struct address*)address->node.next;
}
