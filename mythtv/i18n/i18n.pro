include ( ../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files = mythfrontend_it.qm mythfrontend_es.qm mythfrontend_ca.qm

INSTALLS += trans

SOURCES += dummy.c
