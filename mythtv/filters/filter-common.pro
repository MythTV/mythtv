include ( ../../config.mak )
include ( ../../settings.pro )

TEMPLATE = lib
CONFIG -= moc qt
CONFIG += plugin thread
target.path = $${PREFIX}/lib/mythtv/filters
INSTALLS = target

QMAKE_CFLAGS_RELEASE += -Wno-missing-prototypes
QMAKE_CFLAGS_DEBUG += -Wno-missing-prototypes
