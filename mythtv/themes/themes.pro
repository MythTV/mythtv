include ( ../config.mak )
include ( ../settings.pro )

QMAKE_STRIP = echo

TEMPLATE = app
CONFIG -= moc qt

themes.path = $${PREFIX}/share/mythtv/themes/
themes.files = blue defaultosd blueosd oldosd default G.A.N.T. classic Titivillus Titivillus-OSD Iulius Minimalist-wide

fonts.path = $${PREFIX}/share/mythtv
fonts.files = FreeSans.ttf FreeMono.ttf

INSTALLS += themes fonts

# Input
SOURCES += dummy.c
