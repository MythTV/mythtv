include ( ../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files  = mythdvd_it.qm mythdvd_es.qm mythdvd_ca.qm
trans.files += mythdvd_nl.qm mythdvd_de.qm mythdvd_dk.qm
trans.files += mythdvd_pt.qm mythdvd_sv.qm

INSTALLS += trans

SOURCES += dummy.c
