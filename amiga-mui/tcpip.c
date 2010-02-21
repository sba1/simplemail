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
** tcpip.c
*/

#define __USE_OLD_TIMEVAL__

#include "debug.h"

#include <proto/exec.h>

#ifdef __MORPHOS__
	#include <sys/socket.h>
	#include <net/socketbasetags.h>
	#include <proto/socket.h>
#elif __AROS__
	#include <bsdsocket/socketbasetags.h>
	#include <proto/bsdsocket.h>
#else
	#ifdef AMITCP_SDK
		#include <amitcp/socketbasetags.h>
	#else
		#ifdef ROADSHOW_SDK
			#include <libraries/bsdsocket.h>
			#include <proto/bsdsocket.h>
		#else
			#include <bsdsocket/socketbasetags.h>
			#include <clib/miami_protos.h>
			#include <pragmas/miami_pragmas.h>
		#endif
	#endif
#endif

#ifndef NO_SSL
#ifdef USE_AMISSL3
#include <libraries/amisslmaster.h>
#include <proto/amisslmaster.h>
#endif
#include <proto/amissl.h>
#endif

#include "tcpip.h"
#include "subthreads_amiga.h"

#ifdef __MORPHOS__
#ifdef SocketBase
#undef SocketBase
#endif
#define SocketBase ((struct thread_s*)FindTask(NULL)->tc_UserData)->socketlib
#else
/* calling FindTask(NULL) below makes problems when compiling */
#define SocketBase ((struct thread_s*)(((struct ExecBase*)SysBase)->ThisTask)->tc_UserData)->socketlib
#define ISocket ((struct thread_s*)(((struct ExecBase*)SysBase)->ThisTask)->tc_UserData)->isocket
#endif

struct Library *OpenLibraryInterface(STRPTR name, int version, void *interface_ptr);
void CloseLibraryInterface(struct Library *lib, void *interface);

/**
 * Opens the socket lib. Must be done for every thread which want to use
 * bsd socket functions.
 */
int open_socket_lib(void)
{
	struct thread_s *thread = (struct thread_s*)FindTask(NULL)->tc_UserData;
	if (!thread) return 0; /* assert */

	if (!thread->socketlib)
	{
		if ((thread->socketlib = OpenLibrary("bsdsocket.library", 4)))
		{
#ifdef __AMIGAOS4__
			if ((thread->isocket = (struct SocketIFace*)GetInterface(thread->socketlib,"main",1,NULL)))
			{
#endif
				thread->socketlib_opencnt = 1;
				return 1;
#ifdef __AMIGAOS4__
			}
			CloseLibrary(thread->socketlib);
#endif
		}
	} else
	{
		thread->socketlib_opencnt++;
		return 1;
	}
	return 0;
}

/**
 * Close the socket lib.
 */
void close_socket_lib(void)
{
	struct thread_s *thread = (struct thread_s*)FindTask(NULL)->tc_UserData;
	if (!thread) return; /* assert */

	if (!(--thread->socketlib_opencnt))
	{
#ifdef __AMIGAOS4__
		if (thread->isocket) DropInterface((struct Interface*)thread->isocket);
#endif
		if (thread->socketlib)
		{
			CloseLibrary(thread->socketlib);
			thread->socketlib = NULL;
		}
	}
}

/* Returns true if given interface is online, if the information is not querryable
   we will return 1 */
int is_online(char *iface)
{
#if defined(AMITCP_SDK) || defined(ROADSHOW_SDK) || defined(__AROS__)
	return 1;
#else
	struct Library *MiamiBase = OpenLibrary("miami.library",10); /* required by MiamiIsOnline() */
	int rc = 1;
	if (MiamiBase)
	{
		rc = MiamiIsOnline(iface);
		CloseLibrary(MiamiBase);
	}
	return rc;
#endif
}

#ifndef NO_SSL
struct Library *AmiSSLBase;
#ifdef __AMIGAOS4__
struct AmiSSLIFace *IAmiSSL;
#else
void *IAmiSSL;
#endif

#ifdef USE_AMISSL3
struct Library *AmiSSLMasterBase;
struct AmiSSLMasterIFace *IAmiSSLMaster;
#endif

static int ssl_in_use;
static SSL_CTX *ctx;
#endif

int open_ssl_lib(void)
{
#ifdef NO_SSL
	return 0;
#else
	if (!open_socket_lib()) return 0;

	if (!AmiSSLBase)
	{
#ifdef USE_AMISSL3

		if ((AmiSSLMasterBase = OpenLibraryInterface("amisslmaster.library",AMISSLMASTER_MIN_VERSION, &IAmiSSLMaster)))
		{
			if (InitAmiSSLMaster(AMISSL_CURRENT_VERSION, TRUE))
			{
				if ((AmiSSLBase = OpenAmiSSL()))
				{
					if ((IAmiSSL = (struct AmiSSLIFace *)GetInterface(AmiSSLBase,"main",1,NULL)))
					{
						if (InitAmiSSL(AmiSSL_SocketBase, (ULONG)SocketBase, TAG_DONE) == 0)
						{

#else
		if ((AmiSSLBase = OpenLibraryInterface("amissl.library",1,&IAmiSSL)))
		{
			if (!InitAmiSSL(AmiSSL_Version,
					AmiSSL_CurrentVersion,
					AmiSSL_Revision, AmiSSL_CurrentRevision,
					AmiSSL_SocketBase, (ULONG)SocketBase,
					/*	AmiSSL_VersionOverride, TRUE,*/ /* If you insist */
					TAG_DONE))
			{
#endif
				SSLeay_add_ssl_algorithms();
				SSL_load_error_strings();

				if ((ctx = SSL_CTX_new(SSLv23_client_method())))
				{
					/* Everything is ok */
					ssl_in_use++;
					return 1;
				}
#ifdef USE_AMISSL3
							CleanupAmiSSL(TAG_DONE);
						}
						DropInterface((struct Interface*)IAmiSSL);
					}
					CloseAmiSSL();
					AmiSSLBase = NULL;
				}
			}
			CloseLibraryInterface(AmiSSLMasterBase,AmiSSLMasterBase);
			AmiSSLMasterBase = NULL;
		}

#else
				CleanupAmiSSL(TAG_DONE);
			}
			CloseLibraryInterface(AmiSSLBase,IAmiSSL);
			AmiSSLBase = NULL;
		}
#endif

	} else
	{
		ssl_in_use++;
		return 1;
	}

	close_socket_lib();
	return 0;
#endif
}

void close_ssl_lib(void)
{
#ifndef NO_SSL
	if (!ssl_in_use) return;
	if (!(--ssl_in_use))
	{
		SSL_CTX_free(ctx);
		ctx = NULL;
		CleanupAmiSSL(TAG_DONE);
		CloseLibrary(AmiSSLBase);
		AmiSSLBase = NULL;
		close_socket_lib();
	}
#endif
}

#ifndef NO_SSL
SSL_CTX *ssl_context(void)
{
	return ctx;
}
#endif

long tcp_herrno(void)
{
	long id;

#if defined(_DCC) || defined(__MORPHOS__)
	struct TagItem tags[2]={{0, 0}, {TAG_DONE, 0}};
	tags[0].ti_Tag=(ULONG)SBTM_GETREF(SBTC_HERRNO);
	tags[0].ti_Data=(ULONG)&id;

	if(SocketBaseTagList(tags) != 0)
#else
	if(SocketBaseTags(
		SBTM_GETREF(SBTC_HERRNO), (ULONG)&id,
	TAG_DONE) != 0)
#endif
	{
		id = -1;
	}

	return id;
}

long tcp_errno(void)
{
	return Errno();
}

void myclosesocket(int fd)
{
	CloseSocket(fd);
}
