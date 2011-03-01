include ( ../../settings.pro )

QMAKE_STRIP = echo

TEMPLATE = app
CONFIG -= moc qt

QMAKE_COPY_DIR = MYTHPYTHON=$${PYTHON} sh ./cpsvndir
win32:QMAKE_COPY_DIR = sh ./cpsimple

installscripts.path = $${PREFIX}/share/mythtv
installscripts.files = database/*

installinternetscripts.path = $${PREFIX}/share/mythtv
installinternetscripts.files = internetcontent metadata hardwareprofile

INSTALLS += installscripts installinternetscripts

SOURCES += dummy.c
