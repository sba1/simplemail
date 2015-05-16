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

#include "signature.h"

#include <stdlib.h>
#include <string.h>

#include "support_indep.h"

/*****************************************************************************/

struct signature *signature_malloc(void)
{
	struct signature *sig;

	if ((sig = (struct signature*)malloc(sizeof(struct signature))))
	{
		memset(sig,0,sizeof(struct signature));
	}
	return sig;
}

/*****************************************************************************/

struct signature *signature_duplicate(struct signature *s)
{
	struct signature *ns = signature_malloc();
	if (ns)
	{
		ns->name = mystrdup(s->name);
		ns->signature = mystrdup(s->signature);
	}
	return ns;
}

/*****************************************************************************/

void signature_free(struct signature *s)
{
	if (s->name) free(s->name);
	if (s->signature) free(s->signature);
	free(s);
}

/*****************************************************************************/
