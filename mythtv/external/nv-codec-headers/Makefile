PREFIX = /usr/local
LIBDIR = lib
INSTALL = install
SED = sed

all:
ifeq ($(OS),Windows_NT)
	$(SED) 's#@@PREFIX@@#$(shell cygpath -m ${PREFIX})#' ffnvcodec.pc.in > ffnvcodec.pc
else
	$(SED) 's#@@PREFIX@@#$(PREFIX)#' ffnvcodec.pc.in > ffnvcodec.pc
endif

install: all
	$(INSTALL) -m 0755 -d '$(DESTDIR)$(PREFIX)/include/ffnvcodec'
	$(INSTALL) -m 0644 include/ffnvcodec/*.h '$(DESTDIR)$(PREFIX)/include/ffnvcodec'
	$(INSTALL) -m 0755 -d '$(DESTDIR)$(PREFIX)/$(LIBDIR)/pkgconfig'
	$(INSTALL) -m 0644 ffnvcodec.pc '$(DESTDIR)$(PREFIX)/$(LIBDIR)/pkgconfig'

uninstall:
	rm -rf '$(DESTDIR)$(PREFIX)/include/ffnvcodec' '$(DESTDIR)$(PREFIX)/$(LIBDIR)/pkgconfig/ffnvcodec.pc'

.PHONY: all install uninstall

