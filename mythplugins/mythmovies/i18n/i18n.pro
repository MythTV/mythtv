include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files += mythmovies_sv.qm mythmovies_hu.qm

INSTALLS += trans

SOURCES += dummy.c
