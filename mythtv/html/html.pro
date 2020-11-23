include ( ../settings.pro )

TEMPLATE = aux

html.path = $${PREFIX}/share/mythtv/html/
html.files = frontend_index.qsp backend_index.qsp overview.html menu.qsp robots.txt favicon.ico
html.files += css images js misc setup samples tv
html.files += 3rdParty xslt video debug apps

INSTALLS += html
