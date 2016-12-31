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

/**
 * @file string_pools.c
 *
 * A pool of strings resembles a set of strings with a reference count.
 */

#include "string_pools.h"

#include <stdlib.h>

#include "support_indep.h"
#include "hash.h"

/*****************************************************************************/

static const int string_pool_version = 0;

/*****************************************************************************/

struct ref_string
{
	char *str;
	unsigned int count;
};

struct string_pool
{
	/** Number of total references */
	int total_ref_count;

	struct ref_string *ref_strings;
	unsigned int ref_strings_num;
	unsigned int ref_strings_allocated;

	unsigned int id_next;

	struct hash_table ht;
};

/*****************************************************************************/

struct string_pool *string_pool_create(void)
{
	struct string_pool *p = malloc(sizeof(*p));
	if (!p)
	{
		return NULL;
	}
	memset(p, 0, sizeof(*p));
	if (!(hash_table_init(&p->ht, 12, NULL)))
	{
		free(p);
		return NULL;
	}
	return p;
}

/*****************************************************************************/

static int string_pool_ensure_space(struct string_pool *p, int wanted_size)
{
	unsigned int new_ref_strings_allocated;
	struct ref_string *new_ref_strings;

	if (p->ref_strings_allocated <= wanted_size && p->ref_strings)
		return 1;

	new_ref_strings_allocated = (wanted_size+1)*2;

	if (!(new_ref_strings = realloc(p->ref_strings, new_ref_strings_allocated * sizeof(*new_ref_strings))))
		return 0;

	p->ref_strings_allocated = new_ref_strings_allocated;
	p->ref_strings = new_ref_strings;

	return 1;
}

/*****************************************************************************/

int string_pool_load(struct string_pool *sp, char *filename)
{
	FILE *fh;
	char buf[128];
	int rc = 0;
	int num_of_strings;
	int ver;
	int i;

	if (!(fh = fopen(filename, "rb")))
		return 0;

	fread(buf, 1, 4, fh);
	if (strncmp("SMSP",buf,4))
		goto bailout;
	if (fread(&ver, 1, 4, fh) != 4)
		goto bailout;
	if (ver != string_pool_version)
		goto bailout;
	if (fread(&num_of_strings, 1, 4, fh) != 4)
		goto bailout;

	for (i=0; i < num_of_strings; i++)
	{
		unsigned int c;
		int l;
		char *s;
		int pos;

		if (fread(&c, 1, 4, fh) != 4)
			goto bailout;
		if (fread(&l, 1, 4, fh) != 4)
			goto bailout;
		if (!(s = malloc(l+1)))
			goto bailout;
		if (fread(s, 1, l, fh) != l)
			goto bailout;
		s[l] = 0;
		fseek(fh, 3 - l % 4, SEEK_CUR);

		pos = sp->total_ref_count;

		if (!string_pool_ensure_space(sp, pos + 1))
			goto bailout;
		hash_table_insert(&sp->ht, s, i);
		sp->ref_strings[pos].count = c;
		sp->ref_strings[pos].str = s;
		sp->total_ref_count = pos + 1;
	}
	rc = 1;
bailout:
	fclose(fh);
	return rc;
}

/*****************************************************************************/

int string_pool_save(struct string_pool *sp, char *filename)
{
	FILE *fh;
	int i;

	if (!(fh = fopen(filename, "wb")))
		return 0;

	fwrite("SMSP",1,4,fh);
	fwrite(&string_pool_version,1,4,fh);
	fwrite(&sp->ref_strings_num,1,4,fh);

	for (i=0; i < sp->ref_strings_num; i++)
	{
		unsigned int c = sp->ref_strings[i].count;
		char *str = sp->ref_strings[i].str;
		const char pad[] =  {0,0,0};

		int l = strlen(str);
		fwrite(&c, 1, sizeof(c), fh);
		fwrite(&l, 1, sizeof(l), fh);
		fwrite(str, 1, l, fh);
		fwrite(pad, 1, 3 - l % 4, fh);
	}

	fclose(fh);
	return 1;
}

/*****************************************************************************/

void string_pool_delete(struct string_pool *p)
{
	/* Will also free the strings */
	hash_table_clean(&p->ht);
	free(p->ref_strings);
	free(p);
}

/*****************************************************************************/

int string_pool_ref(struct string_pool *p, char *string)
{
	struct hash_entry *he;

	if (!(he = hash_table_lookup(&p->ht, string)))
	{
		char *dupped_string;

		if (!(dupped_string = mystrdup(string)))
		{
			return -1;
		}

		if (!(he = hash_table_insert(&p->ht, dupped_string, 0)))
		{
			free(dupped_string);
			return -1;
		}

		he->data = p->id_next;

		/* Reserve more space if needed */
		if (p->id_next == p->ref_strings_allocated)
		{
			unsigned int new_ref_strings_allocated = (p->ref_strings_allocated+2)*2;
			struct ref_string *new_ref_strings = realloc(p->ref_strings, new_ref_strings_allocated * sizeof(*new_ref_strings));
			if (!new_ref_strings)
			{
				/* FIXME: Remove entry again */
				return -1;
			}
			p->ref_strings_allocated = new_ref_strings_allocated;
			p->ref_strings = new_ref_strings;
		}
		p->ref_strings[p->id_next].count = 0;
		p->ref_strings[p->id_next].str = dupped_string;
		p->id_next++;
		p->ref_strings_num++;
	}
	p->ref_strings[he->data].count++;
	return he->data;
}

/*****************************************************************************/

void string_pool_deref_by_str(struct string_pool *p, char *string)
{
	struct hash_entry *he = hash_table_lookup(&p->ht, string);
	if (!he)
	{
		return;
	}
	p->ref_strings[he->data].count--;
}

/*****************************************************************************/

void string_pool_deref_by_id(struct string_pool *p, int id)
{
	p->ref_strings[id].count--;
}

/*****************************************************************************/

char *string_pool_get(struct string_pool *p, int id)
{
	return p->ref_strings[id].str;
}

/*****************************************************************************/

int string_pool_get_id(struct string_pool *p, const char *string)
{
	struct hash_entry *he = hash_table_lookup(&p->ht, string);
	if (!he)
	{
		return -1;
	}
	return he->data;
}
