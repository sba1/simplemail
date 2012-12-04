#
# Makefile for the tests.
#

include ../common-sources.mk

SRCS = $(addprefix ../,$(NONARCHSRCS))
OBJS = $(SRCS:../%.c=test-objs/%.o)

CC = gcc
CFLAGS=-g -I .. -I ../indep-include/ -I ../gtk \
	-DNODEBUG -Dstricmp=strcasecmp -Dstrnicmp=strncasecmp -Werror

TESTEXES=\
	boyermoore_unittest \
	mail_unittest \
	filter_unittest \
	support_indep_unittest

test-objs/%.o: ../%.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# General rule to compile the unit tests
%_unittest: %_unittest.c $(OBJS)
	@-$(CC) $< $(OBJS) $(CFLAGS) -lcunit 2>test-objs/$@error
	perl gen-stubs.pl <test-objs/$@error >$@stubs.c
	$(CC) $@stubs.c $< $(OBJS) $(CFLAGS) -lcunit -o $@

.PHONY: all
all: dirs $(TESTEXES) memleaks

.PHONY: dirs
dirs:
	mkdir -p test-objs

.PHONY: memleaks
memleaks:
	$(foreach TESTEXE,$(TESTEXES), valgrind ./$(TESTEXE);)

.PHONY: clean
clean:
	-rm -Rf test-objs $(TESTEXES) $(addsuffix .o,$(TESTEXES))
