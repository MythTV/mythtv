include ( ../../mythconfig.mak )
include ( ../../settings.pro )

INCLUDEPATH += $${PREFIX}/include/mythtv

TEMPLATE = app
CONFIG += thread opengl

target.path = $${PREFIX}/bin

INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += ../mytharchive/archiveutil.h
SOURCES += main.cpp ../mytharchive/archiveutil.cpp

LIBS += -L$${PREFIX}/lib $$EXTRA_LIBS
LIBS += -lmyth-$$LIBVERSION -lmythtv-$$LIBVERSION 
LIBS += -lmythui-$$LIBVERSION -lmythfreemheg-$$LIBVERSION -lmythlivemedia-$$LIBVERSION
LIBS += -lmythavutil-$$LIBVERSION
LIBS += -lmythavcodec-$$LIBVERSION
LIBS += -lmythavformat-$$LIBVERSION

macx {
    # libmyth depends on the following:
    using_firewire:using_backend:LIBS += -F$${CONFIG_MAC_AVC} -framework AVCVideoServices
}
