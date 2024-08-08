
// C headers
#include <cstdlib>
#include <unistd.h>

// QT headers
#include <QApplication>

// MythTV headers
#include <libmyth/mythcontext.h>
#include <libmythbase/mythdate.h>
#include <libmythbase/mythdb.h>
#include <libmythui/mythuitext.h>

// MythWeather headers
#include "sourceManager.h"
#include "weather.h"
#include "weatherScreen.h"
#include "weatherSetup.h"

Weather::Weather(MythScreenStack *parent, const QString &name, SourceManager *srcMan)
    : MythScreenType(parent, name),
      m_weatherStack(new MythScreenStack(GetMythMainWindow(), "weather stack")),
      m_nextpageInterval(gCoreContext->GetDurSetting<std::chrono::seconds>("weatherTimeout", 10s)),
      m_nextPageTimer(new QTimer(this))
{
    if (!srcMan)
    {
        m_srcMan = new SourceManager();
        // No point in doing this as the very first thing we are going to do
        // is destroy the sources and reload them.
#if 0
        m_srcMan->startTimers();
        m_srcMan->doUpdate();
#endif
        m_createdSrcMan = true;
    }
    else
    {
        m_srcMan = srcMan;
        m_createdSrcMan = false;
    }

    connect(m_nextPageTimer, &QTimer::timeout, this, &Weather::nextpage_timeout );

    m_allScreens = loadScreens();
}

Weather::~Weather()
{
    if (m_createdSrcMan)
        delete m_srcMan;

    clearScreens();

    if (m_weatherStack)
        GetMythMainWindow()->PopScreenStack();
}

bool Weather::Create()
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("weather-ui.xml", "weatherbase", this);
    if (!foundtheme)
    {
        LOG(VB_GENERAL, LOG_ERR, "Missing required window - weatherbase.");
        return false;
    }

    bool err = false;

    UIUtilE::Assign(this, m_pauseText, "pause_text", &err);
    UIUtilE::Assign(this, m_headerText, "header", &err);
    UIUtilE::Assign(this, m_updatedText, "update_text", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Window weatherbase is missing required elements.");
        return false;
    }

    if (m_pauseText)
    {
        m_pauseText->SetText(tr("Paused"));
        m_pauseText->Hide();
    }

    return true;
}

void Weather::clearScreens()
{
    m_currScreen = nullptr;

    m_curScreenNum = 0;
    while (!m_screens.empty())
    {
        WeatherScreen *screen = m_screens.back();
        m_weatherStack->PopScreen(screen, false, false);
        m_screens.pop_back();
        delete screen;
    }
}

void Weather::setupScreens()
{
    SetupScreens();
}

bool Weather::SetupScreens()
{
    // Delete any existing screens
    clearScreens();

    m_paused = false;
    m_pauseText->Hide();

    // Refresh sources
    m_srcMan->clearSources();
    m_srcMan->findScriptsDB();
    m_srcMan->setupSources();

    MSqlQuery db(MSqlQuery::InitCon());
    QString query =
            "SELECT screen_id, container, units, draworder FROM weatherscreens "
            " WHERE hostname = :HOST ORDER BY draworder;";
    db.prepare(query);
    db.bindValue(":HOST", gCoreContext->GetHostName());
    if (!db.exec())
    {
        MythDB::DBError("Selecting weather screens.", db);
        return false;
    }

    if (!db.size())
    {
        if (m_firstSetup)
        {
            m_firstSetup = false;
            // If no screens exist, run the setup
            MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

            auto *ssetup = new ScreenSetup(mainStack, "weatherscreensetup",
                                           m_srcMan);

            connect(ssetup, &MythScreenType::Exiting, this, &Weather::setupScreens);

            if (ssetup->Create())
            {
                mainStack->AddScreen(ssetup);
            }
            else
            {
                delete ssetup;
            }
        }
        else
        {
            Close();
        }
    }
    else
    {
        while (db.next())
        {
            int id = db.value(0).toInt();
            QString container = db.value(1).toString();
            units_t units = db.value(2).toUInt();
            uint draworder = db.value(3).toUInt();

            ScreenListInfo &screenListInfo = m_allScreens[container];

            WeatherScreen *ws = WeatherScreen::loadScreen(m_weatherStack, &screenListInfo, id);
            if (!ws->Create())
            {
                delete ws;
                continue;
            }

            ws->setUnits(units);
            ws->setInUse(true);
            m_screens.insert(draworder, ws);
            connect(ws, &WeatherScreen::screenReady, this,
                    &Weather::screenReady);
            m_srcMan->connectScreen(id, ws);
        }

        if( m_screens.empty() )
        {
            // We rejected every screen...  sit on this and rotate.
            LOG(VB_GENERAL, LOG_ERR, "No weather screens left, aborting.");
            m_nextPageTimer->stop();
            if( m_updatedText )
            {
                m_updatedText->SetText(tr("None of the configured screens are complete in this theme (missing copyright information)."));
                m_updatedText->Show();
                return true;
            }
            return false;
        }

        m_srcMan->startTimers();
        m_srcMan->doUpdate(true);
    }

    return true;
}

void Weather::screenReady(WeatherScreen *ws)
{
    if (m_firstRun && !m_screens.empty() && ws == m_screens[m_curScreenNum])
    {
        m_firstRun = false;
        showScreen(ws);
        m_nextPageTimer->start(m_nextpageInterval);
    }
    disconnect(ws, &WeatherScreen::screenReady, this, &Weather::screenReady);
}

WeatherScreen *Weather::nextScreen(void)
{
    if (m_screens.empty())
        return nullptr;

    m_curScreenNum = (m_curScreenNum + 1) % m_screens.size();
    return m_screens[m_curScreenNum];
}

WeatherScreen *Weather::prevScreen(void)
{
    if (m_screens.empty())
        return nullptr;

    m_curScreenNum = (m_curScreenNum < 0) ? 0 : m_curScreenNum;
    m_curScreenNum = (m_curScreenNum + m_screens.size() - 1) % m_screens.size();
    return m_screens[m_curScreenNum];
}

bool Weather::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Weather", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
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
            m_nextPageTimer->stop();
            hideScreen();
            Close();
        }
        else
        {
            handled = false;
        }
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void Weather::showScreen(WeatherScreen *ws)
{
    if (!ws)
        return;

    m_currScreen = ws;
    m_weatherStack->AddScreen(m_currScreen, false);
    m_headerText->SetText(m_currScreen->objectName());
    m_updatedText->SetText(m_currScreen->getValue("updatetime"));
}

void Weather::hideScreen()
{
    if (!m_currScreen)
        return;

    m_weatherStack->PopScreen(nullptr, false,false);
}

void Weather::holdPage()
{
    if (!m_nextPageTimer->isActive())
        m_nextPageTimer->start(m_nextpageInterval);
    else
        m_nextPageTimer->stop();

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
    m_nextPageTimer->stop();
    m_srcMan->clearSources();
    m_srcMan->findScripts();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *ssetup = new ScreenSetup(mainStack, "weatherscreensetup", m_srcMan);

    connect(ssetup, &MythScreenType::Exiting, this, &Weather::setupScreens);

    if (ssetup->Create())
    {
        clearScreens();
        mainStack->AddScreen(ssetup);
    }
    else
    {
        delete ssetup;
    }

    m_firstRun = true;
}

void Weather::cursorRight()
{
    WeatherScreen *ws = nextScreen();
    if (ws && ws->canShowScreen())
    {
        hideScreen();
        showScreen(ws);
        if (!m_paused)
            m_nextPageTimer->start(m_nextpageInterval);
    }
}

void Weather::cursorLeft()
{
    WeatherScreen *ws = prevScreen();
    if (ws && ws->canShowScreen())
    {
        hideScreen();
        showScreen(ws);
        if (!m_paused)
            m_nextPageTimer->start(m_nextpageInterval);
    }
}

void Weather::nextpage_timeout()
{
    WeatherScreen *nxt = nextScreen();

    if (nxt && nxt->canShowScreen())
    {
        hideScreen();
        showScreen(nxt);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Next screen not ready");
    }

    m_nextPageTimer->start(m_nextpageInterval);
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
