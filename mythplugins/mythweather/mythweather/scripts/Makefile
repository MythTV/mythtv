include ../../../mythconfig.mak

SUBDIRS  = ca_envcan no_yrno uk_bbc us_nws wunderground

# Do not change anything below this line unless you really know what you are
# doing.

INSTDIR = ${PREFIX}/share/mythtv/mythweather/scripts

install_SUBDIRS   = $(addprefix install/,${SUBDIRS})
uninstall_SUBDIRS = $(addprefix uninstall/,${SUBDIRS})

.PHONY:	${SUBDIRS} ${install_SUBDIRS} ${uninstall_SUBDIRS}

all first clean distclean:

install:	${install_SUBDIRS}
uninstall:	${uninstall_SUBDIRS}

${install_SUBDIRS} ${uninstall_SUBDIRS}:	
	make -C ${@F} ${@D} INSTDIR=${INSTDIR}/${@F}

