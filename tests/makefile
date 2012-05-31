include ../common-sources.mk

SRCS = $(addprefix ../,$(NONARCHSRCS))
OBJS = $(SRCS:../%.c=test-objs/%.o)

CC = gcc
CFLAGS=-g -I .. -I ../indep-include/ -I ../gtk \
	-DNODEBUG -Dstricmp=strcasecmp -Dstrnicmp=strncasecmp

TESTEXES=\
	mail_unittest

test-objs/%.o: ../%.c
	$(CC) $(CFLAGS) -c $< -o $@

test-objs/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

all: dirs $(TESTEXES) memleaks

dirs:
	mkdir -p test-objs

mail_unittest: test-objs/mail_unittest.o $(OBJS)
	-@gcc test-objs/mail_unittest.o $(OBJS) $(CFLAGS) -lcunit 2>test-objs/error
	perl gen-stubs.pl <test-objs/error >stubs.c
	gcc stubs.c test-objs/mail_unittest.o $(OBJS) $(CFLAGS) -lcunit -o $@

memleaks:
	valgrind ./mail_unittest

clean:
	-rm -Rf test-objs
