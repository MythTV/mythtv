include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path   = $${PREFIX}/share/mythtv/i18n/
trans.files  = mythweather_es.qm mythweather_ca.qm mythweather_nl.qm
trans.files += mythweather_dk.qm mythweather_pt.qm mythweather_sv.qm
trans.files += mythweather_de.qm mythweather_ja.qm mythweather_it.qm
trans.files += mythweather_fr.qm mythweather_si.qm mythweather_fi.qm
trans.files += mythweather_ru.qm mythweather_cz.qm

INSTALLS += trans

SOURCES += dummy.c
#The following line was inserted by qt3to4
QT += xml  qt3support 
