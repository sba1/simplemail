/**
 * This is a implementation of the (turbo) bayer/moore string search algorithm.
 * Based upon the code presented at http://www-igm.univ-mlv.fr/~lecroq/string/.
 *
 * @file bayermoore.c
 */

#include <stdlib.h>

#include "bayermoore.h"

#define MAX(a,b) ((a)>(b)?(a):(b))

static void suffixes(char *x, int m, unsigned short *suff)
{
   int f, g, i;
/* TODO: fix possible problem with f */
   suff[m - 1] = m;
   g = m - 1;
   for (i = m - 2; i >= 0; --i)
   {
      if (i > g && suff[i + m - 1 - f] < i - g)
         suff[i] = suff[i + m - 1 - f];
      else {
         if (i < g)
            g = i;
         f = i;
         while (g >= 0 && x[g] == x[g + m - 1 - f])
            --g;
         suff[i] = f - g;
      }
   }
}

/**
 * Computes the good suffix skip table.
 *
 * @param x
 * @param m
 * @param bmGs
 */
static void preBmGs(char *x, int m, unsigned short bmGs[])
{
   int i, j;
   unsigned short suff[m+1];

   suffixes(x, m, suff);

   for (i = 0; i < m; ++i)
      bmGs[i] = m;
   j = 0;
   for (i = m - 1; i >= 0; --i)
      if (suff[i] == i + 1)
         for (; j < m - 1 - i; ++j)
            if (bmGs[j] == m)
               bmGs[j] = m - 1 - i;
   for (i = 0; i <= m - 2; ++i)
      bmGs[m - 1 - suff[i]] = m - 1 - i;
}


/**
 * @brief The context data structure of the bayermoore search
 */
struct bayermoore_context
{
	/** @brief The bad character skip table */
	unsigned short skip_table[UCHAR_MAX];

	/** @brief The good suffix table */
	unsigned short *good_suffix_table;

	char *pat; /**! @brief pattern to be searched for */
	int plen; /**! @brief length of the pattern */
};

/**
 * Creates the bayermoore context.
 *
 * @param p
 * @param plen
 * @return
 */
struct bayermoore_context *bayermoore_create_context(char *p, int plen)
{
	struct bayermoore_context *context;
	unsigned short *next;

	int i;
#if 0
	int j,k;
#endif

	if (!(context = (struct bayermoore_context*)malloc(sizeof(*context))))
		return NULL;

	if (!(context->good_suffix_table = next = (unsigned short*)malloc((plen + 1)*sizeof(context->good_suffix_table[0]))))
	{
		free(context);
		return NULL;
	}

	context->pat = p;
	context->plen = plen;

	/* Prepare bad character skip table */
	for (i = 0; i < sizeof(context->skip_table)/sizeof(context->skip_table[0]); i++)
		context->skip_table[i] = plen;
	for (i = 0; i < plen - 1; i++)
		context->skip_table[(unsigned char)p[i]] = plen - i - 1;

#if 0 /* The wikipedia code */
	/* Prepare the good suffix table */
    for (j = 0; j <= plen; j++) {
        for (i = plen - 1; i >= 1; i--) {
            for (k = 1; k <= j; k++) {
                if (i - k < 0) {
                    break;
                }
                if (p[plen - k] != p[i - k]) {
                    goto nexttry;
                }
            }
            goto matched;
nexttry:
            ;
        }
matched:
        next[j] = plen - i;
    }
#endif
    preBmGs(p, plen, next);
	return context;
}

/**
 * Creates the bayermoore context.
 *
 * @param context
 */
void bayermoore_delete_context(struct bayermoore_context *context)
{
	if (context)
	{
		free(context->good_suffix_table);
		free(context);
	}
}

/**
 * Performs the bayermoore algorithm.
 *
 * @param x the substring which should be found
 * @param m the length of the pattern
 * @param y string to be searched through
 * @param n number of bytes to be searches through
 * @param callback function that is called for every hit. If callback returns 0,
 *        the search is aborted.
 * @param user_data data pointer that is fed into the callback function.
 *
 * @return the position of the last found pattern or -1 if the pattern could not be found.
 */

int bayermoore(struct bayermoore_context *context, char *str, int n, bm_callback callback, void *user_data)
{
	int i, j;
	int rc = -1;

	unsigned short *bmBc = context->skip_table;
	unsigned short *bmGs = context->good_suffix_table;
	int m = context->plen;
	char *substr = context->pat;

	/* Searching */
	j = 0;
	while (j <= n - m)
	{
		for (i = m - 1; i >= 0 && substr[i] == str[i + j]; --i);
		if (i < 0)
		{
			rc = j;
			if (!callback || !callback(substr,j,user_data))
				return rc;
			j += bmGs[0];
		}  else
		{
			j += MAX(bmGs[i], bmBc[(unsigned char)str[i + j]] - m + 1 + i);
		}
	}

   return rc;
}

#ifdef COMPILE_TEST

#include <string.h>
#include <stdio.h>

int callback(char *x, unsigned int pos, void *user_data)
{
	printf("pos=%d\n",pos);
	return 1;
}

int main(int argc, char **argv)
{
	char *txt = "qhello2hellohello2hello2hello";
	char *pat = "hello";

	int rel_pos, pos;

	struct bayermoore_context *context;

	if (!(context = bayermoore_create_context(pat,strlen(pat))))
		return 0;

	bayermoore(context,txt,strlen(txt),callback,NULL);

	printf("\n");

	pos = 0;
	while ((rel_pos = bayermoore(context,txt+pos,strlen(txt+pos),NULL,NULL)) != -1)
	{
		pos += rel_pos;
		printf("%ld\n",pos);
		pos++;
	}
	bayermoore_delete_context(context);
	return 0;
}

#endif
