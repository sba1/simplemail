bin_PROGRAMS = simplemail
INCLUDES = -I$(srcdir)/indep-include

SUBDIRS=	gtk

EXTRA_DIST=	diff_ignore

simplemail_SOURCES = account.c \
                     account.h \
                     addressbook.c \
                     addressbook.h \
                     codecs.c \
                     codecs.h \
                     configuration.c \
                     configuration.h \
                     filter.c \
                     filter.h \
                     folder.c \
                     folder.h \
                     hmac_md5.c \
                     hmac_md5.h \
		     http.h \
		     http.c \
                     lists.c \
                     lists.h \
                     mail.c \
                     mail.h \
                     md5.c \
                     md5.h \
                     parse.c \
                     parse.h \
                     pop3.c \
                     pop3.h \
                     signature.c \
                     signature.h \
                     simplemail.c \
                     simplemail.h \
                     support_indep.c \
                     support_indep.h \
                     smtp.c \
                     smtp.h \
                     tcp.c \
                     tcp.h \
                     text2html.c \
                     text2html.h \
                     trans.c \
                     trans.h

simplemail_DEPENDENCIES = ./gtk/libgui.a
simplemail_LDADD        = ./gtk/libgui.a @GUI_LIBS@
















