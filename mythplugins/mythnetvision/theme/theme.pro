include ( ../../mythconfig.mak )
include ( ../../settings.pro )

QMAKE_STRIP = echo

TARGET = themenop
TEMPLATE = app
CONFIG -= qt moc

defaultfiles.path = $${PREFIX}/share/mythtv/themes/default
defaultfiles.files = default/*.xml

widefiles.path = $${PREFIX}/share/mythtv/themes/default-wide
widefiles.files = default-wide/*.xml

menufiles.path = $${PREFIX}/share/mythtv/
menufiles.files = menus/*.xml

INSTALLS += defaultfiles widefiles menufiles

# Input
SOURCES += ../../themedummy.c
