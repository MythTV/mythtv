include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files += mythzoneminder_sv.qm mythzoneminder_da.qm mythzoneminder_en_gb.qm
trans.files += mythzoneminder_fr.qm mythzoneminder_hu.qm mythzoneminder_en_us.qm

INSTALLS += trans

SOURCES += dummy.c
