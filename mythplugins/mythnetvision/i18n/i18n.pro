include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files += mythnetvision_da.qm mythnetvision_fr.qm mythnetvision_en_gb.qm
trans.files += mythnetvision_sv.qm mythnetvision_et.qm mythnetvision_en_us.qm
trans.files += mythnetvision_el.qm mythnetvision_ru.qm mythnetvision_nl.qm
trans.files += mythnetvision_pt.qm mythnetvision_nb.qm mythnetvision_de.qm
trans.files += mythnetvision_cs.qm mythnetvision_es.qm mythnetvision_fi.qm
trans.files += mythnetvision_ja.qm mythnetvision_pl.qm mythnetvision_en_ca.qm

INSTALLS += trans

SOURCES += dummy.c
