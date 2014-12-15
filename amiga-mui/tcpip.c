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

#ifndef __USE_OLD_TIMEVAL__
#define __USE_OLD_TIMEVAL__
#endif

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
#ifdef __MORPHOS__
#define USE_INLINE_STDARG
#endif
#if defined(USE_OPENSSL)
#include <openssl/ssl.h>
#elif defined(USE_AMISSL3)
#include <libraries/amisslmaster.h>
#include <proto/amisslmaster.h>
#else
#include <proto/amissl.h>
#endif
#endif

#include "ssl.h"
#include "tcpip.h"
#include "subthreads_amiga.h"

#ifdef __MORPHOS__
#ifdef SocketBase
#undef SocketBase
#endif
#define SocketBase ((struct thread_s*)FindTask(NULL)->tc_UserData)->socketlib
#endif

struct Library *OpenLibraryInterface(STRPTR name, int version, void *interface_ptr);
void CloseLibraryInterface(struct Library *lib, void *interface);

/**
 * Opens the socket lib. Must be done for every thread which want to use
 * bsd socket functions.
 */
int open_socket_lib(void)
{
	struct thread_s *thread;

	SM_ENTER;

	if (!(thread = (struct thread_s*)FindTask(NULL)->tc_UserData))
		goto out;

	if (!thread->socketlib)
	{
		if ((thread->socketlib = OpenLibrary("bsdsocket.library", 4)))
		{
#ifdef __AMIGAOS4__
			if ((thread->isocket = (struct SocketIFace*)GetInterface(thread->socketlib,"main",1,NULL)))
			{
#endif
				thread->socketlib_opencnt = 1;
				SM_DEBUGF(10,("Socket library opened %ld times\n",thread->socketlib_opencnt));
				SM_RETURN(1,"%ld");
				return 1;
#ifdef __AMIGAOS4__
			}
			CloseLibrary(thread->socketlib);
#endif
		}
	} else
	{
		thread->socketlib_opencnt++;
		SM_DEBUGF(10,("Socket library opened %ld times\n",thread->socketlib_opencnt));
		SM_RETURN(1,"%ld");
		return 1;
	}
out:
	SM_RETURN(0,"%ld");
	return 0;
}

/**
 * Close the socket lib.
 */
void close_socket_lib(void)
{
	struct thread_s *thread;

	SM_ENTER;

	if (!(thread = (struct thread_s*)FindTask(NULL)->tc_UserData))
		goto out;

	SM_DEBUGF(10,("Socket library opened %ld times\n",thread->socketlib_opencnt-1));

	if (!(--thread->socketlib_opencnt))
	{
#ifdef __AMIGAOS4__
		if (thread->isocket)
		{
			DropInterface((struct Interface*)thread->isocket);
			thread->isocket = NULL;
		}
#endif
		if (thread->socketlib)
		{
			CloseLibrary(thread->socketlib);
			thread->socketlib = NULL;
		}
	}

out:
	SM_LEAVE;
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

#ifdef USE_AMISSL3

/**
 * Close the AmiSSL3 library for the given thread.
 *
 * @param thread
 * @return
 */
static void close_amissl3(struct thread_s *thread)
{
	CleanupAmiSSL(TAG_DONE);
#ifdef __AMIGAOS4__
	DropInterface((struct Interface*)thread->iamissl);
	thread->iamissl = NULL;
#endif
	/* FIXME: Closing amissl will lead to crashes on subsequent calls to socket()
	 * at least on OS4. Therefore, this is disabled when compiling for this
	 * operating system. */
#ifndef __AMIGAOS4__
	CloseAmiSSL();
#endif
	thread->amissllib = NULL;
	CloseLibraryInterface(thread->amisslmasterlib,thread->iamisslmaster);
	thread->iamisslmaster = NULL;
	thread->amisslmasterlib = NULL;

	CloseLibraryInterface(thread->amissllib,thread->iamissl);
	thread->amissllib = NULL;
	thread->iamissl = NULL;
}

/**
 * Open the AmiSSL3 library for the given thread.
 *
 * @param thread
 * @return 1 on success, 0 on failure.
 */
static int open_amissl3(struct thread_s *thread)
{
	thread->amisslmasterlib = NULL;
	thread->amissllib = NULL;

#ifdef __AMIGAOS4__
	thread->iamisslmaster = NULL;
	thread->iamissl =  NULL;
#endif

	if (!(thread->amisslmasterlib = OpenLibraryInterface("amisslmaster.library",AMISSLMASTER_MIN_VERSION, &thread->iamisslmaster)))
		goto bailout;

	if (!InitAmiSSLMaster(AMISSL_CURRENT_VERSION, TRUE))
		goto bailout;

	if (!(thread->amissllib = OpenAmiSSL()))
		goto bailout;

#ifdef __AMIGAOS4__
	if (!(thread->iamissl = (struct AmiSSLIFace *)GetInterface(thread->amissllib,"main",1,NULL)))
		goto bailout;
#endif
	if (InitAmiSSL(AmiSSL_SocketBase, (ULONG)SocketBase, TAG_DONE) != 0)
		goto bailout;

	return 1;
bailout:
	close_amissl3(thread);
	return 0;
}

#endif

int open_ssl_lib(void)
{
#ifdef NO_SSL
	return 0;
#else
	struct thread_s *thread;

	SM_ENTER;

	if (!(thread = (struct thread_s*)FindTask(NULL)->tc_UserData))
		goto out;
	if (!open_socket_lib())
		goto out;
#ifndef USE_OPENSSL
	if (!thread->amissllib)
	{

#ifdef USE_AMISSL3
		if (open_amissl3(thread))
		{
#else
		SM_DEBUGF(10,("Open amissl.library\n"));
		if ((thread->amissllib = OpenLibraryInterface("amissl.library",1,&thread->iamissl)))
		{
			if (!InitAmiSSL(AmiSSL_Version,
					AmiSSL_CurrentVersion,
					AmiSSL_Revision, AmiSSL_CurrentRevision,
					AmiSSL_SocketBase, (ULONG)SocketBase,
					/*	AmiSSL_VersionOverride, TRUE,*/ /* If you insist */
					TAG_DONE))
			{
#endif
#endif
				if ((thread->ssl_ctx = ssl_create_context()))
				{
					/* Everything is ok */
					thread->ssllib_opencnt = 1;
					SM_DEBUGF(10,("AmiSSL opened %ld times\n",thread->ssllib_opencnt));
					SM_RETURN(1,"%ld");
					return 1;
				}
#ifndef USE_OPENSSL
#ifdef USE_AMISSL3
			close_amissl3(thread);
		}
#else
				CleanupAmiSSL(TAG_DONE);
			}
			CloseLibraryInterface(thread->amissllib,thread->iamissl);
			thread->iamissl = NULL;
			thread->amissllib = NULL;
		}
#endif

	} else
	{
		thread->ssllib_opencnt++;
		SM_DEBUGF(10,("AmiSSL opened %ld times\n",thread->ssllib_opencnt));
		SM_RETURN(1,"%ld");
		return 1;
	}
#else /* USE_OPENSSL */
	return 1;
#endif
out:
	close_socket_lib();
	SM_RETURN(0,"%ld");
	return 0;
#endif
}

void close_ssl_lib(void)
{
	struct thread_s *thread;

	SM_ENTER;

	if (!(thread = (struct thread_s*)FindTask(NULL)->tc_UserData))
		goto out;

#ifndef NO_SSL
	if (!thread->ssllib_opencnt)
	{
		SM_LEAVE;
		return;
	}

	SM_DEBUGF(10,("AmiSSL opened %ld times\n",thread->ssllib_opencnt - 1));

	if (!(--thread->ssllib_opencnt))
	{
		SSL_CTX_free(thread->ssl_ctx);
		thread->ssl_ctx = NULL;

#ifndef USE_OPENSSL
#ifdef USE_AMISSL3
		close_amissl3(thread);
#else
		CleanupAmiSSL(TAG_DONE);
		CloseLibraryInterface(thread->amissllib,thread->iamissl);
		thread->amissllib = NULL;
		thread->iamissl = NULL;
#endif
#endif
		close_socket_lib();
	}
#endif
out:
	SM_LEAVE;
}

#ifndef NO_SSL
SSL_CTX *ssl_context(void)
{
	struct thread_s *thread = (struct thread_s*)FindTask(NULL)->tc_UserData;
	if (!thread) return NULL; /* assert */

	SM_DEBUGF(20,("ssl context = %p\n",thread->ssl_ctx));
	return thread->ssl_ctx;
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
	long rc = CloseSocket(fd);
	if (rc != 0)
		SM_DEBUGF(5,("Closing socket %ld failed\n",fd));
	else
		SM_DEBUGF(20,("Closing socket %ld succeeded\n",fd));
}

#if defined(__amigaos4__) && defined(USE_OPENSSL)
/* Temporarily needed for the static openssl link lib */
#undef shutdown
int shutdown(int sockfd, int how)
{
	return ISocket->shutdown(sockfd, how);
}

#endif
