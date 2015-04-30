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
** support.c
*/

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <exec/memory.h>
#include <libraries/iffparse.h> /* MAKE_ID */
#include <mui/BetterString_mcc.h>
#include <mui/NListview_mcc.h>
#include <mui/NListtree_mcc.h>
#include <libraries/locale.h>
#include <dos/dostags.h>
#include <clib/alib_protos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/utility.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/asl.h>

#ifdef __AMIGAOS4__
#include <clib/debug_protos.h>
#endif

#include "codesets.h"
#include "configuration.h"
#include "debug.h"
#include "folder.h"

#include "amigasupport.h"
#include "errorwnd.h"
#include "foldertreelistclass.h"
#include "muistuff.h"
#include "pgp.h"
#include "pgplistclass.h"
#include "smintl.h"
#include "subthreads.h"
#include "support.h"
#include "support_indep.h"

void loop(void); /* gui_main.c */

struct Library *OpenLibraryInterface(STRPTR name, int version, void *interface_ptr);
void CloseLibraryInterface(struct Library *lib, void *interface);

#ifndef __AMIGAOS4__
void kprintf(char *, ...);
#endif

/*****************************************************************************/

int sm_makedir(char *path)
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

/******************************************************************
 Returns the full path of a selected file.
******************************************************************/
char *sm_request_file(char *title, char *path, int save, char *extension)
{
	char *rc = NULL;
	struct FileRequester *fr;

	char *aosext;

	if (extension)
	{
		if (!(aosext = malloc(strlen(extension)+10)))
			return NULL;
		strcpy(aosext,"#?");
		strcat(aosext,extension);
	} else aosext = NULL;

	if ((fr = AllocAslRequestTags(ASL_FileRequest,
				TAG_DONE)))
	{
		if (AslRequestTags(fr,
			ASLFR_InitialDrawer, path,
			aosext?ASLFR_InitialPattern:TAG_IGNORE, aosext,
			aosext?ASLFR_DoPatterns:TAG_IGNORE, TRUE,
			ASLFR_DoSaveMode, save,
			TAG_DONE))
		{
			int len = strlen(fr->fr_File) + strlen(fr->fr_Drawer) + 5;

			if ((rc = malloc(len)))
			{
				strcpy(rc, fr->fr_Drawer);
				if(!AddPart(rc, fr->fr_File, len) || !strlen(fr->fr_File))
				{
					free(rc);
					rc = NULL;
				}
			}
		}

		FreeAslRequest(fr);
	}

	return rc;
}

/******************************************************************
 Opens a requester. Returns 0 if the rightmost gadgets is pressed
 otherwise the position of the gadget from left to right
*******************************************************************/
int sm_request(char *title, char *text, char *gadgets, ...)
{
	int rc;
	char *text_buf;
	va_list ap;

  extern int vsnprintf(char *buffer, size_t buffersize, const char *fmt0, va_list ap);

	if (!(text_buf = malloc(2048)))
		return 0;

  va_start(ap, gadgets);
  vsnprintf(text_buf, 2048, text, ap);
  va_end(ap);

	if (MUIMasterBase)
	{
		rc = MUI_RequestA(App, NULL, 0, title, gadgets, text_buf, NULL);
	} else
	{
		struct EasyStruct es;
		memset(&es,0,sizeof(es));
		es.es_StructSize = sizeof(es);
		es.es_Title = title?title:"SimpleMail";
		es.es_TextFormat = text_buf;
		es.es_GadgetFormat = gadgets;

		rc = EasyRequestArgs(NULL,&es,NULL,NULL);
	}

	free(text_buf);
	return rc;
}

/******************************************************************
 Opens a requester to enter a string. Returns NULL on error.
 Otherwise the malloc()ed string
*******************************************************************/
char *sm_request_string(char *title, char *text, char *contents, int secret)
{
	char *ret = NULL;
	Object *string;
	Object *wnd;

	wnd = WindowObject,
		MUIA_Window_Title, title?title:"SimpleMail",
		MUIA_Window_ID, MAKE_ID('S','R','E','Q'),
		WindowContents, VGroup,
			Child, TextObject, MUIA_Text_PreParse, "\033c", MUIA_Text_Contents, text, End,
			Child, string = BetterStringObject,
				StringFrame,
				MUIA_String_Secret, secret,
				MUIA_String_Contents, contents,
				End,
			End,
		End;

	if (wnd)
	{
		ULONG cancel=0;
		DoMethod(App, OM_ADDMEMBER, (ULONG)wnd);
		DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
		DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)App, 3, MUIM_WriteLong, 1, (ULONG)&cancel);
		DoMethod(string, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

		set(wnd,MUIA_Window_Open,TRUE);
		set(wnd,MUIA_Window_ActiveObject,string);
		loop();

		if (!cancel)
		{
			ret = mystrdup((char*)xget(string,MUIA_String_Contents));
		}
		DoMethod(App, OM_REMMEMBER, (ULONG)wnd);
		MUI_DisposeObject(wnd);
	}
	return ret;
}

/******************************************************************
 Opens a requester to enter a user id and a passwort. Returns 1 on
 success, else 0. The strings are filled in a previously alloced
 login and password buffer but not more than len bytes
*******************************************************************/
int sm_request_login(char *text, char *login, char *password, int len)
{
	int ret = 0;
	Object *login_string;
	Object *pass_string;
	Object *wnd;
	Object *ok_button, *cancel_button;

	char buf[256];
	sm_snprintf(buf,sizeof(buf),_("Please enter the login for %s"),text);

	wnd = WindowObject,
		MUIA_Window_Title, "SimpleMail - Login",
		MUIA_Window_ID, MAKE_ID('S','L','O','G'),
		WindowContents, VGroup,
			Child, TextObject, MUIA_Text_PreParse, "\033c", MUIA_Text_Contents, buf, End,
			Child, HGroup,
				Child, MakeLabel(_("Login/UserID")),
				Child, login_string = BetterStringObject,
					StringFrame,
					MUIA_String_Contents, login,
					MUIA_String_AdvanceOnCR, TRUE,
					MUIA_CycleChain, 1,
					End,
				End,
			Child, HGroup,
				Child, MakeLabel(_("Password")),
				Child, pass_string = BetterStringObject,
					StringFrame,
					MUIA_String_Secret, 1,
					MUIA_CycleChain, 1,
					End,
				End,
			Child, HorizLineObject,
			Child, HGroup,
				Child, ok_button = MakeButton(_("_Ok")),
				Child, cancel_button = MakeButton(_("_Cancel")),
				End,
			End,
		End;

	if (wnd)
	{
		ULONG cancel=0;
		DoMethod(App, OM_ADDMEMBER, (ULONG)wnd);
		DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
		DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)App, 3, MUIM_WriteLong, 1, (ULONG)&cancel);
		DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
		DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 3, MUIM_WriteLong, 1, (ULONG)&cancel);
		DoMethod(pass_string, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
		DoMethod(ok_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

		set(wnd,MUIA_Window_Open,TRUE);
		set(wnd,MUIA_Window_ActiveObject,(*login)?pass_string:login_string);
		loop();

		if (!cancel)
		{
			mystrlcpy(login, (char*)xget(login_string,MUIA_String_Contents), len);
			mystrlcpy(password, (char*)xget(pass_string,MUIA_String_Contents), len);
			ret = 1;
		}
		DoMethod(App, OM_REMMEMBER, (ULONG)wnd);
		MUI_DisposeObject(wnd);
	}
	return ret;
}

/******************************************************************
 Returns a malloc()ed string of a selected pgp. NULL for cancel
 or an error
*******************************************************************/
char *sm_request_pgp_id(char *text)
{
	char *ret = NULL;
	Object *pgp_list;
	Object *wnd;
	Object *ok_button, *cancel_button;

	wnd = WindowObject,
		MUIA_Window_Title, _("SimpleMail - Select PGP Key"),
		MUIA_Window_ID, MAKE_ID('S','R','P','G'),
		WindowContents, VGroup,
			Child, TextObject, MUIA_Text_PreParse, "\033c", MUIA_Text_Contents, text, End,
			Child, NListviewObject,
				MUIA_NListview_NList, pgp_list = PGPListObject,
					End,
				End,
			Child, HorizLineObject,
			Child, HGroup,
				Child, ok_button = MakeButton(_("_Ok")),
				Child, HVSpace,
				Child, cancel_button = MakeButton(_("_Cancel")),
				End,
			End,
		End;

	if (wnd)
	{
		ULONG cancel=0;
		DoMethod(App, OM_ADDMEMBER, (ULONG)wnd);
		DoMethod(pgp_list, MUIM_PGPList_Refresh);

		DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
		DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)App, 3, MUIM_WriteLong, 1, (ULONG)&cancel);
		DoMethod(pgp_list, MUIM_Notify, MUIA_NList_DoubleClick, TRUE, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
		DoMethod(ok_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
		DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
		DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 3, MUIM_WriteLong, 1, (ULONG)&cancel);

		set(wnd,MUIA_Window_Open,TRUE);
		loop();

		if (!cancel)
		{
			struct pgp_key *key;
			DoMethod(pgp_list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, (ULONG)&key);
			if (key)
			{
				ret = malloc(16);
				sprintf(ret,"0x%08X",key->keyid);
			}
		}
		DoMethod(App, OM_REMMEMBER, (ULONG)wnd);
		MUI_DisposeObject(wnd);
	}
	return ret;
}

/******************************************************************
 Returns a selected folder,
*******************************************************************/
struct folder *sm_request_folder(char *text, struct folder *exclude)
{
	struct folder *selected_folder = NULL;
	Object *folder_tree;
	Object *wnd;
	Object *ok_button, *cancel_button;

	wnd = WindowObject,
		MUIA_Window_Title, _("SimpleMail - Select a Folder"),
		MUIA_Window_ID, MAKE_ID('S','R','F','O'),
		WindowContents, VGroup,
			Child, TextObject, MUIA_Text_PreParse, "\033c", MUIA_Text_Contents, text, End,
			Child, NListviewObject,
				MUIA_NListview_NList, folder_tree = FolderTreelistObject,
					MUIA_CycleChain, 1,
					End,
				End,
			Child, HorizLineObject,
			Child, HGroup,
				Child, ok_button = MakeButton(_("_Ok")),
				Child, HVSpace,
				Child, cancel_button = MakeButton(_("_Cancel")),
				End,
			End,
		End;

	if (wnd)
	{
		ULONG cancel=0;
		DoMethod(App, OM_ADDMEMBER, (ULONG)wnd);
		DoMethod(folder_tree, MUIM_FolderTreelist_Refresh, (ULONG)exclude);

		DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
		DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)App, 3, MUIM_WriteLong, 1, (ULONG)&cancel);
		DoMethod(folder_tree, MUIM_Notify, MUIA_NListtree_DoubleClick, MUIV_EveryTime, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
		DoMethod(ok_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
		DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
		DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 3, MUIM_WriteLong, 1, (ULONG)&cancel);

		set(wnd,MUIA_Window_Open,TRUE);
		set(wnd,MUIA_Window_ActiveObject,folder_tree);
		loop();

		if (!cancel)
		{
			struct MUI_NListtree_TreeNode *tree_node;
			tree_node = (struct MUI_NListtree_TreeNode *)xget(folder_tree,MUIA_NListtree_Active);

			if (tree_node)
			{
				if (tree_node->tn_User)
				{
					selected_folder = (struct folder*)tree_node->tn_User;
				}
			}
		}
		DoMethod(App, OM_REMMEMBER, (ULONG)wnd);
		MUI_DisposeObject(wnd);
	}
	return selected_folder;
}

/******************************************************************
 Shows an ASCII file on the screen
*******************************************************************/
void sm_show_ascii_file(char *folder, char *filename)
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
	if (!(buf = malloc(len+40)))
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

/******************************************************************
 Get environment variables
*******************************************************************/
char *sm_getenv(char *name)
{
	static char buf[2048];
	if (GetVar(name,buf,sizeof(buf),0) < 0) return NULL;
	return buf;
}

/******************************************************************
 Set environment variables
*******************************************************************/
void sm_setenv(char *name, char *value)
{
	SetVar(name,value,strlen(value),0);
}

/******************************************************************
 Unset environment variables
*******************************************************************/
void sm_unsetenv(char *name)
{
	DeleteVar(name,GVF_LOCAL_ONLY);
}

/******************************************************************
 An system() replacement
*******************************************************************/
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

/**
 * Checks whether a file is in the given drawer. Returns 1 for
 * success. The given arguments may be absolute or relative.
 *
 * @param filename
 * @param path
 * @return
 */
int sm_file_is_in_drawer(char *filename, char *path)
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

/**
 * Checks whether the given paths represent the same resource.
 *
 * @param path1
 * @param path2
 * @return
 */
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

/******************************************************************
 Parse a given Pattern to be used in sm_match_pattern()
*******************************************************************/
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

/******************************************************************
 Matches a pattern (from sm_parse_pattern()) against an string.
*******************************************************************/
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

/******************************************************************
 Like sprintf() but buffer overrun safe
*******************************************************************/
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

/******************************************************************
 Used for logging
*******************************************************************/
void sm_put_on_serial_line(char *txt)
{
#ifdef CLIB_DEBUG_PROTOS_H
	char c;
	while ((c = *txt++))
	{
		kputc(c);
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

/******************************************************************
 Tells an error message
*******************************************************************/
void tell_str(const char *str)
{
	error_add_message(_(str));
}

/******************************************************************
 Tells an error message from a subtask
*******************************************************************/
void tell_from_subtask(const char *str)
{
	thread_call_parent_function_sync(NULL,tell_str,1,str);
}


#undef _

#ifndef NO_SSL

#ifdef USE_OPENSSL
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
/******************************************************************
 Decodes an pkcs7...API is unfinished! This is a temp solution.
*******************************************************************/
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
			unsigned char *p = (unsigned char*)buf;
			PKCS7 *pkcs7;

			if ((pkcs7 = d2i_PKCS7(NULL, &p, len)))
			{
				PKCS7 *pkcs7_data = pkcs7_get_data(pkcs7,AmiSSLBase,IAmiSSL);
				if (pkcs7_data)
				{
					char *mem = malloc(pkcs7_data->d.data->length+1);
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
