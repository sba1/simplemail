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
** taglines.c
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "configuration.h"
#include "support_indep.h"
#include "taglines.h"

#define TAGLINES_INDEX_VERSION 0

int read_line(FILE *fh, char *buf); /* in addressbook.c */

static int taglines_num;
static int *taglines_positions;
static char *taglines_filename;

int random(int max)
{
	static int x=0;

	if(!x)
	{
		srand(time(0));
		x = 1;
	}

	return (rand() % max);
}

char *get_tagline(void)
{
	char *rc;
	struct tagline *t;
	int nr, len = list_length(&user.config.tagline_list);
	struct node *n;

	if(!len) return NULL;

	n=list_first(&user.config.tagline_list);

	nr = random(len);

	t = (struct tagline *) list_find(&user.config.tagline_list, nr);


	rc = malloc(strlen(t->txt) + 1);
	if(rc != NULL)
	{
		strcpy(rc, t->txt);
	}

	return rc;
}

char * taglines_add_tagline(char *buf)
{
	char *rc = buf;
	char *tagline;
	long len;

	tagline = get_tagline();

	if(tagline != NULL)
	{
		len = strlen(buf) + strlen(tagline);

		rc = malloc(len+1);
		if(rc != NULL)
		{
			strcpy(rc, buf);
			strcat(rc, tagline);
			free(tagline);
			free(buf);
		}
	}
	else
	{
		rc = buf;
	}

	return rc;
}

struct tagline *taglines_create_tagline(char *txt)
{
	struct tagline *rc;

	rc = malloc(sizeof(struct tagline));
	if(rc != NULL)
	{
		rc->txt = malloc(strlen(txt) + 1);
		if(rc->txt != NULL)
		{
			strcpy(rc->txt, txt);
		}
		else
		{
			free(rc);
			rc = NULL;
		}
	}

	return rc;
}

void taglines_free_tagline(struct tagline *t)
{
	if(t != NULL)
	{
		if(t->txt != NULL)
		{
			free(t->txt);
		}
		free(t);
	}
}

/******************************************************************
 Creates a tagline index file for the given tagline file
*******************************************************************/
static void taglines_create_index(char *filename, char *indexname)
{
	FILE *tagfh, *indexfh;
	char *buf;

	if (!(buf = malloc(512))) return;

	if ((tagfh = fopen(filename,"r")))
	{
		if ((indexfh = fopen(indexname,"wb")))
		{
			int pos = 0;
			int ver = TAGLINES_INDEX_VERSION;

			fwrite("SMTI",1,4,indexfh);
			fwrite(&ver,1,4,indexfh);

			while (read_line(tagfh,buf))
			{
				if (strcmp(buf, "%%") == 0)
				{
					fwrite(&pos,1,4,indexfh);
					pos = ftell(indexfh);
				}
			}

			fclose(indexfh);
		}
		fclose(tagfh);
	}
	free(buf);
}


/******************************************************************
 Loads the tagline information (and creates the index if
 neccessary
*******************************************************************/
void taglines_cleanup(void)
{
	free(taglines_filename);
	free(taglines_positions);
	taglines_filename = NULL;
	taglines_positions = NULL;
	taglines_num = 0;
}

/******************************************************************
 Loads the tagline information (and creates the index if
 neccessary)
*******************************************************************/
void taglines_init(char *filename)
{
	FILE *fh;
	char *indexname;
	unsigned int fh_size;
	taglines_cleanup();

	if (!(indexname = malloc(mystrlen(filename)+10)))
		return;

	strcat(indexname,".index");

	if (myfiledatecmp(filename,indexname)>0)
	{
		taglines_create_index(filename,indexname);
	}

	if ((fh = fopen(indexname,"rb")))
	{
		char buf[4];
		fread(buf,1,4,fh);
		if (!strncmp("SMTI",buf,4))
		{
			int ver;
			fread(&ver,1,4,fh);
			if (ver == TAGLINES_INDEX_VERSION)
			{
				fh_size = myfsize(fh);
				if ((taglines_positions = malloc(fh_size - 8 + sizeof(int))))
				{
					fread(taglines_positions,1,fh_size - 8,fh);
					taglines_num = (fh_size - 8) / sizeof(int);
					taglines_positions[taglines_num] = fh_size;
				}
			}
		}

		fclose(fh);
	}
	free(indexname);
}
