include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files += mythmovies_sv.qm mythmovies_da.qm mythmovies_en_gb.qm
trans.files += mythmovies_fr.qm mythmovies_hu.qm mythmovies_en_us.qm

INSTALLS += trans

SOURCES += dummy.c
