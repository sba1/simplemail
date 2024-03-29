#
# Makefile for the tests.
#
# Use the DOVECOT_PREFIX option to set the dovecot prefix
#

include ../common-sources.mk

SRCS = $(addprefix ../,$(NONARCHSRCS)) \
	../gtk/support.c \
	../platform/request_gtk.c \
	../platform/timesupport_glib.c \
	../platform/subthreads_glib.c \
	../platform/tcpip_posix.c \
	../ncurses/gadgets.c \
	../ncurses/text_edit_helper.c

OBJS = $(SRCS:../%.c=test-objs/%.o)

GLIB_CFLAGS  := $(shell pkg-config glib-2.0 --cflags)
GLIB_LDFLAGS := $(shell pkg-config glib-2.0 --libs)
SSL_LDFLAGS :=  $(shell pkg-config openssl --libs)

GTK_CFLAGS := $(shell pkg-config gtk+-x11-2.0 --cflags)

CC = gcc
CFLAGS=-g -ffunction-sections \
	-I .. -I ../indep-include/ -I ../gtk -I ../ncurses $(GLIB_CFLAGS) $(GTK_CFLAGS) \
	-Dstricmp=strcasecmp -Dstrnicmp=strncasecmp -Werror -DUSE_OPENSSL \
	-DSIMPLEMAIL_DATE='"$(shell date +%d.%m.%Y)"'

VALGRIND_OPTS = --leak-check=full --track-origins=yes 

DOVECOT_PREFIX = ./

SIMPLEMAIL_LIB  = simplemail

TESTEXES=\
	addressbook_unittest \
	arrays_unittest \
	boyermoore_unittest \
	codesets_unittest \
	configuration_unittest \
	coroutines_unittest \
	filter_unittest \
	folder_unittest \
	gadgets_unittest \
	hash_unittest \
	index_unittest \
	index_external_unittest \
	imap_helper_unittest \
	imap2_unittest \
	logging_unittest \
	mail_unittest \
	pop3_unittest \
	ringbuffer_unittest \
	string_lists_unittest \
	string_pools_unittest \
	support_indep_unittest \
	test_account_unittest \
	text2html_unittest \
	tcp_unittest

NOMEMLEAKSPERDEFAULT=\
	index_unittest

test-objs/%.o: ../%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# General rule to create the unittest table
gen/%_unittest_table.c: %_unittest.c gen-tests.pl
	mkdir -p gen
	perl gen-tests.pl <$< >$@

# General rule to compile the unit tests
%_unittest: %_unittest.c gen/%_unittest_table.c unittest_tmpl.c lib$(SIMPLEMAIL_LIB).a
	# Generate test data files
	python gen-test-data.py <$< --outdir gen-test-data
	# Attempt to compile the unit test, but don't fail if it fails
	@-$(CC) unittest_tmpl.c -DUNITTEST_TABLE=\"gen/$@_table.c\" -DUNITTEST_FILE=\"$<\" $(CFLAGS) -lcunit -L. -l$(SIMPLEMAIL_LIB) $(SSL_LDFLAGS) $(GLIB_LDFLAGS) $(GTK_LDFLAGS) 2>test-objs/$@error
	# Generate simple stubs from error log
	perl gen-stubs.pl <test-objs/$@error >gen/$@stubs.c
	# Finally, compile again
	$(CC) -MMD -MP gen/$@stubs.c unittest_tmpl.c -DUNITTEST_TABLE=\"gen/$@_table.c\" -DUNITTEST_FILE=\"$<\" $(CFLAGS) -lcunit -L. -l$(SIMPLEMAIL_LIB) $(SSL_LDFLAGS) $(GLIB_LDFLAGS) $(GTK_LDFLAGS) -o $@

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

.PHONY: clean-tmp
clean-tmp:
	rm -Rf SimpleMail-tmp*

# Dovecot support
DOVECOT_URL=http://www.dovecot.org/releases/2.2/dovecot-2.2.9.tar.gz
DOVECOT_TGZ=$(notdir $(DOVECOT_URL))
DOVECOT_SHA1SUM=efba4dee63c7dab2b9950e51874a86ba17e09d0c
DOVECOT_DIR=$(DOVECOT_TGZ:%.tar.gz=%)
DOVECOT_ABSDIR=$(realpath $(DOVECOT_PREFIX))/$(DOVECOT_DIR)
DOVECOT=$(DOVECOT_ABSDIR)-root/sbin/dovecot

$(DOVECOT_TGZ):
	wget -N $(DOVECOT_URL)
	echo "$(DOVECOT_SHA1SUM)  $(DOVECOT_TGZ)" | sha1sum -c

$(DOVECOT_ABSDIR)-root: $(DOVECOT_TGZ)
	rm -Rf $(DOVECOT_DIR)
	tar -xzf $(DOVECOT_TGZ)
	cd $(DOVECOT_DIR); ./configure --prefix=$(DOVECOT_ABSDIR)-root; $(MAKE) ; $(MAKE) install

.PHONY: dovecot-bin
dovecot-bin: $(DOVECOT_ABSDIR)-root

.PHONY: dovecot-conf
dovecot-conf: dovecot-bin
	mkdir -p $(DOVECOT_ABSDIR)-root/home/test
	rm -Rf $(DOVECOT_ABSDIR)-root/etc/dovecot
	cp -R dovecot-conf -Rf $(DOVECOT_ABSDIR)-root/etc/dovecot
	echo 'default_internal_user = $(USER)' >>$(DOVECOT_ABSDIR)-root/etc/dovecot/conf.d/00-default-user.conf
	echo 'default_login_user = $(USER)' >>$(DOVECOT_ABSDIR)-root/etc/dovecot/conf.d/00-default-user.conf
	echo 'passdb {' >>$(DOVECOT_ABSDIR)-root/etc/dovecot/conf.d/00-passwd.conf
	echo '    driver = passwd-file' >>$(DOVECOT_ABSDIR)-root/etc/dovecot/conf.d/00-passwd.conf
	echo '    args = $(DOVECOT_ABSDIR)-root/etc/dovecot/passwd' >>$(DOVECOT_ABSDIR)-root/etc/dovecot/conf.d/00-passwd.conf
	echo '}' >>$(DOVECOT_ABSDIR)-root/etc/dovecot/conf.d/00-passwd.conf
	echo 'userdb {' >>$(DOVECOT_ABSDIR)-root/etc/dovecot/conf.d/00-passwd.conf
	echo '    driver = passwd-file' >>$(DOVECOT_ABSDIR)-root/etc/dovecot/conf.d/00-passwd.conf
	echo '    args = $(DOVECOT_ABSDIR)-root/etc/dovecot/passwd' >>$(DOVECOT_ABSDIR)-root/etc/dovecot/conf.d/00-passwd.conf
	echo '}' >>$(DOVECOT_ABSDIR)-root/etc/dovecot/conf.d/00-passwd.conf
	echo 'test:{plain}test::::$(DOVECOT_ABSDIR)-root/home/test::userdb_mail=maildir:~/Maildir' >$(DOVECOT_ABSDIR)-root/etc/dovecot/passwd

.PHONY: dovecot-default-conf
dovecot-default-conf: dovecot-bin
	cp -R $(DOVECOT_ABSDIR)-root/share/doc/dovecot/example-config $(DOVECOT_ABSDIR)-root/etc/dovecot

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
	./prepare-imap.py
	./imap2_unittest
	$(DOVECOT) stop

# Non-empty, if a unit tests requires started dovecot
#
# $(1): The name of the unit test
define dovecot-needed
$(if $(findstring helper,$(1)),,$(findstring imap,$(1))$(findstring pop,$(1)))
endef

# run template
#
# $(1): The name of the unit test
define run.tmpl
run-$(1): $(1) $(if $(call dovecot-needed,$(1)), dovecot-conf)
	$(if $(call dovecot-needed,$(1)), -$(DOVECOT) stop)
	$(if $(call dovecot-needed,$(1)), $(DOVECOT))
	cd gen-test-data && ../$$<
	$(if $(call dovecot-needed,$(1)), $(DOVECOT) stop)
endef

# "instanciate" run targets
$(foreach testexe,$(TESTEXES), \
 $(eval $(call run.tmpl,$(testexe))) \
)

# gdb template
#
# $(1): The name of the unit test
define gdb.tmpl
gdb-$(1): $(1) $(if $(call dovecot-needed,$(1)), dovecot-conf)
	$(if $(call dovecot-needed,$(1)), -$(DOVECOT) stop)
	$(if $(call dovecot-needed,$(1)), $(DOVECOT))
	cd gen-test-data && gdb ../$$<
	$(if $(call dovecot-needed,$(1)), $(DOVECOT) stop)
endef

# "instanciate" gdb targets
$(foreach testexe,$(TESTEXES), \
 $(eval $(call gdb.tmpl,$(testexe))) \
)


# ddd template
#
# $(1): The name of the unit test
define ddd.tmpl
ddd-$(1): $(1) $(if $(call dovecot-needed,$(1)), dovecot-conf)
	$(if $(call dovecot-needed,$(1)), -$(DOVECOT) stop)
	$(if $(call dovecot-needed,$(1)), $(DOVECOT))
	cd gen-test-data && ddd ../$$<
	$(if $(call dovecot-needed,$(1)), $(DOVECOT) stop)
endef

# "instanciate" ddd targets
$(foreach testexe,$(TESTEXES), \
 $(eval $(call ddd.tmpl,$(testexe))) \
)


# memleaks template
#
# $(1): The name of the unit test
define memleak.tmpl
memleaks-$(1): $(1) $(if $(call dovecot-needed,$(1)), dovecot-conf)
	$(if $(call dovecot-needed,$(1)), -$(DOVECOT) stop)
	$(if $(call dovecot-needed,$(1)), $(DOVECOT))
ifdef USE_VALGRIND_XML
	cd gen-test-data && G_SLICE=always-malloc valgrind $(VALGRIND_OPTS) --xml=yes --xml-file=$$@.xml ../$$<
else
	cd gen-test-data && G_SLICE=always-malloc valgrind $(VALGRIND_OPTS) ../$$<
endif
	$(if $(call dovecot-needed,$(1)), $(DOVECOT) stop)
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
	cd gen-test-data && valgrind --tool=helgrind --xml=yes --xml-file=$$@.xml ../$$<
else
	cd gen-test-data && valgrind --tool=helgrind ../$$<
endif
	$(if $(findstring imap,$(1)), $(DOVECOT) stop)
endef

# "instanciate" memleak targets
$(foreach testexe,$(TESTEXES), \
 $(eval $(call helgrind.tmpl,$(testexe))) \
)

# Separate playground for the editing component
edit.o: edit.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

edit: edit.o lib$(SIMPLEMAIL_LIB).a
	$(CC) edit.o -o $@ -L. -l$(SIMPLEMAIL_LIB) -lncurses -Wl,--gc-sections

.PHONY: debug-edit
debug-edit: edit
	@echo run \'gdb -ex \"target remote localhost:11111\"\' for debugging
	gdbserver :11111 edit

# Now include dependencies, but don't fail if they are not available
-include $(OBJS:.o=.d) $(addsuffix .d,$(TESTEXES)) edit.d
