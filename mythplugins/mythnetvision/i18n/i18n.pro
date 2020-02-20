include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = aux

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files += mythnetvision_bg.qm
trans.files += mythnetvision_cs.qm
trans.files += mythnetvision_da.qm
trans.files += mythnetvision_de.qm
trans.files += mythnetvision_el.qm
trans.files += mythnetvision_en_ca.qm
trans.files += mythnetvision_en_gb.qm
trans.files += mythnetvision_en_us.qm
trans.files += mythnetvision_es_es.qm
trans.files += mythnetvision_es.qm
trans.files += mythnetvision_et.qm
trans.files += mythnetvision_fi.qm
trans.files += mythnetvision_fr.qm
trans.files += mythnetvision_it.qm
trans.files += mythnetvision_ja.qm
trans.files += mythnetvision_nb.qm
trans.files += mythnetvision_nl.qm
trans.files += mythnetvision_pl.qm
trans.files += mythnetvision_pt.qm
trans.files += mythnetvision_ru.qm
trans.files += mythnetvision_sv.qm

INSTALLS += trans
