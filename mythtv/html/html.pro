include ( ../settings.pro )

TEMPLATE = aux

html.path = $${PREFIX}/share/mythtv/html/
html.files = robots.txt favicon.ico
# mythfrontend.html is just a copy of frontend_index.qsp
html.files += mythbackend.html mythfrontend.html
html.files += images
html.files += 3rdParty xslt apps assets

INSTALLS += html
