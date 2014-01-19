/**
 * index_external.c - a external string index implementation for SimpleMail.
 * Copyright (C) 2012  Sebastian Bauer
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
 */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "debug.h"
#include "lists.h"

#include "index.h"
#include "index_private.h"
#include "index_external.h"

/* Get va_vopy() with pre-C99 compilers */
#ifdef __SASC
#define va_copy(dest,src) ((dest) = (src))
#elif defined(__GNUC__) && (__GNUC__ < 3)
#include <varargs.h>
#endif

struct bnode_element
{
	int str_offset;
	int str_len;
	int did;
	int gchild; /* Child containing values that are greater */
};

struct bnode_header
{
	int leaf;
	int num_elements; /* Number of following bnode_elements */
	int lchild; /* Child containing keys/strings that are all (lexicographical) smaller */
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
	FILE *index_file;

	/** Identifies the block for the root node. TODO: Make this persistent */
	int root_node;

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

	fseek(idx->string_file, element->str_offset, SEEK_SET);
	if (!(str = malloc(element->str_len + 1)))
		return 0;
	if (fread(str, 1, element->str_len, idx->string_file) != element->str_len)
		return 0;
	str[element->str_len] = 0;
	return str;
}

/**
 * Lookup the given text.
 *
 * @param idx
 * @param text
 * @param out_block
 * @param out_index
 * @return
 */
static int bnode_lookup(struct index_external *idx, const char *text, int *out_block, int *out_index)
{
	int i;
	bnode *tmp = idx->tmp;
	int prev_block;
	int block = idx->root_node;

	do
	{
		bnode_read_block(idx, tmp, block);
		prev_block = block;

		int lchild = tmp->lchild;

		/* Find the appropriate slot. TODO: Use binary search */
		for (i=0; i < tmp->num_elements; i++)
		{
			int cmp;
			struct bnode_element *e = bnode_get_ith_element_of_node(idx, tmp, i);
			char *str = bnode_read_string(idx, e);
			if (!str) return 0;

			cmp = strcmp(str, text);
			free(str);

			if (!cmp)
			{
				/* Direct match with a separation key */
				goto out;
			}

			/* if str > text then we ascend to lchild (except if we are at a leaf, of course) */
			if (cmp > 0)
				break;

			lchild = e->gchild;
		}

		if (block == lchild && !tmp->leaf)
		{
			fprintf(stderr, "Endless loop detected!\n");
			return 0;
		}
		block = lchild;
	} while (!tmp->leaf);

out:
	*out_block = prev_block;
	*out_index = i;

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
	memset(e, (idx->max_elements_per_node - start)*sizeof(struct bnode_element), 0);
}

/**
 * Dump the children of the given nodes.
 */
static void dump_node_children(struct index_external *idx, bnode *node, const char *prefix)
{
	int i;
	printf("%s: ", prefix, node->lchild);
	for (i=0;i<node->num_elements;i++)
	{
		printf("%d ",bnode_get_ith_element_of_node(idx, node, i)->gchild);
	}
	printf("\n");
}

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
	int block;
	bnode *tmp = idx->tmp;

	if (!bnode_lookup(idx, text, &block, &i))
		return 0;

	bnode_read_block(idx, tmp, block);

	if (1)
	{
		/* Recall that we allocate in our temps one more entry than it would fit in the block, so we surly can
		 * insert the entry now */
		struct bnode_element *e = bnode_get_ith_element_of_node(idx, tmp, i);

		memmove(e + 1, e, (tmp->num_elements - i) * sizeof(struct bnode_element));

		/* New element */
		e->str_offset = offset;
		e->str_len = strlen(text);
		e->gchild = 0;
		e->did = did;
		tmp->num_elements++;
	}

	if (tmp->num_elements == idx->max_elements_per_node)
	{
		/* Now we split the node into two nodes. We keep the median out as we
		 * insert it as a separation value for the two nodes on the parent.
		 */

		int median = tmp->num_elements / 2;
		struct bnode_element *me = bnode_get_ith_element_of_node(idx, tmp, median);
		struct bnode_element me_copy = *me;

		/* First node */
		tmp->num_elements = median - 1;
		tmp->leaf = 1;
		bnode_clear_elements(idx, tmp, median);
		/* This should be done as late as possible */
		bnode_write_block(idx, tmp, block);

		/* Second node */
		idx->tmp3->num_elements = idx->max_elements_per_node - median;
		idx->tmp3->leaf = 1;
		memcpy(bnode_get_ith_element_of_node(idx, idx->tmp3, 0), bnode_get_ith_element_of_node(idx, tmp, median), idx->tmp3->num_elements * sizeof(struct bnode_element));
		bnode_clear_elements(idx, tmp, idx->tmp3->num_elements);
		int tmp3block = bnode_add_block(idx, idx->tmp3);

		if (block == idx->root_node)
		{
			/* Create a new root block if the root was getting full */
			tmp->num_elements = 1;
			tmp->lchild = idx->root_node;
			tmp->leaf = 0;
			me_copy.gchild = tmp3block;
			*bnode_get_ith_element_of_node(idx, tmp, 0) = me_copy;
			bnode_clear_elements(idx, tmp, 1);
			idx->root_node = bnode_add_block(idx, tmp);
		} else
		{
			int lchild;

			/* Read root node, we want to add the new element */
			bnode_read_block(idx, tmp, idx->root_node);

			lchild = tmp->lchild;
			i = 0;

			/* Find the separation key whose left child points to the splitted block */
			for (i=0; i<tmp->num_elements && lchild != block; i++)
				lchild = bnode_get_ith_element_of_node(idx, tmp, i)->gchild;

			if (tmp->num_elements == idx->max_elements_per_node)
			{
				fprintf(stderr, "Splitting a non-direct-child of the root node %d is not supported for now!\n", block);
				exit(1);
			}

			struct bnode_element *e = bnode_get_ith_element_of_node(idx, tmp, i);
			memmove(e + 1, e, (tmp->num_elements - i) * sizeof(struct bnode_element));
			*e = me_copy;
			e->gchild = tmp3block;
			tmp->num_elements++;
			bnode_write_block(idx, tmp, idx->root_node);
		}
	} else
	{
		/* There was enough space in the block, so we simply write it out */
		bnode_write_block(idx, tmp, block);
	}

	return 1;
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
	int i;
	bnode *tmp = idx->tmp;
	int text_len = strlen(text);

	bnode_read_block(idx, tmp, 0);

	/* Find the appropriate slot. TODO: Use binary search */
	for (i=0; i < tmp->num_elements; i++)
	{
		struct bnode_element *e = bnode_get_ith_element_of_node(idx, tmp, i);
		if (text_len > tmp->num_elements)
			continue;
		char *str = bnode_read_string(idx, e);
		if (!str) return 0;
		if (!strncmp(str, text, text_len))
		{
			callback(e->did, userdata);
		}
	}
}

void index_external_dispose(struct index *index)
{
	struct document_node *d;
	struct index_external *idx;

	idx = (struct index_external*)index;

	if (idx->string_file) fclose(idx->string_file);
	if (idx->index_file) fclose(idx->index_file);
	if (idx->tmp3) bnode_free(idx, idx->tmp3);
	if (idx->tmp2) bnode_free(idx, idx->tmp2);
	if (idx->tmp) bnode_free(idx, idx->tmp);
	free(idx);
}

struct index *index_external_create(const char *filename)
{
	struct index_external *idx;
	char buf[380];

	if (!(idx = (struct index_external*)malloc(sizeof(*idx))))
		return NULL;

	memset(idx,0,sizeof(*idx));
	list_init(&idx->document_list);
	idx->block_size = 16384;
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

	idx->tmp->leaf = 1;
	idx->root_node = bnode_add_block(idx, idx->tmp);

	return &idx->index;
bailout:
	index_external_dispose(&idx->index);
	return NULL;
}

int index_external_put_document(struct index *index, int did, const char *text)
{
	struct index_external *idx;
	int i;
	int l = strlen(text);

	idx = (struct index_external*)index;

	/* Determine position and write text */
	long offset = ftell(idx->string_file);
	fputs(text, idx->string_file);

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

	va_copy(substrings_copy,substrings);

	for (i=0;i<num_substrings;i++)
	{
		bnode_find_string(idx, va_arg(substrings_copy, char *), callback, userdata);
//		bnode_insert_string(
//		if (!strstr(d->txt,va_arg(substrings_copy,char *)))
//		{
//			take = 0;
//			break;
//		}
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
