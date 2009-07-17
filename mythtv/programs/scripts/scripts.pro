include ( ../../settings.pro )

QMAKE_STRIP = echo

TEMPLATE = app
CONFIG -= moc qt

installscripts.path = $${PREFIX}/share/mythtv
installscripts.files = database/*

INSTALLS += installscripts

SOURCES += dummy.c
