include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = aux

trans.path   = $${PREFIX}/share/mythtv/i18n/
trans.files += mythweather_bg.qm
trans.files += mythweather_ca.qm
trans.files += mythweather_cs.qm
trans.files += mythweather_da.qm
trans.files += mythweather_de.qm
trans.files += mythweather_el.qm
trans.files += mythweather_en_ca.qm
trans.files += mythweather_en_gb.qm
trans.files += mythweather_en_us.qm
trans.files += mythweather_es_es.qm
trans.files += mythweather_es.qm
trans.files += mythweather_et.qm
trans.files += mythweather_fi.qm
trans.files += mythweather_fr.qm
trans.files += mythweather_hu.qm
trans.files += mythweather_it.qm
trans.files += mythweather_ja.qm
trans.files += mythweather_nb.qm
trans.files += mythweather_nl.qm
trans.files += mythweather_pl.qm
trans.files += mythweather_pt.qm
trans.files += mythweather_ru.qm
trans.files += mythweather_sl.qm
trans.files += mythweather_sv.qm

INSTALLS += trans
