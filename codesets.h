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

#ifndef SM__CODESETS_H
#define SM__CODESETS_H

typedef unsigned char	uft8;

#define uft8size(s) (s)?(strlen(s):0)
#define uft8cpy(dest,src) ((uft8*)strcpy(dest,src))
#define uft8cat(dest,src) ((uft8*)strcat(dest,src))

int uft8len(const uft8 *str);
uft8 *uft8ncpy(uft8 *to, const uft8 *from, int n);

#endif
