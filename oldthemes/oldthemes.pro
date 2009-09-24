!include ( mythconfig.mak ) {
    error("Please run ./configure")
}

QMAKE_STRIP = echo

TEMPLATE = app
CONFIG -= moc qt

QMAKE_COPY_DIR = sh ./cpsvndir
win32:QMAKE_COPY_DIR = sh ./cpsimple

themes.path = $${PREFIX}/share/mythtv/themes/
themes.files = Iulius Iulius-OSD Minimalist-wide Titivillus Titivillus-OSD
themes.files += isthmus Gray-OSD Retro Retro-OSD blue blueosd G.A.N.T

INSTALLS += themes

# Input
SOURCES += dummy.c
