include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files += mytharchive_de.qm mytharchive_si.qm mytharchive_fr.qm
trans.files += mytharchive_sv.qm mytharchive_nl.qm mytharchive_nb.qm
trans.files += mytharchive_fi.qm mytharchive_es.qm mytharchive_et.qm
trans.files += mytharchive_dk.qm mytharchive_cz.qm mytharchive_si.qm

INSTALLS += trans

SOURCES += dummy.c
#The following line was inserted by qt3to4
QT += xml  sql qt3support 
