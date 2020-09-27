#

CC = gcc
CFLAGS = -g -Wall -Wextra 
INCLUDE = -I../include
RM = rm -f

GEN1      = ../bin/rcc
GEN2      = ../bin/rcc2
GEN3      = ../bin/rcc3
SRCDIR    = ../src
SOURCES   = $(wildcard $(SRCDIR)/*.c)
OBJDIR    = ./out
OBJECTS   = $(addprefix $(OBJDIR)/, $(notdir $(SOURCES:.c=.o)))

all: $(GEN3)

$(GEN3): $(OBJECTS)
	$(CC) -o $(GEN3) $(OBJECTS)

$(OBJDIR)/%.s: $(SRCDIR)/%.c
	$(GEN2) -S $(INCLUDE) $< > $@

$(OBJDIR)/%.o: $(OBJDIR)/%.s
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	$(RM) $(GEN3) $(OBJDIR)/* core
