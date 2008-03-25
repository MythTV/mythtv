include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files  = mythcontrols_sv.qm mythcontrols_fi.qm mythcontrols_es.qm
trans.files += mythcontrols_de.qm mythcontrols_nl.qm mythcontrols_dk.qm
trans.files += mythcontrols_et.qm mythcontrols_nb.qm mythcontrols_ru.qm
trans.files += mythcontrols_si.qm mythcontrols_fr.qm mythcontrols_cz.qm

INSTALLS += trans

SOURCES += dummy.c
#The following line was inserted by qt3to4
QT +=  qt3support
