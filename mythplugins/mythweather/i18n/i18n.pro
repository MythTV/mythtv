include ( ../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path   = $${PREFIX}/share/mythtv/i18n/
trans.files  = mythweather_es.qm mythweather_ca.qm mythweather_nl.qm
trans.files += mythweather_dk.qm mythweather_pt.qm

INSTALLS += trans

SOURCES += dummy.c
