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
** headers.c
*/

#include <clib/alib_protos.h>
#include <exec/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

#include <exec/exec.h>
#include <proto/exec.h>

#include <dos/dos.h>
#include <proto/dos.h>

#include <libraries/mui.h>
#include <proto/muimaster.h>
#include <mui/NListview_mcc.h>
#include <mui/NList_mcc.h>
#include <mui/NListtree_mcc.h>
#include <mui/BetterString_mcc.h>
#include <mui/popplaceholder_mcc.h>
#include <mui/TextEditor_mcc.h>

#include <libraries/iffparse.h>
#include <proto/iffparse.h>

#include <proto/locale.h>

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <proto/socket.h>

#include <proto/keymap.h>
#include <proto/icon.h>

#ifndef __AROS__
#include <proto/openurl.h>
#endif

#ifdef __AROS__
#include <expat.h>
#else
#include <expat/expat.h>
#include <proto/expat.h>
#endif
