include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = aux

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files += mythnews_bg.qm
trans.files += mythnews_ca.qm
trans.files += mythnews_cs.qm
trans.files += mythnews_da.qm
trans.files += mythnews_de.qm
trans.files += mythnews_el.qm
trans.files += mythnews_en_ca.qm
trans.files += mythnews_en_gb.qm
trans.files += mythnews_en_us.qm
trans.files += mythnews_es_es.qm
trans.files += mythnews_es.qm
trans.files += mythnews_et.qm
trans.files += mythnews_fi.qm
trans.files += mythnews_fr.qm
trans.files += mythnews_hu.qm
trans.files += mythnews_it.qm
trans.files += mythnews_ja.qm
trans.files += mythnews_nb.qm
trans.files += mythnews_nl.qm
trans.files += mythnews_pl.qm
trans.files += mythnews_pt.qm
trans.files += mythnews_ru.qm
trans.files += mythnews_sl.qm
trans.files += mythnews_sv.qm

INSTALLS += trans
