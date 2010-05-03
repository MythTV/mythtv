include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files  = mythvideo_it.qm mythvideo_es.qm mythvideo_ca.qm
trans.files += mythvideo_nl.qm mythvideo_de.qm mythvideo_da.qm
trans.files += mythvideo_pt.qm mythvideo_sv.qm mythvideo_ja.qm
trans.files += mythvideo_fr.qm mythvideo_sl.qm mythvideo_nb.qm
trans.files += mythvideo_fi.qm mythvideo_et.qm mythvideo_ru.qm
trans.files += mythvideo_cs.qm mythvideo_hu.qm mythvideo_el.qm
trans.files += mythvideo_en_gb.qm mythvideo_en_us.qm

INSTALLS += trans

SOURCES += dummy.c

