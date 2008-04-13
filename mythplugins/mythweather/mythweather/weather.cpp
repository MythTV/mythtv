
// C headers
#include <unistd.h>
#include <cstdlib>

// QT headers
#include <qapplication.h>

// MythTV headers
#include <mythtv/mythdbcon.h>
#include <mythtv/mythcontext.h>

// MythWeather headers
#include "weatherScreen.h"
#include "sourceManager.h"
#include "weatherSetup.h"
#include "weather.h"

Weather::Weather(MythScreenStack *parent, const char *name, SourceManager *srcMan)
    : MythScreenType(parent, name)
{
    m_mainStack = parent;
    m_weatherStack = new MythScreenStack(GetMythMainWindow(), "weather stack");

    m_paused = false;

    m_firstRun = true;
    m_srcMan = srcMan;

    m_pauseText = m_headerText = NULL;

    m_nextpageInterval = gContext->GetNumSetting("weatherTimeout", 10);
    m_nextpageIntArrow = gContext->GetNumSetting("weatherHoldTimeout", 20);

    m_nextpage_Timer = new QTimer(this);
    connect(m_nextpage_Timer, SIGNAL(timeout()), SLOT(nextpage_timeout()) );
    m_allScreens = loadScreens();
}

Weather::~Weather()
{
    if (!gContext->GetNumSetting("weatherbackgroundfetch", 0))
    {
        delete m_srcMan;
    }

    for (WeatherScreen *screen = m_screens.first(); screen;
         screen = m_screens.next())
    {
        if (screen)
            delete screen;
    }

    m_screens.clear();

    if (m_weatherStack)
        GetMythMainWindow()->PopScreenStack();
}

bool Weather::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("weather-ui.xml", "weatherbase", this);

    if (!foundtheme)
    {
        VERBOSE(VB_IMPORTANT, "Missing required window - weatherbase.");
        return false;
    }

    m_pauseText = dynamic_cast<MythUIText *> (GetChild("pause_text"));
    m_headerText = dynamic_cast<MythUIText *> (GetChild("header"));

    if (!m_pauseText || !m_headerText)
    {
        VERBOSE(VB_IMPORTANT, "Window weatherbase is missing required elements.");
        return false;
    }

    if (m_pauseText)
    {
        m_pauseText->SetText(tr("Paused"));
        m_pauseText->Hide();
    }

    setupScreens();

    return true;
}

void Weather::setupScreens()
{
    // Deletes screen objects
    m_screens.clear();
    // it points to an element of screens, which was just deleted;
    m_currScreen = NULL;

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
        m_srcMan->clearSources();
        m_srcMan->findScripts();

        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

        ScreenSetup *ssetup = new ScreenSetup(mainStack, "weatherscreensetup",
                                              m_srcMan);

        if (ssetup->Create())
            mainStack->AddScreen(ssetup);

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

        ScreenListInfo *screenListInfo = m_allScreens[container];

        WeatherScreen *ws = WeatherScreen::loadScreen(m_weatherStack, screenListInfo, id);
        if (!ws->Create())
            continue;

        ws->setUnits(units);
        ws->setInUse(true);
        m_screens.insert(draworder, ws);
        connect(ws, SIGNAL(screenReady(WeatherScreen *)), this,
                SLOT(screenReady(WeatherScreen *)));
        m_srcMan->connectScreen(id, ws);
    }
}

void Weather::screenReady(WeatherScreen *ws)
{
    WeatherScreen *nxt = nextScreen();

    if (m_firstRun && ws == nxt)
    {
        m_firstRun = false;
        showScreen(nxt);
        m_nextpage_Timer->start((int)(1000 * m_nextpageInterval));
    }
    disconnect(ws, SIGNAL(screenReady(WeatherScreen *)), this,
               SLOT(screenReady(WeatherScreen *)));
}

WeatherScreen *Weather::nextScreen()
{
    WeatherScreen *ws = m_screens.next();
    if (!ws)
        ws = m_screens.first();
    return ws;
}

WeatherScreen *Weather::prevScreen()
{
    WeatherScreen *ws = m_screens.prev();
    if (!ws)
        ws = m_screens.last();
    return ws;
}

bool Weather::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Weather", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "LEFT")
            cursorLeft();
        else if (action == "RIGHT")
            cursorRight();
        else if (action == "PAUSE")
            holdPage();
        else if (action == "MENU")
            setupPage();
        else if (action == "UPDATE")
        {
            m_srcMan->doUpdate();
        }
        else if (action == "ESCAPE")
        {
            m_nextpage_Timer->stop();
            hideScreen();
            m_mainStack->PopScreen();
        }
        else
            handled = false;
    }

    if (MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void Weather::showScreen(WeatherScreen *ws)
{
    if (!ws)
        return;

    m_currScreen = ws;
    m_weatherStack->AddScreen(m_currScreen, false);
    m_headerText->SetText(m_currScreen->name());
}

void Weather::hideScreen()
{
    if (!m_currScreen)
        return;

    m_weatherStack->PopScreen(false,false);
}

void Weather::holdPage()
{
    if (!m_nextpage_Timer->isActive())
        m_nextpage_Timer->start(1000 * m_nextpageInterval);
    else
        m_nextpage_Timer->stop();

    m_paused = !m_paused;

    if (m_pauseText)
    {
        if (m_paused)
            m_pauseText->Show();
        else
            m_pauseText->Hide();
    }
}

void Weather::setupPage()
{
    m_srcMan->stopTimers();
    m_nextpage_Timer->stop();
    m_srcMan->clearSources();
    m_srcMan->findScripts();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    ScreenSetup *ssetup = new ScreenSetup(mainStack, "weatherscreensetup",
                                            m_srcMan);

    if (ssetup->Create())
        mainStack->AddScreen(ssetup);

    m_firstRun = true;
    m_srcMan->clearSources();
    m_srcMan->findScriptsDB();
    m_srcMan->setupSources();
    setupScreens();
    m_srcMan->startTimers();
    m_srcMan->doUpdate();
}

void Weather::cursorRight()
{
    WeatherScreen *ws = nextScreen();
    if (ws->canShowScreen())
    {
        hideScreen();
        showScreen(ws);
        holdPage();
    }
}

void Weather::cursorLeft()
{
    WeatherScreen *ws = prevScreen();
    if (ws->canShowScreen())
    {
        hideScreen();
        showScreen(ws);
        holdPage();
    }
}

void Weather::nextpage_timeout()
{
    WeatherScreen *nxt = nextScreen();

    if (nxt->canShowScreen())
    {
        hideScreen();
        showScreen(nxt);
    }
    else
        VERBOSE(VB_GENERAL, "Next screen not ready");

    m_nextpage_Timer->changeInterval((int)(1000 * m_nextpageInterval));
}
