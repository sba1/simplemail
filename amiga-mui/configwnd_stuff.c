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
** configwnd_stuff.c
*/

#include <ctype.h>
#include <stdlib.h>

#include <mui/texteditor_mcc.h>
#include <mui/nlistview_mcc.h>
#include <proto/exec.h>

#include "parse.h"
#include "smintl.h"
#include "support.h"
#include "support_indep.h"

#include "configwnd_stuff.h"

/******************************************************************
 Convert addresses from a texteditor object to an array.
 this is principle the same like in addressbookwnd.c but uses
 parse_mailbox
*******************************************************************/
char **array_of_addresses_from_texteditor(Object *editor, int page, int *error_ptr, Object *config_wnd, Object *config_list)
{
	char *addresses;
	char **new_array = NULL;

	/* Check the validity of the e-mail addresses first */
	if ((addresses = (char*)DoMethod(editor, MUIM_TextEditor_ExportText)))
	{
		struct mailbox mb;
		char *buf = addresses;

		while (isspace((unsigned char)*buf) && *buf) buf++;

		if (*buf)
		{
			while ((buf = parse_mailbox(buf,&mb)))
			{
				/* ensures that buf != NULL if no error */
				while (isspace((unsigned char)*buf)) buf++;
				if (*buf == 0) break;
				free(mb.addr_spec);
				free(mb.phrase);
			}
			if (!buf)
			{
				set(config_list, MUIA_NList_Active, page);
				set(config_wnd,MUIA_Window_ActiveObject,editor);
				sm_request(NULL,_("You have entered an invalid email address. Please correct it."),_("Ok"),NULL);
				FreeVec(addresses);
				array_free(new_array);
				*error_ptr = 1;
				return NULL;
			}

			buf = addresses;
			while ((buf = parse_mailbox(buf,&mb)))
			{
				new_array = array_add_string(new_array,mb.addr_spec);
				free(mb.addr_spec);
				free(mb.phrase);
			}
		}
		FreeVec(addresses);		
	}
	*error_ptr = 0;
	return new_array;
}
