include config.mak

MAKE_SUBDIRS = external
QT_SUBDIRS   = libs filters html programs themes i18n locales

ifdef CONFIG_BINDINGS_PERL
    MAKE_SUBDIRS += bindings/perl
endif

ifdef CONFIG_BINDINGS_PYTHON
    MAKE_SUBDIRS += bindings/python
endif

ifdef CONFIG_BINDINGS_PHP
    MAKE_SUBDIRS += bindings/php
endif


SUBDIRS += $(MAKE_SUBDIRS) $(QT_SUBDIRS)

.PHONY: subdirs $(SUBDIRS)                            \
	clean     $(addsuffix _clean,    $(SUBDIRS))  \
	distclean $(addsuffix _distclean,$(SUBDIRS))  \
	install   $(addsuffix _install,  $(SUBDIRS))  \
	uninstall $(addsuffix _uninstall,$(SUBDIRS))  \
	libs/libmythbase/version.h force

all: libs/libmythbase/version.h subdirs
force:

config.mak:
	$(error run configure to create $@)

# Override PWD in case this is run from a makefile at a higher level with
# make -C mythtv
PWD := $(shell pwd)

libs/libmythbase/version.h:	version.sh force
	sh version.sh $(PWD)

# explicit subdir dependencies
libs: external
filters: libs
programs: libs filters


subdirs: $(SUBDIRS)

settings.pro: config.mak
%.pro: settings.pro

# explicit prerequisites for qmake generated makefiles
libs/Makefile: libs/libs.pro
filters/Makefile: filters/filters.pro
html/Makefile: html/html.pro
programs/Makefile: programs/programs.pro
themes/Makefile: themes/themes.pro
i18n/Makefile: i18n/i18n.pro
locales/Makefile: locales/locales.pro

$(addsuffix /Makefile,$(QT_SUBDIRS)): %/Makefile :
	cd $*; $(QMAKE) QMAKE=$(QMAKE) -o $(@F) $(<F)

$(SUBDIRS): $(addsuffix /Makefile,$(SUBDIRS)) libs/libmythbase/version.h
	$(MAKE) -C $@

$(addsuffix _clean,$(SUBDIRS)): $(addsuffix /Makefile,$(SUBDIRS))
	$(MAKE) -C $(subst _clean,,$@) clean

$(addsuffix _distclean,$(SUBDIRS)): $(addsuffix /Makefile,$(SUBDIRS))
	$(MAKE) -C $(subst _distclean,,$@) distclean

$(addsuffix _install,$(SUBDIRS)): $(addsuffix /Makefile,$(SUBDIRS))
	$(MAKE) -C $(subst _install,,$@) install INSTALL_ROOT=${INSTALL_ROOT}

$(addsuffix _uninstall,$(SUBDIRS)): $(addsuffix /Makefile,$(SUBDIRS))
	$(MAKE) -C $(subst _uninstall,,$@) uninstall

clean: $(addsuffix _clean,$(SUBDIRS))

distclean: $(addsuffix _distclean,$(SUBDIRS))
	-rm -f libs/libmythbase/mythconfig.mak config.mak
	-rm -f libs/libmythbase/mythconfig.h   config.h
	-rm -f libs/libmythbase/version.h
	-rm -f libs/libavutil/avconfig.h
	-rm -f external/FFmpeg/libavutil/avconfig.h
	-rm -f config.ep
	-rm -f $(addsuffix /Makefile,$(QT_SUBDIRS))

install: $(addsuffix _install,$(SUBDIRS))

uninstall: $(addsuffix _uninstall,$(SUBDIRS))
	-rmdir $(INSTALL_ROOT)/${PREFIX}/include/mythtv
	-rmdir $(INSTALL_ROOT)/${PREFIX}/lib/mythtv
	-rmdir $(INSTALL_ROOT)/${PREFIX}/share/mythtv

test:
	-./programs/scripts/unittests.sh
