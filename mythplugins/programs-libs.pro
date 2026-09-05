INCLUDEPATH += $${SYSROOT}$${PREFIX}/include/mythtv
DEPENDPATH *= $${INCLUDEPATH}

LIBS += -L$${SYSROOT}$${LIBDIR} $$EXTRA_LIBS -lmythbase-$$LIBVERSION
LIBS += -lmyth-$$LIBVERSION -lmythui-$$LIBVERSION -lmythupnp-$$LIBVERSION
LIBS += $$mythFFmpegLib(avcodec)
LIBS += $$mythFFmpegLib(avutil)
LIBS += $$mythFFmpegLib(avformat)
LIBS += $$mythFFmpegLib(swresample)

# On Windows, libs are stored with the binaries:
mingw:LIBS += -L$${SYSROOT}$${PREFIX}/bin

# Trigger rebuilds if installed MythTV libs have changed
include (targetdep.pro)
