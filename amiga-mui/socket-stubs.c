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
** socket-stubs.c
*/

#include <sys/types.h>

#include <proto/exec.h>
#include <proto/socket.h>
#include <exec/execbase.h>

#include "socket-stubs.h"
#include "subthreads_amiga.h"

#undef SocketBase
#define SocketBase ((struct thread_s*)(((struct ExecBase*)SysBase)->ThisTask)->tc_UserData)->socketlib

LONG stub_socket(LONG domain, LONG type, LONG protocol)
{
	return socket(domain, type, protocol);
}

LONG stub_bind(LONG s, const void *name, LONG namelen)
{
	return bind(s, name, namelen);
}

LONG stub_listen(LONG s, LONG backlog)
{
	return listen(s, backlog);
}

LONG stub_accept(LONG s, void *addr, LONG *addrlen)
{
	return accept(s, addr, addrlen);
}

LONG stub_connect(LONG s, const void *name, LONG namelen)
{
	return connect(s, name, namelen);
}

LONG stub_send(LONG s, const UBYTE *msg, LONG len, LONG flags)
{
	return send(s, msg, len, flags);
}

LONG stub_sendto(LONG s, const UBYTE *msg, LONG len, LONG flags, const void *to, LONG tolen)
{
	return sendto(s, msg, len, flags, to, tolen);
}

LONG stub_sendmsg(LONG s, void *msg, LONG flags)
{
	return sendmsg(s, msg, flags);
}

LONG stub_recv(LONG s, UBYTE *buf, LONG len, LONG flags)
{
	return recv(s, buf, len, flags);
}

LONG stub_recvfrom(LONG s, UBYTE *buf, LONG len, LONG flags, void *from, LONG *fromlen)
{
	return recvfrom(s, buf, len, flags, from, fromlen);
}

LONG stub_recvmsg(LONG s, void *msg, LONG flags)
{
	return recvmsg(s, msg, flags);
}

LONG stub_shutdown(LONG s, LONG how)
{
	return shutdown(s, how);
}

LONG stub_setsockopt(LONG s, LONG level, LONG optname, const void *optval, LONG optlen)
{
	return setsockopt(s, level, optname, optval, optlen);
}

LONG stub_getsockopt(LONG s, LONG level, LONG optname, void *optval, LONG *optlen)
{
	return getsockopt(s, level, optname, optval, optlen);
}

LONG stub_getsockname(LONG s, void *name, LONG *namelen)
{
	return getsockname(s, name, namelen);
}

LONG stub_getpeername(LONG s, void *name, LONG *namelen)
{
	return getpeername(s, name, namelen);
}

LONG stub_IoctlSocket(LONG d, ULONG request, char *argp)
{
	return IoctlSocket(d, request, argp);
}

LONG stub_CloseSocket(LONG d)
{
	return CloseSocket(d);
}

LONG stub_WaitSelect(LONG nfds, void *readfds, void *writefds, void *exeptfds, void *timeout, ULONG *maskp)
{
	return WaitSelect(nfds, readfds, writefds, exeptfds, timeout, maskp);
}

LONG stub_Dup2Socket(LONG fd1, LONG fd2)
{
	return Dup2Socket(fd1, fd2);
}

LONG stub_getdtablesize(void)
{
	return getdtablesize();
}

void stub_SetSocketSignals(ULONG _SIGINTR, ULONG _SIGIO, ULONG _SIGURG)
{
	return SetSocketSignals(_SIGINTR, _SIGIO, _SIGURG);
}

LONG stub_SetErrnoPtr(void *errno_p, LONG size)
{
	return SetErrnoPtr(errno_p, size);
}

LONG stub_SocketBaseTagList(void *tagList)
{
	return SocketBaseTagList(tagList);
}

LONG stub_GetSocketEvents(ULONG *eventmaskp)
{
	return GetSocketEvents(eventmaskp);
}

LONG stub_Errno(void)
{
	return Errno();
}

LONG stub_gethostname(STRPTR hostname, LONG size)
{
	return gethostname(hostname, size);
}

ULONG stub_gethostid(void)
{
	return gethostid();
}

LONG stub_ObtainSocket(LONG id, LONG domain, LONG type, LONG protocol)
{
	return ObtainSocket(id, domain, type, protocol);
}

LONG stub_ReleaseSocket(LONG fd, LONG id)
{
	return ReleaseSocket(fd, id);
}

LONG stub_ReleaseCopyOfSocket(LONG fd, LONG id)
{
	return ReleaseCopyOfSocket(fd, id);
}

ULONG stub_inet_addr(const UBYTE *p)
{
	return inet_addr(p);
}

ULONG stub_inet_network(const UBYTE *p)
{
	return inet_network(p);
}

char *stub_Inet_NtoA(ULONG s_addr)
{
	return Inet_NtoA(s_addr);
}

ULONG stub_Inet_MakeAddr(ULONG net, ULONG lna)
{
	return Inet_MakeAddr(net, lna);
}

ULONG stub_Inet_LnaOf(LONG s_addr)
{
	return Inet_LnaOf(s_addr);
}

ULONG stub_Inet_NetOf(LONG s_addr)
{
	return Inet_NetOf(s_addr);
}

void *stub_gethostbyname(const UBYTE *name)
{
	return gethostbyname(name);
}

void *stub_gethostbyaddr(const UBYTE *addr, LONG len, LONG type)
{
	return gethostbyaddr(addr, len, type);
}

void *stub_getnetbyname(const UBYTE *name)
{
	return getnetbyname(name);
}

void *stub_getnetbyaddr(LONG net, LONG type)
{
	return getnetbyaddr(net, type);
}

void *stub_getservbyname(const UBYTE *name, const UBYTE *proto)
{
	return getservbyname(name, proto);
}

void *stub_getservbyport(LONG port, const UBYTE *proto)
{
	return getservbyport(port, proto);
}

void *stub_getprotobyname(const UBYTE *name)
{
	return getprotobyname(name);
}

void *stub_getprotobynumber(LONG proto)
{
	return getprotobynumber(proto);
}
