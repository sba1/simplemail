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
#ifndef SM__HASH_H
#define SM__HASH_H

struct hash_entry
{
	const char *string;
	unsigned int data;
};

struct hash_bucket
{
	struct hash_bucket *next;
	struct hash_entry entry;
};

struct hash_table
{
	int bits;
	unsigned int mask; /* The bit mask for accessing the elements */
	unsigned int size; /* Size of the hash table */
	unsigned int data;
	const char *filename;

	struct hash_bucket *table;
};

unsigned long sdbm(const unsigned char *str);

int hash_table_init(struct hash_table *ht, int bits, const char *filename);
void hash_table_clean(struct hash_table *ht);

void hash_table_store(struct hash_table *ht);
void hash_table_clear(struct hash_table *ht);

struct hash_entry *hash_table_insert(struct hash_table *ht, const char *string, unsigned int data);
struct hash_entry *hash_table_lookup(struct hash_table *ht, const char *string);

void hash_table_call_for_every_entry(struct hash_table *ht, void (*func)(struct hash_entry *entry, void *data), void *data);

#endif
