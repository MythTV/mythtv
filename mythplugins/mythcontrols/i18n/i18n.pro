include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files  = mythcontrols_sv.qm mythcontrols_fi.qm mythcontrols_es.qm

INSTALLS += trans

SOURCES += dummy.c
