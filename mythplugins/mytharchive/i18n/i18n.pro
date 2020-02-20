include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = aux

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files += mytharchive_bg.qm
trans.files += mytharchive_cs.qm
trans.files += mytharchive_da.qm
trans.files += mytharchive_de.qm
trans.files += mytharchive_el.qm
trans.files += mytharchive_en_ca.qm
trans.files += mytharchive_en_gb.qm
trans.files += mytharchive_en_us.qm
trans.files += mytharchive_es_es.qm
trans.files += mytharchive_es.qm
trans.files += mytharchive_et.qm
trans.files += mytharchive_fi.qm
trans.files += mytharchive_fr.qm
trans.files += mytharchive_hu.qm
trans.files += mytharchive_it.qm
trans.files += mytharchive_nb.qm
trans.files += mytharchive_nl.qm
trans.files += mytharchive_pl.qm
trans.files += mytharchive_pt.qm
trans.files += mytharchive_ru.qm
trans.files += mytharchive_sl.qm
trans.files += mytharchive_sv.qm
trans.files += mytharchive_zh_hk.qm

INSTALLS += trans
