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
** socket-stubs.h
*/

#ifndef SM__SOCKETSTUBS_H
#define SM__SOCKETSTUBS_H

LONG stub_socket(LONG domain, LONG type, LONG protocol);
LONG stub_bind(LONG s, const void *name, LONG namelen);
LONG stub_listen(LONG s, LONG backlog);
LONG stub_accept(LONG s, void *addr, LONG *addrlen);
LONG stub_connect(LONG s, const void *name, LONG namelen);
LONG stub_send(LONG s, const UBYTE *msg, LONG len, LONG flags);
LONG stub_sendto(LONG s, const UBYTE *msg, LONG len, LONG flags, const void *to, LONG tolen);
LONG stub_sendmsg(LONG s, void *msg, LONG flags);
LONG stub_recv(LONG s, UBYTE *buf, LONG len, LONG flags);
LONG stub_recvfrom(LONG s, UBYTE *buf, LONG len, LONG flags, void *from, LONG *fromlen);
LONG stub_recvmsg(LONG s, void *msg, LONG flags);
LONG stub_shutdown(LONG s, LONG how);
LONG stub_setsockopt(LONG s, LONG level, LONG optname, const void *optval, LONG optlen);
LONG stub_getsockopt(LONG s, LONG level, LONG optname, void *optval, LONG *optlen);
LONG stub_getsockname(LONG s, void *name, LONG *namelen);
LONG stub_getpeername(LONG s, void *name, LONG *namelen);

LONG stub_IoctlSocket(LONG d, ULONG request, char *argp);
LONG stub_CloseSocket(LONG d);
LONG stub_WaitSelect(LONG nfds, void *readfds, void *writefds, void *exeptfds, void *timeout, ULONG *maskp);

LONG stub_Dup2Socket(LONG fd1, LONG fd2);

LONG stub_getdtablesize(void);
void stub_SetSocketSignals(ULONG _SIGINTR, ULONG _SIGIO, ULONG _SIGURG);
LONG stub_SetErrnoPtr(void *errno_p, LONG size);
LONG stub_SocketBaseTagList(void *tagList);

LONG stub_GetSocketEvents(ULONG *eventmaskp);

LONG stub_Errno(void);

LONG stub_gethostname(STRPTR hostname, LONG size);
ULONG stub_gethostid(void);

LONG stub_ObtainSocket(LONG id, LONG domain, LONG type, LONG protocol);
LONG stub_ReleaseSocket(LONG fd, LONG id);
LONG stub_ReleaseCopyOfSocket(LONG fd, LONG id);

ULONG stub_inet_addr(const UBYTE *);
ULONG stub_inet_network(const UBYTE *);
char *stub_Inet_NtoA(ULONG s_addr);
ULONG stub_Inet_MakeAddr(ULONG net, ULONG lna);
ULONG stub_Inet_LnaOf(LONG s_addr);
ULONG stub_Inet_NetOf(LONG s_addr);

void *stub_gethostbyname(const UBYTE *name);
void *stub_gethostbyaddr(const UBYTE *addr, LONG len, LONG type);
void *stub_getnetbyname(const UBYTE *name);
void *stub_getnetbyaddr(LONG net, LONG type);
void *stub_getservbyname(const UBYTE *name, const UBYTE *proto);
void *stub_getservbyport(LONG port, const UBYTE *proto);
void *stub_getprotobyname(const UBYTE *name);
void *stub_getprotobynumber(LONG proto);

#endif /* SM__SOCKETSTUBS_H */
