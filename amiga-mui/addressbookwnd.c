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
#include <libraries/mui.h>
#include <mui/nlistview_mcc.h>
#include <mui/nlisttree_mcc.h>
#include <mui/betterstring_mcc.h>
#include <mui/texteditor_mcc.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/openurl.h>

#include "addressbook.h"
#include "parse.h"
#include "simplemail.h"
#include "smintl.h"

#include "addressbookwnd.h"
#include "addresstreelistclass.h"
#include "compiler.h"
#include "composeeditorclass.h"
#include "mainwnd.h"
#include "muistuff.h"
#include "picturebuttonclass.h"
#include "support_indep.h"

static void addressbook_update(struct MUI_NListtree_TreeNode *treenode, struct addressbook_entry *group);
struct MUI_NListtree_TreeNode *FindListtreeUserData(Object *tree, APTR udata); /* in mainwnd.c */

/* for the address window */
static Object *address_wnd;
static Object *address_tree;

/* the Person Window */
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
	Object *alias_string;
	Object *realname_string;
	Object *homepage_string;
	Object *description_string;
	Object *female_button;
	Object *male_button;
	Object *birthday_string;
	Object *portrait_button;
	Object *email_texteditor;
	Object *portrait_string;

	struct Snail_Data priv;
	struct Snail_Data work;

	struct addressbook_entry *person; /* NULL if new person */

	int num; /* the number of the window */
	/* more to add */
};

/******************************************************************
 Set the contents of this group
*******************************************************************/
static void setsnail(struct Snail_Data *data, struct address_snail_phone *asp)
{
	if (data->title_string) setstring(data->title_string,asp->title);
	if (data->organization_string) setstring(data->organization_string,asp->organization);
	if (data->street_string) setstring(data->street_string,asp->street);
	if (data->city_string) setstring(data->city_string,asp->city);
	if (data->zip_string) setstring(data->zip_string,asp->zip);
	if (data->state_string) setstring(data->state_string,asp->state);
	if (data->country_string) setstring(data->country_string,asp->country);
	if (data->phone1_string) setstring(data->phone1_string,asp->phone1);
	if (data->phone2_string) setstring(data->phone2_string,asp->phone2);
	if (data->mobil_string) setstring(data->mobil_string,asp->mobil);
	if (data->fax_string) setstring(data->fax_string,asp->fax);
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

	if (data->title_string) asp->title = mystrdup((char*)xget(data->title_string,MUIA_String_Contents));
	if (data->organization_string) asp->organization = mystrdup((char*)xget(data->organization_string,MUIA_String_Contents));
	if (data->street_string) asp->street = mystrdup((char*)xget(data->street_string,MUIA_String_Contents));
	if (data->city_string) asp->city = mystrdup((char*)xget(data->city_string,MUIA_String_Contents));
	if (data->zip_string) asp->zip = mystrdup((char*)xget(data->zip_string,MUIA_String_Contents));
	if (data->state_string) asp->state = mystrdup((char*)xget(data->state_string,MUIA_String_Contents));
	if (data->country_string) asp->country = mystrdup((char*)xget(data->country_string,MUIA_String_Contents));
	if (data->phone1_string) asp->phone1 = mystrdup((char*)xget(data->phone1_string,MUIA_String_Contents));
	if (data->phone2_string) asp->phone2 = mystrdup((char*)xget(data->phone2_string,MUIA_String_Contents));
	if (data->mobil_string) asp->mobil = mystrdup((char*)xget(data->mobil_string,MUIA_String_Contents));
	if (data->fax_string) asp->fax = mystrdup((char*)xget(data->fax_string,MUIA_String_Contents));
}

/******************************************************************
 This close and disposed the window (note: this must not be called
 within a normal callback hook, because the object is disposed in
 this function)!
*******************************************************************/
static void person_window_close(struct Person_Data **pdata)
{
	struct Person_Data *data = *pdata;
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
	struct addressbook_entry *new_entry;
	char *addresses;

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
			set(data->wnd,MUIA_Window_ActiveObject,data->email_texteditor);
			DisplayBeep(NULL);
			FreeVec(addresses);
			return;
		}

		/* addresses will be freed below because it is needed again */
	}

	set(data->wnd,MUIA_Window_Open,FALSE);

	if ((new_entry = addressbook_create_person((char*)xget(data->realname_string,MUIA_String_Contents), NULL)))
	{
		char *alias;
		struct MUI_NListtree_TreeNode *treenode = NULL;

		alias = (char*)xget(data->alias_string,MUIA_String_Contents);
		if (alias && *alias) addressbook_set_alias(new_entry, alias);
		addressbook_set_description(new_entry, (char *)xget(data->description_string,MUIA_String_Contents));

		adoptsnail(&new_entry->u.person.priv,&data->priv);
		adoptsnail(&new_entry->u.person.work,&data->work);
		
		if (new_entry->u.person.homepage) free(new_entry->u.person.homepage);
		new_entry->u.person.homepage = mystrdup((char*)xget(data->homepage_string,MUIA_String_Contents));
		if (new_entry->u.person.portrait) free(new_entry->u.person.portrait);
		new_entry->u.person.portrait = mystrdup((char*)xget(data->portrait_string, MUIA_String_Contents));

		if (addresses)
		{
			char *single_address;
			char *buf = addresses;
			while ((buf = parse_addr_spec(buf,&single_address)))
			{
				addressbook_person_add_email(new_entry,single_address);
				free(single_address);
			}
		}

		if (xget(data->female_button,MUIA_Selected)) new_entry->u.person.sex = 1;
		else if (xget(data->male_button,MUIA_Selected)) new_entry->u.person.sex = 2;

    {
    	char *buf = (char*)xget(data->birthday_string,MUIA_String_Contents);
    	int day=0,month=0,year=0;
    	if ((parse_date(buf,&day,&month,&year,NULL,NULL,NULL)))
    	{
    		new_entry->u.person.dob_month = month;
    		new_entry->u.person.dob_day = day;
    		new_entry->u.person.dob_year = year;
    	}
    }

		if (data->person)
		{
			if ((treenode = FindListtreeUserData(address_tree, data->person)))
			{
				DoMethod(address_tree, MUIM_NListtree_Rename, treenode, new_entry, MUIV_NListtree_Rename_Flag_User);
#if 0
				/* Prior versions of NListree had problems with MUIM_NListtree_Rename, this code was used
				   as a workaround */
				APTR parent = (APTR)DoMethod(address_tree, MUIM_NListtree_GetEntry, treenode, MUIV_NListtree_GetEntry_Position_Parent,0);

				DoMethod(address_tree, MUIM_NListtree_Insert, "" /*name*/, new_entry, /*udata */
							   parent,treenode,0);
				DoMethod(address_tree, MUIM_NListtree_Remove, parent,treenode,0);
#endif
			}
		}

		if (!treenode)
		{
			/* Now add it to the listview (in the active list) */
			DoMethod(address_tree, MUIM_NListtree_Insert, "" /*name*/, new_entry, /*udata */
							   MUIV_NListtree_Insert_ListNode_ActiveFallback,MUIV_NListtree_Insert_PrevNode_Tail,0);
		}

		addressbook_free_entry(new_entry);
	}

	person_window_close(pdata);

	/* free the the addresses memory */
	if (addresses) FreeVec(addresses);

	/* update the internal addressbook */
	cleanup_addressbook();
	addressbook_update(NULL,NULL);	
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
		struct Library *OpenURLBase;

		if ((OpenURLBase = OpenLibrary("openurl.library",0)))
		{
			URL_OpenA(uri,NULL);
			CloseLibrary(OpenURLBase);
		}
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
 Opens a person window
*******************************************************************/
void person_window_open(struct addressbook_entry *entry)
{
	Object *wnd, *email_texteditor;
	Object *alias_string, *realname_string, *ok_button, *cancel_button;
	Object *female_button, *male_button, *birthday_string, *homepage_string, *homepage_button;
	Object *description_string, *download_button, *portrait_string, *portrait_button;
	struct Snail_Data priv, work;
	static const char *register_titles[] = {
		"Personal","Private","Work","Notes",NULL
	};
	int num;

	for (num=0; num < MAX_PERSON_OPEN; num++)
		if (!person_open[num]) break;

	priv.title_string = NULL;
	priv.organization_string = NULL;

	wnd = WindowObject,
		(num < MAX_PERSON_OPEN)?MUIA_Window_ID:TAG_IGNORE, MAKE_ID('P','E','R',num),
    MUIA_Window_Title, _("SimpleMail - Edit Person"),
        
		WindowContents, VGroup,
			Child, RegisterGroup(register_titles),
				Child, VGroup,
					Child, HGroup,
						Child, ColGroup(2),
							MUIA_HorizWeight,170,
							Child, MakeLabel(_("Alias")),
							Child, alias_string = BetterStringObject,
								StringFrame,
								MUIA_CycleChain, 1,
								MUIA_String_AdvanceOnCR, TRUE,
								End,
							Child, MakeLabel(_("Real Name")),
							Child, realname_string = BetterStringObject,
								StringFrame,
								MUIA_CycleChain, 1,
								MUIA_String_AdvanceOnCR, TRUE,
								End,
							Child, MakeLabel(_("Description")),
							Child, description_string = BetterStringObject,
								StringFrame,
								MUIA_CycleChain, 1,
								MUIA_String_AdvanceOnCR, TRUE,
								End,
/*							Child, MakeLabel("PGP Key-ID"),
							Child, BetterStringObject,
								StringFrame,
								MUIA_CycleChain, 1,
								MUIA_String_AdvanceOnCR, TRUE,
								End,*/
							Child, MakeLabel(_("Homepage")),
							Child, HGroup,
								MUIA_Group_Spacing, 0,
								Child, homepage_string = BetterStringObject,
									StringFrame,
									MUIA_CycleChain, 1,
									MUIA_String_AdvanceOnCR, TRUE,
									End,
								Child, homepage_button = PopButton(MUII_TapeRecord),
								End,
							Child, MakeLabel(_("Date of birth")),
							Child, HGroup,
								Child, birthday_string = BetterStringObject,
									StringFrame,
									MUIA_CycleChain, 1,
									MUIA_String_AdvanceOnCR, TRUE,
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
								Child, MakeLabel(_("Portrait")),
								Child, HGroup,
									MUIA_Group_Spacing, 0,
									Child, PopaslObject,
										MUIA_Popstring_String, portrait_string = BetterStringObject,MUIA_CycleChain,1,StringFrame, End,
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
						Child, HorizLineTextObject(_("E-Mail addresses")),
						Child, email_texteditor = ComposeEditorObject,
							InputListFrame,
							MUIA_CycleChain,1,
							End,
						End,

/*							Child, MakeLabel("Date of birth"),
							Child, BetterStringObject,
								StringFrame,
								MUIA_CycleChain, 1,
								MUIA_String_AdvanceOnCR, TRUE,
								End,
							End,*/
					End,

				Child, VGroup,
					Child, HVSpace,
					Child, HGroup,
						Child, MakeLabel(_("Street")),
						Child, priv.street_string = BetterStringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							End,
						End,
					Child, ColGroup(4),
						Child, MakeLabel(_("City")),
						Child, priv.city_string = BetterStringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							End,
						Child, MakeLabel(_("ZIP/Postal Code")),
						Child, priv.zip_string = BetterStringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							End,
						Child, MakeLabel(_("State/Province")),
						Child, priv.state_string = BetterStringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							End,
						Child, MakeLabel(_("Country")),
						Child, priv.country_string = BetterStringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							End,
						End,

					Child, HVSpace,
					Child, HorizLineObject,
					Child, HVSpace,

					Child, ColGroup(4),
						Child, MakeLabel(_("Phone numbers")),
						Child, priv.phone1_string = BetterStringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							End,
						Child, MakeLabel(_("Mobile")),
						Child, priv.mobil_string = BetterStringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							End,
						Child, HSpace(0),
						Child, priv.phone2_string = BetterStringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							End,
						Child, MakeLabel(_("Fax")),
						Child, priv.fax_string = BetterStringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							End,
						End,
					Child, HVSpace,
					End,

				Child, VGroup,
					Child, HVSpace,
					Child, HGroup,
						Child, MakeLabel(_("Title")),
						Child, work.title_string = BetterStringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							End,
						Child, MakeLabel(_("Organization")),
						Child, work.organization_string = BetterStringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							End,
						End,
					Child, HVSpace,
					Child, HorizLineObject,
					Child, HVSpace,
					Child, HGroup,
						Child, MakeLabel(_("Street")),
						Child, work.street_string = BetterStringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							End,
						End,
					Child, ColGroup(4),
						Child, MakeLabel(_("City")),
						Child, work.city_string = BetterStringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							End,
						Child, MakeLabel(_("ZIP/Postal Code")),
						Child, work.zip_string = BetterStringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							End,
						Child, MakeLabel(_("State/Province")),
						Child, work.state_string = BetterStringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							End,
						Child, MakeLabel(_("Country")),
						Child, work.country_string = BetterStringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							End,
						End,

					Child, HVSpace,
					Child, HorizLineObject,
					Child, HVSpace,

					Child, ColGroup(4),
						Child, MakeLabel(_("Phone numbers")),
						Child, work.phone1_string = BetterStringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							End,
						Child, MakeLabel(_("Mobile")),
						Child, work.mobil_string = BetterStringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							End,
						Child, HSpace(0),
						Child, work.phone2_string = BetterStringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
							End,
						Child, MakeLabel(_("Fax")),
						Child, work.fax_string = BetterStringObject,
							StringFrame,
							MUIA_CycleChain, 1,
							MUIA_String_AdvanceOnCR, TRUE,
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
			data->homepage_string = homepage_string;
			data->portrait_button = portrait_button;
			data->portrait_string = portrait_string;
			data->priv = priv;
			data->work = work;
			data->person = entry;
			data->num = num;

			/* mark the window as opened */
			person_open[num] = 1;

			DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, person_window_close, data);
			DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, person_window_close, data);
			DoMethod(ok_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, person_window_ok, data);
			DoMethod(download_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, person_download_portrait, data);
			DoMethod(homepage_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 4, MUIM_CallHook, &hook_standard, person_homepage, data);
			DoMethod(female_button, MUIM_Notify, MUIA_Selected, TRUE, male_button, 3, MUIM_Set, MUIA_Selected, FALSE);
			DoMethod(male_button, MUIM_Notify, MUIA_Selected, TRUE, female_button, 3, MUIM_Set, MUIA_Selected, FALSE);
			DoMethod(portrait_string, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime, App, 4, MUIM_CallHook, &hook_standard, person_portrait, data);
			DoMethod(App,OM_ADDMEMBER,wnd);

			/* A person must be changed */
			if (entry && entry->type == ADDRESSBOOK_ENTRY_PERSON)
			{
				int i;
				char **array = (char**)malloc(sizeof(char*)*(entry->u.person.num_emails+1));
				if (array)
				{
					for (i=0;i<entry->u.person.num_emails;i++)
						array[i] = entry->u.person.emails[i];
					array[i] = NULL;
					set(email_texteditor,MUIA_ComposeEditor_Array, array);
					free(array);
				}

				setstring(realname_string, entry->u.person.realname);
				setstring(description_string, entry->u.person.description);
				setstring(alias_string, entry->u.person.alias);
				setstring(homepage_string, entry->u.person.homepage);
				setstring(portrait_string, entry->u.person.portrait);

				if (entry->u.person.sex == 1) set(data->female_button,MUIA_Selected,TRUE);
				else if (entry->u.person.sex == 2) set(data->male_button,MUIA_Selected,TRUE);

				if (entry->u.person.dob_day && ((unsigned int)(entry->u.person.dob_month-1)) < 12)
				{
					char buf[64];
					static const char *month_names[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
					sprintf(buf,"%d %s %d",entry->u.person.dob_day,month_names[entry->u.person.dob_month-1],entry->u.person.dob_year);
					set(birthday_string,MUIA_String_Contents,buf);
				}

				if (entry->u.person.portrait) set(portrait_button,MUIA_PictureButton_Filename,entry->u.person.portrait);

				setsnail(&priv,&entry->u.person.priv);
				setsnail(&work,&entry->u.person.work);
			}

			set(wnd,MUIA_Window_ActiveObject,alias_string);
			set(wnd,MUIA_Window_Open,TRUE);

			return;
		}
		MUI_DisposeObject(wnd);
	}
}





/* the Group Window */
#define MAX_GROUP_OPEN 10
static int group_open[MAX_GROUP_OPEN];

struct Group_Data /* should be a customclass */
{
	Object *wnd;
	Object *alias_string;
	Object *description_string;

	struct addressbook_entry *group; /* NULL if new group */

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
 The group is added to the list (currently only at the end)
*******************************************************************/
static void group_window_ok(struct Group_Data **pdata)
{
	struct Group_Data *data = *pdata;
	struct addressbook_entry *new_entry;
	set(data->wnd,MUIA_Window_Open,FALSE);

	if ((new_entry = addressbook_create_group()))
	{
		struct MUI_NListtree_TreeNode *treenode = NULL;
		char *alias = (char*)xget(data->alias_string,MUIA_String_Contents);
		if (alias && *alias) addressbook_set_alias(new_entry, alias);
		addressbook_set_description(new_entry, (char *)xget(data->description_string,MUIA_String_Contents));

		if (data->group)
		{
			if ((treenode = FindListtreeUserData(address_tree, data->group)))
			{
				DoMethod(address_tree, MUIM_NListtree_Rename, treenode, new_entry, MUIV_NListtree_Rename_Flag_User);
			}
		}

		if (!treenode)
		{
			/* Now add it to the listview (in the active list) */
			DoMethod(address_tree, MUIM_NListtree_Insert, "" /*name*/, new_entry, /*udata */
						   MUIV_NListtree_Insert_ListNode_ActiveFallback,MUIV_NListtree_Insert_PrevNode_Tail,TNF_LIST|TNF_OPEN);
		}

		addressbook_free_entry(new_entry);
	}

	group_window_close(pdata);

	/* update the internal addressbook */
	cleanup_addressbook();
	addressbook_update(NULL,NULL);	
	main_build_addressbook();
}

/******************************************************************
 Opens a group window
*******************************************************************/
static void group_window_open(struct addressbook_entry *entry)
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
					Child, MakeLabel(_("Alias")),
					Child, alias_string = BetterStringObject,
						StringFrame,
						MUIA_CycleChain, 1,
						End,

					Child, MakeLabel(_("Description")),
					Child, description_string = BetterStringObject,
						StringFrame,
						MUIA_CycleChain, 1,
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
			data->group = entry;
			data->num = num;

			/* mark the window as opened */
			group_open[num] = 1;

			DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, group_window_close, data);
			DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, group_window_close, data);
			DoMethod(ok_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 7, MUIM_Application_PushMethod, App, 4, MUIM_CallHook, &hook_standard, group_window_ok, data);
			DoMethod(App,OM_ADDMEMBER,wnd);

			/* A group must be changed */
			if (entry && entry->type == ADDRESSBOOK_ENTRY_GROUP)
			{
				set(alias_string, MUIA_String_Contents, entry->u.group.alias);
				set(description_string, MUIA_String_Contents, entry->u.group.description);
			}

			set(wnd,MUIA_Window_ActiveObject,alias_string);
			set(wnd,MUIA_Window_Open,TRUE);
			return;
		}
		MUI_DisposeObject(wnd);
	}
}


/* the address window functions */

/******************************************************************
 Updates the internal address book
*******************************************************************/
static void addressbook_update(struct MUI_NListtree_TreeNode *treenode, struct addressbook_entry *group)
{
	struct addressbook_entry *entry;

	if (!treenode) treenode = (struct MUI_NListtree_TreeNode *)DoMethod(address_tree,
				MUIM_NListtree_GetEntry, MUIV_NListtree_GetEntry_ListNode_Root, MUIV_NListtree_GetEntry_Position_Head, 0);

	if (!treenode) return;

	while (treenode)
	{
		if ((entry = (struct addressbook_entry *)treenode->tn_User))
		{
			if (treenode->tn_Flags & TNF_LIST)
			{
				struct MUI_NListtree_TreeNode *tn = (struct MUI_NListtree_TreeNode *)DoMethod(address_tree,
						MUIM_NListtree_GetEntry, treenode, MUIV_NListtree_GetEntry_Position_Head, 0);
				struct addressbook_entry *new_group = addressbook_duplicate_entry(entry);
				
				if (new_group)
				{
					addressbook_insert_tail(group,new_group);
					if (tn) addressbook_update(tn,new_group);
				}
			} else
			{
				if (entry->type == ADDRESSBOOK_ENTRY_PERSON)
				{
					struct addressbook_entry *new_person = addressbook_duplicate_entry(entry);

					if (new_person)
						addressbook_insert_tail(group,new_person);
				}
			}
		}
		treenode = (struct MUI_NListtree_TreeNode*)DoMethod(address_tree, MUIM_NListtree_GetEntry, treenode, MUIV_NListtree_GetEntry_Position_Next,0);
	}
}

/******************************************************************
 Save the addressbook to disk
*******************************************************************/
static void addressbook_save_pressed(void)
{
	cleanup_addressbook();
	addressbook_update(NULL,NULL);
	addressbook_save();
}

/******************************************************************
 Adds a new person to the window
*******************************************************************/
static void addressbook_add_person(void)
{
	person_window_open(NULL);
}

/******************************************************************
 Adds a new group to the window
*******************************************************************/
static void addressbook_add_group(void)
{
	group_window_open(NULL);
}

/******************************************************************
 Change the current selected entry
*******************************************************************/
static void addressbook_change(void)
{
	struct MUI_NListtree_TreeNode *treenode = (struct MUI_NListtree_TreeNode *)
				DoMethod(address_tree, MUIM_NListtree_GetEntry, MUIV_NListtree_GetEntry_ListNode_Active, MUIV_NListtree_GetEntry_Position_Active, 0);

	if (treenode)
	{
		struct addressbook_entry *entry = (struct addressbook_entry *)treenode->tn_User;
		if (entry->type == ADDRESSBOOK_ENTRY_PERSON)
		{
			person_window_open(entry);
		} else if (entry->type == ADDRESSBOOK_ENTRY_GROUP)
		{
			group_window_open(entry);
		}
	}
}

/******************************************************************
 The to button has been clicked
*******************************************************************/
static void addressbook_to(void)
{
	struct MUI_NListtree_TreeNode *treenode = (struct MUI_NListtree_TreeNode *)
				DoMethod(address_tree, MUIM_NListtree_GetEntry, MUIV_NListtree_GetEntry_ListNode_Active, MUIV_NListtree_GetEntry_Position_Active, 0);

	if (treenode)
	{
		struct addressbook_entry *entry = (struct addressbook_entry *)treenode->tn_User;
		callback_write_mail_to(entry);
	}
}

/******************************************************************
 Removes the currently selected entry
*******************************************************************/
static void addressbook_delete(void)
{
	DoMethod(address_tree, MUIM_NListtree_Remove, MUIV_NListtree_Remove_ListNode_Active, MUIV_NListtree_Remove_TreeNode_Active, 0);

	/* update the internal addressbook */
	cleanup_addressbook();
	addressbook_update(NULL,NULL);	
	main_build_addressbook();
}

/******************************************************************
 Initialzes the addressbook window
*******************************************************************/
static void addressbook_init(void)
{
	Object *new_person_button, *change_button, *new_group_button, *save_button, *delete_button;
	Object *to_button;

	address_wnd = WindowObject,
		MUIA_Window_ID, MAKE_ID('A','D','B','K'),
    MUIA_Window_Title, _("SimpleMail - Addressbook"),
        
		WindowContents, VGroup,
			Child, HGroup,
				Child, VGroup,
					MUIA_Weight,0,
					Child, to_button = MakeButton(_("_To:")),
					Child, MakeButton(_("_CC:")),
					Child, MakeButton(_("_BCC:")),
					End,
				Child, VGroup,
					Child, HVSpace,
					Child, HGroup,
						MUIA_VertWeight,0,
						Child, save_button = MakePictureButton(_("_Save"),"PROGDIR:Images/Save"),
/*						Child, MakeButton("Search"),*/
						Child, new_person_button = MakePictureButton(_("_Person"),"PROGDIR:Images/User"),
/*						Child, MakeButton("New List"),*/
						Child, new_group_button = MakePictureButton(_("_Group"),"PROGDIR:Images/Group"),
						Child, change_button = MakePictureButton(_("_Edit"),"PROGDIR:Images/Edit"),
						Child, delete_button = MakePictureButton(_("_Delete"),"PROGDIR:Images/Delete"),
						Child, HVSpace,
						End,
					Child, HVSpace,
					End,
				End,
			Child, HGroup,
				Child, NListviewObject,
					MUIA_CycleChain, 1,
					MUIA_NListview_NList, address_tree = AddressTreelistObject, 
						MUIA_AddressTreelist_InAddressbook, TRUE,
						End,
					End,
				End,
			End,
		End;

	if (!address_wnd) return;
	DoMethod(App,OM_ADDMEMBER,address_wnd);
	DoMethod(address_wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, address_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
	DoMethod(save_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 3, MUIM_CallHook, &hook_standard, addressbook_save_pressed);
	DoMethod(new_person_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 3, MUIM_CallHook, &hook_standard, addressbook_add_person);
	DoMethod(new_group_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 3, MUIM_CallHook, &hook_standard, addressbook_add_group);
	DoMethod(change_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 3, MUIM_CallHook, &hook_standard, addressbook_change);
	DoMethod(delete_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 3, MUIM_CallHook, &hook_standard, addressbook_delete);
	DoMethod(to_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 3, MUIM_CallHook, &hook_standard, addressbook_to);
	DoMethod(address_tree, MUIM_Notify, MUIA_NListtree_DoubleClick, MUIV_EveryTime, App, 3, MUIM_CallHook, &hook_standard, addressbook_change);
}

/******************************************************************
 Opens a addressbook
*******************************************************************/
void addressbook_open(void)
{
	if (!address_wnd) addressbook_init();
	if (address_wnd)
	{
		if (!xget(address_wnd, MUIA_Window_Open))
			DoMethod(address_tree, MUIM_AddressTreelist_Refresh,NULL);
		set(address_wnd, MUIA_Window_Open, TRUE);
	}
}

/******************************************************************
 Opens the addressbook with a new person taken from the mail
*******************************************************************/
void addressbook_open_with_new_address(struct mail *m)
{
	struct addressbook_entry *entry = addressbook_get_entry_from_mail(m);
	if (entry)
	{
		addressbook_open();
		person_window_open(entry);
		addressbook_free_entry(entry);
	}
}
