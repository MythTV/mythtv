include ( ../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files  = mythmusic_it.qm mythmusic_es.qm mythmusic_ca.qm
trans.files += mythmusic_nl.qm mythmusic_de.qm mythmusic_dk.qm
trans.files += mythmusic_pt.qm mythmusic_sv.qm mythmusic_fr.qm

INSTALLS += trans

SOURCES += dummy.c
