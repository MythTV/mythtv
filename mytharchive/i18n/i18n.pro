include ( ../mythconfig.mak )
include ( ../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files += mytharchive_de.qm mytharchive_si.qm mytharchive_fr.qm
trans.files += mytharchive_sv.qm mytharchive_nl.qm mytharchive_nb.qm
trans.files += mytharchive_fi.qm mytharchive_es.qm

INSTALLS += trans

SOURCES += dummy.c
