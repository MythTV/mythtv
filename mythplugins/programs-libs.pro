INCLUDEPATH += $${SYSROOT}$${PREFIX}/include/mythtv
INCLUDEPATH += $${SYSROOT}$${PREFIX}/include/mythtv/libmythbase
INCLUDEPATH += $${SYSROOT}$${PREFIX}/include/mythtv/libmythui
INCLUDEPATH += $${SYSROOT}$${PREFIX}/include/mythtv/libmyth
DEPENDPATH *= $${INCLUDEPATH}

LIBS += -L$${LIBDIR} $$EXTRA_LIBS -lmythbase-$$LIBVERSION
LIBS += -lmyth-$$LIBVERSION -lmythui-$$LIBVERSION -lmythupnp-$$LIBVERSION
LIBS += -lmythservicecontracts-$$LIBVERSION
LIBS += -lmythavcodec
LIBS += -lmythavutil
LIBS += -lmythavformat
LIBS += -lmythswresample

# On Windows, libs are stored with the binaries:
mingw:LIBS += -L$${SYSROOT}$${PREFIX}/bin

# Trigger rebuilds if installed MythTV libs have changed
include (targetdep.pro)
