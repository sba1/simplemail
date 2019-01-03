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
 * @file
 *
 * A implementation of hash tables for strings and a single associated
 * data field.
 */

#include "hash.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "support_indep.h"

/*****************************************************************************/

#define MAX(a,b) ((a)>(b)?(a):(b))

/*****************************************************************************/

struct hash_bucket
{
	struct hash_bucket *next;
	struct hash_entry *entry;
};

/*****************************************************************************/

unsigned long sdbm(const unsigned char *str)
{
	unsigned long hash = 0;
	int c;

  while ((c = *str++))
		hash = c + (hash << 6) + (hash << 16) - hash;

	return hash;
}

/*****************************************************************************/

static struct hash_entry *hash_table_new_entry(struct hash_table *ht)
{
	return (struct hash_entry*)malloc(ht->entry_size);
}

/*****************************************************************************/

static void hash_table_free_entry(struct hash_table *ht, struct hash_entry *entry)
{
	if (!entry) return;
	free((char*)entry->string);
	free(entry);
}

/*****************************************************************************/

/**
 * Sets the number of bits used to identify a primary bucket.
 * Enlarging an existing hash table is supported.
 *
 * @param ht
 * @param bits
 * @return 1 on success, 0 on failure.
 */
static int hash_table_set_bits(struct hash_table *ht, int bits)
{
	unsigned int size;
	unsigned int mem_size;
	unsigned int mask;
	struct hash_bucket *table;

	if (!bits)
	{
		return 0;
	}

	if (bits < 4)
	{
		bits = 4;
	}

	if (ht->table != NULL && bits <= ht->bits)
	{
		/* We support only the enlargement of the table for now */
		return 1;
	}

	if (bits > 30)
	{
		return 0;
	}

	size = 1 << bits;
	mem_size = size*sizeof(struct hash_bucket);
	mask = size - 1;
	if (!(table = (struct hash_bucket*)malloc(mem_size)))
		return 0;
	memset(table,0,mem_size);

	if (ht->table != NULL)
	{
		unsigned int num_occupied_buckets = 0;

		unsigned int i;

		/* If the table is already populated, we need to adjust existing elements again */
		for (i=0;i<ht->size;i++)
		{
			struct hash_bucket *hb = &ht->table[i];
			if (hb->entry)
			{
				int secondary_bucket = 0;

				while (hb)
				{
					/* Data for old hash table size */
					struct hash_bucket *nhb;
					struct hash_entry *e;

					/* Data for the new hash table size */
					struct hash_bucket *new_hb;
					struct hash_entry *new_e;
					int new_index;

					e =  hb->entry;
					nhb = hb->next;

					new_index = sdbm((unsigned char*)hb->entry->string) & mask;
					new_hb = &table[new_index];
					new_e = new_hb->entry;

					if (new_e)
					{
						/* There is already something at this index. Note that
						 * the entry must have originally been in a secondary
						 * bucket as we can only enlarge the hash table in
						 * powers of two.
						 */
						*hb = *new_hb;
						new_hb->next = hb;
					} else
					{
						if (secondary_bucket)
						{
							/* Entry was contained in a secondary bucket but is
							 * now a primary one. So we have to free the
							 * secondary bucket now.
							 */
							free(hb);
						}
						num_occupied_buckets++;
					}
					new_hb->entry = e;

					hb = nhb;
					secondary_bucket = 1;
				}
			}
		}
		ht->num_occupied_buckets = num_occupied_buckets;
		free(ht->table);
	}

	ht->bits = bits;
	ht->mask = mask;
	ht->size = size;
	ht->table = table;

	return 1;
}

/*****************************************************************************/

int hash_table_init_with_size(struct hash_table *ht, int bits, unsigned int entry_size, const char *filename)
{
	FILE *fh;

	memset(ht, 0, sizeof(*ht));

	ht->entry_size = MAX(sizeof(struct hash_entry), entry_size);

	if (!hash_table_set_bits(ht, bits))
		return 0;

	ht->filename = filename;

	if (filename)
	{
		if ((fh = fopen(filename,"r")))
		{
			char buf[512];
			fgets(buf,sizeof(buf),fh);
			if (!strncmp(buf,"SMHASH1",7))
			{
				fgets(buf,sizeof(buf),fh);
				ht->data = atoi(buf);

				while (fgets(buf,sizeof(buf),fh))
				{
					int len = strlen(buf);
					char *lc;
					unsigned int data;

					if (len && buf[len-1] == '\n') buf[len-1] = 0;
					if ((len>1) && buf[len-2] == '\r') buf[len-2]=0;

					data = strtoul(buf,&lc,10);
					if (lc)
					{
						if (isspace((unsigned char)*lc))
						{
							char *string;
							lc++;
							if ((string = mystrdup(lc)))
								hash_table_insert(ht, string, data);
						}
					}
				}
			}
			fclose(fh);
		}
	}

	return 1;
}

/*****************************************************************************/

int hash_table_init(struct hash_table *ht, int bits, const char *filename)
{
	return hash_table_init_with_size(ht, bits, sizeof(struct hash_entry), filename);
}

/*****************************************************************************/

static void hash_table_deinit(struct hash_table *ht)
{
	unsigned int i;

	for (i=0;i<ht->size;i++)
	{
		struct hash_bucket *hb = &ht->table[i];

		if (!hb->entry)
		{
			continue;
		}

		hash_table_free_entry(ht, hb->entry);
		hb->entry = NULL;

		hb = hb->next;
		while (hb)
		{
			struct hash_bucket *thb = hb->next;
			hash_table_free_entry(ht, hb->entry);
			free(hb);
			hb = thb;
		}
	}
	ht->num_entries = 0;
	ht->num_occupied_buckets = 0;
}

/*****************************************************************************/

void hash_table_clear(struct hash_table *ht)
{
	unsigned int size, mem_size;

	if (ht->table)
	{
		hash_table_deinit(ht);
		ht->data = 0;

		size = ht->size;
		mem_size = size*sizeof(struct hash_bucket);
		memset(ht->table,0,mem_size);
	}
}

/*****************************************************************************/

void hash_table_clean(struct hash_table *ht)
{
	if (ht->table)
	{
		hash_table_deinit(ht);

		free(ht->table);
		ht->table = NULL;
		ht->data = 0;
	}
}

/*****************************************************************************/

struct hash_entry *hash_table_insert(struct hash_table *ht, const char *string, unsigned int data)
{
	unsigned int index;
	int secondary_bucket;
	struct hash_bucket *hb,*nhb;

	if (!string) return NULL;

	if (ht->num_entries >= ht->size / 4 * 3)
	{
		/* Load factor larger than about 0.75, re-adjust */
		if (!hash_table_set_bits(ht, ht->bits + 1))
			return NULL;
	}

	index = sdbm((const unsigned char*)string) & ht->mask;
	hb = &ht->table[index];

	if ((secondary_bucket = !!hb->entry))
	{
		if (!(nhb = (struct hash_bucket*)malloc(sizeof(struct hash_bucket))))
		{
			return NULL;
		}

		*nhb = *hb;
		hb->next = nhb;
	}

	if (!(hb->entry = hash_table_new_entry(ht)))
	{
		return NULL;
	}

	hb->entry->string = string;
	hb->entry->data = data;

	/* Update counts */
	ht->num_entries++;
	ht->num_occupied_buckets += !secondary_bucket;
	return hb->entry;
}

/*****************************************************************************/

struct hash_entry *hash_table_lookup(struct hash_table *ht, const char *string)
{
	unsigned int index;
	struct hash_bucket *hb;

	if (!string) return NULL;
	index = sdbm((const unsigned char*)string) & ht->mask;
	hb = &ht->table[index];

	if (!hb->entry) return NULL;

	while (hb)
	{
		if (!strcmp(hb->entry->string,string)) return hb->entry;
		hb = hb->next;
	}

	return NULL;
}

/**
 * Callback which is called for every entry as a result of hash_table_store().
 *
 * @param entry points to the entry that shall be saved.
 * @param data is assumed to be of type FILE * here.
 */
static void hash_table_store_callback(struct hash_entry *entry, void *data)
{
	fprintf((FILE*)data,"%d %s\n",entry->data,entry->string);
}

/*****************************************************************************/

void hash_table_store(struct hash_table *ht)
{
	FILE *fh;

	if (!ht->filename) return;

	if ((fh = fopen(ht->filename,"w")))
	{
		fputs("SMHASH1\n",fh);
		fprintf(fh,"%d\n",ht->data);
		hash_table_call_for_each_entry(ht,hash_table_store_callback, fh);
		fclose(fh);
	}
}

/*****************************************************************************/

void hash_table_call_for_each_entry(struct hash_table *ht, void (*func)(struct hash_entry *entry, void *data), void *data)
{
	unsigned int i;
	if (!func) return;

	for (i=0;i<ht->size;i++)
	{
		struct hash_bucket *hb = &ht->table[i];
		while (hb)
		{
			if (hb->entry)
				func(hb->entry,data);
			hb = hb->next;
		}
	}
}
