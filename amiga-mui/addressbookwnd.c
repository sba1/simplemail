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
** addressbookwnd.c
*/

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <libraries/iffparse.h> /* MAKE_ID */
#include <libraries/gadtools.h>
#include <libraries/mui.h>
#include <mui/nlistview_mcc.h>
#include <mui/betterstring_mcc.h>
#include <mui/texteditor_mcc.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#ifdef HAVE_OPENURL
#include <proto/openurl.h>
#endif

#include "addressbook.h"
#include "configuration.h"
#include "parse.h"
#include "pgp.h"
#include "simplemail.h"
#include "smintl.h"
#include "support.h"
#include "support_indep.h"

#include "addressbookwnd.h"
#include "addressentrylistclass.h"
#include "addressgrouplistclass.h"
#include "amigasupport.h"
#include "compiler.h"
#include "composeeditorclass.h"
#include "mainwnd.h"
#include "muistuff.h"
#include "pgplistclass.h"
#include "picturebuttonclass.h"
#include "utf8stringclass.h"

void addressbookwnd_store(void);

/* for the address window */
static Object *address_wnd;
static Object *address_menu;

static Object *group_list;
static Object *address_list;

static Object *show_member_check;

/**********************************************************************/

#define MAX_PERSON_OPEN 10
static int person_open[MAX_PERSON_OPEN];

struct Snail_Data
{
	Object *title_string;
	Object *organization_string;
	Object *street_string;
	Object *city_string;
	Object *zip_string;
	Object *state_string;
	Object *country_string;
	Object *phone1_string;
	Object *phone2_string;
	Object *mobil_string;
	Object *fax_string;
};

struct Person_Data /* should be a customclass */
{
	Object *wnd;
	Object *reg_group;
	Object *alias_string;
	Object *realname_string;
	Object *pgp_string;
	Object *homepage_string;
	Object *description_string;
	Object *female_button;
	Object *male_button;
	Object *birthday_string;
	Object *portrait_button;
	Object *email_texteditor;
	Object *portrait_string;
	Object *person_group_list;

	Object *group_wnd;
	Object *group_wnd_list;

	struct Snail_Data priv;
	struct Snail_Data work;

	struct addressbook_entry_new *person; /* NULL if new person */

	int num; /* the number of the window */
	/* more to add */
};

static char group_name_buf[64];
static struct Hook group_name_display_hook;

/********************************************
 Dislayfunction function for addressgroups
*********************************************/
STATIC ASM SAVEDS VOID group_name_display(REG(a0,struct Hook *h),REG(a2,Object *obj), REG(a1,struct NList_DisplayMessage *msg))
{
	char **array = msg->strings;
	char *group_name = (char *)msg->entry;

	if (group_name)
	{
		utf8tostr(group_name, group_name_buf, sizeof(group_name_buf), user.config.default_codeset);

		*array++ = group_name_buf;
	} else
	{
	 *array++ = _("Name");
	}
}

/******************************************************************
 Set the contents of this group
*******************************************************************/
static void setsnail(struct Snail_Data *data, struct address_snail_phone *asp)
{
	if (data->title_string) setutf8string(data->title_string,asp->title);
	if (data->organization_string) setutf8string(data->organization_string,asp->organization);
	if (data->street_string) setutf8string(data->street_string,asp->street);
	if (data->city_string) setutf8string(data->city_string,asp->city);
	if (data->zip_string) setutf8string(data->zip_string,asp->zip);
	if (data->state_string) setutf8string(data->state_string,asp->state);
	if (data->country_string) setutf8string(data->country_string,asp->country);
	if (data->phone1_string) setutf8string(data->phone1_string,asp->phone1);
	if (data->phone2_string) setutf8string(data->phone2_string,asp->phone2);
	if (data->mobil_string) setutf8string(data->mobil_string,asp->mobil);
	if (data->fax_string) setutf8string(data->fax_string,asp->fax);
}

/******************************************************************
 Addopt the Snail Phone changes
*******************************************************************/
static void adoptsnail(struct address_snail_phone *asp, struct Snail_Data *data)
{
	/* Safe to call free() with NULL */
	free(asp->title);asp->title = NULL;
	free(asp->organization);asp->organization = NULL;
	free(asp->street);
	free(asp->city);
	free(asp->zip);
	free(asp->state);
	free(asp->country);
	free(asp->phone1);
	free(asp->phone2);
	free(asp->mobil);
	free(asp->fax);

	if (data->title_string) asp->title = mystrdup(getutf8string(data->title_string));
	if (data->organization_string) asp->organization = mystrdup(getutf8string(data->organization_string));
	if (data->street_string) asp->street = mystrdup(getutf8string(data->street_string));
	if (data->city_string) asp->city = mystrdup(getutf8string(data->city_string));
	if (data->zip_string) asp->zip = mystrdup(getutf8string(data->zip_string));
	if (data->state_string) asp->state = mystrdup(getutf8string(data->state_string));
	if (data->country_string) asp->country = mystrdup(getutf8string(data->country_string));
	if (data->phone1_string) asp->phone1 = mystrdup(getutf8string(data->phone1_string));
	if (data->phone2_string) asp->phone2 = mystrdup(getutf8string(data->phone2_string));
	if (data->mobil_string) asp->mobil = mystrdup(getutf8string(data->mobil_string));
	if (data->fax_string) asp->fax = mystrdup(getutf8string(data->fax_string));
}

/******************************************************************
 Close group window
*******************************************************************/
static void person_group_window_close(struct Person_Data **pdata)
{
	struct Person_Data *data = *pdata;

	if (data->group_wnd)
	{
		set(data->group_wnd, MUIA_Window_Open, FALSE);
		DoMethod(App, OM_REMMEMBER, data->group_wnd);
		MUI_DisposeObject(data->group_wnd);
		data->group_wnd = NULL;
		data->group_wnd_list = NULL;
	}
}


/******************************************************************
 Opens a window with available groups
*******************************************************************/
static void person_group_added(struct Person_Data **pdata)
{
	struct Person_Data *data = *pdata;

	char *new_group_name;

	DoMethod(data->group_wnd_list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &new_group_name);
	if (new_group_name)
	{
		int found = 0, i;

		/* check if already belonging to the group */

		for (i=0;i<xget(data->person_group_list, MUIA_NList_Entries);i++)
		{
			char *group_name;

			DoMethod(data->person_group_list, MUIM_NList_GetEntry, i, &group_name);
			if (!mystrcmp(group_name,new_group_name))
			{
				found = 1;
				break;
			}
		}

		if (!found) DoMethod(data->person_group_list, MUIM_NList_InsertSingle, new_group_name, MUIV_NList_Insert_Sorted);
	}

	person_group_window_close(pdata);
}

/******************************************************************
 Opens a window with available groups
*******************************************************************/
static void person_add_group(struct Person_Data **pdata)
{
	struct Person_Data *data = *pdata;

	if (!data->group_wnd)
	{
		static char text[256];
		char *name;
		Object *ok_button, *cancel_button;

		name = getutf8string(data->realname_string);
		if (!name || !strlen(name)) name = getutf8string(data->alias_string);

		if (name)
		{
			static char iso_name[64];

			utf8tostr(name, iso_name,sizeof(iso_name), user.config.default_codeset);

			sm_snprintf(text,sizeof(text),_("Add \"%s\" to following groups"), iso_name);
		} else
		{
			sm_snprintf(text,sizeof(text),_("Add contact to following groups"));
		}

		data->group_wnd = WindowObject,
			MUIA_Window_Title, _("SimpleMail - Add Groups"),
			MUIA_Window_ID, MAKE_ID('S','A','G','R'),
			WindowContents, VGroup,
				Child, TextObject, MUIA_Text_PreParse, "\033c", MUIA_Text_Contents, text, End,
				Child, NListviewObject,
					MUIA_NListview_NList, data->group_wnd_list = NListObject,
					MUIA_NList_DisplayHook2, &group_name_display_hook,
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

		if (data->group_wnd)
		{
			int i;

			for (i=0;i<xget(group_list, MUIA_NList_Entries);i++)
			{
				struct addressbook_group *grp;
				DoMethod(group_list, MUIM_NList_GetEntry,i,&grp);
				if (grp)
				{
					DoMethod(data->group_wnd_list, MUIM_NList_InsertSingle, grp->name, MUIV_NList_Insert_Sorted);
				}
			}

			DoMethod(App, OM_ADDMEMBER, data->group_wnd);

			DoMethod(data->group_wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, person_group_window_close, data);
			DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, person_group_window_close, data);
			DoMethod(ok_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, person_group_added, data);
			DoMethod(data->group_wnd_list, MUIM_Notify, MUIA_NList_DoubleClick, TRUE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, person_group_added, data);
		}
	}

	if (data->group_wnd)
	{
		set(data->group_wnd, MUIA_Window_Open, TRUE);
	}
}

/******************************************************************
 This close and disposed the window (note: this must not be called
 within a normal callback hook, because the object is disposed in
 this function)!
*******************************************************************/
static void person_window_close(struct Person_Data **pdata)
{
	struct Person_Data *data = *pdata;

	person_group_window_close(pdata);

	set(data->wnd,MUIA_Window_Open,FALSE);
	DoMethod(App,OM_REMMEMBER,data->wnd);
	MUI_DisposeObject(data->wnd);
	if (data->num < MAX_PERSON_OPEN) person_open[data->num] = 0;
	free(data);
}

/******************************************************************
 This close and disposed the window (note: this must not be called
 within a normal callback hook, because the object is disposed in
 this function)!
 The person is added to the list (currently only at the end)
*******************************************************************/
static void person_window_ok(struct Person_Data **pdata)
{
	struct Person_Data *data = *pdata;
	struct addressbook_entry_new *new_entry;
	char *addresses,*dob;
	int day = 0, month = 0, year = 0;

	/* Check the validity of the e-mail addresses first */
	if ((addresses = (char*)DoMethod(data->email_texteditor, MUIM_TextEditor_ExportText)))
	{
		char *single_address;
		char *buf = addresses;
		while ((buf = parse_addr_spec(buf,&single_address)))
		{
			/* ensures that buf != NULL if no error */
			while (isspace((unsigned char)*buf)) buf++;
			if (*buf == 0) break;
			free(single_address);
		}
		if (!buf)
		{
			set(data->reg_group,MUIA_Group_ActivePage,0);
			set(data->wnd,MUIA_Window_ActiveObject,data->email_texteditor);
			DisplayBeep(NULL);
			FreeVec(addresses);
			return;
		}

		/* addresses will be freed below because it is needed again */
	}

	/* check the validiy of the date */
	dob = (char*)xget(data->birthday_string,MUIA_String_Contents);
	if (dob && *dob)
	{
		if (!(parse_date(dob,&day,&month,&year,NULL,NULL,NULL,NULL)))
		{
			set(data->reg_group,MUIA_Group_ActivePage,0);
			set(data->birthday_string,MUIA_Window_ActiveObject,data->email_texteditor);
			sm_request(NULL,_("The date of birth is given in a invalid format.\n"
                        "Please use dd mon yyyy or dd mm yyyy."),_("Ok"));
			DisplayBeep(NULL);
			return;
		}
	}
	

	set(data->wnd,MUIA_Window_Open,FALSE);

	if ((new_entry = malloc(sizeof(*new_entry))))
	{
		LONG pos;
		int i;

		memset(new_entry,0,sizeof(*new_entry));

		new_entry->realname = mystrdup(getutf8string(data->realname_string));
		new_entry->alias = mystrdup(getutf8string(data->alias_string));
		new_entry->description = mystrdup(getutf8string(data->description_string));

		adoptsnail(&new_entry->priv,&data->priv);
		adoptsnail(&new_entry->work,&data->work);

		new_entry->pgpid = mystrdup(getutf8string(data->pgp_string));
		new_entry->homepage = mystrdup(getutf8string(data->homepage_string));
		new_entry->portrait = mystrdup(getutf8string(data->portrait_string));

		if (addresses)
		{
			char *single_address;
			char *buf = addresses;
			while ((buf = parse_addr_spec(buf,&single_address)))
			{
				new_entry->email_array = array_add_string(new_entry->email_array, single_address);
				free(single_address);
			}
		}

		for (i=0;i<xget(data->person_group_list, MUIA_NList_Entries);i++)
		{
			char *group_name;
			DoMethod(data->person_group_list, MUIM_NList_GetEntry, i, &group_name);
			if (group_name) new_entry->group_array = array_add_string(new_entry->group_array,group_name);
		}

		if (xget(data->female_button,MUIA_Selected)) new_entry->sex = 1;
		else if (xget(data->male_button,MUIA_Selected)) new_entry->sex = 2;

		new_entry->dob_month = month;
		new_entry->dob_day = day;
		new_entry->dob_year = year;

		/* Remove old entry if it was a edit operation */
		if (data->person)
		{
			pos = MUIV_NList_GetPos_Start;
			DoMethod(address_list, MUIM_NList_GetPos, data->person, &pos);
			if (pos != MUIV_NList_GetPos_End) DoMethod(address_list, MUIM_NList_Remove, pos);
		}

		DoMethod(address_list, MUIM_NList_InsertSingle, new_entry, MUIV_NList_Insert_Sorted);

/*		if (new_entry)
		{
			pos = MUIV_NList_GetPos_Start;
			DoMethod(address_list, MUIM_NList_GetPos, new_entry, &pos);
			if (pos != MUIV_NList_GetPos_End) set(address_list, MUIA_NList_Active, pos);
		}*/

		addressbook_free_entry_new(new_entry);
	}

	person_window_close(pdata);

	/* free the the addresses memory */
	if (addresses) FreeVec(addresses);

	/* update the internal addressbook */
	cleanup_addressbook();
	addressbookwnd_store();
	main_build_addressbook();
}

/******************************************************************
 Try to open the homepage
*******************************************************************/
static void person_homepage(struct Person_Data **pdata)
{
	char *uri = (char*)xget((*pdata)->homepage_string,MUIA_String_Contents);

	if (uri)
	{
#ifdef HAVE_OPENURL
		struct Library *OpenURLBase;

		if ((OpenURLBase = OpenLibrary("openurl.library",0)))
		{
			URL_OpenA(uri,NULL);
			CloseLibrary(OpenURLBase);
		}
#endif
	}
}

/******************************************************************
 Portrait has changed
*******************************************************************/
static void person_portrait(struct Person_Data **pdata)
{
	struct Person_Data *data = *pdata;
	set(data->portrait_button, MUIA_PictureButton_Filename, (char*)xget(data->portrait_string,MUIA_String_Contents));
}

/******************************************************************
 Try to downloads the portait of the person
*******************************************************************/
static void person_download_portrait(struct Person_Data **pdata)
{
	struct Person_Data *data = *pdata;
	char *addresses;

	set(App, MUIA_Application_Sleep, TRUE);

	if ((addresses = (char*)DoMethod(data->email_texteditor, MUIM_TextEditor_ExportText)))
	{
		char *single_address;
		char *buf = addresses;
		while ((buf = parse_addr_spec(buf,&single_address)))
		{
			char *filename;
			if ((filename = addressbook_download_portrait(single_address)))
			{
				set(data->portrait_string, MUIA_String_Contents, filename);
				free(filename);
			}
			free(single_address);
		}
		FreeVec(addresses);
	}
	set(App, MUIA_Application_Sleep, FALSE);
}

/******************************************************************
 Hook function
*******************************************************************/
STATIC ASM SAVEDS ULONG person_pgp_strobj(REG(a0,struct Hook *h),REG(a2,Object *list),REG(a1,Object *str))
{
	DoMethod(list, MUIM_PGPList_Refresh);
	return 1;
}

/******************************************************************
 Hook function
******************************************************************/
STATIC ASM SAVEDS VOID person_pgp_objstr(REG(a0,struct Hook *h),REG(a2,Object *list),REG(a1,Object *str))
{
	struct pgp_key *key;
	DoMethod(list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &key);
	if (key)
	{
		DoMethod(str, MUIM_SetAsString, MUIA_String_Contents, "0x%08lX", key->keyid);
	}
}

/******************************************************************
 Opens a person window
*******************************************************************/
static void person_window_open(struct addressbook_entry_new *entry)
{
	Object *wnd, *reg_group, *email_texteditor;
	Object *alias_string, *realname_string, *ok_button, *cancel_button;
	Object *female_button, *male_button, *birthday_string, *homepage_string, *pgp_string, *homepage_button;
	Object *description_string, *download_button, *portrait_string, *portrait_button;
	Object *pgp_popobject, *pgp_list;
	Object *person_group_list, *add_to_group_button, *rem_from_group_button;
	struct Snail_Data priv, work;
	int num;

	static struct Hook pgp_strobj_hook;
	static struct Hook pgp_objstr_hook;

	static char *register_titles[5];
	static int register_titles_are_translated;

	if (!register_titles_are_translated)
	{
		register_titles[0] = _("Personal");
		register_titles[1] = _("Private");
		register_titles[2] = _("Work");
		register_titles[3] = _("Notes");
		register_titles_are_translated = 1;
	};

	init_hook(&pgp_strobj_hook, (HOOKFUNC)person_pgp_strobj);
	init_hook(&pgp_objstr_hook, (HOOKFUNC)person_pgp_objstr);

	init_hook(&group_name_display_hook,(HOOKFUNC)group_name_display);

	for (num=0; num < MAX_PERSON_OPEN; num++)
		if (!person_open[num]) break;

	priv.title_string = NULL;
	priv.organization_string = NULL;

	wnd = WindowObject,
		(num < MAX_PERSON_OPEN)?MUIA_Window_ID:TAG_IGNORE, MAKE_ID('P','E','R',num),
    MUIA_Window_Title, _("SimpleMail - Edit Person"),
        
		WindowContents, VGroup,
			Child, reg_group = RegisterGroup(register_titles),
				Child, VGroup,
					Child, HGroup,
						Child, ColGroup(2),
							MUIA_HorizWeight,170,
							Child, MakeLabel(_("Alias")),
							Child, alias_string = UTF8StringObject,
								StringFrame,
								MUIA_CycleChain, 1,
								MUIA_String_AdvanceOnCR, TRUE,
								MUIA_UTF8String_Charset, user.config.default_codeset,
								End,
							Child, MakeLabel(_("Real Name")),
							Child, realname_string = UTF8StringObject,
								StringFrame,
								MUIA_CycleChain, 1,
								MUIA_String_AdvanceOnCR, TRUE,
								MUIA_UTF8String_Charset, user.config.default_codeset,
								End,
							Child, MakeLabel(_("Description")),
							Child, description_string = UTF8StringObject,
								StringFrame,
								MUIA_CycleChain, 1,
								MUIA_String_AdvanceOnCR, TRUE,
								MUIA_UTF8String_Charset, user.config.default_codeset,
								End,
							Child, MakeLabel(_("PGP Key-ID")),
							Child, pgp_popobject = PopobjectObject,
								MUIA_Popstring_Button, PopButton(MUII_PopUp),
								MUIA_Popstring_String, pgp_string = UTF8StringObject,
									StringFrame,
									MUIA_CycleChain, 1,
									MUIA_String_AdvanceOnCR, TRUE,
									End,
								MUIA_Popobject_StrObjHook, &pgp_strobj_hook,
								MUIA_Popobject_ObjStrHook, &pgp_objstr_hook,
								MUIA_Popobject_Object, NListviewObject,
									MUIA_NListview_NList, pgp_list = PGPListObject,
										MUIA_NList_Title, FALSE,
										End,
									End,
								End,
							Child, MakeLabel(_("Homepage")),
							Child, HGroup,
								MUIA_Group_Spacing, 0,
								Child, homepage_string = UTF8StringObject,
									StringFrame,
									MUIA_CycleChain, 1,
									MUIA_String_AdvanceOnCR, TRUE,
									MUIA_UTF8String_Charset, user.config.default_codeset,
									End,
								Child, homepage_button = PopButton(MUII_TapeRecord),
								End,
							Child, MakeLabel(_("Date of birth")),
							Child, HGroup,
								Child, birthday_string = BetterStringObject,
									StringFrame,
									MUIA_CycleChain, 1,
									MUIA_String_AdvanceOnCR, TRUE,
									MUIA_ShortHelp,_("The date of the birth of the person.\nThe format is dd mon yyyy or dd mm yyyy."),
									End,
								Child, MakeLabel(_("Sex")),

								Child, HGroup,
									MUIA_Group_Spacing,1,
									Child, female_button = ImageObject,
										MUIA_CycleChain,1,
										MUIA_InputMode, MUIV_InputMode_Toggle,
										MUIA_Image_Spec, MUII_RadioButton,
										MUIA_ShowSelState, FALSE,
										End,
									Child, MakeLabel(_("Female")),
									End,
								Child, HGroup,
									MUIA_Group_Spacing,1,
									Child, male_button = ImageObject,
										MUIA_CycleChain,1,
										MUIA_InputMode, MUIV_InputMode_Toggle,
										MUIA_Image_Spec, MUII_RadioButton,
										MUIA_ShowSelState, FALSE,
										End,
									Child, MakeLabel(_("Male")),
									End,
								End,
							End,
						Child, VGroup,
							Child, HGroup,
								Child, MakeLabel(_("Portrait")),
								Child, HGroup,
									MUIA_Group_Spacing, 0,
									Child, PopaslObject,
										MUIA_Popstring_String, portrait_string = UTF8StringObject,
											MUIA_CycleChain,1,
											StringFrame,
											MUIA_UTF8String_Charset, user.config.default_codeset,
											End,
										MUIA_Popstring_Button, PopButton(MUII_PopFile),
										End,
									Child, download_button = PopButton(MUII_Disk),
									End,
								End,
							Child, portrait_button = PictureButtonObject,
								MUIA_PictureButton_FreeVert, TRUE,
								End,
							End,
						End,
					Child, VGroup,
						Child, HGroup,
							Child, VGroup,
								Child, HorizLineTextObject(_("E-Mail addresses")),
								Child, email_texteditor = ComposeEditorObject,
									InputListFrame,
									MUIA_CycleChain,1,
									End,
								End,
							Child, BalanceObject, End,
							Child, VGroup,
								MUIA_Weight, 50,
								Child, HorizLineTextObject(_("Belonging groups")),
								Child, NListviewObject,
									MUIA_NListview_NList, person_group_list = NListObject,
										MUIA_NList_ConstructHook, MUIV_NList_ConstructHook_String,
										MUIA_NList_DestructHook, MUIV_NList_DestructHook_String,
										MUIA_NList_DisplayHook2, &group_name_display_hook,
										End,
									End,
								Child, HGroup,
									Child, add_to_group_button = MakeButton(_("Add")),
									Child, rem_from_group_button = MakeButton(_("Remove")),
									End,
								End,
							End,
						End,
					End,

				Child, VGroup,
					Child, HVSpace,
					Child, HGroup,
						Child, MakeLabel(_("Street")),
						Child, priv.street_string = UTF8StringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							MUIA_UTF8String_Charset, user.config.default_codeset,
							End,
						End,
					Child, ColGroup(4),
						Child, MakeLabel(_("City")),
						Child, priv.city_string = UTF8StringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							MUIA_UTF8String_Charset, user.config.default_codeset,
							End,
						Child, MakeLabel(_("ZIP/Postal Code")),
						Child, priv.zip_string = UTF8StringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							MUIA_UTF8String_Charset, user.config.default_codeset,
							End,
						Child, MakeLabel(_("State/Province")),
						Child, priv.state_string = UTF8StringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							MUIA_UTF8String_Charset, user.config.default_codeset,
							End,
						Child, MakeLabel(_("Country")),
						Child, priv.country_string = UTF8StringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							MUIA_UTF8String_Charset, user.config.default_codeset,
							End,
						End,

					Child, HVSpace,
					Child, HorizLineObject,
					Child, HVSpace,

					Child, ColGroup(4),
						Child, MakeLabel(_("Phone numbers")),
						Child, priv.phone1_string = UTF8StringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							MUIA_UTF8String_Charset, user.config.default_codeset,
							End,
						Child, MakeLabel(_("Mobile")),
						Child, priv.mobil_string = UTF8StringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							MUIA_UTF8String_Charset, user.config.default_codeset,
							End,
						Child, HSpace(0),
						Child, priv.phone2_string = UTF8StringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							MUIA_UTF8String_Charset, user.config.default_codeset,
							End,
						Child, MakeLabel(_("Fax")),
						Child, priv.fax_string = UTF8StringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							MUIA_UTF8String_Charset, user.config.default_codeset,
							End,
						End,
					Child, HVSpace,
					End,

				Child, VGroup,
					Child, HVSpace,
					Child, HGroup,
						Child, MakeLabel(_("Title")),
						Child, work.title_string = UTF8StringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							MUIA_UTF8String_Charset, user.config.default_codeset,
							End,
						Child, MakeLabel(_("Organization")),
						Child, work.organization_string = UTF8StringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							MUIA_UTF8String_Charset, user.config.default_codeset,
							End,
						End,
					Child, HVSpace,
					Child, HorizLineObject,
					Child, HVSpace,
					Child, HGroup,
						Child, MakeLabel(_("Street")),
						Child, work.street_string = UTF8StringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							MUIA_UTF8String_Charset, user.config.default_codeset,
							End,
						End,
					Child, ColGroup(4),
						Child, MakeLabel(_("City")),
						Child, work.city_string = UTF8StringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							MUIA_UTF8String_Charset, user.config.default_codeset,
							End,
						Child, MakeLabel(_("ZIP/Postal Code")),
						Child, work.zip_string = UTF8StringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							MUIA_UTF8String_Charset, user.config.default_codeset,
							End,
						Child, MakeLabel(_("State/Province")),
						Child, work.state_string = UTF8StringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							MUIA_UTF8String_Charset, user.config.default_codeset,
							End,
						Child, MakeLabel(_("Country")),
						Child, work.country_string = UTF8StringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							MUIA_UTF8String_Charset, user.config.default_codeset,
							End,
						End,

					Child, HVSpace,
					Child, HorizLineObject,
					Child, HVSpace,

					Child, ColGroup(4),
						Child, MakeLabel(_("Phone numbers")),
						Child, work.phone1_string = UTF8StringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							MUIA_UTF8String_Charset, user.config.default_codeset,
							End,
						Child, MakeLabel(_("Mobile")),
						Child, work.mobil_string = UTF8StringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							MUIA_UTF8String_Charset, user.config.default_codeset,
							End,
						Child, HSpace(0),
						Child, work.phone2_string = UTF8StringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							MUIA_UTF8String_Charset, user.config.default_codeset,
							End,
						Child, MakeLabel(_("Fax")),
						Child, work.fax_string = UTF8StringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							MUIA_UTF8String_Charset, user.config.default_codeset,
							End,
						End,
					Child, HVSpace,
					End,

				Child, VGroup,
					Child, TextEditorObject,
						InputListFrame,
						MUIA_CycleChain,1,
						End,
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
		struct Person_Data *data = (struct Person_Data*)malloc(sizeof(struct Person_Data));
		if (data)
		{
			data->wnd = wnd;
			data->alias_string = alias_string;
			data->email_texteditor = email_texteditor;
			data->female_button = female_button;
			data->male_button = male_button;
			data->birthday_string = birthday_string;
			data->realname_string = realname_string;
			data->description_string = description_string;
			data->pgp_string = pgp_string;
			data->homepage_string = homepage_string;
			data->portrait_button = portrait_button;
			data->portrait_string = portrait_string;
			data->person_group_list = person_group_list;
			data->reg_group = reg_group;
			data->priv = priv;
			data->work = work;
			data->person = entry;
			data->num = num;
			data->group_wnd = NULL;
			data->group_wnd_list = NULL;

			/* mark the window as opened */
			person_open[num] = 1;

			if (entry && entry->group_array)
			{
				char *group_name;
				int i = 0;

				while ((group_name = entry->group_array[i]))
				{
					DoMethod(data->person_group_list, MUIM_NList_InsertSingle, group_name, MUIV_NList_Insert_Sorted);
					i++;
				}
			}

			DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, person_window_close, data);
			DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, person_window_close, data);
			DoMethod(ok_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, person_window_ok, data);
			DoMethod(add_to_group_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, person_add_group, data);
			DoMethod(rem_from_group_button, MUIM_Notify, MUIA_Pressed, FALSE, person_group_list, 2, MUIM_NList_Remove, MUIV_NList_Remove_Active);
			DoMethod(download_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, person_download_portrait, data);
			DoMethod(homepage_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, person_homepage, data);
			DoMethod(female_button, MUIM_Notify, MUIA_Selected, TRUE, male_button, 3, MUIM_Set, MUIA_Selected, FALSE);
			DoMethod(male_button, MUIM_Notify, MUIA_Selected, TRUE, female_button, 3, MUIM_Set, MUIA_Selected, FALSE);
			DoMethod(portrait_string, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime, App, 4, MUIM_CallHook, &hook_standard, person_portrait, data);
			DoMethod(pgp_list, MUIM_Notify, MUIA_NList_DoubleClick, TRUE, pgp_popobject, 2, MUIM_Popstring_Close, TRUE);
			DoMethod(App,OM_ADDMEMBER,wnd);

			/* A person must be changed */
			if (entry)
			{
				if (entry->email_array)
					set(email_texteditor,MUIA_ComposeEditor_Array, entry->email_array);

				setutf8string(realname_string, entry->realname);
				setutf8string(description_string, entry->description);
				setutf8string(pgp_string, entry->pgpid);
				setutf8string(alias_string, entry->alias);
				setutf8string(homepage_string, entry->homepage);
				setutf8string(portrait_string, entry->portrait);

				if (entry->sex == 1) set(data->female_button,MUIA_Selected,TRUE);
				else if (entry->sex == 2) set(data->male_button,MUIA_Selected,TRUE);

				if (entry->dob_day && ((unsigned int)(entry->dob_month-1)) < 12)
				{
					char buf[64];
					static const char *month_names[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
					sprintf(buf,"%d %s %d",entry->dob_day,month_names[entry->dob_month-1],entry->dob_year);
					set(birthday_string,MUIA_String_Contents,buf);
				}

				if (entry->portrait) set(portrait_button, MUIA_PictureButton_Filename, entry->portrait);

				setsnail(&priv,&entry->priv);
				setsnail(&work,&entry->work);
			}

			set(wnd,MUIA_Window_ActiveObject,alias_string);
			set(wnd,MUIA_Window_Open,TRUE);

			return;
		}
		MUI_DisposeObject(wnd);
	}
}



/**********************************************************************/


#define MAX_GROUP_OPEN 10
static int group_open[MAX_GROUP_OPEN];

struct Group_Data /* should be a customclass */
{
	Object *wnd;
	Object *alias_string;
	Object *description_string;

	struct addressbook_group *group; /* NULL if new group */

	int num; /* the number of the window */
	/* more to add */
};

/******************************************************************
 This close and disposed the window (note: this must not be called
 within a normal callback hook, because the object is disposed in
 this function)!
*******************************************************************/
static void group_window_close(struct Group_Data **pdata)
{
	struct Group_Data *data = *pdata;
	set(data->wnd,MUIA_Window_Open,FALSE);
	DoMethod(App,OM_REMMEMBER,data->wnd);
	MUI_DisposeObject(data->wnd);
	if (data->num < MAX_GROUP_OPEN) group_open[data->num] = 0;
	free(data);
}

/******************************************************************
 This close and disposed the window (note: this must not be called
 within a normal callback hook, because the object is disposed in
 this function)!
 The group is added to the list.
*******************************************************************/
static void group_window_ok(struct Group_Data **pdata)
{
	struct Group_Data *data = *pdata;
	struct addressbook_group *group, *new_group;
	struct addressbook_entry_new *address_entry;
	int i, group_entries, address_entries, needs_entry_refresh = 0;

	/* Check if choosen group name already exists */
	group_entries = xget(group_list, MUIA_NList_Entries);
	for (i=0;i<group_entries;i++)
	{
		DoMethod(group_list, MUIM_NList_GetEntry, i, &group);
		if (group == data->group) continue;
		if (!utf8stricmp(group->name,getutf8string(data->alias_string)))
		{
			sm_request(NULL,_("Group with the given name \"%s\" already exists"),_("OK"),group->name);
			set(data->wnd, MUIA_Window_ActiveObject, data->alias_string);
			return;
		}
	}

	set(data->wnd, MUIA_Window_Open, FALSE);

	if ((new_group = malloc(sizeof(*new_group))))
	{
		memset(new_group,0,sizeof(*new_group));

		new_group->name = mystrdup(getutf8string(data->alias_string));
		new_group->description = mystrdup(getutf8string(data->description_string));

		if (data->group)
		{
			LONG pos = MUIV_NList_GetPos_Start;
			DoMethod(group_list, MUIM_NList_GetPos, data->group, &pos);

			if (pos != MUIV_NList_GetPos_End)
			{
				/* now rename groups within the address entries */	
				address_entries = xget(address_list, MUIA_NList_Entries);
				for (i=0;i<address_entries;i++)
				{
					DoMethod(address_list, MUIM_NList_GetEntry, i, &address_entry);
					if (address_entry)
					{
						int idx = array_index(address_entry->group_array, data->group->name);
						if (idx != -1)
						{
							array_replace_idx(address_entry->group_array, idx, new_group->name);
							needs_entry_refresh = 1;
						}
					}
				}
				
				/* Remove old group */
				DoMethod(group_list, MUIM_NList_Remove, pos);
			}
		}

		DoMethod(group_list, MUIM_NList_InsertSingle, new_group, MUIV_NList_Insert_Sorted);

		/* Activate the element inserted above */
		set(group_list,MUIA_NList_Active,xget(group_list,MUIA_NList_InsertPosition));

		addressbook_free_group(new_group);
	}

	group_window_close(pdata);

	/* update the internal addressbook */
	cleanup_addressbook();
	addressbookwnd_store();
	main_build_addressbook();

	if (needs_entry_refresh)
		DoMethod(address_list,MUIM_NList_Redraw,MUIV_NList_Redraw_All);
}

/******************************************************************
 Opens a group window
*******************************************************************/
static void group_window_open(struct addressbook_group *group)
{
	Object *wnd;
	Object *alias_string, *ok_button, *cancel_button;
	Object *description_string;
	int num;

	for (num=0; num < MAX_GROUP_OPEN; num++)
		if (!group_open[num]) break;

	wnd = WindowObject,
		(num < MAX_GROUP_OPEN)?MUIA_Window_ID:TAG_IGNORE, MAKE_ID('G','R','P',num),
    MUIA_Window_Title, _("SimpleMail - Edit Group"),
        
		WindowContents, VGroup,
			Child, VGroup,
				Child, ColGroup(2),
					Child, MakeLabel(_("_Name")),
					Child, alias_string = UTF8StringObject,
						StringFrame,
						MUIA_CycleChain, 1,
						MUIA_UTF8String_Charset, user.config.default_codeset,
						MUIA_ControlChar, GetControlChar(_("_Name")),
						End,

					Child, MakeLabel(_("_Description")),
					Child, description_string = UTF8StringObject,
						StringFrame,
						MUIA_CycleChain, 1,
						MUIA_UTF8String_Charset, user.config.default_codeset,
						MUIA_ControlChar, GetControlChar(_("_Description")),
						End,
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
		struct Group_Data *data = (struct Group_Data*)malloc(sizeof(struct Group_Data));
		if (data)
		{
			data->wnd = wnd;
			data->alias_string = alias_string;
			data->description_string = description_string;
			data->group = group;
			data->num = num;

			/* mark the window as opened */
			group_open[num] = 1;

			DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, group_window_close, data);
			DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, group_window_close, data);
			DoMethod(ok_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, group_window_ok, data);
			DoMethod(App,OM_ADDMEMBER,wnd);

			/* A group must be changed */
			if (group)
			{
				setutf8string(alias_string, group->name);
				setutf8string(description_string, group->description);
			}

			set(wnd,MUIA_Window_ActiveObject,alias_string);
			set(wnd,MUIA_Window_Open,TRUE);
			return;
		}
		MUI_DisposeObject(wnd);
	}
}


/**********************************************************************/

/* the address window functions */

/******************************************************************
 Updates the internal address book
*******************************************************************/
void addressbookwnd_store(void)
{
	int i;

	for (i=0;i<xget(group_list,MUIA_NList_Entries);i++)
	{
		struct addressbook_group *grp;

		DoMethod(group_list, MUIM_NList_GetEntry, i, &grp);
		addressbook_add_group_duplicate(grp);
	}

	DoMethod(address_list, MUIM_AddressEntryList_Store);
}

/******************************************************************
 Save the addressbook to disk
*******************************************************************/
static void addressbookwnd_save_pressed(void)
{
	cleanup_addressbook();
	addressbookwnd_store();
	addressbook_save();
}

/******************************************************************
 Adds a new person to the window
*******************************************************************/
static void addressbookwnd_add_person(void)
{
	person_window_open(NULL);
}

/******************************************************************
 Adds a new group to the window
*******************************************************************/
static void addressbookwnd_add_group(void)
{
	group_window_open(NULL);
}

/******************************************************************

*******************************************************************/
static void addressbookwnd_refresh_persons(void)
{
	struct addressbook_group *group;

	DoMethod(group_list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &group);

	if (xget(show_member_check,MUIA_Selected) && group)
		set(address_list,MUIA_AddressEntryList_GroupName,group->name);
	else set(address_list,MUIA_AddressEntryList_GroupName, NULL);
}

/******************************************************************
 Remove selected group. Warn's if there a members of the group
*******************************************************************/
static void addressbookwnd_rem_group(void)
{
	struct addressbook_group *group;

	DoMethod(group_list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &group);

	if (group)
	{
		int i;
		int num = 0;
		int entries = xget(address_list,MUIA_NList_Entries);

		for (i=0;i<entries;i++)
		{
			struct addressbook_entry_new *entry;
			DoMethod(address_list,MUIM_NList_GetEntry, i, &entry);
			if (entry && array_contains(entry->group_array,group->name))
				num++;
		}

		if (num)
		{
			if (!sm_request(NULL,_("There are %d addresses belonging to the group.\n"
			                  "Do you really want to delete this group?"),_("Yes|No"), num))
				return;

			for (i=0;i<entries;i++)
			{
				struct addressbook_entry_new *entry;
				DoMethod(address_list,MUIM_NList_GetEntry, i, &entry);
				if (entry)
				{
					int idx = array_index(entry->group_array,group->name);
					if (idx != -1)
						array_remove_idx(entry->group_array,idx);
				}
			}
			DoMethod(address_list,MUIM_NList_Redraw,MUIV_NList_Redraw_All);
		}

		DoMethod(group_list, MUIM_NList_Remove, MUIV_NList_Remove_Active);

		/* update the internal addressbook */
		cleanup_addressbook();
		addressbookwnd_store();
		main_build_addressbook();
	}
}

/******************************************************************
 Opens a edut window for the current selected group
*******************************************************************/
static void addressbookwnd_edit_group(void)
{
	struct addressbook_group *group;

	DoMethod(group_list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &group);

	if (group)
		group_window_open(group);
}

/******************************************************************
 Change the current selected entry
*******************************************************************/
static void addressbookwnd_edit_person(void)
{
	struct addressbook_entry_new *entry;

	DoMethod(address_list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &entry);
	if (entry) person_window_open(entry);
}

/******************************************************************
 Removes the currently selected entry
*******************************************************************/
static void addressbookwnd_remove_person(void)
{
	DoMethod(address_list, MUIM_NList_Remove, MUIV_NList_Remove_Selected);

	/* update the internal addressbook */
	cleanup_addressbook();
	addressbookwnd_store();
	main_build_addressbook();
}

/******************************************************************
 Initialzes the addressbook window
*******************************************************************/
static void addressbookwnd_init(void)
{
	static const struct NewMenu nm_untranslated[] =
	{
		{NM_TITLE, N_("Project"), NULL, 0, 0, NULL},
		{NM_ITEM, N_("Import addressbook..."), NULL, 0, 0, (APTR)1},
		{NM_END, NULL, NULL, 0, 0, NULL}
	};

	static struct NewMenu *nm;

	Object *add_group_button, *edit_group_button, *rem_group_button;
	Object *add_contact_button, *edit_contact_button, *rem_contact_button;
	Object *save_button, *close_button;

	int i;

	/* translate the menu entries */
	if (!nm)
	{
		if (!(nm = malloc(sizeof(nm_untranslated)))) return;
		memcpy(nm,nm_untranslated,sizeof(nm_untranslated));

		for (i=0;i<ARRAY_LEN(nm_untranslated)-1;i++)
		{
			if (nm[i].nm_Label && nm[i].nm_Label != NM_BARLABEL)
			{
				nm[i].nm_Label = mystrdup(_(nm[i].nm_Label));
				if (nm[i].nm_Label[1] == ':') nm[i].nm_Label[1] = 0;
			}
		}
	}

	address_menu = MUI_MakeObject(MUIO_MenustripNM, nm, MUIO_MenustripNM_CommandKeyCheck);

	address_wnd = WindowObject,
		MUIA_HelpNode, "AB_W",
		MUIA_Window_ID, MAKE_ID('A','D','B','K'),
    MUIA_Window_Title, _("SimpleMail - Addressbook"),

		MUIA_Window_Menustrip, address_menu,
        
		WindowContents, VGroup,
			Child, HGroup,
				Child, VGroup,
					MUIA_Weight, 25,
					Child, HorizLineTextObject(_("Groups")),
					Child, NListviewObject,
						MUIA_CycleChain, 1,
						MUIA_NListview_NList, group_list = AddressGroupListObject, End,
						End,
					Child, HGroup,
						Child, add_group_button = MakeButton(_("Add...")),
						Child, edit_group_button = MakeButton(_("Edit...")),
						Child, rem_group_button = MakeButton(_("Remove")),
						End,
					End,
				Child, BalanceObject, End,
				Child, VGroup,
					Child, HorizLineTextObject(_("Contacts")),
					Child, HGroup,
						Child, HVSpace,
						Child, MakeLabel(_("Show members of selected group")),
						Child, show_member_check = MakeCheck(_("Show members of selected group"),FALSE),
						End,
					Child, NListviewObject,
						MUIA_CycleChain, 1,
						MUIA_NListview_NList, address_list = AddressEntryListObject, End,
						End,
					Child, HGroup,
						Child, add_contact_button = MakeButton(_("Add...")),
						Child, edit_contact_button = MakeButton(_("Edit...")),
						Child, rem_contact_button = MakeButton(_("Remove")),
						End,
					End,
				End,
			Child, HorizLineObject,
			Child, HGroup,
				Child, HVSpace,
				Child, save_button = MakeButton(Q_("?textbutton:_Save")),
				Child, HVSpace,
				Child, close_button = MakeButton(Q_("?textbutton:_Close")),
				Child, HVSpace,
				End,
			End,
		End;

	if (!address_wnd) return;
	DoMethod(App,OM_ADDMEMBER,address_wnd);
	DoMethod(address_wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, address_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
	DoMethod(save_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 3, MUIM_CallHook, &hook_standard, addressbookwnd_save_pressed);
	DoMethod(close_button, MUIM_Notify, MUIA_Pressed, FALSE, address_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
	DoMethod(address_wnd, MUIM_Notify, MUIA_Window_MenuAction, 1, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, callback_import_addressbook);

	DoMethod(add_group_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 3, MUIM_CallHook, &hook_standard, addressbookwnd_add_group);
	DoMethod(edit_group_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 3, MUIM_CallHook, &hook_standard, addressbookwnd_edit_group);
	DoMethod(rem_group_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 3, MUIM_CallHook, &hook_standard, addressbookwnd_rem_group);
	DoMethod(group_list, MUIM_Notify, MUIA_NList_DoubleClick, TRUE, App, 3, MUIM_CallHook, &hook_standard, addressbookwnd_edit_group);
	DoMethod(group_list, MUIM_Notify, MUIA_NList_Active, MUIV_EveryTime, App, 3, MUIM_CallHook, &hook_standard,  addressbookwnd_refresh_persons);

	DoMethod(show_member_check, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, App, 3, MUIM_CallHook, &hook_standard, addressbookwnd_refresh_persons);
	DoMethod(add_contact_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 3, MUIM_CallHook, &hook_standard, addressbookwnd_add_person);
	DoMethod(edit_contact_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 3, MUIM_CallHook, &hook_standard, addressbookwnd_edit_person);
	DoMethod(rem_contact_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 3, MUIM_CallHook, &hook_standard,  addressbookwnd_remove_person);
	DoMethod(address_list, MUIM_Notify, MUIA_NList_DoubleClick, MUIV_EveryTime, App, 3, MUIM_CallHook, &hook_standard, addressbookwnd_edit_person);
}

/******************************************************************
 Opens the addressbook
*******************************************************************/
void addressbookwnd_open(void)
{
	if (!address_wnd) addressbookwnd_init();
	if (address_wnd)
	{
		if (!xget(address_wnd, MUIA_Window_Open))
		{
			DoMethod(group_list, MUIM_AddressGroupList_Refresh, NULL);
			DoMethod(address_list, MUIM_AddressEntryList_Refresh, NULL);
		}
		set(address_wnd, MUIA_Window_Open, TRUE);
	}
}

/******************************************************************
 Opens a addressbook and let the given entry be edited
*******************************************************************/
void addressbookwnd_create_entry(struct addressbook_entry_new *entry)
{
	addressbookwnd_open();
	person_window_open(entry);
}

/******************************************************************
 Selects an address entry to the actual one
*******************************************************************/
int addressbookwnd_set_active_alias(char *alias)
{
	if (!address_wnd) addressbookwnd_init();
	if (address_wnd)
	{
		int i;
		int count = xget(address_list, MUIA_NList_Entries);

		for (i=0;i<count;i++)
		{
			struct addressbook_entry_new *entry;

			DoMethod(address_list, MUIM_NList_GetEntry, i, &entry);

			if (!mystricmp(alias,entry->alias)) 
			{
				set(address_list,MUIA_NList_Active, i);
				return 1;
			}
		}
	}
	return 0;
}

/******************************************************************
 Refreshs the addressbook
*******************************************************************/
void addressbookwnd_refresh(void)
{
	DoMethod(address_list, MUIM_AddressEntryList_Refresh, NULL);
	DoMethod(group_list, MUIM_AddressGroupList_Refresh);
}
