AUTOMAKE_OPTIONS = foreign

bin_PROGRAMS = simplemail
INCLUDES = -I$(srcdir)/indep-include -I$(srcdir)/gtk

SUBDIRS=	gtk intl po

EXTRA_DIST=	diff_ignore

simplemail_SOURCES = 	acconfig.h \
			account.c \
			account.h \
			aclocal.m4 \
			addressbook.c \
			addressbook.h \
			codecs.c \
			codecs.h \
			codesets.c \
			codesets.h \
			codesets_table.h \
			configuration.c \
			configuration.h \
			debug.h \
			debug.c \
			estimate.c \
			estimate.h \
			filter.c \
			filter.h \
			folder.c \
			folder.h \
			hash.c \
			hash.h \
			hmac_md5.c \
			hmac_md5.h \
			http.c \
			http.h \
			imap.c \
			imap.h \
			lists.c \
			lists.h \
			mail.c \
			mail.h \
			mbox.c \
			mbox.h \
			md5.c \
			md5.h \
			parse.c \
			parse.h \
			pgp.c \
			pgp.h \
			phrase.c \
			phrase.h \
			pop3.c \
			pop3.h \
			print.c \
			print.h \
			signature.c \
			signature.h \
			simplemail.c \
			simplemail.h \
			smintl.c \
			smintl.h \
			smtp.c \
			smtp.h \
			spam.c \
			spam.h \
			status.c \
			status.h \
			support_indep.c \
			support_indep.h \
			taglines.c \
			taglines.h \
			tcp.c \
			tcp.h \
			text2html.c \
			text2html.h \
			trans.c \
			trans.h \
			version.h

simplemail_DEPENDENCIES = ./gtk/libgui.a
simplemail_LDADD        = ./gtk/libgui.a @GUI_LIBS@
