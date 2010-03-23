include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files += mythzoneminder_sv.qm mythzoneminder_hu.qm mythzoneminder_da.qm mythzoneminder_fr.qm

INSTALLS += trans

SOURCES += dummy.c
