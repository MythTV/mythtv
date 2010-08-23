include ( ../../settings.pro )

TEMPLATE = lib
CONFIG -= moc qt
CONFIG += plugin thread
target.path = $${LIBDIR}/mythtv/filters
INSTALLS = target

QMAKE_CFLAGS += -Wno-missing-prototypes
QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

INCLUDEPATH += ../../libs/libmythtv ../../libs/libmythdb
INCLUDEPATH += ../../external/FFmpeg
DEPENDPATH += ../../libs/libmythtv ../../libs/libmythdb
DEPENDPATH  += ../../external/FFmpeg
