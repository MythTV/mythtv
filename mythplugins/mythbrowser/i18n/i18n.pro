include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = aux

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files += mythbrowser_bg.qm
trans.files += mythbrowser_cs.qm
trans.files += mythbrowser_da.qm
trans.files += mythbrowser_de.qm
trans.files += mythbrowser_el.qm
trans.files += mythbrowser_en_ca.qm
trans.files += mythbrowser_en_gb.qm
trans.files += mythbrowser_en_us.qm
trans.files += mythbrowser_es_es.qm
trans.files += mythbrowser_es.qm
trans.files += mythbrowser_et.qm
trans.files += mythbrowser_fi.qm
trans.files += mythbrowser_fr.qm
trans.files += mythbrowser_hu.qm
trans.files += mythbrowser_it.qm
trans.files += mythbrowser_ja.qm
trans.files += mythbrowser_nb.qm
trans.files += mythbrowser_nl.qm
trans.files += mythbrowser_pl.qm
trans.files += mythbrowser_pt_br.qm
trans.files += mythbrowser_pt.qm
trans.files += mythbrowser_ru.qm
trans.files += mythbrowser_sl.qm
trans.files += mythbrowser_sv.qm
trans.files += mythbrowser_zh_hk.qm

INSTALLS += trans
