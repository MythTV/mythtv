include ( ../../mythconfig.mak )
include ( ../../settings.pro )

QMAKE_STRIP = echo

TARGET = themenop
TEMPLATE = app
CONFIG -= qt moc

!macx:QMAKE_COPY_DIR = sh ../../cpsvndir

defaultfiles.path = $${PREFIX}/share/mythtv/themes/default
defaultfiles.files = default/*.xml default/images/*.png

widefiles.path = $${PREFIX}/share/mythtv/themes/default-wide
widefiles.files = default-wide/*.xml default-wide/images/*.png

menufiles.path = $${PREFIX}/share/mythtv/
menufiles.files = menus/*.xml

INSTALLS += defaultfiles widefiles menufiles

# Input
SOURCES += ../../themedummy.c
#The following line was inserted by qt3to4
QT += xml  qt3support 
