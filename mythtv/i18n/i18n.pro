include ( ../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files = mythfrontend_it.qm mythfrontend_es.qm mythfrontend_ca.qm

menu.path = $${PREFIX}/share/mythtv/
menu.files = tvmenu_it.xml mainmenu_it.xml tv_schedule_it.xml

INSTALLS += trans menu

SOURCES += dummy.c
