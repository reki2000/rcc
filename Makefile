#

CC = gcc
CFLAGS = -g -Wall -Wextra -I./include
RM = rm -f
SHELL=bash

GEN1      = bin/rcc
GEN2      = bin/rcc2
GEN3      = bin/rcc3
SRCDIR    = ./src
SOURCES   = $(wildcard $(SRCDIR)/*.c)
OBJDIR    = ./out
OBJECTS   = $(addprefix $(OBJDIR)/, $(notdir $(SOURCES:.c=.o)))

TESTSRCDIR = unittest
TESTSOURCES   = $(wildcard $(TESTSRCDIR)/*_test.c)

all: $(GEN1)

$(GEN1): $(OBJECTS)
	$(CC) -o $(GEN1) $(OBJECTS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ -c $<

clean:
	$(RM) $(GEN1) $(OBJDIR)/* test/out/* core test/core
	cd gen2 && make clean
	cd gen3 && make clean

gen2: $(GEN2)

$(GEN2): $(GEN1)
	cd gen2 && make

gen3: $(GEN3)

$(GEN3): $(GEN2)
	cd gen3 && make

unittest: clean $(OBJECTS) unittests

unittests: $(TESTSOURCES)
	for f in $^; do echo "testing $$f"; $(CC) $(CFLAGS) -Iinclude -o out/test.out $$f $(OBJDIR)/vec.o; out/test.out; done

test: clean $(GEN1)
	test/test.sh

test-gen2: clean $(GEN2)
	test/test.sh --gen2

test-gen3: clean $(GEN3)
	test/test.sh --gen3

debug: 
	gdb bin/rcc test/core

debug-test:
	gdb test/out/test.out test/core

