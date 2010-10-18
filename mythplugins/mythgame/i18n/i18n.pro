include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files  = mythgame_it.qm mythgame_es.qm mythgame_ca.qm
trans.files += mythgame_nl.qm mythgame_de.qm mythgame_da.qm
trans.files += mythgame_pt.qm mythgame_sv.qm mythgame_sl.qm
trans.files += mythgame_fr.qm mythgame_fi.qm mythgame_et.qm
trans.files += mythgame_nb.qm mythgame_hu.qm mythgame_en_gb.qm
trans.files += mythgame_ru.qm mythgame_el.qm mythgame_en_us.qm
trans.files += mythgame_cs.qm mythgame_pl.qm mythgame_en_ca.qm

INSTALLS += trans

SOURCES += dummy.c

