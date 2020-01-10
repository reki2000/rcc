#

CC = gcc
CFLAGS = -g -Wall -Wextra -I./include

SRCDIR    = ./src
SOURCES   = $(wildcard $(SRCDIR)/*.c)
OBJDIR    = ./out
OBJECTS   = $(addprefix $(OBJDIR)/, $(notdir $(SOURCES:.c=.o)))

.PHONY: test

test: bin/main
	test/test.sh

all: bin/main

bin/main: $(OBJECTS)
	$(CC) -o bin/main $(OBJECTS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	mkdir -p $(OBJDIR)
	$(COMPILER) $(CFLAGS) $(INCLUDE) -o $@ -c $<

clean:
	$(RM) bin/main out/*.o
