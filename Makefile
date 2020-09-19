#

CC = gcc
CFLAGS = -g -Wall -Wextra -I./include
RM = rm -f

PROG      = bin/rcc
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

stage1: clean $(PROG)
	cd stage1 && make clean && make

test: clean $(PROG)
	test/test.sh
