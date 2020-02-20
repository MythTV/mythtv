include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = aux

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files += mythzoneminder_bg.qm
trans.files += mythzoneminder_cs.qm
trans.files += mythzoneminder_da.qm
trans.files += mythzoneminder_de.qm
trans.files += mythzoneminder_el.qm
trans.files += mythzoneminder_en_ca.qm
trans.files += mythzoneminder_en_gb.qm
trans.files += mythzoneminder_en_us.qm
trans.files += mythzoneminder_es_es.qm
trans.files += mythzoneminder_es.qm
trans.files += mythzoneminder_et.qm
trans.files += mythzoneminder_fi.qm
trans.files += mythzoneminder_fr.qm
trans.files += mythzoneminder_hu.qm
trans.files += mythzoneminder_it.qm
trans.files += mythzoneminder_nb.qm
trans.files += mythzoneminder_nl.qm
trans.files += mythzoneminder_pl.qm
trans.files += mythzoneminder_pt.qm
trans.files += mythzoneminder_ru.qm
trans.files += mythzoneminder_sv.qm

INSTALLS += trans
