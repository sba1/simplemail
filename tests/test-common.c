#include "codesets.h"

#include <stdlib.h>

/*****************************************************************************/

static int check_match_mask(const char *expected, match_mask_t *actual)
{
	int i;
	int l = strlen(expected);

	for (i=0; i < l; i++)
	{
		unsigned int mp = match_bitmask_pos(i);

		if (expected[i] == '1')
		{
			if (!match_hit(actual, i))
			{
				printf("expected 1 at %d (mp=%d, actual=%x)\n", i, mp, actual[mp]);
				return 0;
			}
		} else
		{
			if (match_hit(actual, i))
			{
				printf("expected 0 at %d\n", i);
				return 0;
			}
		}
	}
	return 1;
}

/*****************************************************************************/
