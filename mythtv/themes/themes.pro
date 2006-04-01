include ( ../config.mak )
include ( ../settings.pro )

QMAKE_STRIP = echo

TEMPLATE = app
CONFIG -= moc qt

!macx:QMAKE_COPY_DIR = sh ./cpsvndir

themes.path = $${PREFIX}/share/mythtv/themes/
themes.files = blue defaultosd blueosd oldosd default default-wide G.A.N.T. classic DVR

fonts.path = $${PREFIX}/share/mythtv
fonts.files = FreeSans.ttf FreeMono.ttf

INSTALLS += themes fonts

# Input
SOURCES += dummy.c
