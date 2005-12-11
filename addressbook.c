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

#include "expatinc.h"

#include "addressbook.h"
#include "codesets.h"
#include "configuration.h"
#include "debug.h"
#include "http.h"
#include "lists.h"
#include "mail.h"
#include "parse.h"
#include "smintl.h"
#include "support.h"
#include "support_indep.h"

#include "addressbookwnd.h"

static struct list group_list;
static struct list address_list;

static struct list yamimport_group_list;

static void snailphonecpy(struct address_snail_phone *dest, struct address_snail_phone *src);
static void freesnailphone(struct address_snail_phone *dest);

/******************* Group *****************/

/**************************************************************************
 Returns the first group
**************************************************************************/
struct addressbook_group *addressbook_first_group(void)
{
	return (struct addressbook_group*)list_first(&group_list);
}

/**************************************************************************
 Returns the next group
**************************************************************************/
struct addressbook_group *addressbook_next_group(struct addressbook_group *grp)
{
	return (struct addressbook_group*)node_next(&grp->node);
}

/**************************************************************************
 Duplicates a given group
**************************************************************************/
struct addressbook_group *addressbook_duplicate_group(struct addressbook_group *srcgrp)
{
	struct addressbook_group *grp = malloc(sizeof(struct addressbook_group));
	if (!grp) return 0;

	memset(grp,0,sizeof(struct addressbook_group));
	if ((grp->name = mystrdup(srcgrp->name)))
	{
		grp->description = mystrdup(srcgrp->description);
		return grp;
	}
	free(grp);
	return NULL;
}

/**************************************************************************
 Free's the given group
**************************************************************************/
void addressbook_free_group(struct addressbook_group *grp)
{
	if (!grp) return;
	free(grp->name);
	free(grp->description);
	free(grp);
}

/**************************************************************************
 Find an group by name
**************************************************************************/
struct addressbook_group *addressbook_find_group_by_name(utf8 *name)
{
	struct addressbook_group *grp;

	grp = (struct addressbook_group*)list_first(&group_list);
	while (grp)
	{
		if (!mystricmp(name,grp->name)) return grp;
		grp = (struct addressbook_group*)node_next(&grp->node);
	}

	return NULL;
}

/**************************************************************************
 Adds a group into the adressbook
**************************************************************************/
struct addressbook_group *addressbook_add_group(utf8 *name)
{
	struct addressbook_group *grp = malloc(sizeof(struct addressbook_group));
	if (!grp) return 0;

	memset(grp,0,sizeof(struct addressbook_group));
	if ((grp->name = mystrdup(name)))
	{
		list_insert_tail(&group_list,&grp->node);
		return grp;
	}

	free(grp);
	return NULL;
}

/**************************************************************************
 Adds a duplicate of the given group into the list
**************************************************************************/
struct addressbook_group *addressbook_add_group_duplicate(struct addressbook_group *group)
{
	if ((group = addressbook_duplicate_group(group)))
		list_insert_tail(&group_list, &group->node);
	return group;
}


/***************** Entry ********************/

/**************************************************************************
 Returns the first entry
**************************************************************************/
struct addressbook_entry_new *addressbook_first_entry(void)
{
	return (struct addressbook_entry_new*)list_first(&address_list);
}

/**************************************************************************
 Returns the next entry
**************************************************************************/
struct addressbook_entry_new *addressbook_next_entry(struct addressbook_entry_new *entry)
{
	return (struct addressbook_entry_new*)node_next(&entry->node);
}

/**************************************************************************
 Duplicate address entry
**************************************************************************/
struct addressbook_entry_new *addressbook_duplicate_entry_new(struct addressbook_entry_new *entry)
{
	struct addressbook_entry_new *new_entry = (struct addressbook_entry_new*)malloc(sizeof(*entry));

	if (new_entry)
	{
		new_entry->alias = mystrdup(entry->alias);
		new_entry->description = mystrdup(entry->description);
		new_entry->realname = mystrdup(entry->realname);
		new_entry->notepad = mystrdup(entry->notepad);
		new_entry->pgpid = mystrdup(entry->pgpid);
		new_entry->homepage = mystrdup(entry->homepage);
		new_entry->portrait = mystrdup(entry->portrait);

		snailphonecpy(&new_entry->priv, &entry->priv);
		snailphonecpy(&new_entry->work, &entry->work);

		new_entry->dob_day = entry->dob_day;
		new_entry->dob_month = entry->dob_month;
		new_entry->dob_year = entry->dob_year;
		new_entry->sex = entry->sex;

		new_entry->email_array = array_duplicate(entry->email_array);
		new_entry->group_array = array_duplicate(entry->group_array);
	}
	return new_entry;
}

/**************************************************************************
 Add a duplicate of given addressbook entry
**************************************************************************/
struct addressbook_entry_new *addressbook_add_entry_duplicate(struct addressbook_entry_new *entry)
{
	if ((entry = addressbook_duplicate_entry_new(entry)))
		list_insert_tail(&address_list, &entry->node);
	return entry;
}

/**************************************************************************
 Free address entry
**************************************************************************/
void addressbook_free_entry_new(struct addressbook_entry_new *entry)
{
	free(entry->alias);
	free(entry->description);
	free(entry->realname);
	free(entry->notepad);
	free(entry->pgpid);
	free(entry->homepage);
	free(entry->portrait);

	freesnailphone(&entry->priv);
	freesnailphone(&entry->work);

	array_free(entry->email_array);
	array_free(entry->group_array);
}

/**************************************************************************
 Returns the rest of the completed string. NULL if this cannot be done.
 type_ptr will be filled with 0 if the alias has been completed, 1 for the
 realname and all greater than 1 the email. Do not change the result!
**************************************************************************/
char *addressbook_get_entry_completing_part(struct addressbook_entry_new *entry, char *part, int *type_ptr)
{
	int pl;
	int i;

	pl = mystrlen(part);

	if (!mystrnicmp(part,entry->alias,pl) && entry->alias)
	{
		if (type_ptr) *type_ptr = 0;
		return entry->alias + pl;
	}

	if (!mystrnicmp(part,entry->realname,pl))
	{
		if (type_ptr) *type_ptr = 1;
		return entry->realname + pl;
	}

	for (i=0;i<array_length(entry->email_array);i++)
	{
		if (!mystrnicmp(part,entry->email_array[i],pl))
		{
			if (type_ptr) *type_ptr = i + 2;
			return entry->email_array[i] + pl;
		}
	}
	return NULL;
}

/**************************************************************************
 Adds an entry into the adressbook
**************************************************************************/
struct addressbook_entry_new *addressbook_add_entry(char *realname)
{
	struct addressbook_entry_new *entry = (struct addressbook_entry_new*)malloc(sizeof(*entry));
	if (!entry) return NULL;
	memset(entry,0,sizeof(*entry));
	entry->realname = mystrdup(realname);
	list_insert_tail(&address_list,&entry->node);
	return entry;
}

/*************** FileIO ******************/

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
 Put a xml elelemt into a file (if string exists)
**************************************************************************/
static void put_xml_element_string(FILE *fh, char *element, char *contents)
{
	char *src;
	char c;
	if (!contents) return;

	src = contents;

	fprintf(fh,"<%s>",element);
	while (src && (c = *src))
	{
		if (((unsigned char)c)>=128)
		{
			unsigned int code;
			src = uft8toucs(src, &code);
			fprintf(fh,"&#x%04X;",code);
			continue;
		}
		else if (c == '&') fputs("&amp;",fh);
		else fputc(c,fh);
		src++;
	}
	fprintf(fh,"</%s>",element);
}

#ifndef SAVEDS
#ifdef __SASC
#define SAVEDS __saveds
#else
#define SAVEDS
#endif
#endif

static struct {
	int newaddressbook_tag;
	int newcontact_tag;
	int newgroup_tag;
	int private_tag;
	int work_tag;
	char *data_buf;

	struct addressbook_entry_new current_entry;
	struct addressbook_group current_group;

	int is_classic_addressbook;
	int contact_tag;
	int group_tag;
	char *classic_group_name;
} xml_context;

/**************************************************************************
 Start Tag
**************************************************************************/
SAVEDS void xml_start_tag(void *data, const char *el, const char **attr)
{
	/* new addressbook format */
	if (!mystricmp("newaddressbook",el)) xml_context.newaddressbook_tag = 1;
	else if (!mystricmp("newcontact",el))
	{
		if (!xml_context.newcontact_tag)
		{
			memset(&xml_context.current_entry,0,sizeof(xml_context.current_entry));
			xml_context.newcontact_tag = 1;
		}
	}
	else if (!mystricmp("newgroup",el))
	{
		if (!xml_context.newgroup_tag)
		{
			memset(&xml_context.current_group,0,sizeof(xml_context.current_group));
			xml_context.newgroup_tag = 1;
		}
	}
	else if (!mystricmp("private",el))
	{
		if (!xml_context.private_tag)
			xml_context.private_tag = 1;
	}
	else if (!mystricmp("work",el))
	{
		if (!xml_context.work_tag)
			xml_context.work_tag = 1;
	}
	else if (!mystricmp("addressbook",el))
	{
		xml_context.is_classic_addressbook = 1;
	}
	else if (!mystricmp("contact",el))
	{
		if (xml_context.is_classic_addressbook) xml_context.contact_tag++;
	}
	else if (!mystricmp("group",el))
	{
		if (xml_context.is_classic_addressbook) xml_context.group_tag++;
	}
}  /* End of start handler */

/**************************************************************************
 End Tag
**************************************************************************/
SAVEDS void xml_end_tag(void *data, const char *el)
{
	struct address_snail_phone *asp = NULL;
	char *data_buf = xml_context.data_buf;

	if (xml_context.private_tag) asp = &xml_context.current_entry.priv;
	else if (xml_context.work_tag) asp = &xml_context.current_entry.work;

	/* Remove ending spaces */
	if (data_buf)
	{
		int len = strlen(data_buf);
		while (len && isspace((unsigned char)data_buf[len-1]))
		{
			if (isspace((unsigned char)data_buf[len-1]))
				len--;
		}
	}

	if (!mystricmp("newaddressbook",el)) xml_context.newaddressbook_tag = 0;
	else if (!mystricmp("addressbook",el)) xml_context.is_classic_addressbook = 0;
	else if (!mystricmp("newcontact",el) || !mystricmp("contact",el))
	{
		if (xml_context.newcontact_tag || xml_context.contact_tag)
		{
			struct addressbook_entry_new *entry;

			xml_context.newcontact_tag = 0;

			if ((entry = (struct addressbook_entry_new*)malloc(sizeof(struct addressbook_entry_new))))
			{
				/* Add the group to the entry, if it is a classical address book */
				if (xml_context.is_classic_addressbook && xml_context.classic_group_name)
				{
					xml_context.current_entry.group_array =
						array_add_string(xml_context.current_entry.group_array,xml_context.classic_group_name);
				}

				*entry = xml_context.current_entry;
				memset(&xml_context.current_entry,0,sizeof(xml_context.current_entry));
				list_insert_tail(&address_list,&entry->node);
			}
		}
	} else if (!mystricmp("newgroup",el))
	{
		if (xml_context.newgroup_tag)
		{
			struct addressbook_group *group;

			xml_context.newgroup_tag = 0;

			if ((group = (struct addressbook_group*)malloc(sizeof(struct addressbook_group))))
			{
				*group = xml_context.current_group;
				memset(&xml_context.current_group,0,sizeof(xml_context.current_group));
				list_insert_tail(&group_list,&group->node);
			}
		}
	}
  else if (!mystricmp("group",el))
	{
		if (xml_context.newcontact_tag) xml_context.current_entry.group_array = array_add_string(xml_context.current_entry.group_array,data_buf);
		else if (xml_context.is_classic_addressbook && xml_context.group_tag)
		{
			/* Old addressbook group */
			xml_context.group_tag--;
			if (!xml_context.group_tag)
			{
				free(xml_context.classic_group_name);
				xml_context.classic_group_name = NULL;
			}
		}
	}
	else if (!mystricmp("private",el)) xml_context.private_tag = 0;
	else if (!mystricmp("work",el)) xml_context.work_tag = 0;
	else if (!mystricmp("alias",el))
	{
		if (xml_context.newcontact_tag)
			xml_context.current_entry.alias = mystrdup(data_buf);
		else if (xml_context.is_classic_addressbook && xml_context.group_tag == 1 && !xml_context.contact_tag)
		{
			if (!xml_context.classic_group_name)
			{
				xml_context.classic_group_name = mystrdup(data_buf);
				addressbook_add_group(data_buf);
			}
		}
	}
	else if (!mystricmp("name",el))
	{
		if (xml_context.newgroup_tag) xml_context.current_group.name = mystrdup(data_buf);
		else if (xml_context.newcontact_tag || xml_context.contact_tag) xml_context.current_entry.realname = mystrdup(data_buf);
	}
	else if (!mystricmp("description",el))
	{
		if (xml_context.newgroup_tag) xml_context.current_group.description = mystrdup(data_buf);
		else if (xml_context.newcontact_tag || xml_context.contact_tag) xml_context.current_entry.description = mystrdup(data_buf);
	}
	else if (!mystricmp("email",el)) xml_context.current_entry.email_array = array_add_string(xml_context.current_entry.email_array,data_buf);
	else if (!mystricmp("pgpid",el)) xml_context.current_entry.pgpid = mystrdup(data_buf);
	else if (!mystricmp("homepage",el)) xml_context.current_entry.homepage = mystrdup(data_buf);
	else if (!mystricmp("portrait",el)) xml_context.current_entry.portrait = mystrdup(data_buf);
	else if (!mystricmp("note",el)) xml_context.current_entry.notepad = mystrdup(data_buf);
	else if (!mystricmp("sex",el))
	{
		if (!mystricmp(data_buf,"female")) xml_context.current_entry.sex = 1;
		else if (!mystricmp(data_buf,"male")) xml_context.current_entry.sex = 2;
	}
	else if (!mystricmp("birthday",el))
	{
		char *buf = data_buf;
		int i;

		i = atoi(buf);
		if (i >= 1 && i <= 12)
		{
			xml_context.current_entry.dob_month = i;
			if ((buf = strchr(buf,'/'))) buf++;
			if (buf) xml_context.current_entry.dob_day = atoi(buf);
			if ((buf = strchr(buf,'/'))) buf++;
			if (buf) xml_context.current_entry.dob_year = atoi(buf);
		}
	} else
	{
		if (asp)
		{
			if (!mystricmp("title",el)) asp->title = mystrdup(data_buf);
			else if (!mystricmp("organization",el)) asp->organization = mystrdup(data_buf);
			else if (!mystricmp("street",el)) asp->street = mystrdup(data_buf);
			else if (!mystricmp("city",el)) asp->city = mystrdup(data_buf);
			else if (!mystricmp("zip",el)) asp->zip = mystrdup(data_buf);
			else if (!mystricmp("state",el)) asp->state = mystrdup(data_buf);
			else if (!mystricmp("country",el)) asp->country = mystrdup(data_buf);
			else if (!mystricmp("mobil",el)) asp->mobil = mystrdup(data_buf);
			else if (!mystricmp("fax",el)) asp->fax = mystrdup(data_buf);
			else if (!mystricmp("phone",el))
			{
				if (!asp->phone1) asp->phone1 = mystrdup(data_buf);
				else if (!asp->phone2) asp->phone2 = mystrdup(data_buf);
			}
		}
	}

	free(data_buf);
	xml_context.data_buf = NULL;
}

/**************************************************************************
 Converts a single UFT-8 Chracter to ISO-Latin 1 one, very very
 incomplete.
**************************************************************************/
#if 0
static char *uft8toiso(char *chr, char *code)
{
	unsigned int unicode;
	char *ret = uft8toucs(chr,&unicode);
	*code = unicode;
	return ret;
}
#endif

/**************************************************************************
 Read the characters
**************************************************************************/
SAVEDS void xml_char_data(void *data, const XML_Char *s, int len)
{
	if (xml_context.newcontact_tag || xml_context.newgroup_tag ||
	    xml_context.contact_tag || xml_context.group_tag)
	{
		int old_len = 0;
		if (xml_context.data_buf)
			old_len = strlen(xml_context.data_buf);

		if ((xml_context.data_buf = (char*)realloc(xml_context.data_buf,old_len+len+1)))
		{
			unsigned char *src = (char*)s;
			unsigned char *dest = (char*)xml_context.data_buf + old_len;

			if (!old_len)
			{
				/* Skip trailing spaces */
				while ((isspace(*src) && len)) /* is not really correct */
				{
					src++;
					len--;
				}
			}

			while (src && len)
			{
				*dest++ = *src++;
				len--;
			}

			*dest = 0;
		}
	}
}

/**************************************************************************
 Initializes the Addressbook
**************************************************************************/
void init_addressbook(void)
{
	struct addressbook_entry_new *entry;

	list_init(&group_list);
	list_init(&address_list);

	addressbook_load();

	if (user.config.dont_add_default_addresses)
		return;

	/* Add the very important email addresses, in case they are not yet within the
   * addressbook */
	if (!addressbook_find_group_by_name("SimpleMail Team"))
		addressbook_add_group("SimpleMail Team");

	if (!(entry = addressbook_find_entry_by_address("hynek@rz.uni-potsdam.de")))
	{
		if ((entry = addressbook_add_entry("Hynek Schlawack")))
		{
			entry->email_array = array_add_string(entry->email_array,"hynek@rz.uni-potsdam.de");
			entry->email_array = array_add_string(entry->email_array,"hynek@hys.in-berlin.de");
			entry->description = mystrdup(_("Original author of SimpleMail"));
			entry->group_array = array_add_string(entry->group_array, "SimpleMail Team");
		}
	} else
	{
		if (addressbook_find_group_by_name("SimpleMail Team") && 
		    !array_contains(entry->group_array,"SimpleMail Team"))
		{
			char **newarray = array_add_string(entry->group_array,"SimpleMail Team");
			if (newarray) entry->group_array = newarray;
		}
	}

	if (!addressbook_find_entry_by_address("mail@sebastianbauer.info"))
	{
		if ((entry = addressbook_find_entry_by_address("sebauer@t-online.de")))
		{
			char **newarray;

			array_free(entry->email_array);
			entry->email_array = array_add_string(NULL,"mail@sebastianbauer.info");

			newarray = array_add_string(entry->group_array,"SimpleMail Team");
			if (newarray) entry->group_array = newarray;
		} else
		{
			if ((entry = addressbook_add_entry("Sebastian Bauer")))
			{
				entry->email_array = array_add_string(entry->email_array,"mail@sebastianbauer.info");
				entry->description = mystrdup(_("Original author of SimpleMail"));
				entry->group_array = array_add_string(entry->group_array, "SimpleMail Team");
			}
		}
	}

	if (!addressbook_find_entry_by_address("bgollesch@sime.at"))
	{
		if ((entry = addressbook_find_entry_by_address("bgollesch@sime.com")))
		{
			array_free(entry->email_array);
			entry->email_array = array_add_string(NULL,"bgollesch@sime.at");
		} else
		{
			if ((entry = addressbook_add_entry("Bernd Gollesch")))
			{
				entry->email_array = array_add_string(entry->email_array,"bgollesch@sime.at");
				entry->description = mystrdup(_("Contributor of SimpleMail"));
				entry->group_array = array_add_string(entry->group_array, "SimpleMail Team");
			}
		}
	}

	if (!addressbook_find_entry_by_address("henes@biclodon.com"))
	{
		if ((entry = addressbook_find_entry_by_address("henes@morphos.de")))
		{
			int idx;
			char **newarray;

			/* idx must exist, so no check for -1 is needed */
			idx = array_index(entry->email_array,"henes@morphos.de");
			newarray = array_replace_idx(entry->email_array,idx,"henes@biclodon.com");
			if (newarray) entry->email_array = newarray;

			if (!array_contains(entry->group_array,"SimpleMail Team"))
			{
				newarray = array_add_string(entry->group_array,"SimpleMail Team");
				if (newarray) entry->group_array = newarray;
			}
		} else
		if ((entry = addressbook_add_entry("Nicolas Sallin")))
		{
			entry->email_array = array_add_string(entry->email_array,"henes@biclodon.com");
			entry->description = mystrdup(_("MorphOS maintainer of SimpleMail"));
			entry->group_array = array_add_string(entry->group_array, "SimpleMail Team");
			entry->alias = mystrdup("Henes");
		}
	}
}

/**************************************************************************
 Cleanups the addressbook
**************************************************************************/
void cleanup_addressbook(void)
{
	struct addressbook_entry_new *entry;
	struct addressbook_group *group;

	while ((entry = (struct addressbook_entry_new *)list_remove_tail(&address_list)))
		addressbook_free_entry_new(entry);

	while ((group = (struct addressbook_group*)list_remove_tail(&group_list)))
		addressbook_free_group(group);
}

/**************************************************************************
 Load the entries in the current group as XML
**************************************************************************/
static void addressbook_load_entries(FILE *fh)
{
	char *buf;
	XML_Parser p;

	memset(&xml_context,0,sizeof(xml_context));

	if (!(buf = malloc(512))) return;
	if (!(p = XML_ParserCreate(NULL)))
	{
		free(buf);
		return;
	}

	XML_SetElementHandler(p, xml_start_tag, xml_end_tag);
	XML_SetCharacterDataHandler(p, xml_char_data);
	XML_UseParserAsHandlerArg(p);

	for (;;)
	{
		int len;

		len = fread(buf, 1, 512, fh);
		if (len <= 0) break;

		if (!XML_Parse(p, buf, len, 0))
		{
			int column = XML_GetErrorColumnNumber(p);
			int line = XML_GetErrorLineNumber(p);
			enum XML_Error err = XML_GetErrorCode(p);

			SM_DEBUGF(5,("Parsing the xml file failed because %s (code %d) at %d/%d\n",
						XML_ErrorString(err),err,line,column));

			sm_request(NULL,_("Parsing the XML file failed because of\n%s (code %d)\nat line %d column %d"),
							_("Ok"),XML_ErrorString(err),err,line,column);
			break;
		}
 	}

  XML_Parse(p,buf,0,1); /* is_final have to be done */

	XML_ParserFree(p);
	free(buf);
}

/**************************************************************************
 Loads the Addressbook as the xml format
**************************************************************************/
int addressbook_import_sm(char *filename)
{
	int retval = 0;
	FILE *fh;

	if ((fh = fopen(filename,"r")))
	{
		addressbook_load_entries(fh);
		fclose(fh);
		retval = 1;
	}
	return retval;
}

static char *striplr(char *string)
{
	int len;
	char *line = (char *)string;

	if (line)
	{
		if ((len = strlen(line)))
		{
			if(line[len-1] == '\n')
			{
				line[len-1] = 0;
			}
		}
	}

	return string;
}

/**************************************************************************
 Import addressbook entries from YAM
**************************************************************************/
static int yam_import_entries(FILE *fp)
{
	int rc = 1;
	static char line[1024];
	char *charset = user.config.default_codeset?user.config.default_codeset->name:NULL;

	if (!fgets(line,sizeof(line),fp)) return 0;
	while ((!feof(fp)) && (strncmp(line, "@ENDGROUP",9) != 0))
	{
		if (strncmp(line, "@USER", 5) == 0)
		{
			struct addressbook_group *grp;
			struct addressbook_entry_new *newperson;
			if ((newperson = addressbook_add_entry(NULL)))
			{
				newperson->alias = utf8create(striplr(line) + 6, charset);

				if (!fgets(line,sizeof(line),fp)) return 0;
				newperson->email_array = array_add_string(newperson->email_array, striplr(line));

				if (!fgets(line,sizeof(line),fp)) return 0;
				newperson->realname = utf8create(striplr(line), charset);

				if (!fgets(line,sizeof(line),fp)) return 0;
				newperson->description = utf8create(striplr(line), charset);

				if (!fgets(line,sizeof(line),fp)) return 0;
				newperson->priv.phone1 = utf8create(striplr(line), charset);

				if (!fgets(line,sizeof(line),fp)) return 0;
				newperson->priv.street = utf8create(striplr(line), charset);

				if (!fgets(line,sizeof(line),fp)) return 0;
				newperson->priv.zip	= utf8create(striplr(line), charset);

				if (!fgets(line,sizeof(line),fp)) return 0;
				newperson->priv.country = utf8create(striplr(line), charset);

				if (!fgets(line,sizeof(line),fp)) return 0;
				newperson->pgpid = utf8create(striplr(line), charset);

				if (!fgets(line,sizeof(line),fp)) return 0;
				newperson->dob_day   = 10*(line[0]-'0') + (line[1]-'0');
				newperson->dob_month = 10*(line[2]-'0') + (line[3]-'0');
				newperson->dob_year  = 1000*(line[4]-'0') + 100*(line[5]-'0') + 10*(line[6]-'0') + (line[7]-'0');

				if (!fgets(line,sizeof(line),fp)) return 0;
				newperson->portrait = mystrdup(striplr(line));

				if (!fgets(line,sizeof(line),fp)) return 0;
				newperson->homepage = mystrdup(striplr(line));

				while (fgets(line,sizeof(line),fp))
				{
					if (!strncmp(line, "@ENDUSER", 8)) break;
				}

				/* Add YAM import groups to the person */
				grp = (struct addressbook_group*)list_first(&yamimport_group_list);
				while (grp)
				{
					/* Add YAM import group to the addressbook, if not already done */
					if (!addressbook_find_group_by_name(grp->name))
						addressbook_add_group_duplicate(grp);
					newperson->group_array = array_add_string(newperson->group_array,grp->name);
					grp = (struct addressbook_group*)node_next(&grp->node);
				}
			}
		} else if(strncmp(line, "@GROUP", 6) == 0)
		{
			struct addressbook_group *newgroup = malloc(sizeof(struct addressbook_group));

			/* Add the group to the YAM import group list */
			memset(newgroup,0,sizeof(struct addressbook_group));
			if (newgroup)
			{
				newgroup->name = striplr(utf8create(line + 7, charset));

				if (fgets(line,sizeof(line),fp))
					newgroup->description = striplr(utf8create(line, charset));

				list_insert_tail(&yamimport_group_list,&newgroup->node);
			}
			rc = yam_import_entries(fp);
			if (newgroup)
			{
				/* remove the group from the YAM import group list */
				list_remove_tail(&yamimport_group_list);
				addressbook_free_group(newgroup);
			}
		}

		if (!fgets(line,sizeof(line),fp)) return 0;
	}

	return rc;
}

/**************************************************************************
 Import addressbook entries from YAM
**************************************************************************/
int addressbook_import_yam(char *filename)
{
	int rc = 0;
	FILE *fp;

	fp = fopen(filename, "r");
	if (fp != NULL)
	{
		struct addressbook_group *grp = malloc(sizeof(struct addressbook_group));

		/* build a temporary YAM Import group list */
		list_init(&yamimport_group_list);
		if (grp)
		{
			memset(grp,0,sizeof(struct addressbook_group));
			if ((grp->name = mystrdup(Q_("?addressbook:YAM Imports"))))
			{
				list_insert_tail(&yamimport_group_list,&grp->node);
			} else
			{
				free(grp);
			}
		}

		rc = yam_import_entries(fp);
		fclose(fp);

		/* Free the temporary YAM Import group list */
		while ((grp = (struct addressbook_group*)list_remove_tail(&yamimport_group_list)))
			addressbook_free_group(grp);
	}

	return rc;
}

/**************************************************************************
 Load the addressbook. Returns 0 for an error
**************************************************************************/
int addressbook_load(void)
{
	if (!addressbook_import_sm("PROGDIR:.newaddressbook.xml"))
		return addressbook_import_sm("PROGDIR:.addressbook.xml");
	return 1;
}

#define BOOK_UNKNOWN 0
#define BOOK_YAM 1
#define BOOK_SM 2

/**************************************************************************
 Returns the type of an addressbook file
**************************************************************************/
static int addressbook_get_type(char *filename)
{
	int rc = BOOK_UNKNOWN;
	FILE *fp;
	char buf[64];

	if ((fp = fopen(filename, "r")))
	{
		if (fgets(buf,sizeof(buf),fp))
		{
			if (strncmp(buf, "YAB4 - YAM Addressbook",22) == 0) rc = BOOK_YAM;
			else if (strncmp(buf, "<addressbook>",13) == 0) rc = BOOK_SM;
			else if (strncmp(buf, "<newaddressbook>",16) == 0) rc = BOOK_SM;
		}
		fclose(fp);
	}

	return rc;
}

/**************************************************************************
 Add entries from a specified file. Set append to 1 if the addressbook
 should be appended. filename mybe NULL which means that the default
 filename is used. Returns 1 for success.
 TODO: replace addressbook_load().
**************************************************************************/
int addressbook_import_file(char *filename, int append)
{
	int rc = 0;

	if (!filename) filename = "PROGDIR:.newaddressbook.xml";

	if (!append) cleanup_addressbook();

	switch (addressbook_get_type(filename))
	{
		case BOOK_YAM:
			rc = addressbook_import_yam(filename);
			break;

		case BOOK_SM:
			rc = addressbook_import_sm(filename);
			break;

		case BOOK_UNKNOWN:
		default:
			sm_request(NULL, _("Unsupported type of addressbook."), _("Okay"));
			break;
	}
	return rc;
}

/**************************************************************************
 Saves the address_snail_phone structure as xml
**************************************************************************/
static void addressbook_save_snail_phone(char *container, struct address_snail_phone *asp, FILE *fh)
{
	fprintf(fh,"<%s>\n",container);

	put_xml_element_string(fh,"title", asp->title);
	put_xml_element_string(fh,"organization", asp->organization);
	put_xml_element_string(fh,"street", asp->street);
	put_xml_element_string(fh,"city", asp->city);
	put_xml_element_string(fh,"zip", asp->zip);
	put_xml_element_string(fh,"state", asp->state);
	put_xml_element_string(fh,"country", asp->country);
	put_xml_element_string(fh,"mobil", asp->mobil);
	put_xml_element_string(fh,"fax", asp->fax);
	put_xml_element_string(fh,"phone", asp->phone1);
	put_xml_element_string(fh,"phone", asp->phone2);

	fprintf(fh,"</%s>\n",container);
}

/**************************************************************************
 Save an entry
**************************************************************************/
static void addressbook_save_entry(FILE *fh, struct addressbook_entry_new *entry)
{
	int i;

	fputs("<newcontact>\n",fh);
	put_xml_element_string(fh,"alias",entry->alias);
	put_xml_element_string(fh,"name",entry->realname);
	put_xml_element_string(fh,"description",entry->description);
	put_xml_element_string(fh,"pgpid",entry->pgpid);

	for (i=0; i < array_length(entry->email_array);i++)
		put_xml_element_string(fh,"email",entry->email_array[i]);

	put_xml_element_string(fh,"homepage",entry->homepage);
	put_xml_element_string(fh,"portrait",entry->portrait);
	put_xml_element_string(fh,"note",entry->notepad);
	if (entry->sex) put_xml_element_string(fh,"sex",entry->sex==1?"female":"male");

	for (i=0; i < array_length(entry->group_array);i++)
		put_xml_element_string(fh, "group", entry->group_array[i]);

	if (entry->dob_year)
	{
		char buf[128];
		sprintf(buf,"%d/%d/%d",entry->dob_month, entry->dob_day, entry->dob_year);
		put_xml_element_string(fh,"birthday",buf);
	}
	addressbook_save_snail_phone("private",&entry->priv,fh);
	addressbook_save_snail_phone("work",&entry->work,fh);

	fputs("</newcontact>\n",fh);
}

/**************************************************************************
 Save a group
**************************************************************************/
static void addressbook_save_group(FILE *fh, struct addressbook_group *group)
{
	fputs("<newgroup>\n",fh);
	put_xml_element_string(fh,"name",group->name);
	put_xml_element_string(fh,"description",group->description);
	fputs("</newgroup>\n",fh);
}

/**************************************************************************
 Saves the addressbook to disk
**************************************************************************/
void addressbook_save(void)
{
	FILE *fh = fopen("PROGDIR:.newaddressbook.xml","w");
	if (fh)
	{
		struct addressbook_entry_new *entry;
		struct addressbook_group *group;

		fputs("<newaddressbook>\n",fh);

		group = addressbook_first_group();
		while (group)
		{
			addressbook_save_group(fh,group);
			group = addressbook_next_group(group);
		}

		entry = addressbook_first_entry();
		while (entry)
		{
			addressbook_save_entry(fh,entry);
			entry = addressbook_next_entry(entry);
		}

		fputs("</newaddressbook>\n",fh);
		fclose(fh);
	}
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
			sprintf(buf,".%x",sm_get_current_seconds());

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
 Copies the contents of a struct address_snail_phone to another one
**************************************************************************/
static void snailphonecpy(struct address_snail_phone *dest, struct address_snail_phone *src)
{
	dest->title = mystrdup(src->title);
	dest->organization = mystrdup(src->organization);
	dest->street = mystrdup(src->street);
	dest->city = mystrdup(src->city);
	dest->zip = mystrdup(src->zip);
	dest->state = mystrdup(src->state);
	dest->country = mystrdup(src->country);
	dest->phone1 = mystrdup(src->phone1);
	dest->phone2 = mystrdup(src->phone2);
	dest->mobil = mystrdup(src->mobil);
	dest->fax = mystrdup(src->fax);
}

/**************************************************************************
 Frees all strings associated with dest (only the contents! not the
 struct itself)
**************************************************************************/
static void freesnailphone(struct address_snail_phone *dest)
{
	/* Its safe to call free() with NULL! */
	free(dest->title);
	free(dest->organization);
	free(dest->street);
	free(dest->city);
	free(dest->zip);
	free(dest->state);
	free(dest->country);
	free(dest->phone1);
	free(dest->phone2);
	free(dest->mobil);
	free(dest->fax);
}

/**************************************************************************
 ...
**************************************************************************/
struct addressbook_entry_new *addressbook_find_entry_by_address(char *email)
{
	struct addressbook_entry_new *entry;

	entry = addressbook_first_entry();
	while (entry)
	{
		int i;

		for (i=0;i<array_length(entry->email_array);i++)
		{
			if (!mystricmp(email,entry->email_array[i])) return entry;
		}
		entry = addressbook_next_entry(entry);
	}
	return NULL;
}

/**************************************************************************
 ...
**************************************************************************/
struct addressbook_entry_new *addressbook_find_entry_by_alias(char *alias)
{
	struct addressbook_entry_new *entry;

	entry = addressbook_first_entry();
	while (entry)
	{
		if (!utf8stricmp(alias,entry->alias)) return entry;
		entry = addressbook_next_entry(entry);
	}
	return NULL;
}

/**************************************************************************
 ...
**************************************************************************/
struct addressbook_entry_new *addressbook_find_entry_by_realname(char *realname)
{
	struct addressbook_entry_new *entry;

	entry = addressbook_first_entry();
	while (entry)
	{
		if (!utf8stricmp(entry->realname,realname)) return entry;
		entry = addressbook_next_entry(entry);
	}
	return NULL;
}

/**************************************************************************
 Returns the expanded email address of given entry. It prefers to use the
 email address given by the index. If index is out of range, NULL is
 returned
**************************************************************************/
static char *addressbook_get_expanded_email_from_entry_indexed(struct addressbook_entry_new *entry, int index)
{
	string str;
	char *email;

	if (index >= array_length(entry->email_array)) return NULL;

	email = entry->email_array[index];

	if (mystrlen(entry->realname))
	{
		if (!string_initialize(&str,100)) return NULL;

		if (needs_quotation(entry->realname))
		{
			if (!string_append(&str,"\"")) goto bailout;
			if (!string_append(&str,entry->realname)) goto bailout;
			if (!string_append(&str,"\"")) goto bailout;
		} else
		{
			if (!string_append(&str,entry->realname)) goto bailout;
		}
		if (!string_append(&str," <")) goto bailout;
		if (!string_append(&str,email)) goto bailout;
		if (!string_append(&str,">")) goto bailout;
		return str.str;
	}

	return mystrdup(email);
bailout:
	free(str.str);
	return NULL;
}


/**************************************************************************
 This function returns an expanded string consisting of phrases and email
 addresses. Input string is a comma separated list of phrases, email
 addresses or both.
 It uses the address book for that purpose and performs syntax checks
 (uses some parse functions). If NULL is returned something had failed.
**************************************************************************/
char *addressbook_get_expanded(char *unexpand)
{
	struct addressbook_entry_new *entry;
	struct addressbook_group *group;
	char *unexpand_end = unexpand + strlen(unexpand);
	char *buf = unexpand;
	char *tolook; /* used in the unexpanded branch */
	struct mailbox mb;
	string expanded;

	if (!string_initialize(&expanded, 1000)) return NULL;

	mb.phrase = NULL;
	mb.addr_spec = NULL;
	tolook = NULL;

	while (buf < unexpand_end)
	{
		char *ret;

		if ((ret = parse_mailbox(buf,&mb)))
		{
			/* It's a valid email address */
			if (mb.phrase)
			{
				/* note "'s must be setted in special cases */
				if (needs_quotation(mb.phrase))
				{
					if (!string_append(&expanded,"\"")) goto bailout;
					if (!string_append(&expanded, mb.phrase)) goto bailout;
					if (!string_append(&expanded, "\"")) goto bailout;
				} else
				{
					if (!string_append(&expanded, mb.phrase)) goto bailout;
				}
				if (!string_append(&expanded, " <")) goto bailout;
				if (!string_append(&expanded, mb.addr_spec)) goto bailout;
				if (!string_append(&expanded, ">")) goto bailout;
			} else
			{
				/* determine if the address is inside the address book */
				if ((entry = addressbook_find_entry_by_address(mb.addr_spec)))
				{
					/* Found, so we are able to "expand" the address */
					int idx;
					char *addr;

					idx = array_index(entry->email_array,mb.addr_spec);
					addr = addressbook_get_expanded_email_from_entry_indexed(entry,idx);
					if (!addr) goto bailout;
					if (!string_append(&expanded, addr))
					{
						free(addr);
						goto bailout;
					}
				} else
				{
					/* Not found, and hence cannot be expanded so take the plain email address,
             take the lonely address */
					if (!string_append(&expanded, mb.addr_spec)) goto bailout;
				}
			}
			free(mb.phrase);mb.phrase = NULL;
			free(mb.addr_spec); mb.addr_spec = NULL;

			/* advance buffer ptr */
			buf = ret;
		} else
		{
			char *addr;

			/* It's not a valid email address */
			while (isspace((unsigned char)*buf)) buf++;
			ret = strchr(buf,',');
			if (!ret) ret = unexpand_end;

			if (!(tolook = strndup(buf,ret - buf))) goto bailout;

			/* try if string is a groups */
			if ((group = addressbook_find_group_by_name(tolook)))
			{
				/* yes, then extract all email addresses belonging to this group */
				int first = 1;

				entry = addressbook_first_entry();
				while (entry)
				{
					if (array_contains(entry->group_array,group->name))
					{
						if (!(addr = addressbook_get_expanded_email_from_entry_indexed(entry,0)))
							goto bailout;

						if (!first) string_append(&expanded, ",");

						if (!string_append(&expanded, addr))
						{
							free(addr);
							goto bailout;
						}
						first = 0;
					}
					entry = addressbook_next_entry(entry);
				}
			}	else
			{
				if (!(entry = addressbook_find_entry_by_alias(tolook)))
					entry = addressbook_find_entry_by_realname(tolook);

				/* Not found, so bail out as the input is invalid */
				if (!entry) goto bailout;

				if (!(addr = addressbook_get_expanded_email_from_entry_indexed(entry,0)))
					goto bailout;

				if (!string_append(&expanded, addr))
				{
					free(addr);
					goto bailout;
				}
			}

			free(tolook);tolook=NULL;

			/* advance buffer ptr */
			buf = ret;
		}

		if (*buf == ',')
		{
			if (!string_append(&expanded,",")) goto bailout;
			buf++;
			/* then the comma was the last sign which is a error! */
			if (buf >= unexpand_end) goto bailout;
		}
	}
	return expanded.str;

bailout:
	free(tolook);
	free(expanded.str);
	free(mb.phrase);
	free(mb.addr_spec);
	return NULL;
}

/**************************************************************************
 Returns a string array of all addresses within the addressbook.
 array must be free'd with array_free() when no longer used.
**************************************************************************/
char **addressbook_get_array_of_email_addresses(void)
{
	struct addressbook_entry_new *entry;
	char **array = NULL;

	entry = addressbook_first_entry();
	while (entry)
	{
		array_add_array(array,entry->email_array);
		entry = addressbook_next_entry(entry);
	}
	return array;
}

/**************************************************************************
 Completes an groupname/alias/realname/e-mail address of the addressbook
**************************************************************************/
char *addressbook_complete_address(char *address)
{
	int al = strlen(address);
	struct addressbook_entry_new *entry;
	struct addressbook_group *group;

	char *group_name = NULL, *real_name = NULL;

	/* find matching group */
	group = addressbook_first_group();
	while (group)
	{
		if (!utf8stricmp_len(group->name,address,al))
		{
			group_name = group->name;
			break;
		}
		group = addressbook_next_group(group);
	}

	/* find matching realname  */
	entry = addressbook_first_entry();
	while (entry)
	{
		if (!utf8stricmp_len(entry->realname,address,al))
		{
			real_name = entry->realname;
			break;
		}
		entry = addressbook_next_entry(entry);
	}

	/* if two matches have been found, choose the lexicographical smaller string */
	if (real_name && group_name)
	{
		if (utf8stricmp(real_name,group_name) < 0) return real_name + al;
		return group_name + al;
	}

	if (real_name) return real_name + al;
	if (group_name) return group_name + al;

	/* try if there exists a matching alias */
	entry = addressbook_first_entry();
	while (entry)
	{
		if (!utf8stricmp_len(entry->alias,address,al))
			return entry->alias + al;
		entry = addressbook_next_entry(entry);
	}

	/* addresses */
	entry = addressbook_first_entry();
	while (entry)
	{
		int i;

		for (i=0; i < array_length(entry->email_array); i++)
		{
			if (!mystrnicmp(entry->email_array[i],address,al))
				return entry->email_array[i] + al;
		}
		entry = addressbook_next_entry(entry);
	}
	return NULL;
}

