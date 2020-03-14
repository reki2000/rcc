#

CC = gcc
CFLAGS = -g -Wall -Wextra 
INCLUDE = -I../include
RM = rm -f

PROG      = ../bin/rekicc2
SRCDIR    = ../src
SOURCES   = $(wildcard $(SRCDIR)/*.c)
OBJDIR    = ./out
OBJECTS   = $(addprefix $(OBJDIR)/, $(notdir $(SOURCES:.c=.o)))

all: $(PROG)

$(PROG): $(OBJECTS)
	$(CC) -o $(PROG) $(OBJECTS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	../bin/rekicc -S $(INCLUDE) $< > asm.s
	$(CC) $(CFLAGS) -o $@ -c asm.s

clean:
	$(RM) ../bin/rekicc2 out/* core
