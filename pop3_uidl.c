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
 */

#include "pop3_uidl.h"

#include "debug.h"
#include "hash.h"
#include "support.h"
#include "support_indep.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct uidl_entry /* stored on the harddisk */
{
	int size; /**< size of the mail, unused yet, so it is -1 for now */
	char uidl[72]; /**< null terminated, 70 are enough according to RFC 1939 */
};


/*****************************************************************************/

int uidl_open(struct uidl *uidl)
{
	FILE *fh;
	int rc = 0;

	SM_ENTER;

	if (!uidl->filename) return 0;

	if ((fh = fopen(uidl->filename,"rb")))
	{
		unsigned char id[4];
		int uidls_size = myfsize(fh) - 4;
		int cnt = fread(id,1,4,fh);

		if (cnt == 4 && id[0] == 'S' && id[1] == 'M' && id[2] == 'U' && id[3] == 0 &&
		    (uidls_size % sizeof(struct uidl_entry)) == 0)
		{
			uidl->num_entries = (uidls_size)/sizeof(struct uidl_entry);
			if ((uidl->entries = malloc(uidls_size)))
			{
				fread(uidl->entries,1,uidls_size,fh);
				rc = 1;
			}
		}

		fclose(fh);
	}
	SM_RETURN(rc,"%ld");
}

/*****************************************************************************/

int uidl_test(struct uidl *uidl, char *to_check)
{
	int i;
	if (!uidl->entries) return 0;
	for (i=0;i<uidl->num_entries;i++)
	{
		if (!strcmp(to_check,uidl->entries[i].uidl)) return 1;
	}
	return 0;
}

/*****************************************************************************/

void uidl_remove_unused(struct uidl *uidl, int num_remote_uidls, char **remote_uidls)
{
	SM_ENTER;

	if (uidl->entries)
	{
		int i;
		for (i=0; i<uidl->num_entries; i++)
		{
			int j,found=0;
			char *uidl_entry = uidl->entries[i].uidl;
			for (j=0; j<num_remote_uidls; j++)
			{
				if (!remote_uidls[j])
					continue;

				if (!strcmp(uidl_entry,remote_uidls[j]))
				{
					found = 1;
					break;
				}
			}

			if (!found)
				memset(uidl_entry,0,sizeof(uidl->entries[i].uidl));
		}
	}

	SM_LEAVE;
}

/*****************************************************************************/

void uidl_add(struct uidl *uidl, const char *new_uidl)
{
	int i=0;
	FILE *fh;

	SM_ENTER;

	if (!new_uidl || new_uidl[0] == 0) return;
	for (i=0;i<uidl->num_entries;i++)
	{
		if (!uidl->entries[i].uidl[0])
		{
			strcpy(uidl->entries[i].uidl, new_uidl);
			if ((fh = fopen(uidl->filename,"rb+")))
			{
				fseek(fh,4+i*sizeof(struct uidl_entry),SEEK_SET);
				uidl->entries[i].size = -1;
				fwrite(&uidl->entries[i],1,sizeof(uidl->entries[i]),fh);
				fclose(fh);
				SM_LEAVE;
				return;
			}
		}
	}

	SM_DEBUGF(15,("Appending to %s\n",uidl->filename));

	if ((fh = fopen(uidl->filename,"ab")))
	{
		struct uidl_entry entry;
		if (ftell(fh)==0)
		{
			/* we must have newly created the file */
			fwrite("SMU",1,4,fh);
		}
		entry.size = -1;
		mystrlcpy(entry.uidl,new_uidl,sizeof(entry.uidl));
		fwrite(&entry,1,sizeof(entry),fh);
		fclose(fh);
	} else
	{
		SM_DEBUGF(5,("Failed to open %s\n",uidl->filename));
	}

	SM_LEAVE;
}

