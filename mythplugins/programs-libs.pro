INCLUDEPATH += $${SYSROOT}$${PREFIX}/include/mythtv
INCLUDEPATH += $${SYSROOT}$${PREFIX}/include/mythtv/libmythdb
INCLUDEPATH += $${SYSROOT}$${PREFIX}/include/mythtv/libmythui
INCLUDEPATH += $${SYSROOT}$${PREFIX}/include/mythtv/libmyth
DEPENDPATH *= $${INCLUDEPATH}

LIBS += -L$${LIBDIR} $$EXTRA_LIBS -lmythdb-$$LIBVERSION
LIBS += -lmyth-$$LIBVERSION -lmythui-$$LIBVERSION -lmythupnp-$$LIBVERSION

#we probably do not need this on macs anymore since we do not link libmythtv
#mac:using_firewire:using_backend:{
#    LIBS += -F$${CONFIG_MAC_AVC} -framework AVCVideoServices
#}

# On Windows, libs are stored with the binaries:
mingw:LIBS += -L$${SYSROOT}$${PREFIX}/bin

# Trigger rebuilds if installed MythTV libs have changed
include (../../targetdep.pro)
