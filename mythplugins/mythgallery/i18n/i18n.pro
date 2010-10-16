include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files  = mythgallery_it.qm mythgallery_es.qm mythgallery_ca.qm
trans.files += mythgallery_nl.qm mythgallery_de.qm mythgallery_da.qm
trans.files += mythgallery_pt.qm mythgallery_sv.qm mythgallery_fr.qm
trans.files += mythgallery_ja.qm mythgallery_sl.qm mythgallery_nb.qm
trans.files += mythgallery_fi.qm mythgallery_et.qm mythgallery_ru.qm
trans.files += mythgallery_cs.qm mythgallery_hu.qm mythgallery_en_gb.qm
trans.files += mythgallery_el.qm mythgallery_pl.qm mythgallery_en_us.qm

INSTALLS += trans

SOURCES += dummy.c
