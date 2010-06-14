include ( ../settings.pro )

QMAKE_STRIP = echo

TEMPLATE = app
CONFIG -= moc qt

QMAKE_COPY_DIR = sh ./cpsvndir
win32:QMAKE_COPY_DIR = sh ./cpsimple

themes.path = $${PREFIX}/share/mythtv/themes/
themes.files = default default-wide classic DVR
themes.files += Terra defaultmenu mediacentermenu
themes.files += MythCenter MythCenter-wide
themes.files += BlackCurves-OSD isthmus Gray-OSD
themes.files += mythuitheme.dtd

fonts.path = $${PREFIX}/share/mythtv/fonts
fonts.files = FreeSans.ttf FreeSansBold.ttf FreeMono.ttf

INSTALLS += themes fonts

# Input
SOURCES += dummy.c
