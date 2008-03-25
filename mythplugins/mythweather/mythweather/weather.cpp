/*
        MythWeather
        Version 0.8
        January 8th, 2003

        By John Danner & Dustin Doris

        Note: Portions of this code taken from MythMusic

*/

#include <qapplication.h>
//Added by qt3to4:
#include <QPaintEvent>
#include <QPixmap>
#include <QKeyEvent>

#include <unistd.h>
#include <cstdlib>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

#include "weatherScreen.h"
#include "sourceManager.h"
#include "weatherSetup.h"
#include "weather.h"

Weather::Weather(MythMainWindow *parent, SourceManager *srcMan,
                 const char *name) : MythDialog(parent, name)
{
    allowkeys = true;
    paused = false;

    firstRun = true;
    m_srcMan = srcMan;

    newlocRect = QRect(0, 0, size().width(), size().height());
    fullRect = QRect(0, 0, size().width(), size().height());

    nextpageInterval = gContext->GetNumSetting("weatherTimeout", 10);
    nextpageIntArrow = gContext->GetNumSetting("weatherHoldTimeout", 20);

    m_startup = 0;
    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    
    if (!theme->LoadTheme(xmldata, "weather", "weather-"))
    {
        VERBOSE(VB_IMPORTANT, QString("Weather: Couldn't find the theme."));
    }

    screens.setAutoDelete(true);

    /*
     * TODO this can be up to 59 seconds slow, should be a better way to do
     * this.
     */
    /*
     * TODO #2 QObject has timers built in, probably slicker to use those.
     */
    showtime_Timer = new QTimer(this);
    connect(showtime_Timer, SIGNAL(timeout()), SLOT(showtime_timeout()) );
    showtime_Timer->start((int)(60 * 1000));

    nextpage_Timer = new QTimer(this);
    connect(nextpage_Timer, SIGNAL(timeout()), SLOT(nextpage_timeout()) );
    setNoErase();

    // Run once before loading the background container
    // to create a background should the window not
    // contain it's own background - Temporary workaround
    // until mythui port.
    updateBackground();
    setupScreens(xmldata);

    if (!gContext->GetNumSetting("weatherbackgroundfetch", 0))
        showLayout(m_startup);
    showtime_timeout();
}

Weather::~Weather()
{
    delete theme;
    delete m_startup;
    //for (WeatherScreen *screen = screens.first(); screen; screen = screens.next()) {
    //    m_srcMan->disconnectScreen(screen);
    //    delete screen;
    //}
}

void Weather::setupScreens(QDomElement &xml)
{
    // Deletes screen objects
    screens.clear();
    // it points to an element of screens, which was just deleted;
    currScreen = 0;
    if (m_startup)
    {
        delete m_startup;
        m_startup = 0;
    }

    QMap<QString, QDomElement> containers;
    for (QDomNode child = xml.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement e = child.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "font")
            {
                if (!theme->GetFont(e.attribute("name"), false))
                    theme->parseFont(e);
            }
            else if (e.tagName() == "container")
            {
                QRect area;
                QString name;
                int context;
                if (e.attribute("name") == "startup")
                {
                    if (!theme->GetSet("startup"))
                        theme->parseContainer(e, name, context, area);
                    else
                        name = e.attribute("name");
                    WeatherScreen *ws = WeatherScreen::loadScreen(this, theme->GetSet(name));
                    ws->setInUse(true);
                    m_startup = ws;
                }
                else if (e.attribute("name") == "background")
                {
                    theme->parseContainer(e, name, context, area);
                    updateBackground();
                }
                else
                    containers[e.attribute("name")] = e;
            }
            else
            {
                VERBOSE(VB_IMPORTANT, "Unknown element: " + e.tagName());
                exit(0);
            }
        }
    }

    MSqlQuery db(MSqlQuery::InitCon());
    QString query =
            "SELECT screen_id, container, units, draworder FROM weatherscreens "
            " WHERE hostname = :HOST ORDER BY draworder;";
    db.prepare(query);
    db.bindValue(":HOST", gContext->GetHostName());
    if (!db.exec())
    {
        VERBOSE(VB_IMPORTANT, db.lastError().text());
        return;
    }

    if (!db.size())
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), "no screens",
                tr("No Screens defined; Entering Screen Setup."));

        m_srcMan->clearSources();
        m_srcMan->findScripts();
        ScreenSetup ssetup(gContext->GetMainWindow(), m_srcMan);
        ssetup.exec();
        m_srcMan->clearSources();
        m_srcMan->findScriptsDB();
        m_srcMan->setupSources();
        m_srcMan->startTimers();
        m_srcMan->doUpdate();
    }

    // re-execute
    if (!db.exec())
    {
        VERBOSE(VB_IMPORTANT, db.lastError().text());
        return;
    }

    while (db.next())
    {
        int id = db.value(0).toInt();
        QString container = db.value(1).toString();
        units_t units = db.value(2).toUInt();
        uint draworder = db.value(3).toUInt();
        if (!containers.contains(container))
        {
            VERBOSE(VB_IMPORTANT,
                    container + " is in database, but not theme file");
            continue;
        }
        QDomElement e = containers[container];
        QRect area;
        QString name;
        int context;
        if (!theme->GetSet(e.attribute("name")))
            theme->parseContainer(e, name, context, area);
        else name = e.attribute("name");
        WeatherScreen *ws =
                WeatherScreen::loadScreen(this, theme->GetSet(name), id);
        ws->setUnits(units);
        ws->setInUse(true);
        screens.insert(draworder, ws);
        connect(this, SIGNAL(clock_tick()), ws, SLOT(clock_tick()));
        connect(ws, SIGNAL(screenReady(WeatherScreen *)), this,
                SLOT(screenReady(WeatherScreen *)));
        m_srcMan->connectScreen(id, ws);
    }
}

void Weather::screenReady(WeatherScreen *ws)
{
    WeatherScreen *nxt = nextScreen();

    if (firstRun && ws == nxt)
    {
        firstRun = false;
        setPaletteBackgroundPixmap(realBackground);
        showLayout(nxt);
        nextpage_Timer->start((int)(1000 * nextpageInterval));
    }
    disconnect(ws, SIGNAL(screenReady(WeatherScreen *)), this,
               SLOT(screenReady(WeatherScreen *)));
}

WeatherScreen *Weather::nextScreen()
{
    WeatherScreen *ws = screens.next();
    if (!ws)
        ws = screens.first();
    return ws;
}

WeatherScreen *Weather::prevScreen()
{
    WeatherScreen *ws = screens.prev();
    if (!ws)
        ws = screens.last();
    return ws;
}

void Weather::keyPressEvent(QKeyEvent *e)
{
    if (currScreen && currScreen->usingKeys() && currScreen->handleKey(e))
    {
        return;
    }

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Weather", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "LEFT")
            cursorLeft();
        else if (action == "RIGHT")
            cursorRight();
        //else if (action == "SELECT")
        //    resetLocale();
        else if (action == "PAUSE")
            holdPage();
        else if (action == "MENU")
            setupPage();
        else if (action == "UPDATE")
        {
            m_srcMan->doUpdate();
        }
        else
            handled = false;
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

void Weather::updateBackground()
{
    QPixmap bground(size());
    bground.fill(this, 0, 0);

    QPainter tmp(&bground);

    LayerSet *container = theme->GetSet("background");
    if (container)
    {
        container->Draw(&tmp, 0, 0);
    }

    tmp.end();
    realBackground = bground;
    setPaletteBackgroundPixmap(realBackground);
}

void Weather::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();
    QPainter p(this);

    if (r.intersects(fullRect))
        updatePage(&p);

    MythDialog::paintEvent(e);
}

void Weather::updatePage(QPainter *dr)
{
    QRect pr = fullRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    if (currScreen)
        currScreen->draw(&tmp);
    tmp.end();
    dr->drawPixmap(pr.topLeft(), pix);
}

void Weather::showLayout(WeatherScreen *ws)
{
    currScreen = ws;
    ws->showing();
    update();
}

void Weather::processEvents()
{
        qApp->processEvents();
}

void Weather::showtime_timeout()
{
    emit clock_tick();
    update();
}

void Weather::holdPage()
{
    if (!nextpage_Timer->isActive())
        nextpage_Timer->start(1000 * nextpageInterval);
    else nextpage_Timer->stop();
    paused = !paused;
    if (currScreen)
        currScreen->toggle_pause(paused);
    update(fullRect);
}

void Weather::setupPage()
{
    m_srcMan->stopTimers();
    nextpage_Timer->stop();
    m_srcMan->clearSources();
    m_srcMan->findScripts();
    ScreenSetup ssetup(gContext->GetMainWindow(), m_srcMan);
    ssetup.exec();
    firstRun = true;
    m_srcMan->clearSources();
    m_srcMan->findScriptsDB();
    m_srcMan->setupSources();
    setupScreens(xmldata);
    m_srcMan->startTimers();
    m_srcMan->doUpdate();
    // TODO
#if 0
    m_srcMan->stopTimers();
    nextpage_Timer->stop();
    m_srcMan->clearSources();
    m_srcMan->findScripts();
    WeatherSetup setup(gContext->GetMainWindow(), m_srcMan);
    setup.exec();
    firstRun = true;
    m_srcMan->setupSources();
    setupScreens(xmldata);
    nextpageInterval = gContext->GetNumSetting("weatherTimeout", 10);
    nextpageIntArrow = gContext->GetNumSetting("weatherHoldTimeout", 20);
    m_srcMan->startTimers();
    m_srcMan->doUpdate();
#endif
}

void Weather::cursorRight()
{
    WeatherScreen *ws = nextScreen();
    if (ws->canShowScreen())
    {
        if (currScreen)
            currScreen->hiding();
        currScreen = ws;
        currScreen->showing();
        currScreen->toggle_pause(paused);
        update();
        if (!paused)
            nextpage_Timer->start((int)(1000 * nextpageIntArrow));
    }
}

void Weather::cursorLeft()
{
    WeatherScreen *ws = prevScreen();
    if (ws->canShowScreen())
    {
        if (currScreen)
            currScreen->hiding();
        currScreen = ws;
        currScreen->showing();
        currScreen->toggle_pause(paused);
        update();
        if (!paused)
            nextpage_Timer->start((int)(1000 * nextpageIntArrow));
    }
}

void Weather::nextpage_timeout()
{
    WeatherScreen *nxt = nextScreen();

    if (nxt->canShowScreen())
    {
        if (currScreen)
            currScreen->hiding();
       showLayout(nxt);
    }
    else
        VERBOSE(VB_GENERAL, "Next screen not ready");

    nextpage_Timer->changeInterval((int)(1000 * nextpageInterval));
}
