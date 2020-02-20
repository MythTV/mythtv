include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = aux

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files += mythgame_bg.qm
trans.files += mythgame_ca.qm
trans.files += mythgame_cs.qm
trans.files += mythgame_da.qm
trans.files += mythgame_de.qm
trans.files += mythgame_el.qm
trans.files += mythgame_en_ca.qm
trans.files += mythgame_en_gb.qm
trans.files += mythgame_en_us.qm
trans.files += mythgame_es_es.qm
trans.files += mythgame_es.qm
trans.files += mythgame_et.qm
trans.files += mythgame_fi.qm
trans.files += mythgame_fr.qm
trans.files += mythgame_hu.qm
trans.files += mythgame_it.qm
trans.files += mythgame_nb.qm
trans.files += mythgame_nl.qm
trans.files += mythgame_pl.qm
trans.files += mythgame_pt_br.qm
trans.files += mythgame_pt.qm
trans.files += mythgame_ru.qm
trans.files += mythgame_sl.qm
trans.files += mythgame_sv.qm

INSTALLS += trans
