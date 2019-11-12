include ( ../settings.pro )

QMAKE_STRIP = echo

TEMPLATE = app
CONFIG -= moc qt

themes.path = $${PREFIX}/share/mythtv/themes/
themes.files = default default-wide classic DVR Slave
themes.files += Terra defaultmenu mediacentermenu
themes.files += MythCenter MythCenter-wide
themes.files += mythuitheme.dtd

fonts.path = $${PREFIX}/share/mythtv/
fonts.files = fonts

INSTALLS += themes fonts

# Input
SOURCES += dummy.c
