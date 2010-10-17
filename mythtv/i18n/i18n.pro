include ( ../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files  = mythfrontend_it.qm mythfrontend_es.qm mythfrontend_ca.qm
trans.files += mythfrontend_nl.qm mythfrontend_fr.qm mythfrontend_de.qm
trans.files += mythfrontend_da.qm mythfrontend_pt.qm mythfrontend_sv.qm
trans.files += mythfrontend_ja.qm mythfrontend_sl.qm mythfrontend_fi.qm
trans.files += mythfrontend_nb.qm mythfrontend_is.qm mythfrontend_pt_br.qm
trans.files += mythfrontend_cs.qm mythfrontend_et.qm mythfrontend_en_gb.qm
trans.files += mythfrontend_pl.qm mythfrontend_tr.qm mythfrontend_en_us.qm
trans.files += mythfrontend_ru.qm mythfrontend_he.qm
trans.files += mythfrontend_hu.qm mythfrontend_el.qm mythfrontend_bg.qm

INSTALLS += trans

SOURCES += dummy.c
