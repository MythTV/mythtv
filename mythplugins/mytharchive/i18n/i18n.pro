include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files += mytharchive_de.qm mytharchive_sl.qm mytharchive_fr.qm
trans.files += mytharchive_sv.qm mytharchive_nl.qm mytharchive_nb.qm
trans.files += mytharchive_fi.qm mytharchive_es.qm mytharchive_et.qm
trans.files += mytharchive_da.qm mytharchive_cs.qm mytharchive_en_gb.qm
trans.files += mytharchive_sl.qm mytharchive_hu.qm mytharchive_en_us.qm
trans.files += mytharchive_ru.qm mytharchive_el.qm mytharchive_pt.qm
trans.files += mytharchive_pl.qm

INSTALLS += trans

SOURCES += dummy.c
