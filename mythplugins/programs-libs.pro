INCLUDEPATH += $${SYSROOT}$${PREFIX}/include/mythtv
DEPENDPATH *= $${INCLUDEPATH}

LIBS += -L$${SYSROOT}$${LIBDIR} $$EXTRA_LIBS -lmythbase-$$LIBVERSION
LIBS += -lmyth-$$LIBVERSION -lmythui-$$LIBVERSION -lmythupnp-$$LIBVERSION
LIBS += -lmythavcodec-$$LIBVERSION
LIBS += -lmythavutil-$$LIBVERSION
LIBS += -lmythavformat-$$LIBVERSION
LIBS += -lmythswresample-$$LIBVERSION

# On Windows, libs are stored with the binaries:
mingw:LIBS += -L$${SYSROOT}$${PREFIX}/bin

# Trigger rebuilds if installed MythTV libs have changed
include (targetdep.pro)
