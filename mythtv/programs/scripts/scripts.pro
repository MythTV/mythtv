include ( ../../settings.pro )

QMAKE_STRIP = echo

TEMPLATE = app
CONFIG -= moc qt

installscripts.path = $${PREFIX}/share/mythtv
installscripts.files = database/*

installinternetscripts.path = $${PREFIX}/share/mythtv
installinternetscripts.files = internetcontent metadata hardwareprofile

INSTALLS += installscripts installinternetscripts

SOURCES += dummy.c
