/**
 * index_unittest - a simple test for the simple string index interface for SimpleMail.
 * Copyright (C) 2012  Sebastian Bauer
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <CUnit/Basic.h>

#include "index.h"
#include "index_naive.h"
#include "index_external.h"

static const char zauberlehrling[] =
	"Hat der alte Hexenmeister\n"
	"sich doch einmal wegbegeben!\n"
	"Und nun sollen seine Geister\n"
	"auch nach meinem Willen leben.\n"
	"Seine Wort und Werke\n"
	"merkt ich und den Brauch,\n"
	"und mit Geistesstärke\n"
	"tu ich Wunder auch.\n\n"
	"Walle! walle\n"
	"Manche Strecke,\n"
	"daß, zum Zwecke,\n"
	"Wasser fließe\n"
	"und mit reichem, vollem Schwalle\n"
	"zu dem Bade sich ergieße.\n\n"
	"Und nun komm, du alter Besen!\n"
	"Nimm die schlechten Lumpenhüllen;\n"
	"bist schon lange Knecht gewesen:\n"
	"nun erfülle meinen Willen!\n"
	"Auf zwei Beinen stehe,\n"
	"oben sei ein Kopf,\n"
	"eile nun und gehe\n"
	"mit dem Wassertopf!\n\n"
	"Walle! walle\n"
	"manche Strecke,\n"
	"daß, zum Zwecke,\n"
	"Wasser fließe\n"
	"und mit reichem, vollem Schwalle\n"
	"zu dem Bade sich ergieße.\n\n"
	"Seht, er läuft zum Ufer nieder,\n"
	"Wahrlich! ist schon an dem Flusse,\n"
	"und mit Blitzesschnelle wieder\n"
	"ist er hier mit raschem Gusse.\n"
	"Schon zum zweiten Male!\n"
	"Wie das Becken schwillt!\n"
	"Wie sich jede Schale\n"
	"voll mit Wasser füllt!\n\n"
	"Stehe! stehe!\n"
	"denn wir haben\n"
	"deiner Gaben\n"
	"vollgemessen! -\n"
	"Ach, ich merk es! Wehe! wehe!\n"
	"Hab ich doch das Wort vergessen!\n\n"
	"Ach, das Wort, worauf am Ende\n"
	"er das wird, was er gewesen.\n"
	"Ach, er läuft und bringt behende!\n"
	"Wärst du doch der alte Besen!\n"
	"Immer neue Güsse\n"
	"bringt er schnell herein,\n"
	"Ach! und hundert Flüsse\n"
	"stürzen auf mich ein.\n\n"
	"Nein, nicht länger\n"
	"kann ichs lassen;\n"
	"will ihn fassen.\n"
	"Das ist Tücke!\n"
	"Ach! nun wird mir immer bänger!\n"
	"Welche Miene! welche Blicke!\n\n"
	"O du Ausgeburt der Hölle!\n"
	"Soll das ganze Haus ersaufen?\n"
	"Seh ich über jede Schwelle\n"
	"doch schon Wasserströme laufen.\n"
	"Ein verruchter Besen,\n"
	"der nicht hören will!\n"
	"Stock, der du gewesen,\n"
	"steh doch wieder still!\n\n"
	"Willst am Ende\n"
	"gar nicht lassen?\n"
	"Will dich fassen,\n"
	"will dich halten\n"
	"und das alte Holz behende\n"
	"mit dem scharfen Beile spalten.\n\n"
	"Seht da kommt er schleppend wieder!\n"
	"Wie ich mich nur auf dich werfe,\n"
	"gleich, o Kobold, liegst du nieder;\n"
	"krachend trifft die glatte Schärfe.\n"
	"Wahrlich, brav getroffen!\n"
	"Seht, er ist entzwei!\n"
	"Und nun kann ich hoffen,\n"
	"und ich atme frei!\n\n"
	"Wehe! wehe!\n"
	"Beide Teile\n"
	"stehn in Eile\n"
	"schon als Knechte\n"
	"völlig fertig in die Höhe!\n"
	"Helft mir, ach! ihr hohen Mächte!\n\n"
	"Und sie laufen! Naß und nässer\n"
	"wirds im Saal und auf den Stufen.\n"
	"Welch entsetzliches Gewässer!\n"
	"Herr und Meister! hör mich rufen! -\n"
	"Ach, da kommt der Meister!\n"
	"Herr, die Not ist groß!\n"
	"Die ich rief, die Geister\n"
	"werd ich nun nicht los.\n\n"
	"\"In die Ecke,\n"
	"Besen, Besen!\n"
	"Seids gewesen.\n"
	"Denn als Geister\n"
	"ruft euch nur zu diesem Zwecke,\n"
	"erst hervor der alte Meister.\"";

/*******************************************************/


/**
 * Read the content of the file given by the filename.
 *
 * @param filename the name of the file which should be read.
 *
 * @return the contents or NULL on an error. The returned
 *  value must be freed with free() when no longer in use.
 */
static char *read_file_contents(const char *filename)
{
	long size;
	char *ret = NULL;
	char *contents = NULL;
	FILE *fh;

	if (!(fh = fopen(filename,"r")))
		return NULL;

	fseek(fh,0,SEEK_END);
	size = ftell(fh);
	if (size < 1)
		goto out;
	fseek(fh,0,SEEK_SET);

	if (!(contents = malloc(size+1)))
		goto out;
	if ((fread(contents, 1, size, fh) != size))
		goto out;
	contents[size] = 0;

	ret = contents;
	contents = NULL;
out:
	fclose(fh);
	free(contents);
	return ret;
}


/*******************************************************/

static int test_index_naive_callback_called;

static int test_index_naive_callback(int did, void *userdata)
{
	CU_ASSERT(did==4);
	test_index_naive_callback_called = 1;
}

static int test_index_naive_callback2(int did, void *userdata)
{
	CU_ASSERT(0);
}

/*******************************************************/

static int test_index_contains_all_suffixes_callback(int did, void *userdata)
{
	(*(int*)userdata) = 1;
}

static void test_index_contains_all_suffixes(struct index *index, const char *text, int did)
{
	int i;
	int textl = strlen(text);
	for (i=0; i<textl; i++)
	{
		int called = 0;
		index_find_documents(index, test_index_contains_all_suffixes_callback, &called, 1, text + i);
		CU_ASSERT(called == 1);
		if (called != 1)
		{
			fprintf(stderr, "Offset %d couldn't be found\n", i);
		}
	}
}

/*******************************************************/

static void test_index_for_algorithm(struct index_algorithm *alg, const char *name)
{
	struct index *index;
	char *text;
	int ok;
	int nd;

	test_index_naive_callback_called = 0;

	index = index_create(alg, name);
	CU_ASSERT(index != NULL);

	ok = index_put_document(index,4,"This is a very long text.");
	CU_ASSERT(ok != 0);

	ok = index_put_document(index,12,"This is a short text.");
	CU_ASSERT(ok != 0);

	nd = index_find_documents(index,test_index_naive_callback,NULL,1,"very");
	CU_ASSERT(test_index_naive_callback_called == 1);
	CU_ASSERT(nd == 1);

	ok = index_put_document(index,20,zauberlehrling);
	CU_ASSERT(ok != 0);

	test_index_contains_all_suffixes(index, zauberlehrling, 20);

	ok = index_remove_document(index,4);
	CU_ASSERT(ok != 0);

	nd = index_find_documents(index,test_index_naive_callback2,NULL,1,"very");
	CU_ASSERT(nd == 0);

	text = read_file_contents("of-human-bondage.txt");
	CU_ASSERT(text != NULL);

	ok = index_put_document(index,32,text);
	CU_ASSERT(ok != 0);

	index_dispose(index);
	free(text);
}

/*******************************************************/

/* @Test */
void test_index_naive(void)
{
	test_index_for_algorithm(&index_naive, "naive-index.dat");
}

/*******************************************************/

/* @Test */
void test_index_external(void)
{
	test_index_for_algorithm(&index_external, "/tmp/external-index.dat");
}
