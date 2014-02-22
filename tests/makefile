#
# Makefile for the tests.
#

include ../common-sources.mk

SRCS = $(addprefix ../,$(NONARCHSRCS)) ../gtk/support.c ../gtk/subthreads.c ../gtk/tcpip.c
OBJS = $(SRCS:../%.c=test-objs/%.o)

GLIB_CFLAGS  := $(shell pkg-config glib-2.0 --cflags)
GLIB_LDFLAGS := $(shell pkg-config glib-2.0 --libs)
SSL_LDFLAGS :=  $(shell pkg-config openssl --libs)

GTK_CFLAGS := $(shell pkg-config gtk+-x11-2.0 --cflags)

CC = gcc
CFLAGS=-g -I .. -I ../indep-include/ -I ../gtk $(GLIB_CFLAGS) $(GTK_CFLAGS) \
	-Dstricmp=strcasecmp -Dstrnicmp=strncasecmp -Werror
VALGRIND_OPTS = --leak-check=full --track-origins=yes 

SIMPLEMAIL_LIB  = simplemail

TESTEXES=\
	boyermoore_unittest \
	codesets_unittest \
	configuration_unittest \
	filter_unittest \
	folder_unittest \
	index_unittest \
	index_external_unittest \
	imap2_unittest \
	mail_unittest \
	support_indep_unittest \
	text2html_unittest \
	tcp_unittest

NOMEMLEAKSPERDEFAULT=\
	index_unittest

test-objs/%.o: ../%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# General rule to compile the unit tests
%_unittest: %_unittest.c lib$(SIMPLEMAIL_LIB).a
	perl gen-tests.pl <$< >$@_table.c
	@-$(CC) unittest_tmpl.c -DUNITTEST_TABLE=\"$@_table.c\" -DUNITTEST_FILE=\"$<\" $(CFLAGS) -lcunit -L. -l$(SIMPLEMAIL_LIB) $(SSL_LDFLAGS) $(GLIB_LDFLAGS) $(GTK_LDFLAGS) 2>test-objs/$@error
	perl gen-stubs.pl <test-objs/$@error >$@stubs.c
	$(CC) $@stubs.c unittest_tmpl.c -DUNITTEST_TABLE=\"$@_table.c\" -DUNITTEST_FILE=\"$<\" $(CFLAGS) -lcunit -L. -l$(SIMPLEMAIL_LIB) $(SSL_LDFLAGS) $(GLIB_LDFLAGS) $(GTK_LDFLAGS) -o $@

.PHONY: all
all: files $(TESTEXES) memleaks

of-human-bondage.txt: of-human-bondage.txt.gz
	gzip -c -d $< >$@

index_unittest: of-human-bondage.txt

.PHONY: files
files: test-profile.tar.bz2
	tar xjf test-profile.tar.bz2

lib$(SIMPLEMAIL_LIB).a: $(OBJS)
	ar rcs $@ $(OBJS)

.PHONY: clean
clean:
	-rm -Rf test-objs $(TESTEXES) $(addsuffix .o,$(TESTEXES))


# Dovecot support
DOVECOT_URL=http://www.dovecot.org/releases/2.2/dovecot-2.2.9.tar.gz
DOVECOT_TGZ=$(notdir $(DOVECOT_URL))
DOVECOT_DIR=$(DOVECOT_TGZ:%.tar.gz=%)
DOVECOT_ABSDIR=$(shell pwd)/$(DOVECOT_DIR)
DOVECOT=$(DOVECOT_DIR)-root/sbin/dovecot

$(DOVECOT_TGZ):
	wget -N $(DOVECOT_URL)

$(DOVECOT_DIR)-root: $(DOVECOT_TGZ)
	rm -Rf $(DOVECOT_DIR)
	tar -xzf $(DOVECOT_TGZ)
	cd $(DOVECOT_DIR); ./configure --prefix=$(shell pwd)/$(DOVECOT_DIR)-root; $(MAKE) ; $(MAKE) install

.PHONY: dovecot-bin
dovecot-bin: $(DOVECOT_DIR)-root

.PHONY: dovecot-conf
dovecot-conf: dovecot-bin
	mkdir -p $(DOVECOT_DIR)-root/home/test
	rm -Rf $(DOVECOT_DIR)-root/etc/dovecot
	cp -R dovecot-conf -Rf $(DOVECOT_DIR)-root/etc/dovecot
	echo 'default_internal_user = $(USER)' >>$(DOVECOT_DIR)-root/etc/dovecot/conf.d/00-default-user.conf
	echo 'default_login_user = $(USER)' >>$(DOVECOT_DIR)-root/etc/dovecot/conf.d/00-default-user.conf
	echo 'passdb {' >>$(DOVECOT_DIR)-root/etc/dovecot/conf.d/00-passwd.conf
	echo '    driver = passwd-file' >>$(DOVECOT_DIR)-root/etc/dovecot/conf.d/00-passwd.conf
	echo '    args = $(DOVECOT_ABSDIR)-root/etc/dovecot/passwd' >>$(DOVECOT_DIR)-root/etc/dovecot/conf.d/00-passwd.conf
	echo '}' >>$(DOVECOT_DIR)-root/etc/dovecot/conf.d/00-passwd.conf
	echo 'userdb {' >>$(DOVECOT_DIR)-root/etc/dovecot/conf.d/00-passwd.conf
	echo '    driver = passwd-file' >>$(DOVECOT_DIR)-root/etc/dovecot/conf.d/00-passwd.conf
	echo '    args = $(DOVECOT_ABSDIR)-root/etc/dovecot/passwd' >>$(DOVECOT_DIR)-root/etc/dovecot/conf.d/00-passwd.conf
	echo '}' >>$(DOVECOT_DIR)-root/etc/dovecot/conf.d/00-passwd.conf
	echo 'test:{plain}test::::$(DOVECOT_ABSDIR)-root/home/test::userdb_mail=maildir:~/Maildir' >$(DOVECOT_DIR)-root/etc/dovecot/passwd

.PHONY: dovecot-default-conf
dovecot-default-conf: dovecot-bin
	cp -R $(DOVECOT_DIR)-root/share/doc/dovecot/example-config $(DOVECOT_DIR)-root/etc/dovecot

.PHONY: dovecot-start
dovecot-start: dovecot-conf
	$(DOVECOT)

.PHONY: dovecot-stop
dovecot-stop: dovecot-conf
	$(DOVECOT) stop

.PHONY: imap-testsuite
imap-testsuite: dovecot-conf imap2_unittest
	-$(DOVECOT) stop
	$(DOVECOT)
	sleep 2
	./imap2_unittest
	$(DOVECOT) stop

# run template
define run.tmpl
run-$(1): $(1) $(if $(findstring imap,$(1)), dovecot-conf)
	$(if $(findstring imap,$(1)), -$(DOVECOT) stop)
	$(if $(findstring imap,$(1)), $(DOVECOT))
	./$$<
	$(if $(findstring imap,$(1)), $(DOVECOT) stop)
endef

# "instanciate" run targets
$(foreach testexe,$(TESTEXES), \
 $(eval $(call run.tmpl,$(testexe))) \
)

# memleaks template
define memleak.tmpl
memleaks-$(1): $(1) $(if $(findstring imap,$(1)), dovecot-conf)
	$(if $(findstring imap,$(1)), -$(DOVECOT) stop)
	$(if $(findstring imap,$(1)), $(DOVECOT))
ifdef USE_VALGRIND_XML
	G_SLICE=always-malloc valgrind $(VALGRIND_OPTS) --xml=yes --xml-file=$$@.xml ./$$<
else
	G_SLICE=always-malloc valgrind $(VALGRIND_OPTS) ./$$<
endif
	$(if $(findstring imap,$(1)), $(DOVECOT) stop)
endef

# "instanciate" memleak targets
$(foreach testexe,$(TESTEXES), \
 $(eval $(call memleak.tmpl,$(testexe))) \
)

# Define our default memleak targets, i.e., targets that need
# to be done for the memleaks target.
MEMLEAKS_TARGETS = $(foreach testexe,$(filter-out $(NOMEMLEAKSPERDEFAULT),$(TESTEXES)),memleaks-$(testexe))

.PHONY: memleaks
memleaks: $(MEMLEAKS_TARGETS)


# helgrind template
define helgrind.tmpl
helgrind-$(1): $(1) $(if $(findstring imap,$(1)), dovecot-conf)
	$(if $(findstring imap,$(1)), -$(DOVECOT) stop)
	$(if $(findstring imap,$(1)), $(DOVECOT))
ifdef USE_VALGRIND_XML
	valgrind --tool=helgrind --xml=yes --xml-file=$$@.xml ./$$<
else
	valgrind --tool=helgrind ./$$<
endif
	$(if $(findstring imap,$(1)), $(DOVECOT) stop)
endef

# "instanciate" memleak targets
$(foreach testexe,$(TESTEXES), \
 $(eval $(call helgrind.tmpl,$(testexe))) \
)
