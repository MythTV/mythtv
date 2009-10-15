include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files  = mythflix_fi.qm mythflix_es.qm mythflix_sv.qm mythflix_nl.qm
trans.files += mythflix_et.qm mythflix_nb.ts mythflix_ru.qm mythflix_sl.qm
trans.files += mythflix_cs.qm mythflix_fr.qm
trans.files += mythflix_hu.qm

INSTALLS += trans

SOURCES += dummy.c 
