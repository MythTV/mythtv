include ( ../config.mak )
include ( ../settings.pro )

QMAKE_STRIP = echo

TEMPLATE = app
CONFIG -= moc qt

QMAKE_COPY_DIR = sh ./cpsvndir
win32:QMAKE_COPY_DIR = sh ./cpsimple

themes.path = $${PREFIX}/share/mythtv/themes/
themes.files = blue defaultosd blueosd default default-wide G.A.N.T classic DVR
themes.files += isthmus Terra defaultmenu

fonts.path = $${PREFIX}/share/mythtv
fonts.files = FreeSans.ttf FreeSansBold.ttf FreeMono.ttf

INSTALLS += themes fonts

# Input
SOURCES += dummy.c
