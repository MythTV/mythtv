include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = aux

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files += mythmusic_bg.qm
trans.files += mythmusic_ca.qm
trans.files += mythmusic_cs.qm
trans.files += mythmusic_da.qm
trans.files += mythmusic_de.qm
trans.files += mythmusic_el.qm
trans.files += mythmusic_en_ca.qm
trans.files += mythmusic_en_gb.qm
trans.files += mythmusic_en_us.qm
trans.files += mythmusic_es_es.qm
trans.files += mythmusic_es.qm
trans.files += mythmusic_et.qm
trans.files += mythmusic_fi.qm
trans.files += mythmusic_fr.qm
trans.files += mythmusic_hu.qm
trans.files += mythmusic_it.qm
trans.files += mythmusic_ja.qm
trans.files += mythmusic_nb.qm
trans.files += mythmusic_nl.qm
trans.files += mythmusic_pl.qm
trans.files += mythmusic_pt_br.qm
trans.files += mythmusic_pt.qm
trans.files += mythmusic_ru.qm
trans.files += mythmusic_sl.qm
trans.files += mythmusic_sv.qm

INSTALLS += trans
