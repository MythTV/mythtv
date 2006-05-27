include ( ../config.mak )
include ( ../settings.pro )

TEMPLATE = app
CONFIG += thread opengl
TARGET = mythtv-setup
target.path = $${PREFIX}/bin

INCLUDEPATH += ../libs/libmythtv ../libs ../libs/libmyth

LIBS += -L../libs/libmyth -L../libs/libmythtv -L../libs/libavcodec
LIBS += -L../libs/libavformat -L../libs/libavutil -L../libs/libmythui
LIBS += -L../libs/libmythfreemheg

LIBS += -lmythtv-$$LIBVERSION -lmythavformat-$$LIBVERSION
LIBS += -lmythavcodec-$$LIBVERSION -lmythavutil-$$LIBVERSION
LIBS += -lmythfreemheg-$$LIBVERSION
LIBS += -lmyth-$$LIBVERSION -lmythui-$$LIBVERSION $$EXTRA_LIBS

LIBS += `freetype-config --libs`

macx:using_firewire:LIBS += -F$${CONFIG_MAC_AVC} -framework AVCVideoServices

isEmpty(QMAKE_EXTENSION_SHLIB) {
  QMAKE_EXTENSION_SHLIB=so
}
isEmpty(QMAKE_EXTENSION_LIB) {
  QMAKE_EXTENSION_LIB=a
}
TARGETDEPS += ../libs/libmythui/libmythui-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../libs/libmyth/libmyth-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../libs/libmythtv/libmythtv-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../libs/libavcodec/libmythavcodec-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../libs/libavformat/libmythavformat-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../libs/libavutil/libmythavutil-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}

DEPENDPATH += ../libs/libmythtv ../libs/libmyth ../libs/libavcodec
DEPENDPATH += ../libs/libavformat ../libs/libavutil ../libs/libmythui

INCLUDEPATH += ../libs/libmythtv/dvbdev

# Input
HEADERS += backendsettings.h
SOURCES += backendsettings.cpp checksetup.cpp main.cpp

menu.path = $${PREFIX}/share/mythtv/
menu.files += setup.xml

INSTALLS = target
INSTALLS += menu
