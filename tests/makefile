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
	mail_unittest \
	support_indep_unittest \
	text2html_unittest

test-objs/%.o: ../%.c
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
all: dirs $(TESTEXES) memleaks

.PHONY: dirs
dirs:
	mkdir -p test-objs/gtk

lib$(SIMPLEMAIL_LIB).a: $(OBJS)
	ar rcs $@ $(OBJS)

# Define memleak targets
MEMLEAKS_TARGETS = $(foreach testexe,$(TESTEXES),memleaks-$(testexe))

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

.PHONY: clean
clean:
	-rm -Rf test-objs $(TESTEXES) $(addsuffix .o,$(TESTEXES))
