include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files  = mythvideo_it.qm mythvideo_es.qm mythvideo_ca.qm
trans.files += mythvideo_nl.qm mythvideo_de.qm mythvideo_dk.qm
trans.files += mythvideo_pt.qm mythvideo_sv.qm mythvideo_ja.qm
trans.files += mythvideo_fr.qm mythvideo_si.qm mythvideo_nb.qm
trans.files += mythvideo_fi.qm

INSTALLS += trans

SOURCES += dummy.c
