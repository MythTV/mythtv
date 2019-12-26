include ( ../../settings.pro )

TEMPLATE = aux

installscripts.path = $${PREFIX}/share/mythtv
installscripts.files = database/*

installinternetscripts.path = $${PREFIX}/share/mythtv
installinternetscripts.files = internetcontent metadata hardwareprofile

INSTALLS += installscripts installinternetscripts
