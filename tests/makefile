#
# Makefile for the tests.
#

include ../common-sources.mk

SRCS = $(addprefix ../,$(NONARCHSRCS)) ../gtk/support.c ../gtk/subthreads.c
OBJS = $(SRCS:../%.c=test-objs/%.o)

GLIB_CFLAGS  := $(shell pkg-config glib-2.0 --cflags)
GLIB_LDFLAGS := $(shell pkg-config glib-2.0 --libs)

GTK_CFLAGS := $(shell pkg-config gtk+-x11-2.0 --cflags)

CC = gcc
CFLAGS=-g -I .. -I ../indep-include/ -I ../gtk $(GLIB_CFLAGS) $(GTK_CFLAGS) \
	-DNODEBUG -Dstricmp=strcasecmp -Dstrnicmp=strncasecmp -Werror
VALGRIND_OPTS = --leak-check=full --track-origins=yes 

SIMPLEMAIL_LIB  = simplemail

TESTEXES=\
	boyermoore_unittest \
	codesets_unittest \
	configuration_unittest \
	filter_unittest \
	folder_unittest \
	index_unittest \
	imap2_unittest \
	mail_unittest \
	support_indep_unittest \
	text2html_unittest

test-objs/%.o: ../%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# General rule to compile the unit tests
%_unittest: %_unittest.c lib$(SIMPLEMAIL_LIB).a
	perl gen-tests.pl <$< >$@_table.c
	@-$(CC) unittest_tmpl.c -DUNITTEST_TABLE=\"$@_table.c\" -DUNITTEST_FILE=\"$<\" $(CFLAGS) -lcunit -L. -l$(SIMPLEMAIL_LIB) $(GLIB_LDFLAGS) $(GTK_LDFLAGS) 2>test-objs/$@error
	perl gen-stubs.pl <test-objs/$@error >$@stubs.c
	$(CC) $@stubs.c unittest_tmpl.c -DUNITTEST_TABLE=\"$@_table.c\" -DUNITTEST_FILE=\"$<\" $(CFLAGS) -lcunit -L. -l$(SIMPLEMAIL_LIB) $(GLIB_LDFLAGS) $(GTK_LDFLAGS) -o $@

.PHONY: all
all: dirs files $(TESTEXES) memleaks

.PHONY: dirs
dirs:
	mkdir -p test-objs/gtk

.PHONY: files
files: test-profile.tar.bz2
	tar xjf test-profile.tar.bz2

lib$(SIMPLEMAIL_LIB).a: $(OBJS)
	ar rcs $@ $(OBJS)

# Define memleak targets
MEMLEAKS_TARGETS = $(foreach testexe,$(TESTEXES),memleaks-$(testexe))

# run template
define run.tmpl
run-$(1): $(1)
	./$$<
endef

# "instanciate" run targets
$(foreach testexe,$(TESTEXES), \
 $(eval $(call run.tmpl,$(testexe))) \
)

# memleaks template
define memleak.tmpl
memleaks-$(1): $(1)
ifdef USE_VALGRIND_XML
	valgrind $(VALGRIND_OPTS) --xml=yes --xml-file=$$@.xml ./$$<
else
	valgrind $(VALGRIND_OPTS) ./$$<
endif
endef

# "instanciate" memleak targets
$(foreach testexe,$(TESTEXES), \
 $(eval $(call memleak.tmpl,$(testexe))) \
)

.PHONY: memleaks
memleaks: $(MEMLEAKS_TARGETS)


# helgrind template
define helgrind.tmpl
helgrind-$(1): $(1)
ifdef USE_VALGRIND_XML
	valgrind --tool=helgrind --xml=yes --xml-file=$$@.xml ./$$<
else
	valgrind --tool=helgrind ./$$<
endif
endef

# "instanciate" memleak targets
$(foreach testexe,$(TESTEXES), \
 $(eval $(call helgrind.tmpl,$(testexe))) \
)


.PHONY: clean
clean:
	-rm -Rf test-objs $(TESTEXES) $(addsuffix .o,$(TESTEXES))


# Dovecot support
DOVECOT_URL=http://www.dovecot.org/releases/2.2/dovecot-2.2.9.tar.gz
DOVECOT_TGZ=$(notdir $(DOVECOT_URL))
DOVECOT_DIR=$(DOVECOT_TGZ:%.tar.gz=%)

$(DOVECOT_TGZ):
	wget -N $(DOVECOT_URL)

$(DOVECOT_DIR)-root: $(DOVECOT_TGZ)
	rm -Rf $(DOVECOT_DIR)
	tar -xzf $(DOVECOT_TGZ)
	cd $(DOVECOT_DIR); ./configure --prefix=$(shell pwd)/$(DOVECOT_DIR)-root; $(MAKE) ; $(MAKE) install

.PHONY: dovecot-bin
dovecot-bin: $(DOVECOT_DIR)-root

.PHONY: dovecot
dovecot: dovecot-bin
	rm -Rf $(DOVECOT_DIR)-root/etc/dovecot
	cp -R dovecot-conf -Rf $(DOVECOT_DIR)-root/etc/dovecot
	echo 'default_internal_user = $(USER)' >>$(DOVECOT_DIR)-root/etc/dovecot/conf.d/00-default-user.conf
	echo 'default_login_user = $(USER)' >>$(DOVECOT_DIR)-root/etc/dovecot/conf.d/00-default-user.conf
	echo 'passdb {' >>$(DOVECOT_DIR)-root/etc/dovecot/conf.d/00-passwd.conf
	echo '    driver = passwd-file' >>$(DOVECOT_DIR)-root/etc/dovecot/conf.d/00-passwd.conf
	echo '    args = $(shell readlink -f $(DOVECOT_DIR)-root/etc/dovecot/passwd)' >>$(DOVECOT_DIR)-root/etc/dovecot/conf.d/00-passwd.conf
	echo '}' >>$(DOVECOT_DIR)-root/etc/dovecot/conf.d/00-passwd.conf

dovecot-conf: dovecot-bin
	cp -R $(DOVECOT_DIR)-root/share/doc/dovecot/example-config $(DOVECOT_DIR)-root/etc/dovecot
