/***************************************************************************
 SimpleMail - Copyright (C) 2000 Hynek Schlawack and Sebastian Bauer

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
***************************************************************************/

/*
** $Id$
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "addressbook.h"
#include "lists.h"
#include "parse.h"
#include "support.h"

static struct addressbook_entry root_entry;

/**************************************************************************
 Reads a line. The buffer doesn't contain an LF's
**************************************************************************/
int read_line(FILE *fh, char *buf)
{
	int len;

	if (!fgets(buf,512,fh)) return 0;
	
	len = strlen(buf);
	if (len)
	{
		if (buf[len-1] == '\n') buf[len-1] = 0;
		else if (buf[len-2] == '\r') buf[len-2] = 0;
	}
	return 1;
}

/**************************************************************************
 Initializes the Addressbook
**************************************************************************/
void init_addressbook(void)
{
	struct addressbook_entry *entry;
	root_entry.type = ADDRESSBOOK_ENTRY_GROUP;
	list_init(&root_entry.group.list);

	if (!addressbook_load())
	{
		entry = addressbook_new_person(NULL, "Hynek Schlawack", "hynek@schlawack.net");
		addressbook_set_description(entry, "Orginal author of SimpleMail");
		addressbook_person_add_email(entry, "Hynek.Schlawack@t-online.de");

		entry = addressbook_new_person(NULL, "Sebastian Bauer", "sebauer@t-online.de");
		addressbook_set_description(entry, "Orginal author of SimpleMail");
		addressbook_person_add_email(entry, "Sebastian.Bauer@in.stud.tu-ilmenau.de");
	}
}

/**************************************************************************
 Cleanups the addressbook
**************************************************************************/
void cleanup_addressbook(void)
{
	struct addressbook_entry *entry;

	while ((entry = (struct addressbook_entry *)list_remove_tail(&root_entry.group.list)))
	{
		addressbook_free_entry(entry);
	}

	root_entry.type = ADDRESSBOOK_ENTRY_GROUP;
	list_init(&root_entry.group.list);
}

/**************************************************************************
 Load the entries in the current group. (recursive)
**************************************************************************/
static void addressbook_load_entries(struct addressbook_entry *group, FILE *fh, char *buf)
{
	struct addressbook_entry *entry;

	while (read_line(fh,buf))
	{
		if (!mystrnicmp(buf,"@USER ",6))
		{
			if ((entry = addressbook_new_person(group,NULL,NULL)))
			{
				addressbook_set_alias(entry,&buf[6]);
				while (read_line(fh,buf))
				{
					if (!mystrnicmp(buf,"RealName=",9))
					{
						if (entry->person.realname) free(entry->person.realname);
						entry->person.realname = strdup(&buf[9]);
					}

					if (!mystrnicmp(buf,"EMail=",6)) addressbook_person_add_email(entry,&buf[6]);
					if (!mystrnicmp(buf,"Description=",12)) addressbook_set_description(entry,&buf[12]);

					if (!mystricmp(buf,"@ENDUSER"))
					{
						/* User has been read */
						break;
					}
				}
			}
		}

		if (!mystricmp(buf,"@ENDGROUP"))
		{
			/* the group has been read */
			break;
		}

		if (!mystrnicmp(buf,"@GROUP ", 7))
		{
			if ((entry = addressbook_new_group(group)))
			{
				addressbook_set_alias(entry, &buf[7]);
				addressbook_load_entries(entry,fh,buf);
			}
		}
	}
}

/**************************************************************************
 Load the addressbook. Returns 0 for an error
**************************************************************************/
int addressbook_load(void)
{
	int retval = 0;
	FILE *fh = fopen("PROGDIR:.addressbook","r");
	if (fh)
	{
		char *buf = (char*)malloc(512);
		if (buf)
		{
			if (fgets(buf,512,fh))
			{
				if (!strncmp(buf,"SMAB",4))
				{
					buf[0] = 0;
					addressbook_load_entries(&root_entry,fh,buf);
					retval = 1;
				}
			}
			free(buf);
		}
		fclose(fh);
	}
	return retval;
}

/**************************************************************************
 Saves a address group (recursivly)
**************************************************************************/
static void addressbook_save_group(struct addressbook_entry *group, FILE *fh)
{
	struct addressbook_entry *entry;
	int i;

	entry = addressbook_first(group);
	while (entry)
	{
		if (entry->type == ADDRESSBOOK_ENTRY_PERSON)
		{
			fprintf(fh,"@USER %s\n",entry->person.alias?entry->person.alias:"");
			if (entry->person.realname) fprintf(fh,"RealName=%s\n",entry->person.realname);
			if (entry->person.description) fprintf(fh,"Description=%s\n",entry->person.description);
			for (i=0;i<entry->person.num_emails;i++)
			{
				fprintf(fh,"EMail=%s\n",entry->person.emails[i]);
			}
			fprintf(fh,"@ENDUSER\n");
		} else
		{
			if (entry->type == ADDRESSBOOK_ENTRY_GROUP)
			{
				fprintf(fh,"@GROUP %s\n",entry->group.alias?entry->group.alias:"");
				addressbook_save_group(entry,fh);
				fprintf(fh,"@ENDGROUP\n");
			}
		}

		entry = addressbook_next(entry);
	}
}

/**************************************************************************
 Saves the addressbook to disk
**************************************************************************/
void addressbook_save(void)
{
	FILE *fh = fopen("PROGDIR:.addressbook","w");
	if (fh)
	{
		fputs("SMAB - SimpleMail Addressbook\n",fh);
		addressbook_save_group(&root_entry,fh);
		fclose(fh);
	}
}

/**************************************************************************
 Sets the alias of an addressbook entry
**************************************************************************/
void addressbook_set_alias(struct addressbook_entry *entry, char *alias)
{
	char **string = NULL;

	switch (entry->type)
	{
		case	ADDRESSBOOK_ENTRY_GROUP: string = &entry->group.alias; break;
		case	ADDRESSBOOK_ENTRY_PERSON: string = &entry->person.alias; break;
		case	ADDRESSBOOK_ENTRY_LIST: string = &entry->list.alias; break;
	}

	if (string)
	{
		if (*string) free(*string);
		*string = mystrdup(alias);
	}
}

/**************************************************************************
 Sets the descripton of an addressbook entry
**************************************************************************/
void addressbook_set_description(struct addressbook_entry *entry, char *desc)
{
	char **string = NULL;

	switch (entry->type)
	{
		case	ADDRESSBOOK_ENTRY_GROUP: string = &entry->group.description; break;
		case	ADDRESSBOOK_ENTRY_PERSON: string = &entry->person.description; break;
		case	ADDRESSBOOK_ENTRY_LIST: string = &entry->list.description; break;
	}

	if (string)
	{
		if (*string) free(*string);
		*string = mystrdup(desc);
	}
}

/**************************************************************************
 adds an email address to the given person (the string is duplicated).
 returns 1 for succsess
**************************************************************************/
int addressbook_person_add_email(struct addressbook_entry *entry, char *email)
{
	if (!email) return 0;
	if (entry->type == ADDRESSBOOK_ENTRY_PERSON)
	{
		char **new_array = (char**)malloc(sizeof(char*)*(entry->person.num_emails+1));
		if (new_array)
		{
			if ((new_array[entry->person.num_emails] = strdup(email)))
			{
				if (entry->person.emails)
				{
					memcpy(new_array, entry->person.emails, sizeof(char*)*entry->person.num_emails);
					free(entry->person.emails);
				}
				entry->person.num_emails++;
				entry->person.emails = new_array;
				return 1;
			}
		}
	}
	return 0;
}

/**************************************************************************
 Inserts an entry into the given group
**************************************************************************/
void addressbook_insert_tail(struct addressbook_entry *list, struct addressbook_entry *new_entry)
{
	if (!list)
		list_insert_tail(&root_entry.group.list,&new_entry->node);
	else
	{
		if (list->type == ADDRESSBOOK_ENTRY_GROUP)
		{
			list_insert_tail(&list->group.list,&new_entry->node);
		}
	}
}

/**************************************************************************
 Creates a new person. Does not attach it anywhere
**************************************************************************/
struct addressbook_entry *addressbook_create_person(char *realname, char *email)
{
	struct addressbook_entry *entry = (struct addressbook_entry *)malloc(sizeof(struct addressbook_entry));
	if (entry)
	{
		memset(entry,0,sizeof(struct addressbook_entry));
		entry->type = ADDRESSBOOK_ENTRY_PERSON;
		entry->person.realname = mystrdup(realname);
		addressbook_person_add_email(entry,email);
	}
	return entry;
}


/**************************************************************************
 Adds a person to the addressbook. list maybe NULL or from type
 ADDRESSBOOK_ENTRY_GROUP
**************************************************************************/
struct addressbook_entry *addressbook_new_person(struct addressbook_entry *list, char *realname, char *email)
{
	struct addressbook_entry *entry = addressbook_create_person(realname, email);
	
	if (entry)
	{
		if (!list)
			list_insert_tail(&root_entry.group.list,&entry->node);
		else
		{
			if (list->type == ADDRESSBOOK_ENTRY_GROUP)
			{
				list_insert_tail(&list->group.list,&entry->node);
			}
		}
	}
	return entry;
}

/**************************************************************************
 Creates a new group. Does not attach it anywhere
**************************************************************************/
struct addressbook_entry *addressbook_create_group(void)
{
	struct addressbook_entry *entry = (struct addressbook_entry *)malloc(sizeof(struct addressbook_entry));
	if (entry)
	{
		memset(entry,0,sizeof(struct addressbook_entry));
		entry->type = ADDRESSBOOK_ENTRY_GROUP;
	}
	return entry;
}


/**************************************************************************
 Adds a person to the addressbook. list maybe NULL or from type
 ADDRESSBOOK_ENTRY_GROUP
**************************************************************************/
struct addressbook_entry *addressbook_new_group(struct addressbook_entry *list)
{
	struct addressbook_entry *entry = addressbook_create_group();
	if (entry)
	{
		if (!list)
			list_insert_tail(&root_entry.group.list,&entry->node);
		else
		{
			if (list->type == ADDRESSBOOK_ENTRY_GROUP)
			{
				list_insert_tail(&list->group.list,&entry->node);
			}
		}
	}
	return entry;
}

/* the mode of the find function */
#define ADDRESSBOOK_FIND_ENTRY_ALIAS			0
#define ADDRESSBOOK_FIND_ENTRY_REALNAME	1
#define ADDRESSBOOK_FIND_ENTRY_EMAIL			2

/**************************************************************************
 Find a addressbook entry of any kind (at first the aliases are tried,
 then the Real Name and then the real addressed)
 This function must be reworked!! I don't like it very much. E.g. the args
 are not good.
**************************************************************************/
static struct addressbook_entry *addressbook_find_entry(struct addressbook_entry *entry, char *entry_contents, int complete, int *hits, int mode)
{
	int found = 0;
	int cl = strlen(entry_contents);
	if (!entry) entry = &root_entry;

	if (entry->type == ADDRESSBOOK_ENTRY_PERSON)
	{
		int i;
		if (complete)
		{
			if (mode == ADDRESSBOOK_FIND_ENTRY_ALIAS) found = !mystricmp(entry_contents,entry->person.alias);
			if (mode == ADDRESSBOOK_FIND_ENTRY_REALNAME) found = !mystricmp(entry_contents,entry->person.realname);
			if (mode == ADDRESSBOOK_FIND_ENTRY_EMAIL)
			{
				for (i=0; i<entry->person.num_emails && !found;i++)
					found = !mystricmp(entry_contents,entry->person.emails[i]);
			}
		} else
		{
			if (mode == ADDRESSBOOK_FIND_ENTRY_ALIAS) found = !mystrnicmp(entry_contents,entry->person.alias,cl);
			if (mode == ADDRESSBOOK_FIND_ENTRY_REALNAME) found = !mystrnicmp(entry_contents,entry->person.realname,cl);
			if (mode == ADDRESSBOOK_FIND_ENTRY_EMAIL)
			{
				for (i=0; i<entry->person.num_emails && !found;i++)
					found = !mystrnicmp(entry_contents,entry->person.emails[i],cl);
			}
		}
	}

	if (entry->type == ADDRESSBOOK_ENTRY_GROUP)
	{
		if (mode == ADDRESSBOOK_FIND_ENTRY_ALIAS)
		{
			if (complete)
			{
				found = !mystricmp(entry_contents,entry->group.alias);
			} else
			{
				found = !mystrnicmp(entry_contents,entry->group.alias,cl);
			}
		}

		if (!found)
		{
			struct addressbook_entry *ret = NULL;
			struct addressbook_entry *e = addressbook_first(entry);

			while (e)
			{
				struct addressbook_entry *ret2 = addressbook_find_entry(e,entry_contents,complete,hits,mode);
				if (ret2 && !hits) return ret2;
				if (ret2) ret = ret2;
				e = addressbook_next(e);
			}
			return ret;
		}
	}

	if (found)
	{
		if (hits) (*hits)++;
		return entry;
	}

	return NULL;
}

/**************************************************************************
 Returns the realname of the given e-Mail address. NULL if e-mail is
 not in the addressbook
**************************************************************************/
char *addressbook_get_realname(char *email)
{
	struct addressbook_entry *entry = addressbook_find_entry(NULL, email, 1, NULL, ADDRESSBOOK_FIND_ENTRY_EMAIL);
	if (entry) return entry->person.realname;
	return NULL;
}

/**************************************************************************
 Duplicates a given entry. If it is a group, the group members are not
 duplicated!
**************************************************************************/
struct addressbook_entry *addressbook_duplicate_entry(struct addressbook_entry *entry)
{
	struct addressbook_entry *new_entry = (struct addressbook_entry *)malloc(sizeof(struct addressbook_entry));
	if (new_entry)
	{
		memset(new_entry,0,sizeof(struct addressbook_entry));
		new_entry->type = entry->type;

		switch (entry->type)
		{
			case	ADDRESSBOOK_ENTRY_GROUP:
						{
							new_entry->group.alias = mystrdup(entry->group.alias);
							new_entry->group.description = mystrdup(entry->group.description);
							list_init(&new_entry->group.list);
						}
						break;

			case	ADDRESSBOOK_ENTRY_PERSON:
						{
							new_entry->person.alias = mystrdup(entry->person.alias);
							new_entry->person.realname = mystrdup(entry->person.realname);
							new_entry->person.pgpid = mystrdup(entry->person.pgpid);
							new_entry->person.homepage = mystrdup(entry->person.homepage);
							new_entry->person.street = mystrdup(entry->person.street);
							new_entry->person.city = mystrdup(entry->person.city);
							new_entry->person.country = mystrdup(entry->person.country);
							new_entry->person.phone = mystrdup(entry->person.phone);
							new_entry->person.description = mystrdup(entry->person.description);

							if ((new_entry->person.emails = (char**)malloc(entry->person.num_emails*sizeof(char*))))
							{
								int i;
								for (i=0;i<entry->person.num_emails;i++)
									new_entry->person.emails[i] = mystrdup(entry->person.emails[i]);
								new_entry->person.num_emails = entry->person.num_emails;
							}
							new_entry->person.dob = entry->person.dob;
						}
						break;

			case	ADDRESSBOOK_ENTRY_LIST:
						{
						}
						break;
		}
	}
	return new_entry;
}

/**************************************************************************
 Free all memory of the entry (recursivly)
**************************************************************************/
void addressbook_free_entry(struct addressbook_entry *entry)
{
	switch (entry->type)
	{
		case	ADDRESSBOOK_ENTRY_GROUP:
					{
						struct addressbook_entry *e = addressbook_first(entry);
						if (entry->group.alias) free(entry->group.alias);
						if (entry->group.description) free(entry->group.description);

						while (e)
						{
							struct addressbook_entry *h = addressbook_next(e);
							addressbook_free_entry(e);
							e = h;
						}
					}
					break;

		case	ADDRESSBOOK_ENTRY_PERSON:
					{
						int i;

						if (entry->person.alias) free(entry->person.alias);
						if (entry->person.realname) free(entry->person.realname);
						if (entry->person.pgpid) free(entry->person.pgpid);
						if (entry->person.homepage) free(entry->person.homepage);
						if (entry->person.street) free(entry->person.street);
						if (entry->person.city) free(entry->person.city);
						if (entry->person.country) free(entry->person.country);
						if (entry->person.phone) free(entry->person.alias);
						if (entry->person.description) free(entry->person.description);

						for (i=0;i<entry->person.num_emails;i++)
						{
							if (entry->person.emails[i]) free(entry->person.emails[i]);
						}
						if (entry->person.emails) free(entry->person.emails);
					}
					break;

		case	ADDRESSBOOK_ENTRY_LIST:
					{
					}
					break;
	}
	free(entry);
}

/**************************************************************************
 Returns the address string of the given entry (recursivly).
 The returned string is unexpanded
**************************************************************************/
char *addressbook_get_address_str(struct addressbook_entry *entry)
{
	char *str = NULL;
	if (entry->type == ADDRESSBOOK_ENTRY_GROUP)
	{
		struct addressbook_entry *e;
		if (entry->group.alias) return strdup(entry->group.alias);
		e = addressbook_first(entry);

		if (e)
		{
			if (!(str = addressbook_get_address_str(e))) return NULL;
			e = addressbook_next(e);
		}

		while (e)
		{
			char *new_str = addressbook_get_address_str(e);
			char *new2_str;

			if (!new_str)
			{
				if (str) free(str);
				return NULL;
			}

			if ((new2_str = (char*)malloc(strlen(new_str)+strlen(str)+10)))
			{
				sprintf(new2_str, "%s, %s", str, new_str);
			}

			free(new_str);
			free(str);
			str = new2_str;

			e = addressbook_next(e);
		}
	} else
	{
		if (entry->type == ADDRESSBOOK_ENTRY_PERSON)
		{
			if (entry->person.alias) return strdup(entry->person.alias);
			if (entry->person.realname) return strdup(entry->person.realname);
			if (entry->person.num_emails) return strdup(entry->person.emails[0]);
		}
	}
	return str;
}

/**************************************************************************
 Returns the address string of the given entry (recursivly).
 The returned string is expanded (means it contains the e-mail address)
**************************************************************************/
char *addressbook_get_address_str_expanded(struct addressbook_entry *entry)
{
	char *str = NULL;
	if (entry->type == ADDRESSBOOK_ENTRY_GROUP)
	{
		struct addressbook_entry *e = addressbook_first(entry);

		while (e)
		{
			char *entry_str = addressbook_get_address_str_expanded(e);
			if (!entry_str)
			{
				if (str) free(str);
				return NULL;
			}
			str = stradd(str,entry_str);
			if ((e = addressbook_next(e)))
				str = stradd(str,",");
		}
	} else
	{
		if (entry->type == ADDRESSBOOK_ENTRY_PERSON && entry->person.num_emails)
		{
			if (entry->person.realname && strlen(entry->person.realname))
			{
				/* note "'s must be setted in special cases */
				str = stradd(str,entry->person.realname);
				str = stradd(str," <");
				str = stradd(str,entry->person.emails[0]);
				return stradd(str,">");
			} else
			{
				return strdup(entry->person.emails[0]);
			}
		}
		return NULL;
	}
	return str;
}


/**************************************************************************
 This function returns a expanded string of e-mail Addresses. It uses
 the address book for that purpose and performs syntax checks (uses
 some parse functions). If NULL is returned something had failed.
**************************************************************************/
char *addressbook_get_expand_str(char *unexpand)
{
	struct parse_address paddr;
	char *unexpand_end = unexpand + strlen(unexpand);
	char *buf = unexpand;
	char *expand = NULL;

	while (buf < unexpand_end)
	{
		char *ret;

		memset(&paddr, 0, sizeof(paddr));
		if ((ret = parse_address(buf,&paddr)))
		{
			struct mailbox *mb = (struct mailbox *)list_first(&paddr.mailbox_list);
			while (mb)
			{
				if (mb->phrase)
				{
					/* note "'s must be setted in special cases */
					expand = stradd(expand,mb->phrase);
					expand = stradd(expand," <");
					expand = stradd(expand,mb->addr_spec);
					expand = stradd(expand,">");
				} else
				{
					/* first check if the address is in the address book */
					struct addressbook_entry *entry = addressbook_find_entry(NULL,mb->addr_spec,1,NULL,ADDRESSBOOK_FIND_ENTRY_ALIAS);
					if (!entry) entry = addressbook_find_entry(NULL,mb->addr_spec,1,NULL,ADDRESSBOOK_FIND_ENTRY_REALNAME);
					if (!entry) entry = addressbook_find_entry(NULL,mb->addr_spec,1,NULL,ADDRESSBOOK_FIND_ENTRY_EMAIL);

					if (entry)
					{
						char *new_str = addressbook_get_address_str_expanded(entry);
						if (!new_str)
						{
							if (expand) free(expand);
							break;
						}
						expand = stradd(expand,new_str);
						free(new_str);
					} else
					{
						/* No, so take the lonly address */
						expand = stradd(expand,mb->addr_spec);
					}
				}

				mb = (struct mailbox *)node_next(&mb->node);
				/* add a comma if another entry follows */
				if (mb) expand = stradd(expand,",");
			}
			buf = ret;
			free_address(&paddr);
		} else
		{
			char *tolook;
			ret = strchr(buf,',');
			if (!ret) ret = unexpand_end;

			if ((tolook = strndup(buf,ret - buf)))
			{
				char *new_str;
				struct addressbook_entry *entry = addressbook_find_entry(NULL,tolook,1,NULL,ADDRESSBOOK_FIND_ENTRY_ALIAS);
				if (!entry) entry = addressbook_find_entry(NULL,tolook,1,NULL,ADDRESSBOOK_FIND_ENTRY_REALNAME);
				if (!entry) entry = addressbook_find_entry(NULL,tolook,1,NULL,ADDRESSBOOK_FIND_ENTRY_EMAIL);

				if (!entry)
				{
					if (expand) free(expand);
					return NULL;
				}

				if (!(new_str = addressbook_get_address_str_expanded(entry)))
				{
					if (expand) free(expand);
					return NULL;
				}
				expand = stradd(expand,new_str);
				free(new_str);
			}
			buf = ret;
		}

		if (*buf == ',')
		{
			expand = stradd(expand,",");
			buf++;
			/* then the comma was the last sign which is a error! */
			if (buf >= unexpand_end)
			{
				if (expand) free(expand);
				return NULL;
			}
		}
	}
	return expand;
}

/**************************************************************************
 Completes an alias/realname/e-mail address of the addressbook
**************************************************************************/
char *addressbook_complete_address(char *address)
{
	int hits = 0;
	int al = strlen(address);
	struct addressbook_entry *entry;

	entry = addressbook_find_entry(NULL, address, 0, &hits, ADDRESSBOOK_FIND_ENTRY_ALIAS);
	if (entry && hits == 1)
		return entry->person.alias + al; /* alias should be removed out of the union */

	if (!entry) entry = addressbook_find_entry(NULL, address, 0, &hits, ADDRESSBOOK_FIND_ENTRY_REALNAME);
	if (entry && hits == 1)
		return entry->person.realname + al; /* alias should be removed out of the union */

	if (!entry) entry = addressbook_find_entry(NULL, address, 0, &hits, ADDRESSBOOK_FIND_ENTRY_EMAIL);
	if (entry && hits == 1)
		return entry->person.emails[0] + al; /* alias should be removed out of the union */

	return NULL;
}

/**************************************************************************
 Returns the first entry of the addressbook (must be a group or NULL)
**************************************************************************/
struct addressbook_entry *addressbook_first(struct addressbook_entry *group)
{
	if (!group) return (struct addressbook_entry *)list_first(&root_entry.group.list);
	if (group->type == ADDRESSBOOK_ENTRY_GROUP)
	{
		return (struct addressbook_entry *)list_first(&group->group.list);
	}
	return NULL;
}

/**************************************************************************
 Returns the next entry of the addressbook entry
 (does not work correctly atm!)
**************************************************************************/
struct addressbook_entry *addressbook_next(struct addressbook_entry *entry)
{
	struct addressbook_entry *new_entry;
	new_entry = (struct addressbook_entry *)node_next(&entry->node);
	return new_entry;
}


