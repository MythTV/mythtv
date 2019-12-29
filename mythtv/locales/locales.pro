include ( ../settings.pro )

TEMPLATE = aux

locales.path = $${PREFIX}/share/mythtv/locales/
locales.files = *.xml

INSTALLS += locales
