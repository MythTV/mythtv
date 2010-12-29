include ../../config.mak

FILES  = MythBackend.php MythBase.php MythFrontend.php MythTVChannel.php
FILES += MythTV.php MythTVProgram.php MythTVRecording.php
FILES += MythTVStorageGroup.php

INSTDIR = ${INSTALL_ROOT}${PREFIX}/share/mythtv/bindings/php
INSTALL = /usr/bin/install

all:
clean:
distclean: clean

install:
	$(INSTALL) -m 755 -d ${INSTDIR}/
	for i in ${FILES} ; do ${INSTALL} -m 644 $$i ${INSTDIR}/ ; done
	
uninstall:
	for i in ${FILES} ; do ${RM} ${INSTDIR}/$$i ; done

.PHONY: all clean distclean install uninstall
