include ( ../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files  = mythfrontend_bg.qm
trans.files += mythfrontend_ca.qm
trans.files += mythfrontend_cs.qm
trans.files += mythfrontend_da.qm
trans.files += mythfrontend_de.qm
trans.files += mythfrontend_el.qm
trans.files += mythfrontend_en_gb.qm
trans.files += mythfrontend_en_us.qm
trans.files += mythfrontend_es.qm
trans.files += mythfrontend_et.qm
trans.files += mythfrontend_fi.qm
trans.files += mythfrontend_fr.qm
trans.files += mythfrontend_he.qm
trans.files += mythfrontend_hu.qm
trans.files += mythfrontend_is.qm
trans.files += mythfrontend_it.qm
trans.files += mythfrontend_ja.qm
trans.files += mythfrontend_nb.qm
trans.files += mythfrontend_nl.qm
trans.files += mythfrontend_pl.qm
trans.files += mythfrontend_pt.qm
trans.files += mythfrontend_pt_br.qm
trans.files += mythfrontend_ru.qm
trans.files += mythfrontend_sl.qm
trans.files += mythfrontend_sv.qm
trans.files += mythfrontend_tr.qm

INSTALLS += trans

SOURCES += dummy.c
