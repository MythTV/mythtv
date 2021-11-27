include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = aux

defaultfiles.path = $${PREFIX}/share/mythtv/themes/default
defaultfiles.files = default/*.xml default/images/*.png

widefiles.path = $${PREFIX}/share/mythtv/themes/default-wide
widefiles.files = default-wide/*.xml default-wide/images/*.png

menufiles.path = $${PREFIX}/share/mythtv/
menufiles.files = menus/*.xml

htmlfiles.path = $${PREFIX}/share/mythtv/themes/default/htmls
htmlfiles.files = htmls/*.html

INSTALLS += defaultfiles widefiles menufiles htmlfiles
