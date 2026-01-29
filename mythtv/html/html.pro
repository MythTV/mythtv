include ( ../settings.pro )

TEMPLATE = aux

html.path = $${PREFIX}/share/mythtv/html/
html.files = robots.txt favicon.ico
html.files += mythbackend.html mythfrontend.html
html.files += css
html.files += images
html.files += 3rdParty xslt apps assets

INSTALLS += html
