include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files += mythnetvision_da.qm mythnetvision_fr.qm mythnetvision_sv.qm
trans.files += mythnetvision_et.qm

INSTALLS += trans

SOURCES += dummy.c
