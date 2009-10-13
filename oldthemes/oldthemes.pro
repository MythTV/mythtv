!include ( mythconfig.mak ) {
    error("Please run ./configure")
}

QMAKE_STRIP = echo

TEMPLATE = app
CONFIG -= moc qt

QMAKE_COPY_DIR = sh ./cpsvndir
win32:QMAKE_COPY_DIR = sh ./cpsimple

themes.path = $${PREFIX}/share/mythtv/themes/
themes.files = Iulius Minimalist-wide Titivillus
themes.files += Retro blue G.A.N.T

INSTALLS += themes

# Input
SOURCES += dummy.c
