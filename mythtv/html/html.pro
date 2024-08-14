include ( ../settings.pro )

TEMPLATE = aux

backend.path = $${PREFIX}/share/mythtv/html/
backend.target = apps/backend/main.js
backend.depends += backend/src/app/* backend/src/app/*/* backend/src/app/*/*/* backend/src/app/*/*/*/* backend/src/app/*/*/*/*/* backend/src/app/*/*/*/*/*/*
backend.commands = cd backend && npm install && npm run build
PRE_TARGETDEPS += apps/backend/main.js
QMAKE_EXTRA_TARGETS += backend

frontend.path = $${PREFIX}/share/mythtv/html/
frontend.target = apps/frontend.js
frontend.depends += frontend/src/* frontend/src/*/*
frontend.commands = cd frontend && npm install && npm run build && rm -f ../apps/*.map
PRE_TARGETDEPS += apps/frontend.js
QMAKE_EXTRA_TARGETS += frontend

html.path = $${PREFIX}/share/mythtv/html/
html.files = frontend_index.qsp backend_index.qsp overview.html menu.qsp robots.txt favicon.ico
# mythfrontend.html is just a copy of frontend_index.qsp
html.files += mythbackend.html mythfrontend.html
html.files += css images js misc setup samples tv
html.files += 3rdParty xslt video debug apps assets

INSTALLS += html
