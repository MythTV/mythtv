include ( ../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files = mythweather_es.qm mythweather_ca.qm mythweather_nl.qm

INSTALLS += trans

SOURCES += dummy.c
