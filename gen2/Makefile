#

CC = gcc
CFLAGS = -g -Wall -Wextra 
INCLUDE = -I../include
RM = rm -f

PROG      = ../bin/rcc2
SRCDIR    = ../src
SOURCES   = $(wildcard $(SRCDIR)/*.c)
OBJDIR    = ./out
OBJECTS   = $(addprefix $(OBJDIR)/, $(notdir $(SOURCES:.c=.o)))

all: $(PROG)

$(PROG): $(OBJECTS)
	$(CC) -o $(PROG) $(OBJECTS)

$(OBJDIR)/%.s: $(SRCDIR)/%.c
	../bin/rcc -o $@ -S $(INCLUDE) $<

$(OBJDIR)/%.o: $(OBJDIR)/%.s
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	$(RM) $(PROG) $(OBJDIR)/* core
