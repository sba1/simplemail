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
 * @file support.c
 */

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <exec/memory.h>
#include <libraries/locale.h>
#include <dos/dostags.h>
#include <clib/alib_protos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/utility.h>

#ifdef __AMIGAOS4__
#include <clib/debug_protos.h>
#endif

#include "codesets.h"
#include "configuration.h"
#include "debug.h"
#include "folder.h"
#include "logging.h"

#include "amigasupport.h"
#include "errorwnd.h"
#include "muistuff.h"
#include "smintl.h"
#include "subthreads.h"
#include "support.h"
#include "support_indep.h"

#ifndef __AMIGAOS4__
void kprintf(char *, ...);
#endif

/*****************************************************************************/

int sm_makedir(const char *path)
{
	int rc;
	BPTR lock = Lock(path,ACCESS_READ);
	if (lock)
	{
		UnLock(lock);
		return 1;
	}

#ifdef __AMIGAOS4__
	if ((lock = CreateDirTree(path)))
	{
		rc = 1;
		UnLock(lock);
	} else rc = 0;
#else

	if ((lock = CreateDir(path)))
	{
		UnLock(lock);
		return 1;
	}

	rc = 0;

	if (IoErr() == 205) /* Object not found */
	{
		char *buf = (char*)AllocVec(strlen(path)+1,MEMF_CLEAR|MEMF_REVERSE);
		if (buf)
		{
			char *buf2;
			char *last_buf = NULL;
			int times = 0; /* how many paths has been splitted? */

			rc = 1;

			strcpy(buf,path);

			/* split every path */
			while ((buf2 = PathPart(buf)))
			{
				/* path cannot be be splitted more */
				if (buf2 == last_buf) break;
				times++;

				/* clear the '/' sign */
				*buf2 = 0;

				/* try if directory can be created */
				if ((lock = CreateDir(buf)))
				{
					/* yes, so leave the loop */
					UnLock(lock);
					break;
				}
				if (IoErr() != 205) break;
				last_buf = buf2;
			}

			/* put back the '/' sign and create the directories */
			while(times--)
			{
				buf[strlen(buf)] = '/';

				if ((lock = CreateDir(buf))) UnLock(lock);
				else
				{
					/* an error has happened */
					rc = 0;
					break;
				}
			}

			FreeVec(buf);
		}
	}
#endif
	return rc;
}

/*****************************************************************************/

int sm_add_part(char *drawer, const char *filename, int buf_size)
{
	AddPart(drawer,(char*)filename,buf_size);
	return 1;
}

/*****************************************************************************/

char *sm_file_part_nonconst(char *filename)
{
	return (char*)FilePart(filename);
}

/*****************************************************************************/

char *sm_path_part(char *filename)
{
	return (char*)PathPart(filename);
}

/*****************************************************************************/

void sm_show_ascii_file(const char *folder, const char *filename)
{
	BPTR lock;
	int len;
	char *buf;
	char *dirname;

	if (!folder) folder = "";

	if (!(lock = Lock(folder,ACCESS_READ)))
		return;

	dirname = NameOfLock(lock);
	UnLock(lock);
	if (!dirname) return;

	len = strlen(dirname)+strlen(filename) + 6;
	if (!(buf = (char *)malloc(len+40)))
	{
		FreeVec(dirname);
		return;
	}

	strcpy(buf,"SYS:Utilities/Multiview \"");
	strcat(buf,dirname);
	AddPart(buf+27,filename,len);
	strcat(buf+27,"\"");
	sm_system(buf,NULL);
	free(buf);
	FreeVec(dirname);
}

/******************************************************************
 Play a sound (implemented in gui_main.c)
*******************************************************************/
/*
void sm_play_sound(char *filename)
{
}
*/


/*****************************************************************************/

char *sm_getenv(char *name)
{
	static char buf[2048];
	if (GetVar(name,buf,sizeof(buf),0) < 0) return NULL;
	return buf;
}

/*****************************************************************************/

void sm_setenv(char *name, char *value)
{
	SetVar(name,value,strlen(value),0);
}

/*****************************************************************************/

void sm_unsetenv(char *name)
{
	DeleteVar(name,GVF_LOCAL_ONLY);
}

/*****************************************************************************/

int sm_system(char *command, char *output)
{
	BPTR fhi,fho;
	int error = -1;

	if ((fhi = Open("NIL:", MODE_OLDFILE)))
	{
		if ((fho = Open(output?output:"NIL:", MODE_NEWFILE)))
		{
			error = SystemTags(command,
						SYS_Input, fhi,
						SYS_Output, fho,
						NP_StackSize, 50000,
						TAG_DONE);

			Close(fho);
		}
		Close(fhi);
	}
	return error;
}

/*****************************************************************************/

int sm_file_is_in_drawer(const char *filename, const char *path)
{
	BPTR dir = Lock(path,ACCESS_READ);
	int rc = 0;
	if (dir)
	{
		BPTR file = Lock(filename,ACCESS_READ);
		if (file)
		{
			BPTR parent;
			if ((parent = ParentDir(file)))
			{
				if (SameLock(parent,dir)==LOCK_SAME) rc = 1;
				UnLock(parent);
			}
			UnLock(file);
		}
		UnLock(dir);
	}
	return rc;
}

/*****************************************************************************/

int sm_is_same_path(char *path1, char *path2)
{
	int is_same;
	BPTR l1,l2;

	if (!(l1 = Lock(path1,ACCESS_READ))) return 0;
	if (!(l2 = Lock(path2,ACCESS_READ)))
	{
		UnLock(l1);
		return 0;
	}

	if (SameLock(l1,l2)==LOCK_SAME) is_same = 1;
	else is_same = 0;

	UnLock(l2);
	UnLock(l1);

	return is_same;
}

/*****************************************************************************/

char *sm_parse_pattern(utf8 *utf8_str, int flags)
{
	char *source = NULL;
	char *dest = NULL;

	if ((flags & SM_PATTERN_NOPATT))
	{
		/* only a copy of the string is needed when we are not using the patternmatching */
		dest = mystrdup((char*)utf8_str);
	} else
	{
		source = utf8tostrcreate(utf8_str, user.config.default_codeset);
		if ((flags & SM_PATTERN_SUBSTR))
		{
			int new_len = strlen(source)+5;
			char *new_source = (char *)malloc(new_len);
			if (new_source)
			{
				sm_snprintf(new_source, new_len, "#?%s#?", source);
				free(source);
				source = new_source;
			}
		}
	}

	if (source)
	{
		int dest_len = strlen(source)*2+2;
		dest = (char *)malloc(dest_len);
		if (dest)
		{
			if ((flags & SM_PATTERN_NOCASE))
			{
#ifndef __AMIGAOS4__
				/* there is a bug in ParsePatternNoCase() until V39 */
				if (DOSBase->dl_lib.lib_Version < 39)
				{
					char *conv;
					for (conv=source; *conv != '\0'; conv++)
					{
						*conv = ToUpper(*conv);
					}
				}
#endif
				if (ParsePatternNoCase(source, dest, dest_len) < 0)
				{
					free(dest);
					dest = NULL;
				}
			} else
			{
				if (ParsePattern(source, dest, dest_len) < 0)
				{
					free(dest);
					dest = NULL;
				}
			}
		}
		free(source);
	}
	return dest;
}

/*****************************************************************************/

int sm_match_pattern(char *pat, utf8 *utf8_str, int flags)
{
	char *str;
	int match = 0;

	if ((flags & SM_PATTERN_NOPATT))
	{
		if ((flags & SM_PATTERN_NOCASE))
		{
			if ((flags & SM_PATTERN_SUBSTR))
			{
				if ((flags & SM_PATTERN_ASCII7))
				{
					match = !!mystristr((char*)utf8_str, pat);
				} else
				{
					match = !!utf8stristr((char*)utf8_str, pat);
				}
			} else
			{
				if ((flags & SM_PATTERN_ASCII7))
				{
					match = !utf8stricmp((char*)utf8_str, pat);
				} else
				{
					match = !mystricmp((char*)utf8_str, pat);
				}
			}
		} else
		{
			/* no special handling needed for UTF8 */
			if ((flags & SM_PATTERN_SUBSTR))
			{
				match = !!strstr((char*)utf8_str, pat);
			} else
			{
				match = !mystrcmp((char*)utf8_str, pat);
			}
		}
		return match;
	}

	if (!(flags & SM_PATTERN_ASCII7))
	{
		str = utf8tostrcreate(utf8_str, user.config.default_codeset);
	} else
	{
		str = (char*)utf8_str;
	}

	if (pat && str)
	{
		if ((flags & SM_PATTERN_NOCASE)) match = MatchPatternNoCase(pat, str);
		else match = MatchPattern(pat, str);
	}

	if (!(flags & SM_PATTERN_ASCII7)) free(str);
	return match;
}

/*****************************************************************************/

int sm_snprintf(char *buf, int n, const char *fmt, ...)
{
  int r;

  extern int vsnprintf(char *buffer, size_t buffersize, const char *fmt0, va_list ap);

  va_list ap;

  va_start(ap, fmt);
  r = vsnprintf(buf, n, fmt, ap);
  va_end(ap);
  return r;
}

/*****************************************************************************/

void sm_put_on_serial_line(char *txt)
{
#ifdef CLIB_DEBUG_PROTOS_H
	char c;
	while ((c = *txt++))
	{
#ifdef __amigaos4__
		RawPutChar(c);
#else
		kputc(c);
#endif
	}
#else
	char buf[800];
	char c;
	int i;

	i = 0;

	while ((c = *txt++) && i < sizeof(buf)-3)
	{
		if (c == '%') buf[i++] = c;
		buf[i++] = c;
	}

	buf[i] = 0;

	kprintf(buf);
#endif
}

/*****************************************************************************/

void tell_str(const char *str)
{
	logg(ERROR, 0, NULL, NULL, 0, _(str), LAST);
}

/*****************************************************************************/

void tell_from_subtask(const char *str)
{
	thread_call_parent_function_sync(NULL,tell_str,1,str);
}


#undef _

#ifndef NO_SSL

#ifdef USE_OPENSSL
struct AmiSSLIFace {int dummy; };
#include <openssl/ssl.h>
#else

#ifdef __MORPHOS__
#define USE_INLINE_STDARG
#endif
#include <proto/amissl.h>
#ifdef USE_AMISSL3
#include <libraries/amisslmaster.h>
#include <proto/amisslmaster.h>
#endif

#ifndef __AMIGAOS4__
struct AmiSSLIFace {int dummy; };
#endif
#endif

static PKCS7 *pkcs7_get_data(PKCS7 *pkcs7, struct Library *AmiSSLBase, struct AmiSSLIFace *IAmiSSL)
{
	if (PKCS7_type_is_signed(pkcs7))
	{
		return pkcs7_get_data(pkcs7->d.sign->contents,AmiSSLBase,IAmiSSL);
	}
	if (PKCS7_type_is_data(pkcs7))
	{
		return pkcs7;
	}
	return NULL;
}
#endif

/*****************************************************************************/

int pkcs7_decode(char *buf, int len, char **dest_ptr, int *len_ptr)
{
#ifndef NO_SSL
	int rc = 0;

	struct Library *AmiSSLBase;
	struct AmiSSLIFace *IAmiSSL;

	/* TODO: use open_ssl_lib() */
#ifndef USE_OPENSSL
#ifdef USE_AMISSL3
	struct Library *AmiSSLMasterBase;
	struct AmiSSLMasterIFace *IAmiSSLMaster;

	if ((AmiSSLMasterBase = OpenLibraryInterface("amisslmaster.library",AMISSLMASTER_MIN_VERSION, &IAmiSSLMaster)))
	{
		if (InitAmiSSLMaster(AMISSL_CURRENT_VERSION, TRUE))
		{
			if ((AmiSSLBase = OpenAmiSSL()))
			{
#ifdef __AMIGAOS4__
				if ((IAmiSSL = (struct AmiSSLIFace *)GetInterface(AmiSSLBase,"main",1,NULL)))
				{
#else
				IAmiSSL = NULL; /* Keep the compiler happy */
#endif
					if (InitAmiSSL(TAG_DONE) == 0)
					{
#else
	if ((AmiSSLBase = OpenLibraryInterface("amissl.library",1,&IAmiSSL)))
	{
		if (!InitAmiSSL(AmiSSL_Version,
				AmiSSL_CurrentVersion,
				AmiSSL_Revision, AmiSSL_CurrentRevision,
				TAG_DONE))
		{
#endif
#endif

#if defined(USE_OPENSSL) || defined(__cplusplus)
			const
#endif
			unsigned char *p = (unsigned char*)buf;
			PKCS7 *pkcs7;

			if ((pkcs7 = d2i_PKCS7(NULL, &p, len)))
			{
				PKCS7 *pkcs7_data = pkcs7_get_data(pkcs7,AmiSSLBase,IAmiSSL);
				if (pkcs7_data)
				{
					char *mem = (char *)malloc(pkcs7_data->d.data->length+1);
					if (*dest_ptr)
					{
						memcpy(mem,pkcs7_data->d.data->data,pkcs7_data->d.data->length);
						mem[pkcs7_data->d.data->length]=0;
						*dest_ptr = mem;
						*len_ptr = pkcs7_data->d.data->length;
						rc = 1;
					}
				}
				PKCS7_free(pkcs7);
			}
#ifndef USE_OPENSSL
#ifdef USE_AMISSL3
						CleanupAmiSSL(TAG_DONE);
#ifdef __AMIGAOS4__
					}
					DropInterface((struct Interface*)IAmiSSL);
#endif
				}
				CloseAmiSSL();
			}
		}
		CloseLibraryInterface(AmiSSLMasterBase,AmiSSLMasterBase);
	}
#else
			CleanupAmiSSL(TAG_DONE);
		}
		CloseLibraryInterface(AmiSSLBase,IAmiSSL);
	}
#endif
#endif
	return rc;
#else
	return 0;
#endif
}
