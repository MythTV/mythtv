SUBDIRS = FFmpeg

.PHONY: subdirs $(SUBDIRS)                            \
	clean     $(addsuffix _clean,    $(SUBDIRS))  \
	distclean $(addsuffix _distclean,$(SUBDIRS))  \
	install   $(addsuffix _install,  $(SUBDIRS))  \
	uninstall $(addsuffix _uninstall,$(SUBDIRS))

subdirs: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

qmake_all:
	\$(NONE)

$(addsuffix _clean,$(SUBDIRS)):
	$(MAKE) -C $(subst _clean,,$@) clean

clean: $(addsuffix _clean,$(SUBDIRS))

$(addsuffix _distclean,$(SUBDIRS)):
	$(MAKE) -C $(subst _distclean,,$@) distclean

distclean: clean $(addsuffix _distclean,$(SUBDIRS))

$(addsuffix _install,$(SUBDIRS)):
	$(MAKE) -C $(subst _install,,$@) install

install: $(addsuffix _install,$(SUBDIRS))

$(addsuffix _uninstall,$(SUBDIRS)):
	$(MAKE) -C $(subst _uninstall,,$@) uninstall

uninstall: $(addsuffix _uninstall,$(SUBDIRS))