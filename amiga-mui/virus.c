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
** virus.c
*/

#include "virus.h"

#include <proto/xvs.h>
#include <proto/xfdmaster.h>

/******************************************************************
 Checks for viruses, if req=TRUE, a requester will also pop up
 if no viruses were found, otherwise only when needed.
 Returncodes:
	0 = Error
	1 = Ok, no Virus
	2 = Virus detected not removed
	3 = Virus detected and removed.
*******************************************************************/
int virus_check_mail(struct mail *m, int req)
{
	int rc = 0;

	/* We decrunch also the file using xfd as virus-checking w/o decrunch has no sense. */
	
	

	return rc;
}