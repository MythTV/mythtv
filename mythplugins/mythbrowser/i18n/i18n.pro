include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files += mythbrowser_de.qm mythbrowser_sl.qm mythbrowser_fr.qm
trans.files += mythbrowser_sv.qm mythbrowser_nl.qm mythbrowser_nb.qm
trans.files += mythbrowser_fi.qm mythbrowser_es.qm mythbrowser_pt_br.qm
trans.files += mythbrowser_et.qm mythbrowser_ru.qm mythbrowser_da.qm
trans.files += mythbrowser_cs.qm mythbrowser_sl.qm mythbrowser_en_gb.qm
trans.files += mythbrowser_hu.qm mythbrowser_en_us.qm mythbrowser_el.qm
trans.files += mythbrowser_pt.qm mythbrowser_ja.qm mythbrowser_pl.qm

INSTALLS += trans

SOURCES += dummy.c
