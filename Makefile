# Dependencies are stored in Makefie.dep. To rebuild this file run
# `make dep`.

STD = -std=c11 -pedantic
WARN =
OPT = -O2
DEBUG = -g -ggdb

BISON = bison -d
FLEX = flex

PREFIX = /usr/local
INSTALL_BIN = $(PREFIX)/bin
INSTALL = install

CFLAGS = $(STD) $(WARN) $(OPT) $(DEBUG)
LDFLAGS = $(DEBUG)

CHINNU_CC = $(QUIET_CC)$(CC) $(CFLAGS)
CHINNU_LD = $(QUIET_LINK)$(CC) $(LDFLAGS)
CHINNU_BISON = $(QUIET_BISON)$(BISON)
CHINNU_FLEX = $(QUIET_FLEX)$(FLEX)
CHINNU_INSTALL = $(QUIET_INSTALL)$(INSTALL)

QUIET_CC      = @printf '    \033[34;1m%b\033[0m \033[33;1m%b\033[0m\n' CC $@ 1>&2;
QUIET_LINK    = @printf '    \033[34;1m%b\033[0m \033[33;1m%b\033[0m\n' LINK $@ 1>&2;
QUIET_BISON   = @printf '    \033[34;1m%b\033[0m \033[33;1m%b\033[0m\n' BISON $@ 1>&2;
QUIET_FLEX    = @printf '    \033[34;1m%b\033[0m \033[33;1m%b\033[0m\n' FLEX $@ 1>&2;
QUIET_INSTALL = @printf '    \033[34;1m%b\033[0m \033[33;1m%b\033[0m\n' INSTALL $@ 1>&2;

CHINNU_NAME = chinnu
CHINNU_SOURCES = ${wildcard *.c}
CHINNU_OBJECTS = ${CHINNU_SOURCES:.c=.o}

all: $(CHINNU_NAME)

.PHONY: all

# Deps (use make dep to generate this)
include Makefile.dep

$(CHINNU_NAME): chinnu.tab.o chinnu.lex.o $(CHINNU_OBJECTS)
	$(CHINNU_LD) -o $@ $^

chinnu.tab.c:
	$(CHINNU_BISON) chinnu.y

chinnu.lex.c:
	$(CHINNU_FLEX) -o chinnu.lex.c chinnu.l

%.o: %.c
	$(CHINNU_CC) -c $<

install: all
	@mkdir -p $(INSTALL_BIN)
	$(CHINNU_INSTALL) $(CHINNU_NAME) $(INSTALL_BIN)

dep:
	$(CHINNU_CC) -MM *.c > Makefile.dep

.PHONY: dep

clean:
	rm -rf $(CHINNU_NAME) *.o chinnu.lex.c chinnu.tab.*

.PHONY: clean
