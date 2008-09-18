INCLUDEPATH += $${PREFIX}/include/mythtv

LIBS += -L$${LIBDIR} $$EXTRA_LIBS -lmythdb-$$LIBVERSION
LIBS += -lmythavutil-$$LIBVERSION -lmythavcodec-$$LIBVERSION
LIBS += -lmyth-$$LIBVERSION -lmythui-$$LIBVERSION -lmythupnp-$$LIBVERSION
LIBS += -lmythfreemheg-$$LIBVERSION -lmythlivemedia-$$LIBVERSION
LIBS += -lmythtv-$$LIBVERSION -lmythavformat-$$LIBVERSION

mac:using_firewire:using_backend:LIBS += -F$${CONFIG_MAC_AVC} -framework AVCVideoServices

# On Windows, libs are stored with the binaries:
mingw:LIBS += -L$${PREFIX}/bin

# Trigger rebuilds if installed MythTV libs have changed
include (../../targetdep.pro)
