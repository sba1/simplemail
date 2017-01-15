/**
 * index_external.c - an external string index implementation for SimpleMail.
 * Copyright (C) 2013  Sebastian Bauer
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file index_external.c
 *
 * This is a naive implementation of an external memory data structure that
 * can be used for full text search. It is basically a b tree that stores all
 * suffixes of each document that is inserted.
 *
 * It is very slow at the moment, in particular as the strings are read multiple
 * times from the external media.
 */

#include "index_external.h"

#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "index_private.h"
#include "lists.h"

#include "support.h"

/* Get va_vopy() with pre-C99 compilers */
#ifdef __SASC
#define va_copy(dest,src) ((dest) = (src))
#elif defined(__GNUC__) && (__GNUC__ < 3)
#include <varargs.h>
#endif

/* Define this if some debug options should be compiled in */
/*#define DEBUG_FUNCTIONS*/

struct bnode_element
{
	int str_offset;
	int str_len;
	union
	{
		struct
		{
			int did;
		} leaf;

		/** Child containing values that are all greater or equal */
		struct
		{
			int gchild;
		} internal;
	};
};

struct bnode_header
{
	int leaf;
	int num_elements; /* Number of following bnode_elements */
	int lchild; /* Child containing keys/strings that are all (lexicographical) smaller */
	int sibling;
};

typedef struct bnode_header bnode;

struct index_external
{
	struct index index;
	struct list document_list;

	int block_size;
	int max_elements_per_node;
	int number_of_blocks;

	FILE *string_file;
	FILE *offset_file;
	FILE *index_file;

	/** Identifies the block for the root node. TODO: Make this persistent */
	int root_node;

	/**
	 * Defines the maximum length for which a substring search is accurate.
	 * Searches with longer substring will shorten the substring to this length.
	 */
	int max_substring_len;

	bnode *tmp;
	bnode *tmp2;
	bnode *tmp3;
};

/**
 * Create a new node for the given index/btree.
 *
 * @param idx
 * @return
 */
static bnode *bnode_create(struct index_external *idx)
{
	struct bnode_header *bnode;

	if (!(bnode = malloc(idx->block_size + sizeof(struct bnode_element))))
		return NULL;

	memset(bnode, 0, idx->block_size + sizeof(struct bnode_element));
	bnode->sibling = -1;
	return bnode;
}

/**
 * Free the memory associated to the given bnode.
 *
 * @param idx
 * @param bnode
 */
static void bnode_free(struct index_external *idx, bnode *bnode)
{
	free(bnode);
}

/**
 * Return the i'th element of the given bnode.
 *
 * @param idx
 * @param bnode_header
 * @param elem
 * @return
 */
static struct bnode_element *bnode_get_ith_element_of_node(struct index_external *idx, bnode *bnode, int i)
{
	return &(((struct bnode_element*)(bnode+1))[i]);
}

/**
 * Writes the given block to the given address.
 *
 * @param idx
 * @param node
 * @param address
 * @return
 */
static int bnode_write_block(struct index_external *idx, const bnode *node, int address)
{
	unsigned int offset = address * idx->block_size;
	if (fseek(idx->index_file, offset, SEEK_SET))
		return 0;
	if (fwrite(node, 1, idx->block_size, idx->index_file) != idx->block_size)
		return 0;
	return 1;
}

/**
 * Adds the given block to the index and return the block number.
 *
 * @param idx
 * @param node
 * @return -1 for an error.
 */
static int bnode_add_block(struct index_external *idx, const bnode *node)
{
	int new_address = idx->number_of_blocks++;
	if (!bnode_write_block(idx, node, new_address))
		return -1;
	return new_address;
}

/**
 * Reads the given block for the given address to the given node.
 *
 * @param idx
 * @param node
 * @param address
 * @return
 */
static int bnode_read_block(struct index_external *idx, bnode *node, int address)
{
	size_t rc;
	unsigned int offset = address * idx->block_size;
	if (fseek(idx->index_file, offset, SEEK_SET))
		return 0;
	rc = fread(node, 1, idx->block_size, idx->index_file);
	if (rc != idx->block_size)
		return 0;
	return 1;
}

/**
 * Reads the entire string for the given element,
 *
 * @param idx
 * @param element
 * @return the string which must be freed with free() when no longer in use.
 */
static char *bnode_read_string(struct index_external *idx, struct bnode_element *element)
{
	char *str;
	int str_len = element->str_len;
	if (str_len > idx->max_substring_len)
		str_len = idx->max_substring_len;

	fseek(idx->string_file, element->str_offset, SEEK_SET);
	if (!(str = malloc(str_len + 1)))
		return 0;
	if (fread(str, 1, str_len, idx->string_file) != str_len)
		return 0;
	str[str_len] = 0;
	return str;
}

/**
 * Compares contents represented by the given element with the given text. If str is the contents,
 * then the function places the return value of strcmp(str, text) into *out_cmp.
 *
 * @param idx
 * @param e
 * @param text
 * @param out_cmp
 * @return 0 on failure, 1 on success.
 */
static int bnode_compare_string(struct index_external *idx, struct bnode_element *e, const char *text, int *out_cmp)
{
	char *str = bnode_read_string(idx, e);
	if (!str) return 0;
	*out_cmp = strncmp(str, text, idx->max_substring_len);

	free(str);
	return 1;
}

#define BNODE_PATH_MAX_NODES 24

/**
 * Represents a path in the bnode tree.
 */
struct bnode_path
{
	int max_level;
	struct
	{
		int block;
		int key_index;
	} node[BNODE_PATH_MAX_NODES];
};



/**
 * Find the slot with the separation key for the given text. The slot with
 * the separation key for a given text is the slot whose value is not
 * lexicographically smaller than the text but whose left neighbor slot value
 * is lexicographically smaller than the text.
 *
 * @param idx
 * @param tmp
 * @param text
 * @param out_slot
 * @param out_directmatch
 * @return
 */
static int bnode_find_separation_slot(struct index_external *idx, bnode *tmp, const char *text, int *out_slot, int *out_directmatch)
{
	int l, h, hcmp;

	/* Variable l is inclusive, h is exclusive */
	l = 0;
	h = tmp->num_elements;
	hcmp = 1;

	while (l < h)
	{
		int cmp;
		int m = (l + h)/2;

		struct bnode_element *e = bnode_get_ith_element_of_node(idx, tmp, m);
		if (!bnode_compare_string(idx, e, text, &cmp))
			return 0;

		/* < results in getting the first element. We could also use <= to get the last element */
		if (cmp < 0)
		{
			l = m + 1;
		} else
		{
			h = m;
			hcmp = cmp;
		}
	}

	*out_slot = h;
	*out_directmatch = hcmp == 0;
	return 1;
}

/**
 * Lookup the given text.
 *
 * @param idx
 * @param text
 * @param out_block_array
 * @param out_index
 * @return
 */
static int bnode_lookup(struct index_external *idx, const char *text, struct bnode_path *path)
{
	int i;
	bnode *tmp = idx->tmp;
	int block = idx->root_node;
	int level = 0;

	do
	{
		int lchild;
		int direct_match = 0;

		bnode_read_block(idx, tmp, block);

		if (!bnode_find_separation_slot(idx, tmp, text, &i, &direct_match))
			return 0;

		path->max_level = level;
		path->node[level].block = block;
		path->node[level].key_index = i;

		if (tmp->leaf)
			break;

		if (i == 0) lchild = tmp->lchild;
		else lchild = bnode_get_ith_element_of_node(idx, tmp, i - 1)->internal.gchild;

		if (block == lchild && !tmp->leaf)
		{
#ifdef DEBUG_FUNCTIONS
			fprintf(stderr, "Endless loop detected!\n");
#endif
			return 0;
		}

		/* Otherwise, it was no direct match, and the separation key is lexicographically strictly
		 * greater hence the sub tree defined by the left child, which is lexicographically strictly
		 * smaller. will contain the key (or a better suited value if the key is not contained).
		 */
		block = lchild;
		level++;
		if (level == BNODE_PATH_MAX_NODES)
			return 0;
	} while (!tmp->leaf);

	return 1;
}

/**
 * Clear all elements of the given node begining at start.
 *
 * @param idx
 * @param n
 * @param start
 */
static void bnode_clear_elements(struct index_external *idx, bnode *n, int start)
{
	struct bnode_element *e = bnode_get_ith_element_of_node(idx, n, start);
	memset(e, 0, (idx->max_elements_per_node - start)*sizeof(struct bnode_element));
}

#ifdef DEBUG_FUNCTIONS

/**
 * Dump the children of the given nodes.
 */
static void dump_node_children(struct index_external *idx, bnode *node, const char *prefix)
{
	int i;
	printf("%s: ", prefix, node->lchild);
	for (i=0;i<node->num_elements;i++)
	{
		struct bnode_element *be;
		be = bnode_get_ith_element_of_node(idx, node, i);

		printf("%d ", node->leaf?be->leaf.did:be->internal.gchild);
	}
	printf("\n");
}

/**
 * Dump the entire index.
 *
 * @param idx
 * @param block
 * @param level
 */
static void dump_index(struct index_external *idx, int block, int level)
{
	int i;
	bnode *tmp = bnode_create(idx);
	if (!bnode_read_block(idx, tmp, block))
		return;

	printf("dump_index(block=%d, level=%d, leaf=%d, sibling=%d) elements=%d\n", block, level, tmp->leaf, tmp->sibling, tmp->num_elements);
	if (!tmp->leaf)
		dump_index(idx, tmp->lchild, level + 1);

	for (i=0; i<tmp->num_elements; i++)
	{
		char *str;
		struct bnode_element *e;
		char buf[16];

		e = bnode_get_ith_element_of_node(idx, tmp, i);
		if (!(str = bnode_read_string(idx, e)))
		{
			fprintf(stderr, "Couldn't read string!\n");
			exit(-1);
		}
		snprintf(buf,sizeof(buf),"%s", str);

		{
			int k;
			for (k=0;k<level;k++)
				printf(" ");
		}
		printf("l%d: b%d: i%d: o%04d ", level, block, i, e->str_offset);

		{
			int k;
			for (k=0;k<strlen(buf);k++)
			{
				if (!buf[k]) break;
				if (buf[k] == 10) buf[k] = ' ';
				printf("%c", buf[k]);
			}
			printf("\n");
		}
		if (!tmp->leaf)
			dump_index(idx, e->internal.gchild, level + 1);

		free(str);
	}

	bnode_free(idx, tmp);
}

/**
 * Verify whether the index is consitent, i.e., whether all strings are in increasing order.
 *
 * @param
 * @param block
 * @param level
 */
static void verify_index(struct index_external *idx, int block, int level)
{
	int i;
	bnode *tmp = bnode_create(idx);
	char *str1 = strdup("");
	char *str2;
	int offset1 = -1;

	if (!bnode_read_block(idx, tmp, block))
		return;

	if (!tmp->leaf)
		verify_index(idx, tmp->lchild, level + 1);

	for (i=0; i<tmp->num_elements; i++)
	{
		struct bnode_element *e;
		int rc;

		e = bnode_get_ith_element_of_node(idx, tmp, i);
		str2 = bnode_read_string(idx, e);
		if ((rc = strcmp(str1,str2)) > 0)
		{
			int *a = 0;
			printf("violation\n");
			printf("%d: %d: %d: %d-%d=%d |%s |%s (%d %d)\n", level, block, i, str1[0], str2[0], rc, str1, str2, offset1, e->str_offset);
			*a = 0;
			exit(1);
		}


		if (!tmp->leaf)
			verify_index(idx, e->internal.gchild, level + 1);

		free(str1);
		str1 = str2;
		offset1= e->str_offset;
	}

	free(str1);
	bnode_free(idx, tmp);
}


/**
 * Count the total number of strings to which the index refers.
 * This includes the strings of the internal nodes as well as the leaves.
 *
 * @param idx
 * @param block
 * @param level
 */
static int count_index(struct index_external *idx, int block, int level)
{
	int i, count = 0;
	bnode *tmp = bnode_create(idx);

	if (!bnode_read_block(idx, tmp, block))
		return 0;

	if (!tmp->leaf)
		count += count_index(idx, tmp->lchild, level + 1);

	for (i=0; i<tmp->num_elements; i++)
	{
		struct bnode_element *e;
		int rc;

		e = bnode_get_ith_element_of_node(idx, tmp, i);
		if (!tmp->leaf)
			count += count_index(idx, e->internal.gchild, level + 1);
		count++;
	}

	bnode_free(idx, tmp);
	return count;
}

/**
 * Count the number of strings, to which all the leaves refer.
 *
 * @param idx
 * @param block
 * @param level
 */
static int count_index_leaves(struct index_external *idx, int block, int level)
{
	int i, count = 0;
	bnode *tmp = bnode_create(idx);

	if (!bnode_read_block(idx, tmp, block))
		return -1;

	if (!tmp->leaf)
		count += count_index(idx, tmp->lchild, level + 1);

	for (i=0; i<tmp->num_elements; i++)
	{
		struct bnode_element *e;
		int rc;

		e = bnode_get_ith_element_of_node(idx, tmp, i);
		if (!tmp->leaf)
		{
			int c = count_index_leaves(idx, e->internal.gchild, level + 1);
			if (c == -1)
			{
				return -1;
			}
			count += c;
		} else
		{
			count++;
		}
	}

	bnode_free(idx, tmp);
	return count;
}

#endif

/**
 * Inserts the given string into the bnode tree.
 *
 * @param idx
 * @param did
 * @param txt
 * @return
 */
static int bnode_insert_string(struct index_external *idx, int did, int offset, const char *text)
{
	int i;
	int current_level;
	int block;
	bnode *tmp = idx->tmp;
	struct bnode_path path;
	struct bnode_element new_element;

	if (!bnode_lookup(idx, text, &path))
		return 0;

	current_level = path.max_level;
	new_element.str_offset = offset;
	new_element.str_len = strlen(text);
	new_element.leaf.did = did;

	while (current_level >= 0)
	{
		struct bnode_element *e;

		block = path.node[current_level].block;
		i = path.node[current_level].key_index;

		bnode_read_block(idx, tmp, block);

		if (!tmp->leaf && current_level == path.max_level)
		{
#ifdef DEBUG_FUNCTIONS
			fprintf(stderr, "Cannot insert into a non-leaf\n");
			exit(1);
#endif
		}

		/* Recall that we allocate in our temps one more entry than it would fit in the block, so we surly can
		 * insert the entry now */
		e = bnode_get_ith_element_of_node(idx, tmp, i);

		memmove(e + 1, e, (tmp->num_elements - i) * sizeof(struct bnode_element));

		/* New element */
		*e = new_element;
		tmp->num_elements++;

		if (tmp->num_elements == idx->max_elements_per_node)
		{
			int tmp3block;

			/* Now we split the node into two nodes. We keep the median in but also
			 * insert it as a separation value for the two nodes on the parent.
			 */

			int median = tmp->num_elements / 2;
			int start_of_2nd_node = median;
			struct bnode_element *me = bnode_get_ith_element_of_node(idx, tmp, median);
			struct bnode_element me_copy = *me;

			/* First node */
			tmp->num_elements = median;

			/* Second node */
			idx->tmp3->num_elements = idx->max_elements_per_node - start_of_2nd_node;
			idx->tmp3->lchild = me->internal.gchild;
			idx->tmp3->leaf = tmp->leaf;
			idx->tmp3->sibling = tmp->sibling;
			memcpy(bnode_get_ith_element_of_node(idx, idx->tmp3, 0), bnode_get_ith_element_of_node(idx, tmp, start_of_2nd_node), idx->tmp3->num_elements * sizeof(struct bnode_element));
			bnode_clear_elements(idx, idx->tmp3, idx->tmp3->num_elements);
			tmp3block = bnode_add_block(idx, idx->tmp3);

			/* This should be done as late as possible */
			tmp->sibling = tmp3block;
			bnode_clear_elements(idx, tmp, median);
			bnode_write_block(idx, tmp, block);

			if (block == idx->root_node)
			{
#ifdef DEBUG_FUNCTIONS
				assert(current_level == 0);
#endif

				/* Create a new root block if the root was getting full */
				tmp->num_elements = 1;
				tmp->lchild = idx->root_node;
				tmp->leaf = 0;
				tmp->sibling = -1;
				me_copy.internal.gchild = tmp3block;
				*bnode_get_ith_element_of_node(idx, tmp, 0) = me_copy;
				bnode_clear_elements(idx, tmp, 1);
				idx->root_node = bnode_add_block(idx, tmp);
				break;
			}

			new_element = me_copy;
			new_element.internal.gchild = tmp3block;
		} else
		{
			/* There was enough space in the block */
			bnode_write_block(idx, tmp, block);
			break;
		}
		current_level--;
	}
	return 1;
}

/**
 * The iterator data.
 */
struct bnode_string_iter_data
{
	/** Allocated bnode */
	bnode *node;

	/** The current index */
	int index;

	/** The document id */
	int did;
};

/**
 * The document did to which the iter points.
 *
 * @param iter
 * @return
 */
int bnode_string_iter_data_get_did(struct bnode_string_iter_data *iter)
{
	return iter->did;
}

/**
 * Sets up an iterator to find the given text.
 *
 * @param idx the index.
 * @param text the text to look for.
 * @param iter the previous iterator or NULL if this is the first iterator.
 * @return the new iterator or NULL, if no more matches could have been found.
 */
static struct bnode_string_iter_data *bnode_find_string_iter(struct index_external *idx, const char *text, struct bnode_string_iter_data *iter)
{
	int cmp;
	int index;
	struct bnode_element *be;

	if (!iter)
	{
		struct bnode_path path;
		int block;

		if (!(iter = malloc(sizeof(*iter))))
			return NULL;

		if (!(iter->node = bnode_create(idx)))
		{
			free(iter);
			return NULL;
		}

		if (!bnode_lookup(idx, text, &path))
			return 0;

		block = path.node[path.max_level].block;
		index = path.node[path.max_level].key_index;

		if (!bnode_read_block(idx, iter->node, block))
			return 0;
	} else
	{
		index = iter->index + 1;
	}

	if (index == iter->node->num_elements)
	{
		if (iter->node->sibling == -1)
			goto done;

		if (!bnode_read_block(idx, iter->node, iter->node->sibling))
			goto done;

		index = 0;
	}

	be = bnode_get_ith_element_of_node(idx, iter->node, index);
	if (!bnode_compare_string(idx, be, text, &cmp))
		goto done;
	if (cmp) goto done;
	iter->index = index;
	iter->did = be->leaf.did;
	return iter;
done:
	bnode_free(idx, iter->node);
	free(iter);
	return NULL;
}

/**
 * Tries to find the given string.
 *
 * @param idx
 * @param text
 * @return
 */
static int bnode_find_string(struct index_external *idx, const char *text, int (*callback)(int did, void *userdata), void *userdata)
{
	struct bnode_string_iter_data *iter = NULL;
	int nd = 0;

	while ((iter = bnode_find_string_iter(idx, text, iter)))
	{
		callback(bnode_string_iter_data_get_did(iter), userdata);
		nd++;
	}
	return nd;
}



static void index_external_dispose(struct index *index)
{
	struct index_external *idx;

	idx = (struct index_external*)index;

	if (idx->offset_file) fclose(idx->offset_file);
	if (idx->string_file) fclose(idx->string_file);
	if (idx->index_file) fclose(idx->index_file);
	if (idx->tmp3) bnode_free(idx, idx->tmp3);
	if (idx->tmp2) bnode_free(idx, idx->tmp2);
	if (idx->tmp) bnode_free(idx, idx->tmp);
	free(idx);
}

static struct index *index_external_create_with_opts(const char *filename, int block_size)
{
	struct index_external *idx;
	char buf[380];

	if (!(idx = (struct index_external*)malloc(sizeof(*idx))))
		return NULL;

	memset(idx,0,sizeof(*idx));
	list_init(&idx->document_list);
	idx->block_size = block_size;
	idx->max_elements_per_node = (idx->block_size - sizeof(struct bnode_header)) / sizeof(struct bnode_element);

	if (!(idx->tmp = bnode_create(idx)))
		goto bailout;

	if (!(idx->tmp2 = bnode_create(idx)))
		goto bailout;

	if (!(idx->tmp3 = bnode_create(idx)))
		goto bailout;

	sm_snprintf(buf, sizeof(buf), "%s.index", filename);
	if (!(idx->index_file = fopen(buf, "w+b")))
		goto bailout;

	sm_snprintf(buf, sizeof(buf), "%s.strings", filename);
	if (!(idx->string_file = fopen(buf, "w+b")))
		goto bailout;

	sm_snprintf(buf, sizeof(buf), "%s.offsets", filename);
	if (!(idx->offset_file = fopen(buf, "w+b")))
		goto bailout;

	idx->tmp->leaf = 1;
	idx->root_node = bnode_add_block(idx, idx->tmp);
	idx->max_substring_len = 32;

	return &idx->index;
bailout:
	index_external_dispose(&idx->index);
	return NULL;
}

static struct index *index_external_create(const char *filename)
{
	return index_external_create_with_opts(filename, 16384);
}

/**
 * Appends the given string to the string file. The string will be zero-byte terminated.
 *
 * @param idx the index.
 * @param text the text to be inserted.
 * @param offset returns the starting offset of the string that has been inserted.
 * @return 0 on failure, else something different.
 */
static int index_external_append_string(struct index_external *idx, const char *text, long *offset)
{
	/* Determine position and write text */
	if (fseek(idx->string_file, 0, SEEK_END) != 0)
		return 0;
	*offset = ftell(idx->string_file);
	fputs(text, idx->string_file);
	fputc(0, idx->string_file);

	return 1;
}

/**
 * Structure representing an offset/did pair.
 */
struct offset_entry
{
	int offset;
	int did;
};

/**
 * Appends the given offset did pair to the offsets file.
 */
static int index_external_append_offset_did_pair(struct index_external *idx, int offset, int did)
{
	struct offset_entry entry;

	if (fseek(idx->offset_file, 0, SEEK_END) != 0)
		return 0;

	entry.offset = offset;
	entry.did = did;

	if (fwrite(&entry, 1, sizeof(entry), idx->offset_file) != sizeof(entry))
		return 0;
	return 1;
}

int index_external_put_document(struct index *index, int did, const char *text)
{
	struct index_external *idx;
	int i;
	int l = strlen(text);
	long offset;

	idx = (struct index_external*)index;

	if (!index_external_append_string(idx, text, &offset))
		return 0;

	if (!index_external_append_offset_did_pair(idx, offset, did))
		return 0;

	for (i=0; i < l; i++)
	{
		if (!(bnode_insert_string((struct index_external*)index, did, offset + i, text + i)))
			return 0;
	}

	return 1;
}

int index_external_remove_document(struct index *index, int did)
{
	return 0;
}

int index_external_find_documents(struct index *index, int (*callback)(int did, void *userdata), void *userdata, int num_substrings, va_list substrings)
{
	int i;
	int nd = 0;
	struct index_external *idx;
	va_list substrings_copy;

	idx = (struct index_external*)index;

	if (num_substrings != 1)
	{
#ifdef DEBUG_FUNCTIONS
		fprintf(stderr, "Searching with only one sub string is supported for now.\n");
		exit(-1);
#else
		return 0;
#endif
	}
	va_copy(substrings_copy,substrings);

	for (i=0;i<num_substrings;i++)
	{
		nd = bnode_find_string(idx, va_arg(substrings_copy, char *), callback, userdata);
	}

	va_end(substrings_copy);

	return nd;
}

/*****************************************************/

struct index_algorithm index_external =
{
		index_external_create,
		index_external_dispose,
		index_external_put_document,
		index_external_remove_document,
		index_external_find_documents
};
