include ( ../settings.pro )

TEMPLATE = aux

html.path = $${PREFIX}/share/mythtv/html/
html.files = frontend_index.qsp backend_index.qsp overview.html menu.qsp robots.txt favicon.ico
# mythfrontend.html is just a copy of frontend_index.qsp
html.files += mythbackend.html mythfrontend.html
html.files += css images js misc setup samples tv
html.files += 3rdParty xslt video debug apps assets

# This solves a problem with refresh or bookmark of http://localhost:6744/settings/xxxxx
# where xxxxx can be any of several angular settings URLs
html.extra = mkdir -p $(INSTALL_ROOT)/usr/share/mythtv/html ; ln -sf $(INSTALL_ROOT)/usr/share/mythtv/html $(INSTALL_ROOT)/usr/share/mythtv/html/settings

INSTALLS += html
