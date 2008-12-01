include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files += mythphone_de.qm mythphone_sv.qm mythphone_fr.qm
trans.files += mythphone_ca.qm mythphone_da.qm mythphone_es.qm
trans.files += mythphone_it.qm mythphone_ja.qm mythphone_nl.qm
trans.files += mythphone_pt.qm mythphone_sl.qm mythphone_et.qm
trans.files += mythphone_nb.qm mythphone_cs.qm

INSTALLS += trans

SOURCES += dummy.c
