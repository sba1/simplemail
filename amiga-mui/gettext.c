/* This file is based on gettext sources, but specialiced for freeciv.
It may not work with other programs, as the functions do not do the
stuff of gettext package. Also client and server cannot use one data file,
but need to load it individually.

And I did not want to port gettext completely, which would have been
much more work!

Modification history;
08.08.2000 : Dirk St�cker <stoecker@epost.de>
  Initial version
27.08.2000 : Dirk St�cker <stoecker@epost.de>
  fixes to match changed calling mechanism
26.09.2000 : Oliver Gantert <lucyg@t-online.de>
  changed includes to work with vbcc WarpOS
16.12.2000 : Dirk St�cker <stoecker@epost.de>
  removed changes, as it works without also
26.12.2001 : Sebastian Bauer <sebauer@t-online.de>
  uses now the prefered languages to determine the gmostr
26.12.2001 : Sebastian Bauer <sebauer@t-online.de>
  lets SAS open the locale library for us now
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <proto/exec.h>
#include <proto/locale.h>

char *sm_getenv(const char *name);

/* The magic number of the GNU message catalog format.  */
#define _MAGIC 0x950412de
#define _MAGIC_SWAPPED 0xde120495

#define SWAP(i) (((i) << 24) | (((i) & 0xff00) << 8) | (((i) >> 8) & 0xff00) | ((i) >> 24))
#define GET(data) (domain.must_swap ? SWAP (data) : (data))

typedef unsigned long nls_uint32;

/* Header for binary .mo file format.  */
struct mo_file_header
{
  nls_uint32 magic;  /* The magic number.  */
  nls_uint32 revision;  /* The revision number of the file format.  */
  nls_uint32 nstrings;  /* The number of strings pairs.  */
  nls_uint32 orig_tab_offset;  /* Offset of table with start offsets of original strings.  */
  nls_uint32 trans_tab_offset;  /* Offset of table with start offsets of translation strings.  */
  nls_uint32 hash_tab_size;  /* Size of hashing table.  */
  nls_uint32 hash_tab_offset;  /* Offset of first hashing entry.  */
};

struct string_desc
{
  nls_uint32 length;  /* Length of addressed string.  */
  nls_uint32 offset;  /* Offset of string in file.  */
};

struct loaded_domain
{
	char *data;
  int must_swap;
  nls_uint32 nstrings;
  struct string_desc *orig_tab;
  struct string_desc *trans_tab;
  nls_uint32 hash_size;
  nls_uint32 *hash_tab;
};

static struct loaded_domain domain = {0, 0, 0, 0, 0, 0, 0};

static long openmo(char *dir, char *loc)
{
  char filename[512];
  FILE *fd;
  long size;
  struct mo_file_header *data;

  if(!dir || !loc)
    return 0;

  sprintf(filename, "%s/%s.mo", dir, loc);
#ifdef DEBUG
  printf("locale file name: %s\n", filename);
#endif

  if((fd = fopen(filename, "rb")))
  {
    if(!fseek(fd, 0, SEEK_END))
    {
      if((size = ftell(fd)) != EOF)
      {
        if(!fseek(fd, 0, SEEK_SET))
        {
          if((data = (struct mo_file_header *) malloc(size)))
          {
            if(fread(data, size, 1, fd) == 1)
            {
              if((data->magic == _MAGIC || data->magic == _MAGIC_SWAPPED) && !data->revision) /* no need to swap! */
              {
                domain.data = (char*)data;
                domain.must_swap = data->magic != _MAGIC;
                domain.nstrings = GET(data->nstrings);
                domain.orig_tab = (struct string_desc *) ((char *) data + GET(data->orig_tab_offset));
                domain.trans_tab = (struct string_desc *) ((char *) data + GET(data->trans_tab_offset));
                domain.hash_size = GET(data->hash_tab_size);
                domain.hash_tab = (nls_uint32 *) ((char *) data + GET(data->hash_tab_offset));

								if (!strcmp("pl",loc))
								{
	              	char *conv_pl;

									/* convert iso charset into amigapl but only if the env variable is set
									 * On AmigaOS4 and MorphOS the iso code is used, this is only for compatiblity
									 * on systems which still use AmigaPL */

									conv_pl = sm_getenv("AmigaPLSimpleMail");

									if (conv_pl && (*conv_pl == 'y' || *conv_pl == '1'))
									{
										unsigned int i;
									  for (i=0;i<domain.nstrings;i++)
									  {
									    int j;
									    int off = GET(domain.trans_tab[i].offset);
									    char *trans_string = domain.data + off;
	
									    /* Now convert every char in size the string */
									    for (j=0;j<GET(domain.trans_tab[i].length) && j+off<size;j++)
									    {
									    	unsigned char c = trans_string[j];
									    	if (c > 127)
									    	{
									    		/* yes a table would be better, but I'm too lazy */
									    		switch (c)
									    		{
														case	0xb1: c=0xe2; break;
														case	0xe6: c=0xea; break;
														case	0xea: c=0xeb; break;
														case	0xb3: c=0xee; break;
														case	0xf1: c=0xef; break;
														case	0xf3: c=0xf3; break;
														case	0xb6: c=0xf4; break;
														case	0xbc: c=0xfa; break;
														case	0xbf: c=0xfb; break;
														case	0xa1: c=0xc2; break;
														case	0xc6: c=0xca; break;
														case	0xca: c=0xcb; break;
														case	0xa3: c=0xce; break;
														case	0xd1: c=0xcf; break;
														case	0xd3: c=0xd3; break;
														case	0xa6: c=0xd4; break;
														case	0xac: c=0xda; break;
														case	0xaf: c=0xdb; break;
									    		}
													trans_string[j] = c;
									    	}
									    }
									  }
									}
								}
              }
              else
                free(data);
            }
            else
              free(data);
          }
        }
      }
    }
    fclose(fd);
  }
  return (long) domain.data;
}

/* languages supported by freeciv */
struct LocaleConv {
  char *langstr;
  char *gmostr;
} LocaleConvTab[] = {
{"deutsch", "de"},
{"german", "de"},
{"danish", "da"},
/*{"", "en_GB"}, does not exist on Amiga */
{"espa�ol", "es"},
{"spanish", "es"},
{"fran�ais", "fr"},
{"french", "fr"},
{"hrvatski", "hr"},
{"italiano", "it"},
{"italian", "it"},
{"nihongo", "ja"},
{"nederlands", "nl"},
{"norsk", "no"},
{"polski", "pl"},
{"polish","pl"},
{"portugu�s" , "pt"},
{"portugu�s-brasil", "pt_BR"},
{"russian", "ru"},
{"svenska", "sv"},
{"suomi","fi"},
{"magyar","ma"},
{0, 0},
};

/* Returns NULL if is not in the list */
static char *find_gmostr(char *lang)
{
	int i;
	if (!lang) return NULL;
	for (i=0;LocaleConvTab[i].langstr;i++)
	{
		if (!stricmp(LocaleConvTab[i].langstr,lang)) return LocaleConvTab[i].gmostr;
	}
	return NULL;
}

void bindtextdomain(char * pack, char * dir)
{
	struct Locale *l;
  int i;

  if(openmo(dir, sm_getenv("LANG")))
    return;

  if(openmo(dir, sm_getenv("LANGUAGE")))
    return;

	if((l = OpenLocale(0)))
	{
		for (i=0;i<10;i++)
		{
			if (!openmo(dir,find_gmostr(l->loc_PrefLanguages[i])))
			{
				if (openmo(dir,l->loc_PrefLanguages[i])) break;
			} else break;
		}

		CloseLocale(l);
	}
}

#define HASHWORDBITS 32
static unsigned long hash_string(const char *str_param)
{
  unsigned long int hval, g;
  const char *str = str_param;

  /* Compute the hash value for the given string.  */
  hval = 0;
  while (*str != '\0')
  {
    hval <<= 4;
    hval += (unsigned long) *str++;
    g = hval & ((unsigned long) 0xf << (HASHWORDBITS - 4));
    if (g != 0)
    {
      hval ^= g >> (HASHWORDBITS - 8);
      hval ^= g;
    }
  }
  return hval;
}

static char *find_msg(const char * msgid)
{
  long top, act, bottom;

  if(!domain.data || !msgid)
    return NULL;

  /* Locate the MSGID and its translation.  */
  if(domain.hash_size > 2 && domain.hash_tab != NULL)
  {
    /* Use the hashing table.  */
    nls_uint32 len = strlen (msgid);
    nls_uint32 hash_val = hash_string (msgid);
    nls_uint32 idx = hash_val % domain.hash_size;
    nls_uint32 incr = 1 + (hash_val % (domain.hash_size - 2));
    nls_uint32 nstr;

    for(;;)
    {
      if(!(nstr = GET(domain.hash_tab[idx]))) /* Hash table entry is empty.  */
	return NULL;

      if(GET(domain.orig_tab[nstr - 1].length) == len && strcmp(msgid, domain.data +
      GET(domain.orig_tab[nstr - 1].offset)) == 0)
        return (char *) domain.data + GET(domain.trans_tab[nstr - 1].offset);

      if(idx >= domain.hash_size - incr)
	idx -= domain.hash_size - incr;
      else
        idx += incr;
    }
  }

  /* Now we try the default method:  binary search in the sorted
     array of messages.  */
  bottom = act = 0;
  top = domain.nstrings;
  while(bottom < top)
  {
    int cmp_val;

    act = (bottom + top) / 2;
    cmp_val = strcmp(msgid, domain.data + GET(domain.orig_tab[act].offset));
    if(cmp_val < 0)
      top = act;
    else if(cmp_val > 0)
      bottom = act + 1;
    else
      break;
  }

  /* If an translation is found return this.  */
  return bottom >= top ? NULL : (char *) domain.data + GET(domain.trans_tab[act].offset);
}


char *gettext(const char *msgid)
{
  char *res;

  if(!(res = find_msg(msgid)))
  {
    res = (char *) msgid;
#ifdef DEBUG
    Printf("Did not find: '%s'\n", res);
#endif
  }

  return res;
}

char *setlocale(int a, char *b)
{
  return "C";
}
