from lib.utils import *

log('script version %s started' % ADDONVERSION, debug=True)

# kodi startup, service is disabled, exit
if sys.argv == [''] and not ADDON.getSettingBool('service'):
    log('service not enabled', debug=True)

# scraper test, run from addon settings
elif len(sys.argv) == 2 and sys.argv[1] == 'test':
    from lib.scrapertest import *
    test_scrapers()

# kodi startup, service is enabled, start main loop
elif not WIN.getProperty('culrc.running') == 'true':
    from lib import gui
    gui.MAIN()

# service is running, but gui was exited, user clicked lyrics button, reshow gui
elif not WIN.getProperty('culrc.guirunning') == 'TRUE':
    WIN.setProperty('culrc.force','TRUE')

# service is running, gui is viisible, user clicked the lyrics button, do nothing
else:
    log('script already running', debug=True)
    if not ADDON.getSettingBool('silent'):
        xbmcgui.Dialog().notification(ADDONNAME, LANGUAGE(32158), time=2000, sound=False)

log('script version %s ended' % ADDONVERSION, debug=True)
