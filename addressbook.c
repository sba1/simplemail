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
** addressbook.c
*/

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "addressbook.h"
#include "http.h"
#include "lists.h"
#include "mail.h"
#include "parse.h"
#include "support.h"
#include "support_indep.h"

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
	list_init(&root_entry.u.group.list);

	if (!addressbook_load())
	{
		entry = addressbook_new_person(NULL, "Hynek Schlawack", "hynek@schlawack.net");
		addressbook_set_description(entry, "Original author of SimpleMail");
		addressbook_person_add_email(entry, "Hynek.Schlawack@t-online.de");

		entry = addressbook_new_person(NULL, "Sebastian Bauer", "sebauer@t-online.de");
		addressbook_set_description(entry, "Original author of SimpleMail");
		addressbook_person_add_email(entry, "Sebastian.Bauer@in.stud.tu-ilmenau.de");
	}
}

/**************************************************************************
 Cleanups the addressbook
**************************************************************************/
void cleanup_addressbook(void)
{
	struct addressbook_entry *entry;

	while ((entry = (struct addressbook_entry *)list_remove_tail(&root_entry.u.group.list)))
	{
		addressbook_free_entry(entry);
	}

	root_entry.type = ADDRESSBOOK_ENTRY_GROUP;
	list_init(&root_entry.u.group.list);
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
						if (entry->u.person.realname) free(entry->u.person.realname);
						entry->u.person.realname = mystrdup(&buf[9]);
					}

					if (!mystrnicmp(buf,"EMail=",6)) addressbook_person_add_email(entry,&buf[6]);
					if (!mystrnicmp(buf,"Description=",12)) addressbook_set_description(entry,&buf[12]);
					if (!mystrnicmp(buf,"Street=",7)) entry->u.person.street = mystrdup(&buf[7]);
					if (!mystrnicmp(buf,"City=",5)) entry->u.person.city = mystrdup(&buf[5]);
					if (!mystrnicmp(buf,"Country=",8)) entry->u.person.country = mystrdup(&buf[8]);
					if (!mystrnicmp(buf,"Homepage=",9)) entry->u.person.homepage = mystrdup(&buf[9]);
					if (!mystrnicmp(buf,"Phone1=",7)) entry->u.person.phone1 = mystrdup(&buf[7]);
					if (!mystrnicmp(buf,"Phone2=",7)) entry->u.person.phone2 = mystrdup(&buf[7]);
					if (!mystrnicmp(buf,"Portrait=",9)) entry->u.person.portrait = mystrdup(&buf[9]);

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
			fprintf(fh,"@USER %s\n",entry->u.person.alias?entry->u.person.alias:"");
			if (entry->u.person.realname) fprintf(fh,"RealName=%s\n",entry->u.person.realname);
			if (entry->u.person.description) fprintf(fh,"Description=%s\n",entry->u.person.description);
			for (i=0;i<entry->u.person.num_emails;i++)
			{
				fprintf(fh,"EMail=%s\n",entry->u.person.emails[i]);
			}
			if (entry->u.person.street) fprintf(fh,"Street=%s\n",entry->u.person.street);
			if (entry->u.person.city) fprintf(fh,"City=%s\n",entry->u.person.city);
			if (entry->u.person.country) fprintf(fh,"Country=%s\n",entry->u.person.country);
			if (entry->u.person.homepage) fprintf(fh,"Homepage=%s\n",entry->u.person.homepage);
			if (entry->u.person.phone1) fprintf(fh,"Phone1=%s\n",entry->u.person.phone1);
			if (entry->u.person.phone2) fprintf(fh,"Phone2=%s\n",entry->u.person.phone2);
			if (entry->u.person.portrait) fprintf(fh,"Portrait=%s\n",entry->u.person.portrait);
			fprintf(fh,"@ENDUSER\n");
		} else
		{
			if (entry->type == ADDRESSBOOK_ENTRY_GROUP)
			{
				fprintf(fh,"@GROUP %s\n",entry->u.group.alias?entry->u.group.alias:"");
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
		case	ADDRESSBOOK_ENTRY_GROUP: string = &entry->u.group.alias; break;
		case	ADDRESSBOOK_ENTRY_PERSON: string = &entry->u.person.alias; break;
		case	ADDRESSBOOK_ENTRY_LIST: string = &entry->u.list.alias; break;
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
		case	ADDRESSBOOK_ENTRY_GROUP: string = &entry->u.group.description; break;
		case	ADDRESSBOOK_ENTRY_PERSON: string = &entry->u.person.description; break;
		case	ADDRESSBOOK_ENTRY_LIST: string = &entry->u.list.description; break;
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
		char **new_array = (char**)malloc(sizeof(char*)*(entry->u.person.num_emails+1));
		if (new_array)
		{
			if ((new_array[entry->u.person.num_emails] = mystrdup(email)))
			{
				if (entry->u.person.emails)
				{
					memcpy(new_array, entry->u.person.emails, sizeof(char*)*entry->u.person.num_emails);
					free(entry->u.person.emails);
				}
				entry->u.person.num_emails++;
				entry->u.person.emails = new_array;
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
		list_insert_tail(&root_entry.u.group.list,&new_entry->node);
	else
	{
		if (list->type == ADDRESSBOOK_ENTRY_GROUP)
		{
			list_insert_tail(&list->u.group.list,&new_entry->node);
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
		entry->u.person.realname = mystrdup(realname);
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
			list_insert_tail(&root_entry.u.group.list,&entry->node);
		else
		{
			if (list->type == ADDRESSBOOK_ENTRY_GROUP)
			{
				list_insert_tail(&list->u.group.list,&entry->node);
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
			list_insert_tail(&root_entry.u.group.list,&entry->node);
		else
		{
			if (list->type == ADDRESSBOOK_ENTRY_GROUP)
			{
				list_insert_tail(&list->u.group.list,&entry->node);
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
			if (mode == ADDRESSBOOK_FIND_ENTRY_ALIAS) found = !mystricmp(entry_contents,entry->u.person.alias);
			if (mode == ADDRESSBOOK_FIND_ENTRY_REALNAME) found = !mystricmp(entry_contents,entry->u.person.realname);
			if (mode == ADDRESSBOOK_FIND_ENTRY_EMAIL)
			{
				for (i=0; i<entry->u.person.num_emails && !found;i++)
					found = !mystricmp(entry_contents,entry->u.person.emails[i]);
			}
		} else
		{
			if (mode == ADDRESSBOOK_FIND_ENTRY_ALIAS) found = !mystrnicmp(entry_contents,entry->u.person.alias,cl);
			if (mode == ADDRESSBOOK_FIND_ENTRY_REALNAME) found = !mystrnicmp(entry_contents,entry->u.person.realname,cl);
			if (mode == ADDRESSBOOK_FIND_ENTRY_EMAIL)
			{
				for (i=0; i<entry->u.person.num_emails && !found;i++)
					found = !mystrnicmp(entry_contents,entry->u.person.emails[i],cl);
			}
		}
	}

	if (entry->type == ADDRESSBOOK_ENTRY_GROUP)
	{
		if (mode == ADDRESSBOOK_FIND_ENTRY_ALIAS)
		{
			if (complete)
			{
				found = !mystricmp(entry_contents,entry->u.group.alias);
			} else
			{
				found = !mystrnicmp(entry_contents,entry->u.group.alias,cl);
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
	if (entry) return entry->u.person.realname;
	return NULL;
}

/**************************************************************************
 Returns the portrait of the owner of the given e-Mail address. NULL
 if e-mail is not in the addressbook or no picture is in the galery
**************************************************************************/
char *addressbook_get_portrait(char *email)
{
	struct addressbook_entry *entry = addressbook_find_entry(NULL, email, 1, NULL, ADDRESSBOOK_FIND_ENTRY_EMAIL);
	if (entry) return entry->u.person.portrait;
	return NULL;
}

/**************************************************************************
 Returns a path to the portait of the given e-mail. The returned string
 is allocated with malloc().
**************************************************************************/
char *addressbook_download_portrait(char *email)
{
	if (sm_makedir("PROGDIR:.portraits"))
	{
		char filename[64];
		char *buf;

		strcpy(filename,"PROGDIR:.portraits/");

		while (1)
		{
			int i;
			FILE *fh;

			buf = filename+strlen(filename);

			for (i=0;i<12 && email[i];i++)
			{
				if (isalpha((unsigned char)email[i]))
					*buf++ = email[i];
			}
			sprintf(buf,".%lx",time(NULL));

			if (!(fh = fopen(filename,"rb"))) break;
			fclose(fh);
		}
		if (http_download_photo(filename, email))
		{
			return mystrdup(filename);
		}
	}
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
							new_entry->u.group.alias = mystrdup(entry->u.group.alias);
							new_entry->u.group.description = mystrdup(entry->u.group.description);
							list_init(&new_entry->u.group.list);
						}
						break;

			case	ADDRESSBOOK_ENTRY_PERSON:
						{
							new_entry->u.person.alias = mystrdup(entry->u.person.alias);
							new_entry->u.person.realname = mystrdup(entry->u.person.realname);
							new_entry->u.person.pgpid = mystrdup(entry->u.person.pgpid);
							new_entry->u.person.homepage = mystrdup(entry->u.person.homepage);
							new_entry->u.person.street = mystrdup(entry->u.person.street);
							new_entry->u.person.city = mystrdup(entry->u.person.city);
							new_entry->u.person.country = mystrdup(entry->u.person.country);
							new_entry->u.person.phone1 = mystrdup(entry->u.person.phone1);
							new_entry->u.person.phone2 = mystrdup(entry->u.person.phone2);
							new_entry->u.person.notepad = mystrdup(entry->u.person.notepad);
							new_entry->u.person.description = mystrdup(entry->u.person.description);
							new_entry->u.person.portrait = mystrdup(entry->u.person.portrait);

							if ((new_entry->u.person.emails = (char**)malloc(entry->u.person.num_emails*sizeof(char*))))
							{
								int i;
								for (i=0;i<entry->u.person.num_emails;i++)
									new_entry->u.person.emails[i] = mystrdup(entry->u.person.emails[i]);
								new_entry->u.person.num_emails = entry->u.person.num_emails;
							}
							new_entry->u.person.dob = entry->u.person.dob;
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
						if (entry->u.group.alias) free(entry->u.group.alias);
						if (entry->u.group.description) free(entry->u.group.description);

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

						if (entry->u.person.alias) free(entry->u.person.alias);
						if (entry->u.person.realname) free(entry->u.person.realname);
						if (entry->u.person.pgpid) free(entry->u.person.pgpid);
						if (entry->u.person.homepage) free(entry->u.person.homepage);
						if (entry->u.person.street) free(entry->u.person.street);
						if (entry->u.person.city) free(entry->u.person.city);
						if (entry->u.person.country) free(entry->u.person.country);
						if (entry->u.person.phone1) free(entry->u.person.phone1);
						if (entry->u.person.phone2) free(entry->u.person.phone2);
						if (entry->u.person.notepad) free(entry->u.person.notepad);
						if (entry->u.person.description) free(entry->u.person.description);
						if (entry->u.person.portrait) free(entry->u.person.portrait);

						for (i=0;i<entry->u.person.num_emails;i++)
						{
							if (entry->u.person.emails[i]) free(entry->u.person.emails[i]);
						}
						if (entry->u.person.emails) free(entry->u.person.emails);
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
		if (entry->u.group.alias) return mystrdup(entry->u.group.alias);
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
			if (entry->u.person.alias) return mystrdup(entry->u.person.alias);
			if (entry->u.person.realname) return mystrdup(entry->u.person.realname);
			if (entry->u.person.num_emails) return mystrdup(entry->u.person.emails[0]);
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
		if (entry->type == ADDRESSBOOK_ENTRY_PERSON && entry->u.person.num_emails)
		{
			if (entry->u.person.realname && strlen(entry->u.person.realname))
			{
				if (needs_quotation(entry->u.person.realname))
				{
					str = stradd(str,"\"");
					str = stradd(str,entry->u.person.realname);
					str = stradd(str,"\"");
				} else
				{
					str = stradd(str,entry->u.person.realname);
				}
				
				str = stradd(str," <");
				str = stradd(str,entry->u.person.emails[0]);
				return stradd(str,">");
			} else
			{
				return mystrdup(entry->u.person.emails[0]);
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
					if (needs_quotation(mb->phrase))
					{
						expand = stradd(expand,"\"");
						expand = stradd(expand,mb->phrase);
						expand = stradd(expand,"\"");
					} else
					{
						expand = stradd(expand,mb->phrase);
					}

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
struct addressbook_entry *addressbook_get_entry_from_mail(struct mail *m)
{
	struct addressbook_entry *e = NULL;
	char *from = mail_find_header_contents(m, "from");
	if (from)
	{
		struct parse_address addr;
		if (parse_address(from,&addr))
		{
			struct mailbox *mb = (struct mailbox *)list_first(&addr.mailbox_list);
			if (mb)
			{
				e = addressbook_create_person(mb->phrase, mb->addr_spec);
			}

			free_address(&addr);
		}
	}
	return e;
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
		return entry->u.person.alias + al; /* alias should be removed out of the union */

	if (!entry) entry = addressbook_find_entry(NULL, address, 0, &hits, ADDRESSBOOK_FIND_ENTRY_REALNAME);
	if (entry && hits == 1)
		return entry->u.person.realname + al; /* alias should be removed out of the union */

	if (!entry) entry = addressbook_find_entry(NULL, address, 0, &hits, ADDRESSBOOK_FIND_ENTRY_EMAIL);
	if (entry && hits == 1)
	{
		int i;
		for (i=0; i<entry->u.person.num_emails;i++)
		{
			if (!mystrnicmp(address,entry->u.person.emails[i],al))
				return entry->u.person.emails[i] + al; /* alias should be removed out of the union */
		}
	}

	return NULL;
}

/**************************************************************************
 Returns the first entry of the addressbook (must be a group or NULL)
**************************************************************************/
struct addressbook_entry *addressbook_first(struct addressbook_entry *group)
{
	if (!group) return (struct addressbook_entry *)list_first(&root_entry.u.group.list);
	if (group->type == ADDRESSBOOK_ENTRY_GROUP)
	{
		return (struct addressbook_entry *)list_first(&group->u.group.list);
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

