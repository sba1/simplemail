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
** tcpip.h
*/

#if defined(__AMIGAOS4__) || defined(__AROS__)
#  include <proto/bsdsocket.h>
#elif defined(_AMIGA) || defined(__MORPHOS)
#  include <proto/socket.h>
#endif

#ifndef SM__TCP_H
#include "tcp.h"
#endif

int open_socket_lib(void);
void close_socket_lib(void);
int is_online(const char *iface);
int open_ssl_lib(void);
void close_ssl_lib(void);

#ifndef NO_SSL
SSL_CTX *ssl_context(void);
#endif

long tcp_herrno(void);
long tcp_errno(void);

void myclosesocket(int fd);

#if defined(_AMIGA) || defined(__AMIGAOS4__) || defined(__MORPHOS__)

#include <exec/execbase.h>
#include <proto/exec.h>
#include "subthreads_amiga.h"

#ifdef __AMIGAOS4__
#define SocketBase ((struct thread_s*)FindTask(NULL)->tc_UserData)->socketlib
#define AmiSSLBase ((struct thread_s*)FindTask(NULL)->tc_UserData)->amissllib
#define AmiSSLMasterBase ((struct thread_s*)FindTask(NULL)->tc_UserData)->amisslmasterlib
#define ISocket ((struct thread_s*)FindTask(NULL)->tc_UserData)->isocket
#define IAmiSSL ((struct thread_s*)FindTask(NULL)->tc_UserData)->iamissl
#define IAmiSSLMaster ((struct thread_s*)FindTask(NULL)->tc_UserData)->iamisslmaster
#else
#define SocketBase ((struct thread_s*)(((struct ExecBase*)SysBase)->ThisTask)->tc_UserData)->socketlib
#define AmiSSLBase ((struct thread_s*)(((struct ExecBase*)SysBase)->ThisTask)->tc_UserData)->amissllib
#define AmiSSLMasterBase ((struct thread_s*)(((struct ExecBase*)SysBase)->ThisTask)->tc_UserData)->amisslmasterlib
#endif
#endif

