include ../../config.mak

ifdef INSTALL_ROOT
ROOT_FLAGS = PERL_INSTALL_ROOT="$(INSTALL_ROOT)"
endif

ifneq ($(PREFIX:/=), /usr)
	ifneq ($(PREFIX:/=), /usr/local)
		PREFIX_FLAGS=PREFIX="$(PREFIX)"
	endif
endif

all: perl_build

clean:
	@-if [ -f Makefile.perl ] ; then $(MAKE) -f Makefile.perl clean ; rm -f Makefile.perl ; fi

distclean: clean
	-rm -f Makefile.perl.old

Makefile.perl: Makefile.PL
	perl Makefile.PL MAKEFILE=Makefile.perl $(PREFIX_FLAGS) $(PERL_CONFIG_OPTS)

perl_build: Makefile.perl
	@-$(MAKE) -f Makefile.perl

install: all
	$(MAKE) -f Makefile.perl pure_install $(PREFIX_FLAGS) $(ROOT_FLAGS)

uninstall:
	$(warning make unsinstall is not supported for the perl bindings)

.PHONY: all clean distclean install perl_build uninstall
