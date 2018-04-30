include ( ../settings.pro )

TEMPLATE = lib
CONFIG -= moc qt
CONFIG += plugin thread
target.path = $${LIBDIR}/mythtv/filters
INSTALLS = target

QMAKE_CFLAGS += -Wno-missing-prototypes
QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

INCLUDEPATH += ../../libs/libmythtv ../../libs/libmythbase
INCLUDEPATH += ../.. ../../external/FFmpeg
DEPENDPATH += ../../libs/libmythtv ../../libs/libmythbase
DEPENDPATH  += ../.. ../../external/FFmpeg

macx:LIBS += $$EXTRA_LIBS

android {
    # to discriminate filters in a flat directory structure
    TARGET = mythfilter$${TARGET}
}
