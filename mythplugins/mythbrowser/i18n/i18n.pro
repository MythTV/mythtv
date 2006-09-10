include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files += mythbrowser_de.qm mythbrowser_si.qm mythbrowser_fr.qm
trans.files += mythbrowser_sv.qm mythbrowser_nl.qm mythbrowser_nb.qm
trans.files += mythbrowser_fi.qm mythbrowser_es.qm mythbrowser_pt_br.qm
trans.files += mythbrowser_et.qm mythbrowser_ru.qm

INSTALLS += trans

SOURCES += dummy.c
