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

#include "expatinc.h"

#include "addressbook.h"
#include "codesets.h"
#include "configuration.h"
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
		return grp;
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
	if (entry)
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

static int addressbook_tag;
static int contact_tag;
static int group_tag;
static int newgroup_tag;
static int private_tag;
static int work_tag;
static char *data_buf;

static struct addressbook_entry_new current_entry;
static struct addressbook_group current_group;

/**************************************************************************
 Start Tag
**************************************************************************/
SAVEDS void xml_start_tag(void *data, const char *el, const char **attr)
{
/*	struct addressbook_entry *entry;*/
/*	XML_Parser p = (XML_Parser)data;*/

/*	if (!(entry = (struct addressbook_entry*)XML_GetUserData(p))) return;*/

	if (!mystricmp("addressbook",el)) addressbook_tag = 1;
	else if (!mystricmp("contact",el))
	{
		if (!contact_tag)
		{
			memset(&current_entry,0,sizeof(current_entry));
			contact_tag = 1;
		}
	}
	else if (!mystricmp("newgroup",el))
	{
		if (!newgroup_tag)
		{
			memset(&current_group,0,sizeof(current_group));
			newgroup_tag = 1;
		}
	} else if(!mystricmp("group",el))
	{
		if (!contact_tag)
		{
/*			entry = addressbook_new_group(entry);*/
			group_tag++;
/*			XML_SetUserData(p,entry);*/
		}
	}
	else if (!mystricmp("private",el))
	{
		if (!private_tag)
			private_tag = 1;
	}
	else if (!mystricmp("work",el))
	{
		if (!work_tag)
			work_tag = 1;
	}
}  /* End of start handler */

/**************************************************************************
 End Tag
**************************************************************************/
SAVEDS void xml_end_tag(void *data, const char *el)
{
/*	struct addressbook_entry *entry;*/
	struct address_snail_phone *asp = NULL;
/*	XML_Parser p = (XML_Parser)data;*/

/*	if (!(entry = (struct addressbook_entry*)XML_GetUserData(data))) return;*/

	if (private_tag) asp = &current_entry.priv;
	else if (work_tag) asp = &current_entry.work;

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

	if (!mystricmp("addressbook",el)) addressbook_tag = 0;
	else if (!mystricmp("contact",el))
	{
		if (contact_tag)
		{
			struct addressbook_entry_new *entry;

			contact_tag = 0;

			if ((entry = (struct addressbook_entry_new*)malloc(sizeof(struct addressbook_entry_new))))
			{
				*entry = current_entry;
				memset(&current_entry,0,sizeof(current_entry));
				list_insert_tail(&address_list,&entry->node);
			}
		}
	} else if (!mystricmp("newgroup",el))
	{
		if (newgroup_tag)
		{
			struct addressbook_group *group;

			newgroup_tag = 0;

			if ((group = (struct addressbook_group*)malloc(sizeof(struct addressbook_group))))
			{
				*group = current_group;
				memset(&current_group,0,sizeof(current_group));
				list_insert_tail(&group_list,&group->node);
			}
		}
	} else if (!mystricmp("group",el))
	{
		if (group_tag)
		{
/*			entry = addressbook_get_group(entry);
			XML_SetUserData(p,entry);*/
			group_tag--;
		}
	}
	else if (!mystricmp("private",el)) private_tag = 0;
	else if (!mystricmp("work",el)) work_tag = 0;
	else if (!mystricmp("alias",el))
	{
		if (!contact_tag)
		{
			if (group_tag)
			{
				if (!addressbook_find_group_by_name(data_buf))
					addressbook_add_group(data_buf);
			}
		} else
		{
			current_entry.alias = mystrdup(data_buf);
		}
	}
	else if (!mystricmp("name",el))
	{
		if (newgroup_tag) current_group.name = mystrdup(data_buf);
		else if (contact_tag) current_entry.realname = mystrdup(data_buf);
	}
	else if (!mystricmp("description",el))
	{
		if (newgroup_tag) current_group.description = mystrdup(data_buf);
		else if (contact_tag) current_entry.description = mystrdup(data_buf);
	}
	else if (!mystricmp("email",el)) current_entry.email_array = array_add_string(current_entry.email_array,data_buf);
	else if (!mystricmp("pgpid",el)) current_entry.pgpid = mystrdup(data_buf);
	else if (!mystricmp("homepage",el)) current_entry.homepage = mystrdup(data_buf);
	else if (!mystricmp("portrait",el)) current_entry.portrait = mystrdup(data_buf);
	else if (!mystricmp("note",el)) current_entry.notepad = mystrdup(data_buf);
	else if (!mystricmp("sex",el))
	{
		if (!mystricmp(data_buf,"female")) current_entry.sex = 1;
		else if (!mystricmp(data_buf,"male")) current_entry.sex = 2;
	}
	else if (!mystricmp("birthday",el))
	{
		char *buf = data_buf;
		int i;

		i = atoi(buf);
		if (i >= 1 && i <= 12)
		{
			current_entry.dob_month = i;
			if ((buf = strchr(buf,'/'))) buf++;
			if (buf) current_entry.dob_day = atoi(buf);
			if ((buf = strchr(buf,'/'))) buf++;
			if (buf) current_entry.dob_year = atoi(buf);
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

	if (data_buf)
	{
		free(data_buf);
		data_buf = NULL;
	}
}

/**************************************************************************
 Converts a single UFT-8 Chracter to ISO-Latin 1 one, very very
 incomplete.
**************************************************************************/
static char *uft8toiso(char *chr, char *code)
{
	unsigned int unicode;
	char *ret = uft8toucs(chr,&unicode);
	*code = unicode;
	return ret;
}

/**************************************************************************
 Read the characters
**************************************************************************/
SAVEDS void xml_char_data(void *data, const XML_Char *s, int len)
{
	if (contact_tag || group_tag || newgroup_tag)
	{
		int old_len = 0;
		if (data_buf)
			old_len = strlen(data_buf);

		if ((data_buf = (char*)realloc(data_buf,old_len+len+1)))
		{
			unsigned char *src = (char*)s;
			unsigned char *dest = (char*)data_buf + old_len;

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
	list_init(&group_list);
	list_init(&address_list);

	if (!addressbook_load())
	{
		struct addressbook_entry_new *entry;

		if ((entry = addressbook_add_entry("Hynek Schlawack")))
		{
			entry->email_array = array_add_string(entry->email_array,"hynek@rz.uni-potsdam.de");
			entry->email_array = array_add_string(entry->email_array,"hynek@hys.in-berlin.de");
			entry->description = mystrdup(_("Original author of SimpleMail"));
		}

		if ((entry = addressbook_add_entry("Sebastian Bauer")))
		{
			entry->email_array = array_add_string(entry->email_array,"mail@sebastianbauer.info");
			entry->description = mystrdup(_("Original author of SimpleMail"));
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

	if (!(buf = malloc(512))) return;
	if (!(p = XML_ParserCreate(NULL)))
	{
		free(buf);
		return;
	}

	XML_SetElementHandler(p, xml_start_tag, xml_end_tag);
	XML_SetCharacterDataHandler(p, xml_char_data);
/*	XML_SetUserData(p,group);*/
	XML_UseParserAsHandlerArg(p);

	for (;;)
	{
		int len;

		len = fread(buf, 1, 512, fh);
		if (len <= 0) break;

    if (!XML_Parse(p, buf, len, 0)) break;
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
			struct addressbook_entry_new *newperson;

			if ((newperson = (struct addressbook_entry_new*)malloc(sizeof(*newperson))))
			{
				memset(newperson,0,sizeof(*newperson));

				newperson->alias = utf8create(striplr(line) + 6, charset);

				if (!fgets(line,sizeof(line),fp)) return 0;
				newperson->email_array = array_add_string(newperson->email_array, utf8create(striplr(line), charset));

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

				newperson->group_array = array_add_string(newperson->group_array,"YAM Imports");
			}
		} else if(strncmp(line, "@GROUP", 6) == 0)
		{
/*			struct addressbook_entry *newgroup = addressbook_new_group(group);

			newgroup->alias = striplr(utf8create(line + 7, charset));

			if (!fgets(line,sizeof(line),fp)) return 0;
			newgroup->description = striplr(utf8create(line, charset));*/

			/* call recursivly, loading groups is not supported as the addressbooks differ to much */
			rc = yam_import_entries(fp);
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
		rc = yam_import_entries(fp);
		fclose(fp);
	}

	return rc;
}

/**************************************************************************
 Load the addressbook. Returns 0 for an error
**************************************************************************/
int addressbook_load(void)
{
	return addressbook_import_sm("PROGDIR:.addressbook.xml");
}

#define BOOK_UNKNOWN 0
#define BOOK_YAM 1
#define BOOK_SM  2

/**************************************************************************
 Returns the type of an addressbook file
**************************************************************************/
static int addressbook_get_type(char *filename)
{
	int rc = BOOK_UNKNOWN;
	FILE *fp;
	char *buf;

	fp = fopen(filename, "r");
	if(fp != NULL)
	{
		buf = malloc(23);
		if(buf != NULL)
		{
			if(fread(buf, 23, 1, fp) == 1)
			{
				if(strncmp(buf, "YAB4 - YAM Addressbook",22) == 0)
				{
					rc = BOOK_YAM;
				} else if(strncmp(buf, "<addressbook>",13) == 0)
				{
					rc = BOOK_SM;
				}
			}
			free(buf);
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

	if (!filename) filename = "PROGDIR:.addressbook.xml";

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

	fputs("<contact>\n",fh);
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

	if (entry->dob_year)
	{
		char buf[128];
		sprintf(buf,"%d/%d/%d",entry->dob_month, entry->dob_day, entry->dob_year);
		put_xml_element_string(fh,"birthday",buf);
	}
	addressbook_save_snail_phone("private",&entry->priv,fh);
	addressbook_save_snail_phone("work",&entry->work,fh);
	fputs("</contact>\n",fh);
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
	FILE *fh = fopen("PROGDIR:.addressbook.xml","w");
	if (fh)
	{
		struct addressbook_entry_new *entry;
		struct addressbook_group *group;

		fputs("<addressbook>\n",fh);

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

		fputs("</addressbook>\n",fh);
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
 Completes an alias/realname/e-mail address of the addressbook
**************************************************************************/
char *addressbook_complete_address(char *address)
{
	int al = strlen(address);
	struct addressbook_entry_new *entry;

	/* alias */
	entry = addressbook_first_entry();
	while (entry)
	{
		if (!utf8stricmp_len(entry->alias,address,al))
			return entry->alias + al;
		entry = addressbook_next_entry(entry);
	}

	/* realname */
	entry = addressbook_first_entry();
	while (entry)
	{
		if (!utf8stricmp_len(entry->realname,address,al))
			return entry->realname + al;
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

