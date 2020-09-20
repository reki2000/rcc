#

CC = gcc
CFLAGS = -g -Wall -Wextra -I./include
RM = rm -f

PROG      = bin/rcc
PROG2     = bin/rcc2
SRCDIR    = ./src
SOURCES   = $(wildcard $(SRCDIR)/*.c)
OBJDIR    = ./out
OBJECTS   = $(addprefix $(OBJDIR)/, $(notdir $(SOURCES:.c=.o)))

all: $(PROG)

$(PROG): $(OBJECTS)
	$(CC) -o $(PROG) $(OBJECTS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ -c $<

clean:
	$(RM) $(PROG) $(OBJDIR)/* test/out/* core test/core

$(PROG2): stage1

stage1: clean $(PROG)
	cd stage1 && make clean && make

test: clean $(PROG)
	test/test.sh

test-stage1: $(PROG2)
	test/test.sh --stage1
