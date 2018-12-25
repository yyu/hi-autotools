# src/Makefile.  Generated from Makefile.in by configure.

# Package-specific substitution variables
package = Jupiter
version = 1.0
tarname = jupiter
distdir = $(tarname)-$(version)

# Prefix-specific substitution variables
prefix      = /usr/local
exec_prefix = ${prefix}
bindir      = ${exec_prefix}/bin

# VPATH-specific substitution variables
srcdir = ../../src
VPATH  = ../../src

all: jupiter

CFLAGS = -g -O0

jupiter: main.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(srcdir)/main.c

Makefile: Makefile.in ../config.status
	cd .. && ./config.status src/$@

../config.status: ../configure
	cd .. && ./config.status --recheck

check: all
	./jupiter | grep "Hello from .*jupiter!"
	@echo "*** ALL TESTS PASSED ***"

install:
	install -d              $(DESTDIR)$(bindir)
	install -m 0755 jupiter $(DESTDIR)$(bindir)

uninstall:
	-rm $(DESTDIR)$(bindir)/jupiter

clean:
	-rm jupiter

.PHONY: all clean check install uninstall
