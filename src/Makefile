# Copyright (c) 2014, Eric Fritz
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of this software
# and associated documentation files (the "Software"), to deal in the Software without
# restriction, including without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all copies or
# substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
# BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
# DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
# Dependencies are stored in Makefie.dep. To rebuild this file run
# `make dep`.

STD = -std=gnu99 -pedantic-errors
WARN =
OPT = -O2
DEBUG = -g -ggdb

BISON = bison
FLEX = flex

PREFIX = /usr/local
INSTALL_BIN = $(PREFIX)/bin
INSTALL = install

CFLAGS = $(STD) $(WARN) $(OPT) $(DEBUG)
LDFLAGS = -lm

CHINNU_CC = $(QUIET_CC)$(CC) $(CFLAGS)
CHINNU_LD = $(QUIET_LINK)$(CC) $(DEBUG)
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
	$(CHINNU_LD) -o $@ $^ $(LDFLAGS)

chinnu.tab.c:
	$(CHINNU_BISON) chinnu.y

chinnu.lex.c:
	$(CHINNU_FLEX) chinnu.l

%.o: %.c
	$(CHINNU_CC) -c $<

%.c: %.y
%.c: %.l

install: all
	@mkdir -p $(INSTALL_BIN)
	$(CHINNU_INSTALL) $(CHINNU_NAME) $(INSTALL_BIN)

dep:
	$(CHINNU_CC) -MM *.c > Makefile.dep

.PHONY: dep

clean:
	rm -rf $(CHINNU_NAME) *.o chinnu.lex.* chinnu.tab.*

.PHONY: clean
