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
** configwnd.c
*/

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <libraries/asl.h>
#include <libraries/iffparse.h> /* MAKE_ID */
#include <libraries/mui.h>
#include <mui/texteditor_mcc.h>
#include <mui/betterstring_mcc.h>
#include <mui/nlistview_mcc.h>
#include <mui/popplaceholder_mcc.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "account.h"
#include "codesets.h"
#include "configuration.h"
#include "debug.h"
#include "imap.h"
#include "lists.h"
#include "parse.h"
#include "phrase.h"
#include "pop3.h"
#include "signature.h"
#include "simplemail.h"
#include "smintl.h"
#include "spam.h"
#include "support.h"
#include "support_indep.h"

#include "audioselectgroupclass.h"
#include "compiler.h"
#include "composeeditorclass.h"
#include "configwnd.h"
#include "muistuff.h"
#include "multistringclass.h"
#include "picturebuttonclass.h"
#include "utf8stringclass.h"
#include "signaturecycleclass.h"
#include "configwnd_stuff.h"
#include "appicon.h"

void account_recv_port_update(void);
static void account_refresh_signature_cycle(void);

static Object *config_wnd;
static Object *user_dst_check;
static Object *user_folder_string;
static Object *user_charset_string;
static Object *appicon_show_cycle;
static Object *appicon_label_popph;
static Object *receive_preselection_radio;
static Object *receive_sizes_sizes;
static Object *receive_autocheck_string;
static Object *receive_autocheckifonline_check;
static Object *receive_autocheckonstartup_check;
static Object *receive_sound_check;
static Object *receive_sound_string;
static Object *receive_arexx_check;
static Object *receive_arexx_popasl;
static Object *receive_arexx_string;
static Object *read_fixedfont_string;
static Object *read_propfont_string;
static Object *read_wrap_checkbox;
static Object *read_linkunderlined_checkbox;
static Object *read_smilies_checkbox;
static Object *read_palette;
static Object *mails_readmisc_check[6];
static Object *mails_readmisc_check_group;
static Object *mails_readmisc_all_check;
static Object *mails_readmisc_additional_string;
static Object *mails_readmisc_close_after_last;
static Object *mails_readmisc_next_after_move;
static struct MUI_Palette_Entry read_palette_entries[7];

static Object *write_wordwrap_string;
static Object *write_wordwrap_cycle;
static Object *write_replywrap_check;
static Object *write_replystripsig_check;
static Object *write_replyciteemptyl_check;
static Object *write_forward_as_attachment_check;

static Object *readhtml_mail_editor;

static Object *account_account_list;
static Object *account_account_name_string;
static Object *account_user_group;
static Object *account_name_string;
static Object *account_email_string;
static Object *account_reply_string;
static Object *account_def_signature_cycle;
static Object *account_recv_type_radio;
static Object *account_recv_server_string;
static Object *account_recv_port_string;
static Object *account_recv_login_string;
static Object *account_recv_password_string;
static Object *account_recv_ask_checkbox;
static Object *account_recv_active_check;
static Object *account_recv_delete_check;
static Object *account_recv_apop_cycle;
static Object *account_recv_ssl_check;
static Object *account_recv_stls_check;
static Object *account_recv_avoid_check;
static Object *account_send_server_string;
static Object *account_send_port_string;
static Object *account_send_login_string;
static Object *account_send_password_string;
static Object *account_send_auth_check;
static Object *account_send_pop3_check;
static Object *account_send_ip_check;
static Object *account_send_secure_check;
static Object *account_add_button;
static Object *account_remove_button;

static Object *signature_use_checkbox;
static Object *signature_signature_list;
static Object *signature_texteditor;
static Object *signature_name_string;

static Object *phrase_phrase_list;
static Object *phrase_addresses_string;
static Object *phrase_write_welcome_string;
static Object *phrase_write_welcomeaddr_popph;
static Object *phrase_write_close_string;
static Object *phrase_reply_welcome_popph;
static Object *phrase_reply_intro_popph;
static Object *phrase_reply_close_popph;
static Object *phrase_forward_initial_popph;
static Object *phrase_forward_terminating_popph;

static Object *spam_mark_before_check;
static Object *spam_auto_check;
static Object *spam_addr_book_is_white_check;
static Object *spam_white_list_editor;
static Object *spam_black_list_editor;
static Object *spam_reset_ham_stat_button;
static Object *spam_reset_spam_stat_button;
static Object *spam_spam_text;
static Object *spam_ham_text;

static Object *config_group;
static Object *config_list;

static struct account *account_last_selected;
static struct phrase *phrase_last_selected;
static struct signature *signature_last_selected;

static Object *image_prefs_main_obj;
static Object *image_prefs_account_obj;
static Object *image_prefs_receive_obj;
static Object *image_prefs_write_obj;
static Object *image_prefs_read_obj;
static Object *image_prefs_readplain_obj;
static Object *image_prefs_readhtml_obj;
static Object *image_prefs_signature_obj;
static Object *image_prefs_phrase_obj;
static Object *image_prefs_spam_obj;

#define GROUPS_USER			0
#define GROUPS_ACCOUNT		1
#define GROUPS_RECEIVE 	2
#define GROUPS_WRITE			3
#define GROUPS_READMISC	4
#define GROUPS_READ			5
#define GROUPS_READHTML	6
#define GROUPS_PHRASE		7
#define GROUPS_SIGNATURE	8
#define GROUPS_SPAM			9
#define GROUPS_MAX				10

static Object *groups[GROUPS_MAX];
static Object *config_last_visisble_group;

/******************************************************************
 Stores the current setting into the last selected account
*******************************************************************/
static void account_store(void)
{
	if (account_last_selected)
	{
		/* Save the account if a server was selected */
		free(account_last_selected->account_name);
		free(account_last_selected->name);
		free(account_last_selected->email);
		free(account_last_selected->reply);
		free(account_last_selected->def_signature);
		free(account_last_selected->pop->name);
		free(account_last_selected->pop->login);
		free(account_last_selected->pop->passwd);
		free(account_last_selected->imap->name);
		free(account_last_selected->imap->login);
		free(account_last_selected->imap->passwd);
		free(account_last_selected->smtp->name);
		free(account_last_selected->smtp->auth_login);
		free(account_last_selected->smtp->auth_password);

		account_last_selected->account_name = mystrdup(getutf8string(account_account_name_string));
		account_last_selected->name = mystrdup(getutf8string(account_name_string));
		account_last_selected->email = mystrdup(getutf8string(account_email_string));
		account_last_selected->reply = mystrdup(getutf8string(account_reply_string));
		account_last_selected->def_signature = mystrdup((char*)xget(account_def_signature_cycle, MUIA_SignatureCycle_SignatureName));
		account_last_selected->recv_type = xget(account_recv_type_radio, MUIA_Radio_Active);
		account_last_selected->pop->name = mystrdup((char*)xget(account_recv_server_string, MUIA_String_Contents));
		account_last_selected->pop->login = mystrdup((char*)xget(account_recv_login_string, MUIA_String_Contents));
		account_last_selected->pop->ask = xget(account_recv_ask_checkbox,MUIA_Selected);
		account_last_selected->pop->passwd = account_last_selected->pop->ask?NULL:mystrdup((char*)xget(account_recv_password_string, MUIA_String_Contents));
		account_last_selected->pop->active = xget(account_recv_active_check, MUIA_Selected);
		account_last_selected->pop->del = xget(account_recv_delete_check, MUIA_Selected);
		account_last_selected->pop->apop = xget(account_recv_apop_cycle, MUIA_Cycle_Active);
		account_last_selected->pop->ssl = xget(account_recv_ssl_check, MUIA_Selected);
		account_last_selected->pop->stls = xget(account_recv_stls_check, MUIA_Selected);
		account_last_selected->pop->nodupl = xget(account_recv_avoid_check, MUIA_Selected);
		account_last_selected->pop->port = xget(account_recv_port_string, MUIA_String_Integer);
		account_last_selected->imap->port = xget(account_recv_port_string, MUIA_String_Integer);
		account_last_selected->imap->name = mystrdup((char*)xget(account_recv_server_string, MUIA_String_Contents));
		account_last_selected->imap->login = mystrdup((char*)xget(account_recv_login_string, MUIA_String_Contents));
		account_last_selected->imap->ask = xget(account_recv_ask_checkbox,MUIA_Selected);
		account_last_selected->imap->passwd = mystrdup((char*)xget(account_recv_password_string, MUIA_String_Contents));
		account_last_selected->imap->active = xget(account_recv_active_check, MUIA_Selected);
		account_last_selected->imap->ssl = xget(account_recv_ssl_check, MUIA_Selected);
		account_last_selected->smtp->port = xget(account_send_port_string, MUIA_String_Integer);
		account_last_selected->smtp->ip_as_domain = xget(account_send_ip_check, MUIA_Selected);
		account_last_selected->smtp->pop3_first = xget(account_send_pop3_check, MUIA_Selected);
		account_last_selected->smtp->secure = xget(account_send_secure_check, MUIA_Selected);
		account_last_selected->smtp->name = mystrdup((char*)xget(account_send_server_string, MUIA_String_Contents));
		account_last_selected->smtp->auth = xget(account_send_auth_check, MUIA_Selected);
		account_last_selected->smtp->auth_login = mystrdup((char*)xget(account_send_login_string, MUIA_String_Contents));
		account_last_selected->smtp->auth_password = mystrdup((char*)xget(account_send_password_string, MUIA_String_Contents));
	}
}

/******************************************************************
 Loads the current selected account
*******************************************************************/
static void account_load(void)
{
	struct account *account = account_last_selected;
	if (account)
	{
		nnsetutf8string(account_account_name_string,account->account_name);
		setutf8string(account_name_string,account->name);
		nnset(account_email_string, MUIA_UTF8String_Contents, account->email);
		setutf8string(account_reply_string,account->reply);
		nnset(account_def_signature_cycle, MUIA_SignatureCycle_SignatureName, account->def_signature);
		nnset(account_recv_type_radio, MUIA_Radio_Active, account->recv_type);
		nnset(account_recv_server_string, MUIA_String_Contents, account->pop->name);
		set(account_recv_port_string,MUIA_String_Integer,account->pop->port);
		setstring(account_recv_login_string,account->pop->login);
		SetAttrs(account_recv_password_string,
							MUIA_String_Contents,account->pop->passwd,
							MUIA_Disabled,account->pop->ask,
							TAG_DONE);
		nnset(account_recv_ask_checkbox, MUIA_Selected, account->pop->ask);
		setcheckmark(account_recv_active_check,account->pop->active);
		SetAttrs(account_recv_delete_check,MUIA_Selected, account->pop->del, MUIA_Disabled, account->recv_type, TAG_DONE);
		set(account_recv_apop_cycle,MUIA_Cycle_Active,account->pop->apop);
		nnset(account_recv_ssl_check,MUIA_Selected,account->pop->ssl);
		nnset(account_recv_stls_check,MUIA_Selected,account->pop->stls);
		set(account_recv_stls_check,MUIA_Disabled,!account->pop->ssl);
		setcheckmark(account_recv_avoid_check,account->pop->nodupl);
		nnset(account_send_server_string, MUIA_String_Contents, account->smtp->name);
		set(account_send_port_string,MUIA_String_Integer,account->smtp->port);
		setstring(account_send_login_string,account->smtp->auth_login);
		setstring(account_send_password_string,account->smtp->auth_password);
		/* To avoid the activation we set this with no notify and disable the gadgets ourself */
		nnset(account_send_auth_check, MUIA_Selected, account->smtp->auth);
		set(account_send_login_string, MUIA_Disabled, !account->smtp->auth);
		set(account_send_password_string, MUIA_Disabled, !account->smtp->auth);
		setcheckmark(account_send_pop3_check,account->smtp->pop3_first);
		setcheckmark(account_send_ip_check,account->smtp->ip_as_domain);
		setcheckmark(account_send_secure_check, account->smtp->secure);
	}
}

/******************************************************************
 Gets the Signature which was last selected
*******************************************************************/
static void signature_store(void)
{
	if (signature_last_selected)
	{
		char *text_buf;
		if (signature_last_selected->name) free(signature_last_selected->name);
		if (signature_last_selected->signature) free(signature_last_selected->signature);

		text_buf = (char*)DoMethod(signature_texteditor, MUIM_TextEditor_ExportText);

		signature_last_selected->name = mystrdup(getutf8string(signature_name_string));
		signature_last_selected->signature = utf8create(text_buf,user.config.default_codeset?user.config.default_codeset->name:NULL);
		if (text_buf) FreeVec(text_buf);
	}
}

/******************************************************************
 Loads the current selected signature
*******************************************************************/
static void signature_load(void)
{
	struct signature *signature = signature_last_selected;
	if (signature)
	{
		char *sign = utf8tostrcreate(signature->signature?signature->signature:"",user.config.default_codeset);
		nnsetutf8string(signature_name_string,signature->name);
		set(signature_texteditor,MUIA_TextEditor_Contents, sign);
		free(sign);
	}
}

/******************************************************************
 Gets the phrase which was last selected
*******************************************************************/
static void phrase_store(void)
{
	if (phrase_last_selected)
	{
		free(phrase_last_selected->addresses);
		free(phrase_last_selected->write_welcome);
		free(phrase_last_selected->write_welcome_repicient);
		free(phrase_last_selected->write_closing);
		free(phrase_last_selected->reply_welcome);
		free(phrase_last_selected->reply_intro);
		free(phrase_last_selected->reply_close);
		free(phrase_last_selected->forward_initial);
		free(phrase_last_selected->forward_finish);

#define UTF8(x) utf8create(x,user.config.default_codeset?user.config.default_codeset->name:"")
		phrase_last_selected->addresses = UTF8((char*)xget(phrase_addresses_string,MUIA_String_Contents));
		phrase_last_selected->write_welcome = UTF8((char*)xget(phrase_write_welcome_string,MUIA_String_Contents));
		phrase_last_selected->write_welcome_repicient = UTF8((char*)xget(phrase_write_welcomeaddr_popph,MUIA_Popph_Contents));
		phrase_last_selected->write_closing = UTF8((char*)xget(phrase_write_close_string,MUIA_String_Contents));
		phrase_last_selected->reply_welcome = UTF8((char*)xget(phrase_reply_welcome_popph,MUIA_Popph_Contents));
		phrase_last_selected->reply_intro = UTF8((char*)xget(phrase_reply_intro_popph,MUIA_Popph_Contents));
		phrase_last_selected->reply_close = UTF8((char*)xget(phrase_reply_close_popph,MUIA_Popph_Contents));
		phrase_last_selected->forward_initial = UTF8((char*)xget(phrase_forward_initial_popph,MUIA_Popph_Contents));
		phrase_last_selected->forward_finish = UTF8((char*)xget(phrase_forward_terminating_popph,MUIA_Popph_Contents));
#undef UTF8
	}
}

/******************************************************************
 Loads the current selected phrase
*******************************************************************/
static void phrase_load(void)
{
	struct phrase *phrase = phrase_last_selected;
	if (phrase)
	{
		char buf[320];
		utf8tostr(phrase->addresses, buf, sizeof(buf), user.config.default_codeset);
		nnset(phrase_addresses_string,MUIA_String_Contents, buf);

		utf8tostr(phrase->write_welcome, buf, sizeof(buf), user.config.default_codeset);
		set(phrase_write_welcome_string,MUIA_String_Contents, buf);

		utf8tostr(phrase->write_welcome_repicient, buf, sizeof(buf), user.config.default_codeset);
		set(phrase_write_welcomeaddr_popph,MUIA_Popph_Contents, buf);

		utf8tostr(phrase->write_closing, buf, sizeof(buf), user.config.default_codeset);
		set(phrase_write_close_string,MUIA_String_Contents, buf);

		utf8tostr(phrase->reply_welcome, buf, sizeof(buf), user.config.default_codeset);
		set(phrase_reply_welcome_popph,MUIA_Popph_Contents, buf);

		utf8tostr(phrase->reply_intro, buf, sizeof(buf), user.config.default_codeset);
		set(phrase_reply_intro_popph,MUIA_Popph_Contents, buf);

		utf8tostr(phrase->reply_close, buf, sizeof(buf), user.config.default_codeset);
		set(phrase_reply_close_popph,MUIA_Popph_Contents, buf);

		utf8tostr(phrase->forward_initial, buf, sizeof(buf), user.config.default_codeset);
		set(phrase_forward_initial_popph,MUIA_Popph_Contents, buf);

		utf8tostr(phrase->forward_finish, buf, sizeof(buf), user.config.default_codeset);
		set(phrase_forward_terminating_popph,MUIA_Popph_Contents, buf);
	}
}

/******************************************************************
 Use the config
*******************************************************************/
static int config_use(void)
{
	int i,j,err;
	char **internet_emails, **spam_white_emails, **spam_black_emails;

	/* check if there are any duplicate addresses */
	account_store();
	for (i=0;i<xget(account_account_list,MUIA_NList_Entries);i++)
	{
		for (j=0;j<xget(account_account_list,MUIA_NList_Entries);j++)
		{
			struct account *ac1, *ac2;
			int invalid = -1;

			if (i==j) continue;
			DoMethod(account_account_list,MUIM_NList_GetEntry, i, &ac1);
			DoMethod(account_account_list,MUIM_NList_GetEntry, j, &ac2);

			if (ac1->email && ac2->email)
			{
				char *addr1, *addr2;
				
				if (parse_addr_spec(ac1->email, &addr1))
				{
					if (parse_addr_spec(ac2->email, &addr2))
					{
						if (!mystricmp(addr1,addr2))
						{
							set(config_list, MUIA_NList_Active, GROUPS_ACCOUNT);
							set(account_account_list, MUIA_NList_Active, j);
							set(config_wnd,MUIA_Window_ActiveObject,account_email_string);
							sm_request(NULL,_("SimpleMail currently doesn't support the same email address for\n"
															  "multiple accounts. Please ensure that every email address is unique.\n"
															  "However, you can set the reply address field to your prefered addresse."),_("Ok"));
							return 0;
						}
						free(addr2);
					} else invalid = j;
					free(addr1);
				} else invalid = i;

				if (invalid != -1)
				{
					set(config_list, MUIA_NList_Active, GROUPS_ACCOUNT);
					set(account_account_list, MUIA_NList_Active, invalid);
					set(config_wnd,MUIA_Window_ActiveObject,account_email_string);
					sm_request(NULL,_("No valid email address entered. Please correct it."),_("Ok"));
					return 0;
				}
			}
		}
	}

	/* check if there are any duplicate signatures */
	signature_store();
	for (i=0;i<xget(signature_signature_list,MUIA_NList_Entries);i++)
	{
		struct signature *sign1, *sign2;
		DoMethod(signature_signature_list,MUIM_NList_GetEntry, i, &sign1);

		if ((mystrcmp(sign1->name, MUIV_SignatureCycle_NoSignature) == 0) ||
		    (mystrcmp(sign1->name, MUIV_SignatureCycle_Default) == 0))
		{
			set(config_list, MUIA_NList_Active, GROUPS_SIGNATURE);
			set(signature_signature_list, MUIA_NList_Active, i);
			set(config_wnd,MUIA_Window_ActiveObject,signature_name_string);
			sm_request(NULL,_("This Signaturename is not allowed."), _("Ok"),NULL);
			return 0;
		}

		for (j=0;j<xget(signature_signature_list,MUIA_NList_Entries);j++)
		{
			if (i==j) continue;
			DoMethod(signature_signature_list,MUIM_NList_GetEntry, j, &sign2);

			if (mystrcmp(sign1->name, sign2->name) == 0)
			{
				set(config_list, MUIA_NList_Active, GROUPS_SIGNATURE);
				set(signature_signature_list, MUIA_NList_Active, j);
				set(config_wnd,MUIA_Window_ActiveObject,signature_name_string);
				sm_request(NULL,_("Signaturenames must be unique."), _("Ok"),NULL);
				return 0;
			}
		}
	}

	internet_emails = array_of_addresses_from_texteditor(readhtml_mail_editor,6,&err,config_wnd,config_list);
	if (err) return 0;

	spam_white_emails = array_of_addresses_from_texteditor(spam_white_list_editor,9,&err,config_wnd,config_list);
	if (err)
	{
		array_free(internet_emails);
		return 0;
	}

	spam_black_emails = array_of_addresses_from_texteditor(spam_black_list_editor,9,&err,config_wnd,config_list);
	if (err)
	{
		array_free(internet_emails);
		array_free(spam_white_emails);
		return 0;
	}

	array_free(user.config.internet_emails);
	user.config.internet_emails = internet_emails;
	array_free(user.config.spam_white_emails);
	user.config.spam_white_emails = spam_white_emails;
	array_free(user.config.spam_black_emails);
	user.config.spam_black_emails = spam_black_emails;

	free(user.new_folder_directory);
	user.new_folder_directory = mystrdup((char*)xget(user_folder_string,MUIA_String_Contents));

	if (user.new_folder_directory && mystricmp(user.new_folder_directory,user.folder_directory))
	{
		sm_request(NULL,_("You have changed the folder direcory! You must quit and restart SimpleMail to see an effect."),_("Ok"));
	}

	user.config.header_flags = 0;

	if (xget(mails_readmisc_all_check,MUIA_Selected))
		user.config.header_flags |= SHOW_HEADER_ALL;

	for (i=0;i<sizeof(mails_readmisc_check)/sizeof(Object*);i++)
	{
		if (xget(mails_readmisc_check[i],MUIA_Selected)) user.config.header_flags |= (1<<i);
	}

	array_free(user.config.header_array);
	user.config.header_array = array_duplicate((char**)xget(mails_readmisc_additional_string,MUIA_MultiString_ContentsArray));

	user.config.readwnd_close_after_last = xget(mails_readmisc_close_after_last,MUIA_Selected);
	user.config.readwnd_next_after_move = xget(mails_readmisc_next_after_move,MUIA_Selected);

	if (user.config.appicon_label) free(user.config.appicon_label);
	if (user.config.read_propfont) free(user.config.read_propfont);
	if (user.config.read_fixedfont) free(user.config.read_fixedfont);
	if (user.config.receive_sound_file) free(user.config.receive_sound_file);
	if (user.config.receive_arexx_file) free(user.config.receive_arexx_file);

	user.config.dst = xget(user_dst_check,MUIA_Selected);
	user.config.default_codeset = codesets_find((char*)xget(user_charset_string,MUIA_String_Contents));
	user.config.appicon_show = xget(appicon_show_cycle, MUIA_Cycle_Active);
	user.config.appicon_label = mystrdup((char*)xget(appicon_label_popph, MUIA_Popph_Contents));
	user.config.receive_preselection = xget(receive_preselection_radio,MUIA_Radio_Active);
	user.config.receive_size = value2size(xget(receive_sizes_sizes, MUIA_Numeric_Value));
	user.config.receive_autocheck = xget(receive_autocheck_string,MUIA_String_Integer);
	user.config.receive_autoifonline = xget(receive_autocheckifonline_check,MUIA_Selected);
	user.config.receive_autoonstartup = xget(receive_autocheckonstartup_check,MUIA_Selected);
	user.config.receive_sound = xget(receive_sound_check,MUIA_Selected);
	user.config.receive_sound_file = mystrdup((char*)xget(receive_sound_string, MUIA_String_Contents));
	user.config.receive_arexx = xget(receive_arexx_check,MUIA_Selected);
	user.config.receive_arexx_file = mystrdup((char*)xget(receive_arexx_string, MUIA_String_Contents));
	user.config.signatures_use = xget(signature_use_checkbox, MUIA_Selected);
	user.config.write_wrap = xget(write_wordwrap_string,MUIA_String_Integer);
	user.config.write_wrap_type = xget(write_wordwrap_cycle,MUIA_Cycle_Active);
	user.config.write_reply_quote = xget(write_replywrap_check,MUIA_Selected);
	user.config.write_reply_stripsig = xget(write_replystripsig_check,MUIA_Selected);
	user.config.write_reply_citeemptyl = xget(write_replyciteemptyl_check,MUIA_Selected);
	user.config.write_forward_as_attachment = xget(write_forward_as_attachment_check,MUIA_Selected);

	user.config.read_propfont = mystrdup((char*)xget(read_propfont_string,MUIA_String_Contents));
	user.config.read_fixedfont = mystrdup((char*)xget(read_fixedfont_string,MUIA_String_Contents));
	user.config.read_background = ((read_palette_entries[0].mpe_Red >> 24)<<16) | ((read_palette_entries[0].mpe_Green>>24)<<8) | (read_palette_entries[0].mpe_Blue>>24);
	user.config.read_text = ((read_palette_entries[1].mpe_Red >> 24)<<16)       | ((read_palette_entries[1].mpe_Green>>24)<<8) | (read_palette_entries[1].mpe_Blue>>24);
	user.config.read_quoted = ((read_palette_entries[2].mpe_Red >> 24)<<16)     | ((read_palette_entries[2].mpe_Green>>24)<<8) | (read_palette_entries[2].mpe_Blue>>24);
	user.config.read_old_quoted = ((read_palette_entries[3].mpe_Red >> 24)<<16) | ((read_palette_entries[3].mpe_Green>>24)<<8) | (read_palette_entries[3].mpe_Blue>>24);
	user.config.read_link = ((read_palette_entries[4].mpe_Red >> 24)<<16)       | ((read_palette_entries[4].mpe_Green>>24)<<8) | (read_palette_entries[4].mpe_Blue>>24);
	user.config.read_header_background = ((read_palette_entries[5].mpe_Red >> 24)<<16) | ((read_palette_entries[5].mpe_Green>>24)<<8) | (read_palette_entries[5].mpe_Blue>>24);
	user.config.read_wordwrap = xget(read_wrap_checkbox, MUIA_Selected);
	user.config.read_link_underlined = xget(read_linkunderlined_checkbox,MUIA_Selected);
	user.config.read_smilies = xget(read_smilies_checkbox, MUIA_Selected);
	user.config.spam_mark_moved = xget(spam_mark_before_check,MUIA_Selected);
	user.config.spam_auto_check = xget(spam_auto_check,MUIA_Selected);
	user.config.spam_addrbook_is_white = xget(spam_addr_book_is_white_check,MUIA_Selected);

	/* Copy the accounts */
	account_store();
	clear_config_accounts();
	for (i=0;i<xget(account_account_list,MUIA_NList_Entries);i++)
	{
		struct account *ac;
		DoMethod(account_account_list,MUIM_NList_GetEntry, i, &ac);
		insert_config_account(ac);
	}

	/* Copy the phrase */
	phrase_store();
	clear_config_phrases();
	for (i=0;i<xget(phrase_phrase_list,MUIA_NList_Entries);i++)
	{
		struct phrase *phr;
		DoMethod(phrase_phrase_list,MUIM_NList_GetEntry, i, &phr);
		insert_config_phrase(phr);
	}

	/* Copy the signature */
	signature_store();
	clear_config_signatures();
	for (i=0;i<xget(signature_signature_list,MUIA_NList_Entries);i++)
	{
		struct signature *sign;
		DoMethod(signature_signature_list,MUIM_NList_GetEntry, i, &sign);
		insert_config_signature(sign);
	}

	close_config();
	callback_config_changed();
	appicon_refresh(1); /* this is amiga specific */
	return 1;
}

/******************************************************************
 Save the configuration
*******************************************************************/
static void config_save(void)
{
	if (config_use())
	{
		save_config();
	}
}

/******************************************************************
 Save the configuration
*******************************************************************/
static void config_cancel(void)
{
	close_config();
}

/******************************************************************
 A new entry in the config listtree has been selected
*******************************************************************/
static void config_selected(void)
{
	LONG active = xget(config_list,MUIA_NList_Active);
	if (active >= 0 && active < GROUPS_MAX)
	{
		Object *group = groups[active];

		if (group != config_last_visisble_group)
		{
			DoMethod(config_group,MUIM_Group_InitChange);

			if (config_last_visisble_group)
			{
				set(config_last_visisble_group,MUIA_ShowMe,FALSE);
				/* if we come from the signature group, we rebuild the SignatureCycle to reflect
				   the current done settings. */
				if (config_last_visisble_group == groups[GROUPS_SIGNATURE])
				{
					account_refresh_signature_cycle();
				}
			}
			config_last_visisble_group = group;
			set(group,MUIA_ShowMe,TRUE);

			DoMethod(config_group,MUIM_Group_ExitChange);
		}
	}
}

/******************************************************************
 Init the user group
*******************************************************************/
static int init_user_group(void)
{
  static char *appicon_show_labels[4];
	static const char *appicon_popph_array[] =
	{
		"%t|Total messages",
		"%n|New messages",
		"%u|Unread messages",
		"%s|Sent messages",
		"%o|Outgoing messages",
		"%d|Deleted messages",
		NULL
	};

  appicon_show_labels[0] = _("Always");
  appicon_show_labels[1] = _("Iconified");
  appicon_show_labels[2] = _("Never");
  appicon_show_labels[3] = NULL;

	groups[GROUPS_USER] = VGroup,
		MUIA_ShowMe, FALSE,
		MUIA_HelpNode, "CO00",
		Child, HGroup,
			Child, MakeLabel(_("Add adjustment for daylight saving time")),
			Child, user_dst_check = MakeCheck(_("Add adjustment for daylight saving time"),user.config.dst),
			Child, HSpace(0),
			End,
		Child, HGroup,
			Child, MakeLabel(_("Folder directory")),
			Child, PopaslObject,
				MUIA_Popstring_Button, PopButton(MUII_PopDrawer),
				MUIA_Popstring_String, user_folder_string = BetterStringObject,
					StringFrame,
					MUIA_CycleChain,1,
					MUIA_String_Contents,user.new_folder_directory?user.new_folder_directory:user.folder_directory,
					End,
				End,
			End,
		Child, HGroup,
			Child, MakeLabel(_("Charset used to display text")),
			Child, PoplistObject,
				MUIA_Popstring_Button, PopButton(MUII_PopUp),
				MUIA_Popstring_String, user_charset_string = BetterStringObject,
					StringFrame,
					MUIA_String_Contents, user.config.default_codeset?user.config.default_codeset->name:(codesets_supported()?codesets_supported()[0]:NULL),
					End,
				MUIA_Poplist_Array, codesets_supported(),
				End,
			End,
		Child, HGroup,
		  Child, MakeLabel(_("AppIcon Show")),
		  Child, HGroup, Child, appicon_show_cycle = MakeCycle(_("AppIcon Show"), appicon_show_labels), Child, HSpace(0), End,
			Child, MakeLabel(_("AppIcon Label")),
			Child, appicon_label_popph = PopphObject,
				MUIA_Popph_Array, appicon_popph_array,
				MUIA_Popph_Contents, user.config.appicon_label,
				End,
			End,
		End;
	if (!groups[GROUPS_USER]) return 0;

  set(appicon_show_cycle, MUIA_Cycle_Active, user.config.appicon_show);

	return 1;
}

/******************************************************************
 Initialize the receive group
*******************************************************************/
static int init_tcpip_receive_group(void)
{
	static char *preselection[4];
	static int preselection_translated;

	if (!preselection_translated)
	{
		preselection[0] = _("Disabled");
		preselection[1] = _("Only Sizes");
		preselection[2] = _("Enabled");
		preselection_translated = 1;
	};

	groups[GROUPS_RECEIVE] = VGroup,
		MUIA_ShowMe, FALSE,
		MUIA_HelpNode, "CO02",
		Child, HorizLineTextObject(_("Preselection")),
		Child, HGroup,
			Child, receive_preselection_radio = RadioObject,
				MUIA_Radio_Entries, preselection,
				MUIA_Radio_Active, user.config.receive_preselection,
				End,
			Child, RectangleObject, MUIA_Weight, 33, End,
			Child, VGroup,
				Child, HVSpace,
				Child, receive_sizes_sizes = SizesObject,
					MUIA_CycleChain, 1,
					MUIA_Numeric_Min,0,
					MUIA_Numeric_Max,50,
					MUIA_Numeric_Value, size2value(user.config.receive_size),
					End,
				Child, HVSpace,
				End,
			Child, RectangleObject, MUIA_Weight, 33, End,
			End,
		Child, HorizLineTextObject(_("Automatic operation")),
		Child, HGroup,
			Child, MakeLabel(_("Check for new mail every")),
			Child, receive_autocheck_string = BetterStringObject,
				StringFrame,
				MUIA_CycleChain,1,
				MUIA_String_Acknowledge, TRUE,
				MUIA_String_Integer, user.config.receive_autocheck,
				End,
			Child, TextObject, MUIA_Text_Contents, _("minutes"), End,
			Child, MakeLabel(_("On startup")),
			Child, receive_autocheckonstartup_check = MakeCheck(_("On startup"),user.config.receive_autoonstartup),
			Child, MakeLabel(_("If online")),
			Child, receive_autocheckifonline_check = MakeCheck(_("If online"),user.config.receive_autoifonline),
			End,
		Child, HorizLineTextObject(_("New mails")),
		Child, ColGroup(3),
			Child, MakeLabel(_("ARexx")),
			Child, receive_arexx_check = MakeCheck(_("ARexx"),user.config.receive_arexx),
			Child, receive_arexx_popasl = PopaslObject,
				MUIA_Disabled, !user.config.receive_arexx,
				MUIA_Popstring_Button, PopButton(MUII_PopFile),
				MUIA_Popstring_String, receive_arexx_string = BetterStringObject,
					StringFrame,
					MUIA_CycleChain,1,
					MUIA_String_Acknowledge, TRUE,
					MUIA_String_Contents, user.config.receive_arexx_file,
					End,
				End,

			Child, MakeLabel(_("Sound")),
			Child, receive_sound_check = MakeCheck(_("Sound"),user.config.receive_sound),
			Child, receive_sound_string = AudioSelectGroupObject, MUIA_Disabled, !user.config.receive_sound, End,
			End,
		End;

	if (!groups[GROUPS_RECEIVE]) return 0;

	set(receive_sound_string,MUIA_String_Contents,user.config.receive_sound_file);

	DoMethod(receive_arexx_check, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, receive_arexx_popasl, 3, MUIM_Set, MUIA_Disabled, MUIV_NotTriggerValue);
	DoMethod(receive_sound_check, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, receive_sound_string, 3, MUIM_Set, MUIA_Disabled, MUIV_NotTriggerValue);

	return 1;
}

/******************************************************************
 Add a new account
*******************************************************************/
static void account_add(void)
{
	struct account *account = account_malloc();
	if (account)
	{
		account->account_name = utf8create(_("-- New Account --"),user.config.default_codeset?user.config.default_codeset->name:NULL);

		DoMethod(account_account_list, MUIM_NList_InsertSingle, account, MUIV_NList_Insert_Bottom);
		set(account_account_list, MUIA_NList_Active, MUIV_NList_Active_Bottom);

		set(account_account_name_string, MUIA_BetterString_SelectSize, -utf8len(account->account_name));
		set(config_wnd, MUIA_Window_ActiveObject, account_account_name_string);
	}
}

/******************************************************************
 Remove the current account
*******************************************************************/
static void account_remove(void)
{
	struct account *ac;
	DoMethod(account_account_list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &ac);
	if (ac)
	{
		account_last_selected = NULL;
		DoMethod(account_account_list, MUIM_NList_Remove, MUIV_NList_Remove_Active);
		account_free(ac);
	}
}

/******************************************************************
 
*******************************************************************/
static void account_selected(void)
{
	account_store();
	DoMethod(account_account_list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &account_last_selected);
	account_load();
}

/******************************************************************
 Redraw the List content if there is an input in the Stringgadget
*******************************************************************/
static void account_update(void)
{
	account_store();
	DoMethod(account_account_list, MUIM_NList_Redraw, MUIV_NList_Redraw_Active);
}

/******************************************************************
 Refresh the account SignatureCycle
*******************************************************************/
static void account_refresh_signature_cycle(void)
{
	struct list tmp_signature_list;
	struct signature *sign, *new_sign;
	int i;

	/* temporary signature list for SignatureCycle to display the new signatures */
	list_init(&tmp_signature_list);
	for (i=0;i<xget(signature_signature_list,MUIA_NList_Entries);i++)
	{
		DoMethod(signature_signature_list, MUIM_NList_GetEntry, i, &sign);
		new_sign = signature_malloc();
		if (new_sign)
		{
			/* I only need the names, not the full signature */
			new_sign->name = mystrdup(sign->name);
			list_insert_tail(&tmp_signature_list, &new_sign->node);
		}
	}

	DoMethod(account_user_group, MUIM_Group_InitChange);
	DoMethod(account_def_signature_cycle, MUIM_SignatureCycle_Refresh, &tmp_signature_list);
	DoMethod(account_user_group, MUIM_Group_ExitChange);

	/* free the temporary list */
	while ((sign = (struct signature *)list_remove_tail(&tmp_signature_list)))
	{
		if (sign->name) free(sign->name);
		free(sign);
	}
}

/******************************************************************
 Set the correct port
*******************************************************************/
void account_recv_port_update(void)
{
	int ssl = xget(account_recv_ssl_check,MUIA_Selected);
	int stls = xget(account_recv_stls_check,MUIA_Selected);
	int imap = xget(account_recv_type_radio,MUIA_Radio_Active);
	int port;

	if (imap)
	{
		if (ssl) port = 993;
		else port = 143;
	}
	else
	{
		if (ssl & !stls) port = 995;
		else port = 110;
	}

	set(account_recv_port_string, MUIA_String_Integer, port);
	set(account_recv_stls_check, MUIA_Disabled, !ssl);
}

STATIC ASM SAVEDS VOID account_display(REG(a0,struct Hook *h), REG(a2,char **array), REG(a1,struct account *ent))
{
	if (ent)
	{
		static char buf[320];
		static char email[100];

		utf8tostr(ent->account_name, buf, sizeof(buf), user.config.default_codeset);

		if (ent->email)
			utf8tostr(ent->email, email, sizeof(email), user.config.default_codeset);
		else
			strcpy(email,"-");

		*array++ = buf;
		*array++ = email;
		*array++ = ent->pop->name;
		*array++ = ent->smtp->name;
	} else
	{
		*array++ = _("Name");
		*array++ = _("EMail");
		*array++ = _("Receive");
		*array++ = _("Send");
	}
}

/******************************************************************
 Initalize the account group
*******************************************************************/
static int init_account_group(void)
{
	static char *recv_entries[3];
	static char *apop_labels[4];
	static struct Hook account_display_hook;

	SM_ENTER;

	init_hook(&account_display_hook,(HOOKFUNC)account_display);

	recv_entries[0] = "POP3";
	recv_entries[1] = "IMAP4";

	apop_labels[0] = _("Try");
	apop_labels[1] = _("Enforce");
	apop_labels[2] = _("Don't try");

	groups[GROUPS_ACCOUNT] = VGroup,
		MUIA_ShowMe, FALSE,
		MUIA_HelpNode, "CO01",
		Child, HGroup,
			Child, NListviewObject,
				MUIA_NListview_NList, account_account_list = NListObject,
					MUIA_NList_Title, TRUE,
					MUIA_NList_Format, ",,,",
					MUIA_NList_DisplayHook, &account_display_hook,
					MUIA_NList_DragSortable, TRUE,
					End,
				End,
			Child, VGroup,
				MUIA_Weight,0,
				Child, HVSpace,
				Child, account_add_button = MakeButton(_("_Add Account")),
				Child, HVSpace,
				Child, account_remove_button = MakeButton(_("_Remove Account")),
				Child, HVSpace,
				End,
			End,
		Child, HGroup,
			Child, MakeLabel(_("Account Name")),
			Child, account_account_name_string = UTF8StringObject,
				StringFrame,
				MUIA_CycleChain, 1,
				MUIA_String_AdvanceOnCR, TRUE,
				End,
			End,
		Child, HorizLineTextObject(_("User")),
		Child, account_user_group = ColGroup(2),
			Child, MakeLabel(Q_("?people:Name")),
			Child, account_name_string = UTF8StringObject,
				StringFrame,
				MUIA_CycleChain, 1,
				MUIA_String_AdvanceOnCR, TRUE,
				End,
			Child, MakeLabel(_("E-Mail Address")),
			Child, account_email_string = UTF8StringObject,
				StringFrame,
				MUIA_CycleChain, 1,
				MUIA_String_AdvanceOnCR, TRUE,
				End,
			Child, MakeLabel(_("Reply Address")),
			Child, account_reply_string = UTF8StringObject,
				StringFrame,
				MUIA_CycleChain, 1,
				MUIA_String_AdvanceOnCR, TRUE,
				End,
			/* This two MUST always be the last objects of this group! */
			/* because the SignatureCycle() gets removed and reinserted for update. */
			Child, MakeLabel(_("Signature")),
			Child, account_def_signature_cycle = SignatureCycleObject,
				MUIA_CycleChain, 1,
				MUIA_SignatureCycle_HasDefaultEntry, TRUE,
				End,
			End,
		Child, HorizLineTextObject(_("Receive")),
		Child, ColGroup(2),
			Child, MakeLabel(_("Type")),
			Child, HGroup,
				Child, account_recv_type_radio = RadioObject,
					MUIA_Group_Horiz, TRUE,
					MUIA_Radio_Entries, recv_entries,
					End,
				Child, HVSpace,
				End,
			Child, MakeLabel(_("Server")),
			Child, HGroup,
				Child, account_recv_server_string = BetterStringObject,
					StringFrame,
					MUIA_CycleChain, 1,
					MUIA_String_AdvanceOnCR, TRUE,
				End,
				Child, MakeLabel(_("Port")),
				Child, account_recv_port_string = BetterStringObject,
					StringFrame,
					MUIA_Weight, 33,
					MUIA_CycleChain,1,
					MUIA_String_AdvanceOnCR, TRUE,
					MUIA_String_Accept, "0123456789",
					End,
				End,
			Child, MakeLabel(_("Login")),
			Child,  HGroup,
				Child, account_recv_login_string = BetterStringObject,
					StringFrame,
					MUIA_CycleChain, 1,
					MUIA_String_AdvanceOnCR, TRUE,
					End,
				Child, MakeLabel(_("Password")),
				Child, account_recv_password_string = BetterStringObject,
					StringFrame,
					MUIA_CycleChain, 1,
					MUIA_String_Secret, TRUE,
					MUIA_String_AdvanceOnCR, TRUE,
					End,
				Child, MakeLabel(_("Ask")),
				Child, account_recv_ask_checkbox = MakeCheck(_("Ask"),FALSE),
				End,
			End,

		Child, HGroup,
			Child, VGroup,
				Child, ColGroup(2),
					Child, MakeLabel(_("Active")),
					Child, HGroup,
						Child, account_recv_active_check = MakeCheck(_("Active"), FALSE), 
						Child, HSpace(0),
						End,

					Child, MakeLabel(_("APOP")),
					Child, account_recv_apop_cycle = MakeCycle(_("APOP"),apop_labels), 
					End,
				End,
			Child, HVSpace,
			Child, VGroup,
				Child, ColGroup(2),
					Child, MakeLabel(_("Avoid duplicates")),
					Child, account_recv_avoid_check = MakeCheck(_("Avoid duplicates"), FALSE),

					Child, MakeLabel(_("_Delete mails")),
					Child, account_recv_delete_check = MakeCheck(_("_Delete mails"), FALSE),
					End,
				End,
			Child, HVSpace,
			Child, VGroup,
				Child, ColGroup(2),
					Child, MakeLabel(_("Secure")),
					Child, account_recv_ssl_check = MakeCheck(_("Secure"), FALSE),

					Child, MakeLabel(_("with STLS")),
					Child, account_recv_stls_check = MakeCheck(_("with STLS"), FALSE),
					End,
				End,
			End,

		Child, HorizLineTextObject(_("Send")),
		Child, VGroup,
			Child, ColGroup(2),
				Child, MakeLabel(_("SMTP Server")),
				Child, HGroup,
					Child, account_send_server_string = BetterStringObject,
						StringFrame,
						MUIA_CycleChain, 1,
						MUIA_String_AdvanceOnCR, TRUE,
						End,
					Child, MakeLabel(_("Port")),
					Child, account_send_port_string = BetterStringObject,
						StringFrame,
						MUIA_Weight, 33,
						MUIA_CycleChain,1,
						MUIA_String_AdvanceOnCR,TRUE,
						MUIA_String_Accept,"0123456789",
						End,
					End,
				Child, MakeLabel(_("Login")),
				Child, HGroup,
					Child, account_send_login_string = BetterStringObject,
						StringFrame,
						MUIA_Disabled, TRUE,
						MUIA_CycleChain, 1,
						MUIA_String_AdvanceOnCR, TRUE,
						End,
					Child, MakeLabel(_("Password")),
					Child, account_send_password_string = BetterStringObject,
						StringFrame,
						MUIA_Disabled, TRUE,
						MUIA_CycleChain, 1,
						MUIA_String_AdvanceOnCR, TRUE,
						MUIA_String_Secret, TRUE,
						End,
					Child, MakeLabel(_("Use SMTP AUTH")),
					Child, account_send_auth_check = MakeCheck(_("Use SMTP AUTH"), FALSE),
					End,
				End,
			Child, HGroup,
				Child, MakeLabel(_("Secure")),
				Child, account_send_secure_check = MakeCheck(_("Secure"),FALSE),
				Child, HVSpace,
				Child, MakeLabel(_("Log into POP3 server first")),
				Child, account_send_pop3_check = MakeCheck(_("Log into POP3 server first"),FALSE),
				Child, HVSpace,
				Child, MakeLabel(_("Use IP as domain")),
				Child, account_send_ip_check = MakeCheck(_("Use IP as domain"), FALSE),
				End,
			End,
		End;

	if (!groups[GROUPS_ACCOUNT]) SM_RETURN(0,"%ld");

	DoMethod(account_account_list, MUIM_Notify, MUIA_NList_Active, MUIV_EveryTime, App, 3, MUIM_CallHook, &hook_standard, account_selected);

	/* connect the up/down keys to the List */
	set(account_account_name_string, MUIA_String_AttachedList, account_account_list);

	/* Update the account_list if one of the displayd fields are changed */
	/* This fields must be set without notification in the account_load() function! */
	DoMethod(account_account_name_string, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, App, 3, MUIM_CallHook, &hook_standard, account_update);
	DoMethod(account_email_string, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, App, 3, MUIM_CallHook, &hook_standard, account_update);
	DoMethod(account_recv_server_string, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, App, 3, MUIM_CallHook, &hook_standard, account_update);
	DoMethod(account_send_server_string, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, App, 3, MUIM_CallHook, &hook_standard, account_update);

	DoMethod(account_send_auth_check, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, account_send_login_string, 3, MUIM_Set, MUIA_Disabled, MUIV_NotTriggerValue);
	DoMethod(account_send_auth_check, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, account_send_password_string, 3, MUIM_Set, MUIA_Disabled, MUIV_NotTriggerValue);
	DoMethod(account_send_auth_check, MUIM_Notify, MUIA_Selected, TRUE, MUIV_Notify_Window, 3, MUIM_Set, MUIA_Window_ActiveObject, account_send_login_string);

	DoMethod(account_recv_stls_check, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, App, 3, MUIM_CallHook, &hook_standard, account_recv_port_update);
	DoMethod(account_recv_ssl_check, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, App, 3, MUIM_CallHook, &hook_standard, account_recv_port_update);
	DoMethod(account_recv_type_radio, MUIM_Notify, MUIA_Radio_Active, MUIV_EveryTime, App, 3, MUIM_CallHook, &hook_standard, account_recv_port_update);
	DoMethod(account_recv_ask_checkbox, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, account_recv_password_string, 3, MUIM_Set, MUIA_Disabled, MUIV_TriggerValue);

	DoMethod(account_add_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, account_add);
	DoMethod(account_remove_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, account_remove);

	DoMethod(account_recv_type_radio, MUIM_Notify, MUIA_Radio_Active, MUIV_EveryTime, account_recv_avoid_check, 3, MUIM_Set, MUIA_Disabled, MUIV_TriggerValue);

	set(account_name_string,MUIA_ShortHelp,_("Your full name (required)"));
	set(account_email_string,MUIA_ShortHelp,_("Your E-Mail address for this account (required)"));
	set(account_reply_string,MUIA_ShortHelp,_("Address where the replies of the mails should\nbe sent (required only if different from the e-mail address)."));
	set(account_def_signature_cycle,MUIA_ShortHelp,_("The default signature for this account"));
	set(account_recv_server_string,MUIA_ShortHelp,_("The name of the so called POP3 server from\nwhich you download your e-Mails (required; ask your\nprovider if unknown)."));
	set(account_recv_port_string,MUIA_ShortHelp,_("The port number. Usually 110"));
	set(account_recv_login_string,MUIA_ShortHelp,_("The login/UserID which you got from your ISP."));
	set(account_recv_password_string,MUIA_ShortHelp,_("Your very own password to access the mails\nlocated on the POP3 server."));
	set(account_recv_active_check,MUIA_ShortHelp,_("Deactivate this if you don't want SimpleMail to\ndownload the e-Mails when pressing on 'Fetch'."));
	set(account_recv_delete_check,MUIA_ShortHelp,_("After successul downloading the mails should be deleted\non the POP3 server."));
	set(account_recv_avoid_check,MUIA_ShortHelp,_("When not deleteding e-Mails on the server SimpleMail\ntries to avoid downloading a message twice the next time."));
	set(account_recv_apop_cycle,MUIA_ShortHelp,_("Choose how APOP is handled. If you encounter login probelms\nyou might change this setting to \"Don't try\"."));
	set(account_recv_ssl_check,MUIA_ShortHelp,_("If activated SimpleMail tries to make the connection secure.\nThis is not supported on all servers, so decativate if it doesn't work."));
	set(account_send_server_string,MUIA_ShortHelp,_("The name of the so called SMTP server which is responlible\nto send your e-Mails."));
	set(account_send_port_string,MUIA_ShortHelp,_("The port number. Usually 25"));
	set(account_send_login_string,MUIA_ShortHelp,_("Your login/UserID for the SMTP server.\nOnly required if the SMTP server requires authentication."));
	set(account_send_password_string,MUIA_ShortHelp,_("Your password for the SMTP server.\nOnly required if the SMTP server requires authentication."));
	set(account_send_auth_check,MUIA_ShortHelp,_("Activate this if the SMTP server requires authentication."));
	set(account_send_secure_check,MUIA_ShortHelp,_("Activate this if you want a secure connection\nto the SMTP server. Deactivate this if your SMTP server\ndoesn't support it"));
	set(account_send_pop3_check,MUIA_ShortHelp,_("Activate this if you provider needs that\nyou first log into its POP3 sever."));
	set(account_send_ip_check,MUIA_ShortHelp,_("Send your current IP address together with the intial greetings.\nThis avoids some error headers on some providers."));
	set(account_add_button,MUIA_ShortHelp,_("Add a new account."));
	set(account_remove_button,MUIA_ShortHelp,_("Remove the current account."));

	SM_RETURN(1,"%ld");
}

/******************************************************************
 Init the readmisc group
*******************************************************************/
static int init_write_group(void)
{
	static char *wordwrap_entries[4];
	static int wordwrap_entries_translated;

	SM_ENTER;

	if (!wordwrap_entries_translated)
	{
		wordwrap_entries[0] = _("off");
		wordwrap_entries[1] = _("as you type");
		wordwrap_entries[2] = _("before storing");
		wordwrap_entries_translated = 1;
	}

	groups[GROUPS_WRITE] = VGroup,
		MUIA_ShowMe, FALSE,
		MUIA_HelpNode, "CO03",
		Child, HorizLineTextObject(_("Editor")),
		Child, HGroup,
			Child, MakeLabel(_("Word wrap")),
			Child, write_wordwrap_string = BetterStringObject, StringFrame, MUIA_CycleChain, 1, MUIA_String_Accept, "0123456789", MUIA_String_Integer, user.config.write_wrap, End,
			Child, write_wordwrap_cycle = MakeCycle(NULL,wordwrap_entries),
			End,
		Child, HorizLineTextObject(_("Replying")),
		Child, HGroup,
			Child, ColGroup(2),
				Child, MakeLabel(_("Wrap before quoting")),
				Child, write_replywrap_check = MakeCheck(_("Wrap before quoting"),user.config.write_reply_quote),

				Child, MakeLabel(_("Strip signature before quoting")),
				Child, write_replystripsig_check = MakeCheck(_("Strip signature before quoting"),user.config.write_reply_stripsig),

				Child, MakeLabel(_("Quote empty lines")),
				Child, write_replyciteemptyl_check = MakeCheck(_("Quote empty lines"),user.config.write_reply_citeemptyl),
				End,
			Child, HVSpace,
			End,
		Child, HorizLineTextObject(_("Forwarding")),
		Child, HGroup,
			Child, ColGroup(2),
				Child, MakeLabel(_("Forward mails as attachments")),
				Child, write_forward_as_attachment_check = MakeCheck(_("Forward mails as attachments"),user.config.write_forward_as_attachment),
				End,
			Child, HVSpace,
			End,
		End;

	if (!groups[GROUPS_WRITE]) SM_RETURN(0,"%ld");

	set(write_wordwrap_cycle,MUIA_Cycle_Active, user.config.write_wrap_type);
	SM_RETURN(1,"%ld");
}

/******************************************************************
 Init the readmisc group
*******************************************************************/
static int init_mails_readmisc_group(void)
{
	int i;

	SM_ENTER;

	groups[GROUPS_READMISC] = VGroup,
		MUIA_ShowMe, FALSE,
		MUIA_HelpNode, "CO04",
		Child, HorizLineTextObject(_("Header Configuration")),
		Child, VGroup,
			Child, HGroup,
				Child, MakeLabel(_("Show all headers")),
				Child, mails_readmisc_all_check = MakeCheck(_("Show all headers"),FALSE),
				Child, HVSpace,
				End,
			Child, RectangleObject, MUIA_FixHeight, 6, End,
			Child, mails_readmisc_check_group = HGroup,
				Child, VGroup,
					Child, ColGroup(2),
						Child, MakeLabel(_("From")),
						Child, mails_readmisc_check[0] = MakeCheck(_("From"),FALSE),

						Child, MakeLabel(_("To")),
						Child, mails_readmisc_check[1] = MakeCheck(_("To"),FALSE),

						Child, MakeLabel(_("CC")),
						Child, mails_readmisc_check[2] = MakeCheck(_("CC"),FALSE),

						Child, MakeLabel(_("Subject")),
						Child, mails_readmisc_check[3] = MakeCheck(_("Subject"),FALSE),

						Child, MakeLabel(_("Date")),
						Child, mails_readmisc_check[4] = MakeCheck(_("Date"),FALSE),

						Child, MakeLabel(_("Reply-To")),
						Child, mails_readmisc_check[5] = MakeCheck(_("Reply-To"),FALSE),
						End,
					Child, HVSpace,
					End,
				Child, VGroup,
					Child, HGroup,
						Child, HVSpace,
						Child, TextObject, MUIA_Text_Contents, _("Additional headers"), End,
						Child, HVSpace,
						End,
					Child, mails_readmisc_additional_string = MultiStringObject,
						StringFrame,
						MUIA_MultiString_ContentsArray,user.config.header_array,
						End,
					End,
				End,
			End,
		Child, HorizLineTextObject(_("Readwindow Configuration")),
		Child, VGroup,
			Child, HGroup,
				Child, ColGroup(2),
					Child, MakeLabel(_("Close read window on list end")),
					Child, mails_readmisc_close_after_last = MakeCheck(_("Close read window on list end"),user.config.readwnd_close_after_last),

					Child, MakeLabel(_("Show next mail after move")),
					Child, mails_readmisc_next_after_move = MakeCheck(_("Show next mail after move"),user.config.readwnd_next_after_move),
					End,
				Child, HVSpace,
				End,
			End,
		Child, HVSpace,
		End;

	if (!groups[GROUPS_READMISC]) SM_RETURN(0,"%ld");

  set(mails_readmisc_close_after_last, MUIA_ShortHelp, _("If this option is enabled, SimpleMail will close the read window "
                                                         "after all mails in the folder have been processed (e.g. deleted) by the user "
                                                         "completly in the current direction. Otherwise SimpleMail tries to browse into "
                                                         "the other direction and closes the window only if no other mail exists within "
                                                         "the folder."));

	DoMethod(mails_readmisc_all_check, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, mails_readmisc_check_group, 3, MUIM_Set, MUIA_Disabled, MUIV_TriggerValue);
	if (user.config.header_flags & SHOW_HEADER_ALL) set(mails_readmisc_all_check, MUIA_Selected, TRUE);
	for (i=0;i<sizeof(mails_readmisc_check)/sizeof(Object*);i++)
	{
		if (user.config.header_flags & (1<<i)) set(mails_readmisc_check[i],MUIA_Selected,TRUE);
	}
	SM_RETURN(1,"%ld");
}

/******************************************************************
 Init the read group
*******************************************************************/
static int init_mails_read_group(void)
{
	static char *read_palette_names[7];
	static int read_palette_names_translated;

	SM_ENTER;

	if (!read_palette_names_translated)
	{
		read_palette_names[0] = _("Background");
		read_palette_names[1] = _("Text");
		read_palette_names[2] = _("Quoted Text");
		read_palette_names[3] = _("Old Quoted Text");
		read_palette_names[4] = _("Link Text");
		read_palette_names[5] = _("Header Background");
	};

	read_palette_entries[0].mpe_ID = 0;
	read_palette_entries[0].mpe_Red = MAKECOLOR32((user.config.read_background&0x00ff0000)>>16);
	read_palette_entries[0].mpe_Green = MAKECOLOR32((user.config.read_background&0x0000ff00)>>8);
	read_palette_entries[0].mpe_Blue = MAKECOLOR32((user.config.read_background&0x000000ff));
	read_palette_entries[0].mpe_Group = 0;

	read_palette_entries[1].mpe_ID = 1;
	read_palette_entries[1].mpe_Red = MAKECOLOR32((user.config.read_text&0x00ff0000)>>16);
	read_palette_entries[1].mpe_Green = MAKECOLOR32((user.config.read_text&0x0000ff00)>>8);
	read_palette_entries[1].mpe_Blue = MAKECOLOR32((user.config.read_text&0x00ff));
	read_palette_entries[1].mpe_Group = 1;

	read_palette_entries[2].mpe_ID = 2;
	read_palette_entries[2].mpe_Red = MAKECOLOR32((user.config.read_quoted&0x00ff0000)>>16);
	read_palette_entries[2].mpe_Green = MAKECOLOR32((user.config.read_quoted&0x0000ff00)>>8);
	read_palette_entries[2].mpe_Blue = MAKECOLOR32((user.config.read_quoted&0x000000ff));
	read_palette_entries[2].mpe_Group = 2;

	read_palette_entries[3].mpe_ID = 3;
	read_palette_entries[3].mpe_Red = MAKECOLOR32((user.config.read_old_quoted&0x00ff0000)>>16);
	read_palette_entries[3].mpe_Green = MAKECOLOR32((user.config.read_old_quoted&0x0000ff00)>>8);
	read_palette_entries[3].mpe_Blue = MAKECOLOR32((user.config.read_old_quoted&0x0000ff));
	read_palette_entries[3].mpe_Group = 3;

	read_palette_entries[4].mpe_ID = 4;
	read_palette_entries[4].mpe_Red = MAKECOLOR32((user.config.read_link&0x00ff0000)>>16);
	read_palette_entries[4].mpe_Green = MAKECOLOR32((user.config.read_link&0x0000ff00)>>8);
	read_palette_entries[4].mpe_Blue = MAKECOLOR32((user.config.read_link&0x0000ff));
	read_palette_entries[4].mpe_Group = 4;

	read_palette_entries[5].mpe_ID = 5;
	read_palette_entries[5].mpe_Red = MAKECOLOR32((user.config.read_header_background&0x00ff0000)>>16);
	read_palette_entries[5].mpe_Green = MAKECOLOR32((user.config.read_header_background&0x0000ff00)>>8);
	read_palette_entries[5].mpe_Blue = MAKECOLOR32((user.config.read_header_background&0x0000ff));
	read_palette_entries[5].mpe_Group = 5;

	read_palette_entries[6].mpe_ID = MUIV_Palette_Entry_End;
	read_palette_entries[6].mpe_Red = 0;
	read_palette_entries[6].mpe_Green = 0;
	read_palette_entries[6].mpe_Blue = 0;
	read_palette_entries[6].mpe_Group = 0;

	groups[GROUPS_READ] = VGroup,
		MUIA_ShowMe, FALSE,
		MUIA_HelpNode, "CO05",
		Child, HorizLineTextObject(_("Fonts")),
		Child, ColGroup(2),
			Child, MakeLabel(_("Proportional Font")),
			Child, PopaslObject,
				MUIA_Popasl_Type, ASL_FontRequest,
				MUIA_Popstring_String, read_propfont_string = BetterStringObject, StringFrame, MUIA_String_Contents, user.config.read_propfont,End,
				MUIA_Popstring_Button, PopButton(MUII_PopUp),
				End,

			Child, MakeLabel(_("Fixed Font")),
			Child, PopaslObject,
				MUIA_Popasl_Type, ASL_FontRequest,
				MUIA_Popstring_String, read_fixedfont_string = BetterStringObject, StringFrame, MUIA_String_Contents, user.config.read_fixedfont, End,
				MUIA_Popstring_Button, PopButton(MUII_PopUp),
				ASLFO_FixedWidthOnly, TRUE,
				End,
			End,

		Child, HorizLineTextObject(_("Colors")),
		Child, read_palette = PaletteObject,
			MUIA_Palette_Entries, read_palette_entries,
			MUIA_Palette_Names  , read_palette_names,
			MUIA_Palette_Groupable, FALSE,
			End,

		Child, HorizLineObject,

		Child, HGroup,
			Child, MakeLabel(_("Wordwrap plain text")),
			Child, read_wrap_checkbox = MakeCheck(_("Wordwrap plain text"),user.config.read_wordwrap),
			Child, MakeLabel(_("Underline links")),
			Child, read_linkunderlined_checkbox = MakeCheck(_("Underline links"),user.config.read_link_underlined),
			Child, MakeLabel(_("Use graphical smilies")),
			Child, read_smilies_checkbox = MakeCheck(_("Use graphical smilies"),user.config.read_smilies),
			Child, HVSpace,
			End,
		End;

	if (!groups[GROUPS_READ]) SM_RETURN(0,"%ld");
	SM_RETURN(1,"%ld");
}


/******************************************************************
 Init the readhtml group
*******************************************************************/
static int init_mails_readhtml_group(void)
{
	SM_ENTER;

	groups[GROUPS_READHTML] = VGroup,
		MUIA_ShowMe, FALSE,
		MUIA_HelpNode, "CO06",
		Child, HGroup,
			Child, HVSpace,
			Child, MakeLabel(_("Allow to download images from the internet from")),
			Child, HVSpace,
			End,
		Child, readhtml_mail_editor = ComposeEditorObject,
			InputListFrame,
			MUIA_CycleChain, 1,
			MUIA_TextEditor_FixedFont, TRUE,
			End,
		End;

	if (!groups[GROUPS_READHTML]) SM_RETURN(0,"%ld");
	set(readhtml_mail_editor,MUIA_ComposeEditor_Array,user.config.internet_emails);
	SM_RETURN(1,"%ld");
}

/******************************************************************
 Add a new signature
*******************************************************************/
static void signature_add(void)
{
	struct signature *s = signature_malloc();
	if (s)
	{
		s->name = utf8create(_("-- New Signature --"),user.config.default_codeset?user.config.default_codeset->name:NULL);

		DoMethod(signature_signature_list,MUIM_NList_InsertSingle,s,MUIV_NList_Insert_Bottom);
		set(signature_signature_list,MUIA_NList_Active,MUIV_NList_Active_Bottom);

		set(signature_name_string, MUIA_BetterString_SelectSize, -utf8len(s->name));
		set(config_wnd, MUIA_Window_ActiveObject, signature_name_string);
	}
}

/******************************************************************
 Remove a signature
*******************************************************************/
static void signature_remove(void)
{
	struct signature *sign;
	DoMethod(signature_signature_list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &sign);
	if (sign)
	{
		struct account *ac;
		int i, use_count_accounts = 0, use_count_folders = 0;

		/* Check if the signature is used in any of the accounts */
		for (i=0;i<xget(account_account_list,MUIA_NList_Entries);i++)
		{
			DoMethod(account_account_list,MUIM_NList_GetEntry, i, &ac);
			if (mystrcmp(ac->def_signature, sign->name) == 0)
			{
				use_count_accounts++;
			}
		}
		use_count_folders = callback_folder_count_signatures(sign->name);
		if (use_count_accounts || use_count_folders)
		{
			if (use_count_folders == 0)
			{
				if (!sm_request(NULL,_("The signature is used in %d account(s)."),_("Remove|*Cancel"),use_count_accounts)) return;
			} else
			{
				if (use_count_accounts == 0)
				{
					if (!sm_request(NULL,_("The signature is used in %d folder(s)."),_("Remove|*Cancel"),use_count_folders)) return;
				} else
				{
					if (!sm_request(NULL,_("The signature is used in %d account(s) and %d folder(s)."),_("Remove|*Cancel"),use_count_accounts,use_count_folders)) return;
				}
			}
		}
		/* The Signature will be deleted, we have to reassign the accounts */
		for (i=0;i<xget(account_account_list,MUIA_NList_Entries);i++)
		{
			DoMethod(account_account_list,MUIM_NList_GetEntry, i, &ac);
			/* set to default all accounts with the deleted signature */
			if (mystrcmp(ac->def_signature, sign->name) == 0)
			{
				ac->def_signature = MUIV_SignatureCycle_Default;
			}
			/* If the account was displayed, we must update the gui elements */
			if (account_last_selected == ac)
			{
				nnset(account_def_signature_cycle, MUIA_SignatureCycle_SignatureName, ac->def_signature);
				free(account_last_selected->def_signature);
				account_last_selected->def_signature = mystrdup((char*)xget(account_def_signature_cycle, MUIA_SignatureCycle_SignatureName));
			}
		}
		/* do not touch the folders config! */

		signature_last_selected = NULL;
		DoMethod(signature_signature_list, MUIM_NList_Remove, MUIV_NList_Remove_Active);
		signature_free(sign);
	}
}

/******************************************************************
 
*******************************************************************/
static void signature_selected(void)
{
	signature_store();
	DoMethod(signature_signature_list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &signature_last_selected);
	signature_load();
}

/******************************************************************
 Redraw the List content if there is an input in the Stringgadget
*******************************************************************/
static void signature_update(void)
{
	signature_store();
	DoMethod(signature_signature_list, MUIM_NList_Redraw, MUIV_NList_Redraw_Active);
}

STATIC ASM SAVEDS VOID signature_display(REG(a0,struct Hook *h), REG(a2,char **array), REG(a1,struct signature *ent))
{
	if (ent)
	{
		static char buf[320];
		utf8tostr(ent->name, buf, sizeof(buf), user.config.default_codeset);
		*array = buf;
	} else
	{
		*array = _("Name");
	}
}

/******************************************************************
 Init the signature group
*******************************************************************/
static int init_signature_group(void)
{
	static struct Hook signature_display_hook;

/*	Object *edit_button;*/
	Object *slider = ScrollbarObject, End;
	Object *tagline_button;
	Object *env_button;

	Object *add_button, *rem_button;

	SM_ENTER;

	init_hook(&signature_display_hook,(HOOKFUNC)signature_display);

	groups[GROUPS_SIGNATURE] = VGroup,
		MUIA_ShowMe, FALSE,
		MUIA_HelpNode, "CO08",
		Child, HGroup,
  			Child, MakeLabel(_("Us_e signatures")),
  			Child, signature_use_checkbox = MakeCheck(_("Us_e signatures"),user.config.signatures_use),
  			Child, HVSpace,
				End,

		Child, HGroup,
			Child, NListviewObject,
				MUIA_NListview_NList, signature_signature_list = NListObject,
					MUIA_NList_Title, TRUE,
					MUIA_NList_DisplayHook, &signature_display_hook,
					End,
				End,
			Child, VGroup,
				MUIA_Weight,0,
				Child, HVSpace,
				Child, add_button = MakeButton(_("_Add")),
				Child, HVSpace,
				Child, rem_button = MakeButton(_("_Remove")),
				Child, HVSpace,
				End,
			End,


		Child, HGroup,
			Child, MakeLabel(_("Name")),
			Child, signature_name_string = UTF8StringObject,
				StringFrame,
				MUIA_CycleChain, 1,
				MUIA_String_AdvanceOnCR, TRUE,
				End,
/*			Child, edit_button = MakeButton("Edit in external editor"),*/
			End,
		Child, HGroup,
			MUIA_Group_Spacing, 0,
			Child, signature_texteditor = TextEditorObject,
				InputListFrame,
				MUIA_CycleChain, 1,
				MUIA_TextEditor_Slider, slider,
				MUIA_TextEditor_FixedFont, TRUE,
				End,
			Child, slider,
			End,
		Child, HGroup,
  		Child, tagline_button = MakeButton(_("Insert random tagline")),
  		Child, env_button = MakeButton(_("Insert ENV:Signature")),
  		End,
		End;

	if (!groups[GROUPS_SIGNATURE]) SM_RETURN(0,"%ld");
/*	set(edit_button, MUIA_Weight,0);*/

	/* connect the up/down keys to the List */
	set(signature_name_string, MUIA_String_AttachedList, signature_signature_list);

	/* Keep the list in sync with the field. */
	/* This field must be set without notification in the signature_load() function! */
	DoMethod(signature_name_string, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, App, 3, MUIM_CallHook, &hook_standard, signature_update);

	DoMethod(tagline_button,MUIM_Notify,MUIA_Pressed,FALSE,signature_texteditor,3,MUIM_TextEditor_InsertText,"%t\n",MUIV_TextEditor_InsertText_Cursor);
	DoMethod(env_button,MUIM_Notify,MUIA_Pressed,FALSE,signature_texteditor,3,MUIM_TextEditor_InsertText,"%e",MUIV_TextEditor_InsertText_Cursor);

	DoMethod(add_button,MUIM_Notify, MUIA_Pressed,FALSE,App,6,MUIM_Application_PushMethod,App,3,MUIM_CallHook,&hook_standard, signature_add);
	DoMethod(rem_button,MUIM_Notify, MUIA_Pressed,FALSE,App,6,MUIM_Application_PushMethod,App,3,MUIM_CallHook,&hook_standard, signature_remove);
	DoMethod(signature_signature_list, MUIM_Notify, MUIA_NList_Active, MUIV_EveryTime, App, 3, MUIM_CallHook, &hook_standard, signature_selected);
	SM_RETURN(1,"%ld");
}

/******************************************************************
 Add a new phrase
*******************************************************************/
static void phrase_add(void)
{
	struct phrase *p = phrase_malloc();
	if (p)
	{
		p->addresses = utf8create(_("-- New Phrase --"),user.config.default_codeset?user.config.default_codeset->name:NULL);

		DoMethod(phrase_phrase_list, MUIM_NList_InsertSingle, p, MUIV_NList_Insert_Bottom);
		set(phrase_phrase_list, MUIA_NList_Active, MUIV_NList_Active_Bottom);

		set(phrase_addresses_string, MUIA_BetterString_SelectSize, -utf8len(p->addresses));
		set(config_wnd, MUIA_Window_ActiveObject, phrase_addresses_string);
	}
}

/******************************************************************
 Duplicate the current phrase
*******************************************************************/
static void phrase_dup(void)
{
	struct phrase *phr;
	DoMethod(phrase_phrase_list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &phr);
	phr = phrase_duplicate(phr);
	if (phr)
	{
		DoMethod(phrase_phrase_list, MUIM_NList_InsertSingle, phr, MUIV_NList_Insert_Bottom);
		set(phrase_phrase_list, MUIA_NList_Active, MUIV_NList_Active_Bottom);
	}
}

/******************************************************************
 Remove a phrase
*******************************************************************/
static void phrase_remove(void)
{
	struct phrase *phr;
	DoMethod(phrase_phrase_list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &phr);
	if (phr)
	{
		phrase_last_selected = NULL;
		DoMethod(phrase_phrase_list, MUIM_NList_Remove, MUIV_NList_Remove_Active);
		phrase_free(phr);
	}
}

/******************************************************************
 
*******************************************************************/
static void phrase_selected(void)
{
	phrase_store();
	DoMethod(phrase_phrase_list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &phrase_last_selected);
	phrase_load();
}

/******************************************************************
 Redraw the List content if there is an input in the Stringgadget
*******************************************************************/
static void phrase_update(void)
{
	phrase_store();
	DoMethod(phrase_phrase_list, MUIM_NList_Redraw, MUIV_NList_Redraw_Active);
}

STATIC ASM SAVEDS VOID phrase_display(REG(a0,struct Hook *h), REG(a2,char **array), REG(a1,struct phrase *ent))
{
	if (ent)
	{
		static char buf[320];
		utf8tostr(ent->addresses?((*ent->addresses)?ent->addresses:_("All")):_("All"), buf, sizeof(buf), user.config.default_codeset);
		*array++ = buf;
	} else
	{
		*array++ = _("On addresses");
	}
}


/******************************************************************
 Init the signature group
*******************************************************************/
static int init_phrase_group(void)
{
	Object *add_button, *dup_button, *rem_button;

	static struct Hook phrase_display_hook;

	static const char *write_popph_array[] =
	{
		"\\n|Line break",
		"%r|Recipient: Name",
		"%v|Recipient: First Name",
		"%a|Recipient: Address",
		NULL
	};

	static const char *reply_popph_array[] =
	{
		"\\n|Line break",
		"%r|Recipient: Name",
		"%v|Recipient: First Name",
		"%a|Recipient: Address",
		"%n|Original sender: Name",
		"%f|Original sender: First Name",
		"%e|Original sender: Address",
		"%s|Original message: Subject",
		"%d|Original message: Date",
		"%t|Original message: Time",
		"%w|Original message: Day of week",
		"%m|Original message: Message ID",
		NULL
	};

	SM_ENTER;

	init_hook(&phrase_display_hook,(HOOKFUNC)phrase_display);

	groups[GROUPS_PHRASE] = VGroup,
		MUIA_ShowMe, FALSE,
		MUIA_HelpNode, "CO07",
		Child, HGroup,
			Child, NListviewObject,
				MUIA_NListview_NList, phrase_phrase_list = NListObject,
					MUIA_NList_Title, TRUE,
					MUIA_NList_DisplayHook, &phrase_display_hook,
					End,
				End,
			Child, VGroup,
				MUIA_Weight,0,
				Child, HVSpace,
				Child, add_button = MakeButton(_("_Add")),
				Child, HVSpace,
				Child, dup_button = MakeButton(_("_Duplicate")),
				Child, HVSpace,
				Child, rem_button = MakeButton(_("_Remove")),
				Child, HVSpace,
				End,
			End,
		Child, HGroup,
			Child, MakeLabel(_("Use on addresses which contain")),
			Child, phrase_addresses_string = BetterStringObject,
				StringFrame,
				MUIA_String_AdvanceOnCR, TRUE,
				MUIA_CycleChain,1,
				End,
			End,
		Child, HorizLineTextObject(_("Write")),
		Child, ColGroup(2),
			Child, MakeLabel(_("Welcome")),
			Child, phrase_write_welcome_string = BetterStringObject,
				StringFrame,
				MUIA_String_AdvanceOnCR, TRUE,
				MUIA_CycleChain,1,
				End,
			Child, MakeLabel(_("Welcome with address")),
			Child, phrase_write_welcomeaddr_popph = PopphObject,
				MUIA_Popph_Array, write_popph_array,
				End,
			Child, MakeLabel(Q_("?phrases:Close")),
			Child, phrase_write_close_string = BetterStringObject,
				StringFrame,
				MUIA_String_AdvanceOnCR, TRUE,
				MUIA_CycleChain,1,
				End,		
			End,
		Child, HorizLineTextObject(_("Reply")),
		Child, ColGroup(2),
			Child, MakeLabel(_("Welcome")),
			Child, phrase_reply_welcome_popph = PopphObject,
				MUIA_Popph_Array, reply_popph_array,
				End,

			Child, MakeLabel(_("Intro")),
			Child, phrase_reply_intro_popph = PopphObject,
				MUIA_Popph_Array, reply_popph_array,
				End,

			Child, MakeLabel(_("Close")),
			Child, phrase_reply_close_popph = PopphObject,
				MUIA_Popph_Array, reply_popph_array,
				End,
			End,
		Child, HorizLineTextObject(_("Forward")),
		Child, ColGroup(2),
			Child, MakeLabel(_("Initial")),
			Child, phrase_forward_initial_popph = PopphObject,
				MUIA_Popph_Array, reply_popph_array,
				End,

			Child, MakeLabel(_("Finish")),
			Child, phrase_forward_terminating_popph = PopphObject,
				MUIA_Popph_Array, reply_popph_array,
				End,
			End,
		End;

	if (!groups[GROUPS_PHRASE]) SM_RETURN(0,"%ld");
	/* connect the up/down keys to the List */
	set(phrase_addresses_string, MUIA_String_AttachedList, phrase_phrase_list);

	/* Keep the list in sync with the field. */
	/* This field must be set without notification in the signature_load() function! */
	DoMethod(phrase_addresses_string, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, App, 3, MUIM_CallHook, &hook_standard, phrase_update);

	DoMethod(add_button, MUIM_Notify, MUIA_Pressed,FALSE,App,6,MUIM_Application_PushMethod,App,3,MUIM_CallHook,&hook_standard, phrase_add);
	DoMethod(dup_button, MUIM_Notify, MUIA_Pressed,FALSE,App,6,MUIM_Application_PushMethod,App,3,MUIM_CallHook,&hook_standard, phrase_dup);
	DoMethod(rem_button, MUIM_Notify, MUIA_Pressed,FALSE,App,6,MUIM_Application_PushMethod,App,3,MUIM_CallHook,&hook_standard, phrase_remove);
	DoMethod(phrase_phrase_list, MUIM_Notify, MUIA_NList_Active, MUIV_EveryTime, App, 3, MUIM_CallHook, &hook_standard, phrase_selected);
	SM_RETURN(1,"%ld");
}

/******************************************************************
 Resets the ham statistics
*******************************************************************/
static void spam_cfg_reset_ham(void)
{
	if (!sm_request(NULL,
		_("After reseting the ham statistics the spam filter won't work reliable\n"
		  "until you have classified some mails as ham.\n"
		  "Are you sure to continue?"),
		_("Reset the statistics|Cancel"))) return;

	spam_reset_ham();
	DoMethod(spam_ham_text,MUIM_SetAsString, MUIA_Text_Contents, "%ld",
				spam_num_of_ham_classified_mails());
}

/******************************************************************
 Resets the ham statistics
*******************************************************************/
static void spam_cfg_reset_spam(void)
{
	int res;

	res = sm_request(NULL,
		_("After reseting the spam statistics the spam filter won't work reliable\n"
		  "until you have classified some mails as spam. However, if requested the\n"
		  "statistics can be built from the mails inside the spam folder.\n"
		  "How to proceed?"),
		_("Reset the statistics|Reset and rebuild|Cancel"));

	if (!res) return;

	spam_reset_spam();

	if (res == 2)
	{
		callback_add_spam_folder_to_statistics();
	}

	DoMethod(spam_spam_text,MUIM_SetAsString, MUIA_Text_Contents, "%ld",
				spam_num_of_spam_classified_mails());
}

/******************************************************************
 Init the signature group
*******************************************************************/
int init_spam_group(void)
{
	char spam_buf[16];
	char ham_buf[16];

	SM_ENTER;

	sprintf(spam_buf,"%d",spam_num_of_spam_classified_mails());
	sprintf(ham_buf,"%d",spam_num_of_ham_classified_mails());

	groups[GROUPS_SPAM] = VGroup,
		MUIA_ShowMe, FALSE,
		MUIA_HelpNode, "CO09",
		Child, VGroup,
			Child, HorizLineTextObject(_("General spam settings")),

			Child, ColGroup(2),
				Child, MakeLabel(_("Mark mails as spam before moved to the spam folder")),
				Child, HGroup, Child, spam_mark_before_check = MakeCheck(_("Mark mails as spam before moved to the spam folder"),user.config.spam_mark_moved), Child, HVSpace, End,
				MUIA_ShortHelp, _("If you activate this option any mail you move to the spam folder\n"
													"is marked as spam before the operation. Note that only mails\n"
													"marked as spam can be moved into the spam folder."),
				End,

			Child, ColGroup(2),
				Child, MakeLabel(_("Check new mails for spam content")),
				Child, HGroup, Child, spam_auto_check = MakeCheck(_("Check new mails for spam content"),user.config.spam_auto_check), Child, HVSpace, End,
				MUIA_ShortHelp, _("If you activate this option any new mail is being checked for its\n"
													"spam content. This only works if you have more than 500 mails for\n"
													"every class within the statistics."),
				End,

			Child, HGroup,
				Child, VGroup,
					MUIA_ShortHelp, _("A white list contains email address of people you usually\n"
                            "thrust. This can be used to speed up the spam checking as\n"
                            "mails with a listed sender address are skipped."),
					Child, HorizLineTextObject(_("White list")),
					Child, spam_white_list_editor = ComposeEditorObject,
						InputListFrame,
						MUIA_CycleChain, 1,
						MUIA_TextEditor_FixedFont, TRUE,
						End,
					Child, HGroup,
						MUIA_ShortHelp, _("If enabled all addresses within you addressbook are also\n"
														  "considered to be safe."),
						Child, MakeLabel(_("Also all addresses within the addressbook")),
						Child, spam_addr_book_is_white_check = MakeCheck(_("Also all addresses within the addressbook"),user.config.spam_addrbook_is_white),
						End,
					End,

				Child, VGroup,
					MUIA_ShortHelp, _("A black list contains email address which you don't thrust.\n"
														"Mails from these addresses are always marked as spam."),
					Child, HorizLineTextObject(_("Black list")),
					Child, spam_black_list_editor = ComposeEditorObject,
						InputListFrame,
						MUIA_CycleChain, 1,
						MUIA_TextEditor_FixedFont, TRUE,
						End,
					End,
				End,

			Child, HorizLineTextObject(_("Statistics")),
			Child, VGroup,
				Child, ColGroup(4),
					Child, MakeLabel(_("Number of mails classified as ham")),
					Child, spam_ham_text = TextObject, TextFrame, MUIA_Text_Contents, ham_buf, End,
					Child, spam_reset_ham_stat_button = MakeButton(_("Reset statistics...")),
					Child, HVSpace,

					Child, MakeLabel(_("Number of mails classified as spam")),
					Child, spam_spam_text = TextObject, TextFrame, MUIA_Text_Contents, spam_buf, End,
					Child, spam_reset_spam_stat_button = MakeButton(_("Reset statistics...")),
					Child, HVSpace,
					End,
				End,
			End,
		End;

  if (!groups[GROUPS_SPAM]) SM_RETURN(0,"%ld");

	set(spam_white_list_editor,MUIA_ComposeEditor_Array,user.config.spam_white_emails);
	set(spam_black_list_editor,MUIA_ComposeEditor_Array,user.config.spam_black_emails);

	set(spam_reset_ham_stat_button, MUIA_ShortHelp, _("Resets all ham statistics."));
	set(spam_reset_spam_stat_button, MUIA_ShortHelp, _("Resets all spam statistics."));

	DoMethod(spam_reset_ham_stat_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, spam_cfg_reset_ham);
	DoMethod(spam_reset_spam_stat_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, spam_cfg_reset_spam);

	SM_RETURN(1,"%ld");
}

/******************************************************************
 Init the config window
*******************************************************************/
static void init_config_window(void)
{
	Object *save_button, *use_button, *cancel_button, *test_popph;

	SM_ENTER;

	if (!create_sizes_class()) return;

	if (!(test_popph = PopphObject, End))
	{
		sm_request(NULL, "Couldn't create config window, because\n"
										 "the Popplaceholder custom class is missing.","Ok");
		return;
	}
	
	MUI_DisposeObject(test_popph);

	init_account_group();
	init_user_group();
	init_tcpip_receive_group();
	init_write_group();
	init_mails_readmisc_group();
	init_mails_read_group();
	init_mails_readhtml_group();
	init_signature_group();
	init_phrase_group();
	init_spam_group();

	config_wnd = WindowObject,
		MUIA_HelpNode, "CO_W",
		MUIA_Window_ID, MAKE_ID('C','O','N','F'),
		MUIA_Window_Title, _("SimpleMail - Configuration"),
		WindowContents, VGroup,
			Child, HGroup,
				Child, NListviewObject,
					MUIA_CycleChain, 1,
					MUIA_HorizWeight, 33,
					MUIA_NListview_NList, config_list = NListObject,
						End,
					End,
				Child, BalanceObject, End,
				Child, VGroup,
					ReadListFrame,
					InnerSpacing(6,6),
					MUIA_Background, MUII_PageBack,
					Child, config_group = VGroup,
						Child, groups[GROUPS_USER],
						Child, groups[GROUPS_ACCOUNT],
						Child, groups[GROUPS_RECEIVE],
						Child, groups[GROUPS_WRITE],
						Child, groups[GROUPS_READMISC],
						Child, groups[GROUPS_READ],
						Child, groups[GROUPS_READHTML],
						Child, groups[GROUPS_PHRASE],
						Child, groups[GROUPS_SIGNATURE],
						Child, groups[GROUPS_SPAM],
						Child, RectangleObject,
							MUIA_Weight, 1,
							End,
						End,
					End,
				End,
			Child, HorizLineObject,
			Child, HGroup,
				Child, save_button = MakeButton(Q_("?config:_Save")),
				Child, use_button = MakeButton(_("_Use")),
				Child, cancel_button = MakeButton(_("_Cancel")),
				End,
			End,
		End;

	if (config_wnd)
	{
		static char texts[GROUPS_MAX][200];
		int i;

		image_prefs_main_obj = PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/prefs_main", End;
		image_prefs_account_obj = PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/prefs_account", End;
		image_prefs_receive_obj = PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/prefs_receive", End;
		image_prefs_write_obj = PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/prefs_write", End;
		image_prefs_read_obj = PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/prefs_read", End;
		image_prefs_readplain_obj = PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/prefs_readplain", End;
		image_prefs_readhtml_obj = PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/prefs_readhtml", End;
		image_prefs_signature_obj = PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/prefs_signature", End;
		image_prefs_phrase_obj = PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/prefs_phrase", End;
		image_prefs_spam_obj = PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/prefs_spam", End;

		DoMethod(config_list, MUIM_NList_UseImage, image_prefs_main_obj,      GROUPS_USER, 0);
		DoMethod(config_list, MUIM_NList_UseImage, image_prefs_account_obj,   GROUPS_ACCOUNT, 0);
		DoMethod(config_list, MUIM_NList_UseImage, image_prefs_receive_obj,   GROUPS_RECEIVE, 0);
		DoMethod(config_list, MUIM_NList_UseImage, image_prefs_write_obj,     GROUPS_WRITE, 0);
		DoMethod(config_list, MUIM_NList_UseImage, image_prefs_read_obj,      GROUPS_READMISC, 0);
		DoMethod(config_list, MUIM_NList_UseImage, image_prefs_readplain_obj, GROUPS_READ, 0);
		DoMethod(config_list, MUIM_NList_UseImage, image_prefs_readhtml_obj,  GROUPS_READHTML, 0);
		DoMethod(config_list, MUIM_NList_UseImage, image_prefs_signature_obj, GROUPS_SIGNATURE, 0);
		DoMethod(config_list, MUIM_NList_UseImage, image_prefs_phrase_obj,    GROUPS_PHRASE, 0);
		DoMethod(config_list, MUIM_NList_UseImage, image_prefs_spam_obj,      GROUPS_SPAM, 0);

		sprintf(texts[GROUPS_USER],     "\033o[%d] %s", GROUPS_USER,     _("General"));
		sprintf(texts[GROUPS_ACCOUNT],  "\033o[%d] %s", GROUPS_ACCOUNT,  _("Accounts"));
		sprintf(texts[GROUPS_RECEIVE],  "\033o[%d] %s", GROUPS_RECEIVE,  _("Receive mail"));
		sprintf(texts[GROUPS_WRITE],    "\033o[%d] %s", GROUPS_WRITE,    _("Write"));
		sprintf(texts[GROUPS_READMISC], "\033o[%d] %s", GROUPS_READMISC, _("Reading"));
		sprintf(texts[GROUPS_READ],     "\033o[%d] %s", GROUPS_READ,     _("Reading plain"));
		sprintf(texts[GROUPS_READHTML], "\033o[%d] %s", GROUPS_READHTML, _("Reading HTML"));
		sprintf(texts[GROUPS_PHRASE],   "\033o[%d] %s", GROUPS_PHRASE,   _("Phrases"));
		sprintf(texts[GROUPS_SIGNATURE],"\033o[%d] %s", GROUPS_SIGNATURE,_("Signatures"));
		sprintf(texts[GROUPS_SPAM],     "\033o[%d] %s", GROUPS_SPAM,     _("Spam"));

		account_last_selected = NULL;
		signature_last_selected = NULL;
		phrase_last_selected = NULL;

		set(save_button, MUIA_ShortHelp, _("Save the configuration permanently."));
		set(use_button, MUIA_ShortHelp, _("Use the configuration, but don't save it."));
		set(cancel_button, MUIA_ShortHelp, _("Discards all the changes you have made."));

		DoMethod(App, OM_ADDMEMBER, config_wnd);
		DoMethod(config_wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, config_cancel);
		DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, config_cancel);
		DoMethod(use_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, config_use);
		DoMethod(save_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, config_save);
		DoMethod(config_list, MUIM_Notify, MUIA_NList_Active, MUIV_EveryTime, App, 3, MUIM_CallHook, &hook_standard, config_selected);

		for (i=0;i<GROUPS_MAX;i++)
		{
			DoMethod(config_list, MUIM_NList_InsertSingle, texts[i], MUIV_NList_Insert_Bottom);
		}

		set(config_list,MUIA_NList_Active,0);

		{
			struct account *account;

			account = (struct account*)list_first(&user.config.account_list);
			while (account)
			{
				struct account *new_account = account_duplicate(account);
				if (new_account)
				{
					DoMethod(account_account_list,MUIM_NList_InsertSingle,new_account,MUIV_NList_Insert_Bottom);
				}
				account = (struct account*)node_next(&account->node);
			}
			set(account_account_list, MUIA_NList_Active, 0);
		}

		{
			struct phrase *phrase;
			phrase = (struct phrase*)list_first(&user.config.phrase_list);

			while (phrase)
			{
				struct phrase *new_phrase = phrase_duplicate(phrase);
				if (new_phrase)
				{
					DoMethod(phrase_phrase_list,MUIM_NList_InsertSingle,new_phrase,MUIV_NList_Insert_Bottom);
				}
				phrase = (struct phrase*)node_next(&phrase->node);
			}
			set(phrase_phrase_list, MUIA_NList_Active, 0);
		}

		{
			struct signature *signature;
			signature = (struct signature*)list_first(&user.config.signature_list);

			while (signature)
			{
				struct signature *new_signature = signature_duplicate(signature);
				if (new_signature)
				{
					DoMethod(signature_signature_list,MUIM_NList_InsertSingle,new_signature,MUIV_NList_Insert_Bottom);
				}
				signature = (struct signature*)node_next(&signature->node);
			}
			set(signature_signature_list, MUIA_NList_Active, 0);
		}
	}
	SM_LEAVE;
}

/******************************************************************
 Open the config window
*******************************************************************/
void open_config(void)
{
	if (!config_wnd) init_config_window();
	if (config_wnd)
	{
		set(config_wnd, MUIA_Window_Open, TRUE);
	} else
	{
		SM_DEBUGF(5,("Config window not initialized!\n"));
	}
}

/******************************************************************
 Close and dispose the config window
*******************************************************************/
void close_config(void)
{
	if (config_wnd)
	{
		int i;

		set(config_wnd, MUIA_Window_Open, FALSE);
		DoMethod(App, OM_REMMEMBER, config_wnd);

		account_last_selected = NULL;
		phrase_last_selected = NULL;
		signature_last_selected = NULL;

		/* Free all accounts */
		for (i=0;i<xget(account_account_list,MUIA_NList_Entries);i++)
		{
			struct account *ac;
			DoMethod(account_account_list,MUIM_NList_GetEntry, i, &ac);
			account_free(ac);
		}

		DoMethod(config_list, MUIM_NList_UseImage, NULL, MUIV_NList_UseImage_All, 0);
		if (image_prefs_main_obj) MUI_DisposeObject(image_prefs_main_obj);
		if (image_prefs_account_obj) MUI_DisposeObject(image_prefs_account_obj);
		if (image_prefs_receive_obj) MUI_DisposeObject(image_prefs_receive_obj);
		if (image_prefs_write_obj) MUI_DisposeObject(image_prefs_write_obj);
		if (image_prefs_read_obj) MUI_DisposeObject(image_prefs_read_obj);
		if (image_prefs_readplain_obj) MUI_DisposeObject(image_prefs_readplain_obj);
		if (image_prefs_readhtml_obj) MUI_DisposeObject(image_prefs_readhtml_obj);
		if (image_prefs_signature_obj) MUI_DisposeObject(image_prefs_signature_obj);
		if (image_prefs_phrase_obj) MUI_DisposeObject(image_prefs_phrase_obj);
		if (image_prefs_spam_obj) MUI_DisposeObject(image_prefs_spam_obj);

		MUI_DisposeObject(config_wnd);
		config_wnd = NULL;
		config_last_visisble_group = NULL;
	}
	delete_sizes_class();
}
