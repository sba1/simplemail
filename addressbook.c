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

#include <expat.h>

#include "addressbook.h"
#include "addressbookwnd.h"
#include "http.h"
#include "lists.h"
#include "mail.h"
#include "parse.h"
#include "support.h"
#include "support_indep.h"
#include "smintl.h"

static struct addressbook_entry root_entry;

/**************************************************************************
 Returns the group where the person or group belongs to
**************************************************************************/
static struct addressbook_entry *addressbook_get_group(struct addressbook_entry *entry)
{
	struct list *list;
	struct addressbook_entry *fake = NULL;
	int size;

	if (&root_entry == entry) return NULL;
	if (!(list = node_list(&entry->node))) return NULL;

	size = ((char*)(&fake->u.group.list)) - (char*)fake;
	entry = (struct addressbook_entry*)(((char*)list)-size);

	return entry;
}

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
	while ((c = *src++))
	{
		if (((unsigned char)c)>=128) fprintf(fh,"&#%d;",(unsigned char)c);
		else fputc(c,fh);
	}
	fprintf(fh,"</%s>",element);
}

static int addressbook_tag;
static int contact_tag;
static int group_tag;
static int private_tag;
static int work_tag;
static char *data_buf;

/**************************************************************************
 Start Tag
**************************************************************************/
void xml_start_tag(void *data, const char *el, const char **attr)
{
	struct addressbook_entry *entry;
	XML_Parser p = (XML_Parser)data;

	if (!(entry = (struct addressbook_entry*)XML_GetUserData(p))) return;

	if (!mystricmp("addressbook",el)) addressbook_tag = 1;
	else if (!mystricmp("contact",el))
	{
		if (!contact_tag)
		{
			entry = addressbook_new_person(entry,NULL,NULL);
			contact_tag = 1;
			XML_SetUserData(p,entry);
		}
	} else if(!mystricmp("group",el))
	{
		if (!contact_tag)
		{
			entry = addressbook_new_group(entry);
			group_tag++;
			XML_SetUserData(p,entry);
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
void xml_end_tag(void *data, const char *el)
{
	struct addressbook_entry *entry;
	struct address_snail_phone *asp = NULL;
	XML_Parser p = (XML_Parser)data;

	if (!(entry = (struct addressbook_entry*)XML_GetUserData(data))) return;
	if (private_tag) asp = &entry->u.person.priv;
	else if (work_tag) asp = &entry->u.person.work;

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
			entry = addressbook_get_group(entry);
			XML_SetUserData(p,entry);
			contact_tag = 0;
		}
	} else if (!mystricmp("group",el))
	{
		if (group_tag)
		{
			entry = addressbook_get_group(entry);
			XML_SetUserData(p,entry);
			group_tag--;
		}
	}
	else if (!mystricmp("private",el)) private_tag = 0;
	else if (!mystricmp("work",el)) work_tag = 0;

	else if (!mystricmp("alias",el)) addressbook_set_alias(entry,data_buf);
	else if (!mystricmp("name",el)) entry->u.person.realname = mystrdup(data_buf);
	else if (!mystricmp("email",el)) addressbook_person_add_email(entry,data_buf);
	else if (!mystricmp("description",el)) addressbook_set_description(entry,data_buf);
	else if (!mystricmp("pgpid",el)) entry->u.person.pgpid = mystrdup(data_buf);
	else if (!mystricmp("homepage",el)) entry->u.person.homepage = mystrdup(data_buf);
	else if (!mystricmp("portrait",el)) entry->u.person.portrait = mystrdup(data_buf);
	else if (!mystricmp("note",el)) entry->u.person.notepad = mystrdup(data_buf);
	else if (!mystricmp("sex",el))
	{
		if (!mystricmp(data_buf,"female")) entry->u.person.sex = 1;
		else if (!mystricmp(data_buf,"male")) entry->u.person.sex = 2;
	}
	else if (!mystricmp("birthday",el))
	{
		char *buf = data_buf;
		int i;

		i = atoi(buf);
		if (i >= 1 && i <= 12)
		{
			entry->u.person.dob_month = i;
			if ((buf = strchr(buf,'/'))) buf++;
			if (buf) entry->u.person.dob_day = atoi(buf);
			if ((buf = strchr(buf,'/'))) buf++;
			if (buf) entry->u.person.dob_year = atoi(buf);
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
 Converts a single UFT-8 Chracter to a Unicode character very very
 incomplete. Should return NULL if invalid (actualy not implemented)
**************************************************************************/
static char *uft8toucs(char *chr, unsigned int *code)
{
	unsigned char c = *chr++;
	unsigned int ucs = 0;
	int i,bytes;
	if (!(c & 0x80))
	{
		*code = c;
		return chr;
	} else
	{
		if (!(c & 0x20))
		{
			bytes = 2;
			ucs = c & 0x1f;
		}
		else if (!(c & 0x10))
		{
			bytes = 3;
			ucs = c & 0xf;
		}
		else if (!(c & 0x08))
		{
			bytes = 4;
			ucs = c & 0x7;
		}
		else if (!(c & 0x04))
		{
			bytes = 5;
			ucs = c & 0x3;
		}
		else /* if (!(c & 0x02)) */
		{
			bytes = 6;
			ucs = c & 0x1;
		}

		for (i=1;i<bytes;i++)
			ucs = (ucs << 6) | ((*chr++)&0x3f);
	}
	*code = ucs;
	return chr;
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
void xml_char_data(void *data, const XML_Char *s, int len)
{
	if (contact_tag || group_tag)
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
				char code;
				char *oldsrc = src;
				src = uft8toiso(src,&code);
				if (!code) break; /* null byte will be added below */
				*dest++ = code;
				len -= (char*)src - oldsrc;
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
	struct addressbook_entry *entry;
	root_entry.type = ADDRESSBOOK_ENTRY_GROUP;
	list_init(&root_entry.u.group.list);

	if (!addressbook_load())
	{
		entry = addressbook_new_person(NULL, "Hynek Schlawack", "hynek@rz.uni-potsdam.de");
		addressbook_set_description(entry, "Original author of SimpleMail");
		addressbook_person_add_email(entry, "hynek@hys.in-berlin.de");
		entry->u.person.sex = 2;

		entry = addressbook_new_person(NULL, "Sebastian Bauer", "sebauer@t-online.de");
		addressbook_set_description(entry, "Original author of SimpleMail");
		addressbook_person_add_email(entry, "Sebastian.Bauer@in.stud.tu-ilmenau.de");
		entry->u.person.sex = 2;
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
 Load the entries in the current group as XML
**************************************************************************/
static void addressbook_load_entries(struct addressbook_entry *group, FILE *fh)
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
	XML_SetUserData(p,group);
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

int addressbook_import_sm(char *filename)
{
	int retval = 0;
	FILE *fh;

	if ((fh = fopen(filename,"r")))
	{
		addressbook_load_entries(&root_entry,fh);
		fclose(fh);
		retval = 1;
	}
	return retval;
}

static char *getln(FILE *fp)
{
	char *rc;
	
	rc = malloc(1024);
	if(rc) fgets(rc, 1024, fp);
	rc[strlen(rc)-1]=0;

	return rc;
}

static int yam_import_entries(struct addressbook_entry *group, FILE *fp)
{
	int rc = 1;
	char *line;
	
	line = getln(fp);
	while ((!feof(fp)) && (strncmp(line, "@ENDGROUP",9) != 0))
	{
		if(strncmp(line, "@USER", 5) == 0)
		{
			struct addressbook_entry *newperson;
			char *alias, *name, *email;
			
			alias = mystrdup(line + 6);
			free(line);
			email = getln(fp);
			name = getln(fp);
			
			newperson = addressbook_new_person(group, name, email);
			newperson->alias = alias;
			
			newperson->description = getln(fp);
			newperson->u.person.priv.phone1 = getln(fp);
			newperson->u.person.priv.street = getln(fp);
			newperson->u.person.priv.zip    = getln(fp);
			newperson->u.person.priv.country= getln(fp);
			newperson->u.person.pgpid       = getln(fp);
			
			line = getln(fp);
			newperson->u.person.dob_day   = 10*(line[0]-'0') + (line[1]-'0');
			newperson->u.person.dob_month = 10*(line[2]-'0') + (line[3]-'0');
			newperson->u.person.dob_year  = 1000*(line[4]-'0') + 100*(line[5]-'0') + 10*(line[6]-'0') + (line[7]-'0');
			free(line);
			
			newperson->u.person.portrait    = getln(fp);
			newperson->u.person.homepage    = getln(fp);
			
			line = getln(fp); free(line);  /* Whether sign etc. */
			
			line = getln(fp); free(line);
			if(strncmp(line, "@ENDUSER", 8) != 0)
			{
				rc = 0;
				break;
			}
			
		} else if(strncmp(line, "@GROUP", 6) == 0)
		{
			struct addressbook_entry *newgroup = addressbook_new_group(group);
			
			newgroup->alias = mystrdup(line + 7);
			free(line);
			newgroup->description = getln(fp);
			
			rc = yam_import_entries(newgroup, fp);
		} else
		{
			sm_request(NULL, _("Corrupt YAM-Addressbook!"), _("_Okay"));
			
			rc = 0;
		}
		line = getln(fp);
	}

	return rc;
}

int addressbook_import_yam(char *filename)
{
	int rc = 0;
	FILE *fp;
	char *line;
	
	fp = fopen(filename, "r");
	if(fp != NULL)
	{
		line = getln(fp);
		free(line);
		rc = yam_import_entries(&root_entry, fp);

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

int addressbook_get_type(char *filename)
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
 Add entries from a specified file.
**************************************************************************/
int addressbook_import(void)
{
	int rc = 0;
	char *filename;
	
	filename = sm_request_file(_("Select an addressbook-file."), "PROGDIR:");
	if(filename && *filename)
	{
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
		
		addressbookwnd_refresh();
		
		free(filename);
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
			fputs("<contact>\n",fh);

			put_xml_element_string(fh,"alias",entry->alias);
			put_xml_element_string(fh,"name",entry->u.person.realname);
			put_xml_element_string(fh,"description",entry->description);
			put_xml_element_string(fh,"pgpid",entry->u.person.pgpid);
			for (i=0;i<entry->u.person.num_emails;i++)
				put_xml_element_string(fh,"email",entry->u.person.emails[i]);
			put_xml_element_string(fh,"homepage",entry->u.person.homepage);
			put_xml_element_string(fh,"portrait",entry->u.person.portrait);
			put_xml_element_string(fh,"note",entry->u.person.notepad);
			if (entry->u.person.sex)
				put_xml_element_string(fh,"sex",entry->u.person.sex==1?"female":"male");
			if (entry->u.person.dob_year)
			{
				char buf[128];
				sprintf(buf,"%d/%d/%d",entry->u.person.dob_month,entry->u.person.dob_day,entry->u.person.dob_year);
				put_xml_element_string(fh,"birthday",buf);
			}
			addressbook_save_snail_phone("private",&entry->u.person.priv,fh);
			addressbook_save_snail_phone("work",&entry->u.person.work,fh);
			fputs("</contact>\n",fh);
		} else
		{
			if (entry->type == ADDRESSBOOK_ENTRY_GROUP)
			{
				fputs("<group>\n",fh);
				put_xml_element_string(fh,"alias",entry->alias);
				put_xml_element_string(fh,"description",entry->description);
				addressbook_save_group(entry,fh);
				fputs("</group>\n",fh);
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
	FILE *fh = fopen("PROGDIR:.addressbook.xml","w");
	if (fh)
	{
		fputs("<addressbook>\n",fh);
		addressbook_save_group(&root_entry,fh);
		fputs("</addressbook>\n",fh);
		fclose(fh);
	}
}

/**************************************************************************
 Sets the alias of an addressbook entry
**************************************************************************/
void addressbook_set_alias(struct addressbook_entry *entry, char *alias)
{
	free(entry->alias);
	entry->alias = mystrdup(alias);
}

/**************************************************************************
 Sets the descripton of an addressbook entry
**************************************************************************/
void addressbook_set_description(struct addressbook_entry *entry, char *desc)
{
	free(entry->description);
	entry->description = mystrdup(desc);
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
			if (mode == ADDRESSBOOK_FIND_ENTRY_ALIAS) found = !mystricmp(entry_contents,entry->alias);
			if (mode == ADDRESSBOOK_FIND_ENTRY_REALNAME) found = !mystricmp(entry_contents,entry->u.person.realname);
			if (mode == ADDRESSBOOK_FIND_ENTRY_EMAIL)
			{
				for (i=0; i<entry->u.person.num_emails && !found;i++)
					found = !mystricmp(entry_contents,entry->u.person.emails[i]);
			}
		} else
		{
			if (mode == ADDRESSBOOK_FIND_ENTRY_ALIAS) found = !mystrnicmp(entry_contents,entry->alias,cl);
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
		/* TODO: The following could be uniqued with the above since its now the same */
		if (mode == ADDRESSBOOK_FIND_ENTRY_ALIAS)
		{
			if (complete)
			{
				found = !mystricmp(entry_contents,entry->alias);
			} else
			{
				found = !mystrnicmp(entry_contents,entry->alias,cl);
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
		new_entry->alias = mystrdup(entry->alias);
		new_entry->description = mystrdup(entry->description);

		switch (entry->type)
		{
			case	ADDRESSBOOK_ENTRY_GROUP:
						list_init(&new_entry->u.group.list);
						break;

			case	ADDRESSBOOK_ENTRY_PERSON:
						{
							new_entry->u.person.realname = mystrdup(entry->u.person.realname);
							new_entry->u.person.pgpid = mystrdup(entry->u.person.pgpid);
							new_entry->u.person.homepage = mystrdup(entry->u.person.homepage);
							new_entry->u.person.notepad = mystrdup(entry->u.person.notepad);
							new_entry->u.person.portrait = mystrdup(entry->u.person.portrait);

							if ((new_entry->u.person.emails = (char**)malloc(entry->u.person.num_emails*sizeof(char*))))
							{
								int i;
								for (i=0;i<entry->u.person.num_emails;i++)
									new_entry->u.person.emails[i] = mystrdup(entry->u.person.emails[i]);
								new_entry->u.person.num_emails = entry->u.person.num_emails;
							}

							snailphonecpy(&new_entry->u.person.priv,&entry->u.person.priv);
							snailphonecpy(&new_entry->u.person.work,&entry->u.person.work);

							new_entry->u.person.dob_month = entry->u.person.dob_month;
							new_entry->u.person.dob_day = entry->u.person.dob_day;
							new_entry->u.person.dob_year = entry->u.person.dob_year;
							new_entry->u.person.sex = entry->u.person.sex;
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
 Free all memory of the entry (recursivly)
**************************************************************************/
void addressbook_free_entry(struct addressbook_entry *entry)
{
	free(entry->alias);
	free(entry->description);

	switch (entry->type)
	{
		case	ADDRESSBOOK_ENTRY_GROUP:
					{
						struct addressbook_entry *e = addressbook_first(entry);

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

						if (entry->u.person.realname) free(entry->u.person.realname);
						if (entry->u.person.pgpid) free(entry->u.person.pgpid);
						if (entry->u.person.homepage) free(entry->u.person.homepage);
						if (entry->u.person.notepad) free(entry->u.person.notepad);
						if (entry->u.person.portrait) free(entry->u.person.portrait);

						for (i=0;i<entry->u.person.num_emails;i++)
						{
							if (entry->u.person.emails[i]) free(entry->u.person.emails[i]);
						}
						if (entry->u.person.emails) free(entry->u.person.emails);

						freesnailphone(&entry->u.person.priv);
						freesnailphone(&entry->u.person.work);
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
		if (entry->alias) return mystrdup(entry->alias);
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
			if (entry->alias) return mystrdup(entry->alias);
			if (entry->u.person.realname) return mystrdup(entry->u.person.realname);
			if (entry->u.person.num_emails) return mystrdup(entry->u.person.emails[0]);
		}
	}
	return str;
}

/**************************************************************************
 Returns the expanded string of the given entry, but prefers the given
 e-mail address
**************************************************************************/
static char *addressbook_get_address_str_expanded_email(struct addressbook_entry *entry, char *email)
{
	char *str = NULL;
	if (entry->type == ADDRESSBOOK_ENTRY_PERSON && entry->u.person.num_emails)
	{
		if (!email) email = entry->u.person.emails[0];
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
			str = stradd(str,email);
			return stradd(str,">");
		} else
		{
			return mystrdup(email);
		}
	}
	return NULL;
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
		return addressbook_get_address_str_expanded_email(entry,NULL);
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
	char *unexpand_end = unexpand + strlen(unexpand);
	char *buf = unexpand;
	char *expand = NULL;

	while (buf < unexpand_end)
	{
		char *ret;
		struct mailbox mb;

		if ((ret = parse_mailbox(buf,&mb)))
		{
			if (mb.phrase)
			{
				/* note "'s must be setted in special cases */
				if (needs_quotation(mb.phrase))
				{
					expand = stradd(expand,"\"");
					expand = stradd(expand,mb.phrase);
					expand = stradd(expand,"\"");
				} else
				{
					expand = stradd(expand,mb.phrase);
				}
				expand = stradd(expand," <");
				expand = stradd(expand,mb.addr_spec);
				expand = stradd(expand,">");
			} else
			{
				/* first check if the address is in the address book */
				int email = 0;
				struct addressbook_entry *entry = addressbook_find_entry(NULL,mb.addr_spec,1,NULL,ADDRESSBOOK_FIND_ENTRY_ALIAS);
				if (!entry) entry = addressbook_find_entry(NULL,mb.addr_spec,1,NULL,ADDRESSBOOK_FIND_ENTRY_REALNAME);
				if (!entry)
				{
					entry = addressbook_find_entry(NULL,mb.addr_spec,1,NULL,ADDRESSBOOK_FIND_ENTRY_EMAIL);
					email = 1;
				}

				if (entry)
				{
					char *new_str = email?addressbook_get_address_str_expanded_email(entry,mb.addr_spec):addressbook_get_address_str_expanded(entry);
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
					expand = stradd(expand,mb.addr_spec);
				}
			}
			buf = ret;

			free(mb.phrase);
			free(mb.addr_spec);
		} else
		{
			char *tolook;
			while (isspace((unsigned char)*buf))
				buf++;
			ret = strchr(buf,',');
			if (!ret) ret = unexpand_end;

			if ((tolook = strndup(buf,ret - buf)))
			{
				char *new_str = NULL;
				int email = 0;
				struct addressbook_entry *entry = addressbook_find_entry(NULL,tolook,1,NULL,ADDRESSBOOK_FIND_ENTRY_ALIAS);
				if (!entry) entry = addressbook_find_entry(NULL,tolook,1,NULL,ADDRESSBOOK_FIND_ENTRY_REALNAME);
				if (!entry)
				{
					email = 1;
					entry = addressbook_find_entry(NULL,tolook,1,NULL,ADDRESSBOOK_FIND_ENTRY_EMAIL);
				}

				if (!entry)
				{
					if (expand) free(expand);
					return NULL;
				}

				if (email)
					new_str = addressbook_get_address_str_expanded_email(entry,tolook);
				if (!new_str) new_str = addressbook_get_address_str_expanded(entry);

				if (!new_str)
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
 Walks through the addressbook to look for an e-mail addy. Returns this
 entry or NULL if not found
**************************************************************************/
struct addressbook_entry *addressbook_find_entry_by_address(char *addr)
{
	return addressbook_find_entry(NULL, addr, 1, NULL, ADDRESSBOOK_FIND_ENTRY_EMAIL);

}

/**************************************************************************
 Completes an alias/realname/e-mail address of the addressbook
**************************************************************************/
struct addressbook_entry *addressbook_get_entry_from_mail_header(struct mail *m, char *header)
{
	struct addressbook_entry *e = NULL;
	char *from = mail_find_header_contents(m, header);
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
 Returns the rest of the completed string. NULL if this cannot be done.
 type_ptr will be filled with 0 if the alias has been completed, 1 for the
 realname and all greater than 1 the email
**************************************************************************/
char *addressbook_completed_by_entry(char *part, struct addressbook_entry *entry, int *type_ptr)
{
	int pl;
	int i;

	pl = mystrlen(part);

	if (entry->type != ADDRESSBOOK_ENTRY_PERSON) return NULL;

	if (!mystrnicmp(part,entry->alias,pl) && entry->alias)
	{
		if (type_ptr) *type_ptr = 0;
		return entry->alias + pl;
	}

	if (!mystrnicmp(part,entry->u.person.realname,pl))
	{
		if (type_ptr) *type_ptr = 1;
		return entry->u.person.realname + pl;
	}

	for (i=0;i<entry->u.person.num_emails;i++)
	{
		if (!mystrnicmp(part,entry->u.person.emails[i],pl))
		{
			if (type_ptr) *type_ptr = i + 2;
			return entry->u.person.emails[i] + pl;
		}
	}
	return NULL;
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
	if (entry)/* && hits == 1) */
		return entry->alias + al; /* alias should be removed out of the union */

	if (!entry) entry = addressbook_find_entry(NULL, address, 0, &hits, ADDRESSBOOK_FIND_ENTRY_REALNAME);
	if (entry)/* && hits == 1) */
		return entry->u.person.realname + al; /* alias should be removed out of the union */

	if (!entry) entry = addressbook_find_entry(NULL, address, 0, &hits, ADDRESSBOOK_FIND_ENTRY_EMAIL);
	if (entry)/* && hits == 1) */
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

