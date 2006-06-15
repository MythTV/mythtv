include ( ../config.mak )
include ( ../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files  = mythfrontend_it.qm mythfrontend_es.qm mythfrontend_ca.qm
trans.files += mythfrontend_nl.qm mythfrontend_fr.qm mythfrontend_de.qm
trans.files += mythfrontend_dk.qm mythfrontend_pt.qm mythfrontend_sv.qm
trans.files += mythfrontend_ja.qm mythfrontend_si.qm mythfrontend_fi.qm
trans.files += mythfrontend_zh_tw.qm mythfrontend_nb.qm mythfrontend_is.qm
trans.files += mythfrontend_pt_br.qm mythfrontend_en_gb.qm mythfrontend_cz.qm
trans.files += mythfrontend_et.qm mythfrontend_pl.qm mythfrontend_tr.qm

INSTALLS += trans

SOURCES += dummy.c
