include ( ../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files += mythphone_de.qm mythphone_sv.qm mythphone_fr.qm

INSTALLS += trans

SOURCES += dummy.c
