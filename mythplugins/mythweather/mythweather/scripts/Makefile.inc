include ../../../../mythconfig.mak

ifeq (.${INSTDIR}.,..)

PWD = $(shell pwd)
THISDIR = $(notdir ${PWD})

# INSTDIR not set, recurse up a levels
all:

install install-scripts install-data:
	${MAKE} -C .. install SUBDIRS=${THISDIR}

uninstall uninstall-scripts uninstall-data:
	${MAKE} -C .. uninstall SUBDIRS=${THISDIR}

else

# All is good.  Do it.
all:

install:	install-scripts install-data
uninstall:	uninstall-scripts uninstall-data

install-scripts:
	-[ "${SCRIPTS}" != "" ] && \
	    ${INSTALL} -m 755 -d ${INSTALL_ROOT}${INSTDIR}/ && \
	    for i in ${SCRIPTS} ; do ${INSTALL} -m 755 $$i ${INSTALL_ROOT}${INSTDIR}/ ; done

install-data:
	-[ "${DATA}" != "" ] && \
	    ${INSTALL} -m 755 -d ${INSTALL_ROOT}${INSTDIR}/ && \
	    for i in ${DATA} ; do ${INSTALL} -m 644 $$i ${INSTALL_ROOT}${INSTDIR}/ ; done

uninstall-scripts:
	-[ "${SCRIPTS}" != "" ] && \
	    for i in ${SCRIPTS} ; do ${RM} ${INSTALL_ROOT}${INSTDIR}/$$i ; done

uninstall-data:
	-[ "${DATA}" != "" ] && \
	    for i in ${DATA} ; do ${RM} ${INSTALL_ROOT}${INSTDIR}/$$i ; done

endif
