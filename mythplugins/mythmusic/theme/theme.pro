include ( ../../mythconfig.mak )
include ( ../../settings.pro )
   
QMAKE_STRIP = echo

TARGET = themenop
TEMPLATE = app
CONFIG -= qt moc

QMAKE_COPY_DIR = sh ../../cpsvndir

defaultfiles.path = $${PREFIX}/share/mythtv/themes/default
defaultfiles.files = default/*.xml default/images/*.png

widefiles.path = $${PREFIX}/share/mythtv/themes/default-wide
widefiles.files = default-wide/*.xml default-wide/images/*.png

menufiles.path = $${PREFIX}/share/mythtv/
menufiles.files = menus/*.xml

htmlfiles.path = $${PREFIX}/share/mythtv/themes/default/htmls
htmlfiles.files = htmls/*.html

xmlfiles.path = $${PREFIX}/share/mythtv/mythmusic
xmlfiles.files = ../mythmusic/streams.xml

INSTALLS += defaultfiles widefiles menufiles htmlfiles xmlfiles

# Input
SOURCES += ../../themedummy.c
