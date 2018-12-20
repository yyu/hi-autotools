package = jupiter
version = 1.0
tarname = $(package)
distdir = $(tarname)-$(version)

all clean check jupiter:
	cd src && $(MAKE) $@

dist: $(distdir).tar.gz

distcheck: $(distdir).tar.gz
	gzip -cd $(distdir).tar.gz | tar xvf -
	cd       $(distdir) && $(MAKE) all
	cd       $(distdir) && $(MAKE) check
	cd       $(distdir) && $(MAKE) clean
	rm -rf   $(distdir)
	@echo "*** Package $(distdir).tar.gz is ready for distribution."

$(distdir).tar.gz: $(distdir)
	tar czf $@ $(distdir)
	rm -rf     $(distdir)

$(distdir): FORCE
	mkdir -p        $(distdir)/src
	cp Makefile     $(distdir)
	cp src/Makefile $(distdir)/src
	cp src/main.c   $(distdir)/src

FORCE:
	-rm     $(distdir).tar.gz > /dev/null 2>&1
	-rm -rf $(distdir)        > /dev/null 2>&1

.PHONY: FORCE all clean check dist distcheck
