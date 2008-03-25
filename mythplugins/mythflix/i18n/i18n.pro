include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files  = mythflix_fi.qm mythflix_es.qm mythflix_sv.qm mythflix_nl.qm
trans.files += mythflix_et.qm mythflix_nb.ts mythflix_ru.qm mythflix_si.qm
trans.files += mythflix_cz.qm mythflix_fr.qm

INSTALLS += trans

SOURCES += dummy.c 
#The following line was inserted by qt3to4
QT += xml  sql qt3support 
