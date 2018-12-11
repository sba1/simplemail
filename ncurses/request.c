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

#include "request.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

/*****************************************************************************/

char *sm_request_file(const char *title, const char *path, int save, const char *extension)
{
	return NULL;
}

/*****************************************************************************/

int sm_request(const char *title, const char *text, const char *gadgets, ...)
{
	return 0;
}

/*****************************************************************************/

char *sm_request_string(const char *title, const char *text, const char *contents, int secret)
{
	return NULL;
}

/*****************************************************************************/

int sm_request_login(char *text, char *login, char *password, int len)
{
	return 0;
}

/*****************************************************************************/

char *sm_request_pgp_id(char *text)
{
	return NULL;
}

/*****************************************************************************/

struct folder *sm_request_folder(char *text, struct folder *exclude)
{
	return NULL;
}

