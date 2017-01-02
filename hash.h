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

/** An entry within the table */
struct hash_entry
{
	const char *string;
	unsigned int data;
};

struct hash_table
{
	int bits;
	unsigned int mask; /* The bit mask for accessing the elements */
	unsigned int size; /* Size of the hash table */
	unsigned int data;
	unsigned int num_entries; /* Total number of entries managed by this table */
	unsigned int num_occupied_buckets; /* Total number of occupied primary buckets */
	const char *filename;

	struct hash_bucket *table; /* contains the actual entries, but is opaque */
};

/**
 * Obtain the hash value of a given string.
 *
 * @param str the string from which to obtain the hash value.
 */
unsigned long sdbm(const unsigned char *str);

/**
 * Initialize the given hash table with space for 2^bits entries.
 *
 * @param ht the hash table to initialize.
 * @param bits the number of bits used to identify a bucket.
 * @param filename defines the name of the file that is associated to this hash.
 *  If the file exists, the hash table is initialized with the contents of the
 *  file.
 * @return 1 on success, 0 otherwise.
 */
int hash_table_init(struct hash_table *ht, int bits, const char *filename);

/**
 * Gives back all resources occupied by the given hash table (excluding the
 * memory directly pointed to ht). The hash table can be no longer used after
 * this call returned.
 *
 * @param ht the hash table to clean
 */
void hash_table_clean(struct hash_table *ht);

/**
 * Stores the hash table on the filesystem. This works only, if filename was
 * given at hash_table_init().
 *
 * @param ht the hash table to store.
 */
void hash_table_store(struct hash_table *ht);

/**
 * Removes all entries from the hash table. The hash table can be used again
 * after this call, it is empty.
 *
 * @param ht the hash table to clear.
 */
void hash_table_clear(struct hash_table *ht);

/**
 * Insert a new entry into the hash table. Ownership of the string is given
 * to the hashtable and will be freed via free() when no longer needed.
 *
 * @param ht
 * @param string
 * @param data
 * @return the hash entry.
 */
struct hash_entry *hash_table_insert(struct hash_table *ht, const char *string, unsigned int data);

/**
 * Lookup an entry in the hash table.
 *
 * @param ht the hash table in which to search.
 * @param string the string to lookup
 * @return the entry or NULL.
 */
struct hash_entry *hash_table_lookup(struct hash_table *ht, const char *string);

/**
 * For each entry, call the given function.
 *
 * @param ht the hash table to interate.
 * @param func the function that shall be called.
 * @param data the additional user data that is passed to the function.
 */
void hash_table_call_for_each_entry(struct hash_table *ht, void (*func)(struct hash_entry *entry, void *data), void *data);

#endif
