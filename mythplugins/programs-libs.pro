INCLUDEPATH += $${PREFIX}/include/mythtv

LIBS += -L$${PREFIX}/lib $$EXTRA_LIBS
LIBS += -lmythavutil-$$LIBVERSION -lmythavcodec-$$LIBVERSION
LIBS += -lmyth-$$LIBVERSION -lmythui-$$LIBVERSION -lmythupnp-$$LIBVERSION

mac:using_firewire:using_backend:LIBS += -F$${CONFIG_MAC_AVC} -framework AVCVideoServices

# On Windows, libs are stored with the binaries:
mingw:LIBS += -L$${PREFIX}/bin
