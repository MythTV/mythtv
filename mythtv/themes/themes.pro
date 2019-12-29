include ( ../settings.pro )

TEMPLATE = aux

themes.path = $${PREFIX}/share/mythtv/themes/
themes.files = default default-wide classic DVR Slave
themes.files += Terra defaultmenu mediacentermenu
themes.files += MythCenter MythCenter-wide
themes.files += mythuitheme.dtd

fonts.path = $${PREFIX}/share/mythtv/
fonts.files = fonts

INSTALLS += themes fonts
