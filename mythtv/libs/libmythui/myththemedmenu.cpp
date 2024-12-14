
#include "myththemedmenu.h"

// QT headers
#include <QCoreApplication>
#include <QDir>
#include <QKeyEvent>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

// libmythbase headers
#include "libmythbase/lcddevice.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythmedia.h"
#include "libmythbase/mythplugin.h"
#include "libmythbase/mythsystemlegacy.h"
#include "libmythbase/mythversion.h"

// libmythui headers
#include "mythmainwindow.h"
#include "mythdialogbox.h"
#include "mythgesture.h"
#include "mythuitext.h"
#include "mythuistatetype.h"
#include "xmlparsebase.h"
#include "mythuihelper.h"

bool MythThemedMenuState::Create(void)
{
    if (!LoadWindowFromXML("menu-ui.xml", "mainmenu", this))
        return false;

    m_titleState     = dynamic_cast<MythUIStateType *> (GetChild("titles"));
    m_watermarkState = dynamic_cast<MythUIStateType *> (GetChild("watermarks"));
    m_buttonList     = dynamic_cast<MythUIButtonList *> (GetChild("menu"));
    m_descriptionText = dynamic_cast<MythUIText *> (GetChild("description"));

    if (!m_buttonList)
    {
        LOG(VB_GENERAL, LOG_ERR, "Missing 'menu' buttonlist.");
        return false;
    }

    m_loaded = true;

    return true;
}

void MythThemedMenuState::CopyFrom(MythUIType *base)
{
    auto *st = dynamic_cast<MythThemedMenuState *>(base);
    if (!st)
    {
        LOG(VB_GENERAL, LOG_INFO, "ERROR, bad parsing");
        return;
    }

    m_loaded = st->m_loaded;

    MythScreenType::CopyFrom(base);

    m_titleState     = dynamic_cast<MythUIStateType *> (GetChild("titles"));
    m_watermarkState = dynamic_cast<MythUIStateType *> (GetChild("watermarks"));
    m_buttonList     = dynamic_cast<MythUIButtonList *> (GetChild("menu"));
    m_descriptionText = dynamic_cast<MythUIText *> (GetChild("description"));
}

////////////////////////////////////////////////////////////////////////////

/** \brief Creates a themed menu.
 *
 *  \note
 *  The two ignore variables were used before commit c34f2a10 in
 *  2008. Removing them would have a ripple effect into the callers.
 *
 *  \param cdir         The directory containing the theme. (Ignored)
 *  \param menufile     file name of menu definition file
 *  \param parent       the screen stack that owns this UI type
 *  \param name         the name of this UI type
 *  \param allowreorder Allow reordering of the menu items. (Ignored)
 *  \param state        theme state associated with this menu
 */
MythThemedMenu::MythThemedMenu(const QString &/*cdir*/, const QString &menufile,
                               MythScreenStack *parent, const QString &name,
                               bool /*allowreorder*/, MythThemedMenuState *state)
    : MythThemedMenuState(parent, name), m_state(state)
{
    if (!m_state)
    {
        m_state = new MythThemedMenuState(parent, "themedmenustate");
        m_allocedstate = true;
    }

    SetMenuTheme(menufile);
}

/** \brief Loads the main UI theme, and a menu theme.
 *
 *  See also foundtheme(void), it will return true when called after
 *  this method if this method was successful.
 *
 *  \param menufile name of menu item xml file
 */
void MythThemedMenu::SetMenuTheme(const QString &menufile)
{
    if (!m_state->m_loaded)
    {
        if (m_state->Create())
            m_foundtheme = true;
    }
    else
    {
        m_foundtheme = true;
    }

    if (!m_foundtheme)
        return;

    CopyFrom(m_state);

    connect(m_buttonList, &MythUIButtonList::itemSelected,
            this, &MythThemedMenu::setButtonActive);
    connect(m_buttonList, &MythUIButtonList::itemClicked,
            this, qOverload<MythUIButtonListItem*>(&MythThemedMenu::buttonAction));

    if (!parseMenu(menufile))
        m_foundtheme = false;
}

MythThemedMenu::~MythThemedMenu(void)
{
    if (m_allocedstate)
        delete m_state;
}

/// \brief Returns true iff a theme has been
///        found by a previous call to SetMenuTheme().
bool MythThemedMenu::foundTheme(void) const
{
    return m_foundtheme;
}

/// \brief Get the themed menus callback function and data for that function
void MythThemedMenu::getCallback(void (**lcallback)(void *, QString &),
                                 void **data)
{
    *lcallback = m_state->m_callback;
    *data = m_state->m_callbackdata;
}

/// \brief Set the themed menus callback function and data for that function
void MythThemedMenu::setCallback(void (*lcallback)(void *, QString &),
                                 void *data)
{
    m_state->m_callback = lcallback;
    m_state->m_callbackdata = data;
}

void MythThemedMenu::setKillable(void)
{
    m_state->m_killable = true;
}

QString MythThemedMenu::getSelection(void)
{
    return m_selection;
}

void MythThemedMenu::setButtonActive(MythUIButtonListItem* item)
{
    auto button = item->GetData().value<ThemedButton>();
    if (m_watermarkState)
    {
        if (!(m_watermarkState->DisplayState(button.type)))
            m_watermarkState->Reset();
    }

    if (m_descriptionText)
        m_descriptionText->SetText(button.description);
}

/** \brief keyboard/LIRC event handler.
 *
 *  This translates key presses through the "Main Menu" context into MythTV
 *  actions and then handles them as appropriate.
 */
bool MythThemedMenu::keyPressEvent(QKeyEvent *event)
{
    if (m_ignorekeys)
        return false;

    m_ignorekeys = true;

    MythUIType *type = GetFocusWidget();
    if (type && type->keyPressEvent(event))
    {
        m_ignorekeys = false;
        return true;
    }

    QStringList actions;
    bool handled = false;

    handled = GetMythMainWindow()->TranslateKeyPress("Main Menu", event,
                                                     actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "ESCAPE" ||
            action == "EXIT" ||
            action == "EXITPROMPT" ||
            action == "STANDBYMODE")
        {
            bool    callbacks  = m_state->m_callback;
            bool    lastScreen = (GetMythMainWindow()->GetMainStack()
                                                     ->TotalScreens() == 1);
            QString menuaction = "UPMENU";
            QString selExit    = "EXITING_APP_PROMPT";

            if (action == "EXIT")
                selExit = "EXITING_APP";
            else if (action == "STANDBYMODE")
                selExit = "STANDBY_MODE";

            if (!m_allocedstate)
                handleAction(menuaction);
            else if (m_state->m_killable)
            {
                m_wantpop = true;
                if (callbacks)
                {
                    QString sel = "EXITING_MENU";
                    m_state->m_callback(m_state->m_callbackdata, sel);
                }

                if (lastScreen)
                {
                    if (callbacks)
                        m_state->m_callback(m_state->m_callbackdata, selExit);
                    QCoreApplication::exit();
                }
            }
            else if ((action == "EXIT" || action == "EXITPROMPT" ||
                      action == "STANDBYMODE" ||
                      (action == "ESCAPE" &&
                       (QCoreApplication::applicationName() ==
                        MYTH_APPNAME_MYTHTV_SETUP))) && lastScreen)
            {
                if (callbacks)
                    m_state->m_callback(m_state->m_callbackdata, selExit);
                else
                {
                    QCoreApplication::exit();
                    m_wantpop = true;
                }
            }
        }
        else if (action == "HELP")
        {
            aboutScreen();
        }
        else if (action == "EJECT")
        {
            handleAction(action);
        }
        else
        {
            handled = false;
        }
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    m_ignorekeys = false;

    if (m_wantpop)
        m_screenStack->PopScreen();

    return handled;
}

void MythThemedMenu::aboutToShow()
{
    MythScreenType::aboutToShow();
    m_buttonList->updateLCD();
}

void MythThemedMenu::ShowMenu()
{
    if (m_menuPopup)
        return;

    int override_menu = GetMythDB()->GetNumSetting("OverrideExitMenu");
    QString label = tr("System Menu");
    MythScreenStack* mainStack = GetMythMainWindow()->GetMainStack();
    m_menuPopup = new MythDialogBox(label, mainStack, "menuPopup");

    if (m_menuPopup->Create())
        mainStack->AddScreen(m_menuPopup);

    m_menuPopup->SetReturnEvent(this, "popmenu");

    // HACK Implement a better check for this
    if (QCoreApplication::applicationName() == MYTH_APPNAME_MYTHFRONTEND)
        m_menuPopup->AddButtonV(tr("Enter standby mode"), QVariant("standby"));

    // don't show the exit application option if standby option is enabled
    if (override_menu != 7)
        m_menuPopup->AddButtonV(tr("Exit application"), QVariant("exit"));

    switch (override_menu)
    {
        case 2:
        case 4:
            // shutdown
            m_menuPopup->AddButtonV(tr("Shutdown"), QVariant("shutdown"));
            break;
        case 5:
            // reboot
            m_menuPopup->AddButtonV(tr("Reboot"), QVariant("reboot"));
            break;
        case 3:
        case 6:
            // both
            m_menuPopup->AddButtonV(tr("Shutdown"), QVariant("shutdown"));
            m_menuPopup->AddButtonV(tr("Reboot"), QVariant("reboot"));
            break;
        case 0:
        case 7:
        default:
            break;
    }

    m_menuPopup->AddButtonV(tr("About"), QVariant("about"));
}

void MythThemedMenu::aboutScreen()
{
    QString distro_line;

    QFile file("/etc/os_myth_release");
    if (file.open(QFile::ReadOnly))
    {
        QTextStream t( &file );        // use a text stream
        distro_line = t.readLine();
        file.close();
    }

    QString label = tr("Revision: %1\n Branch: %2\n %3")
                        .arg(GetMythSourceVersion(),
                             GetMythSourcePath(),
                             distro_line);

    MythScreenStack* mainStack = GetMythMainWindow()->GetMainStack();
    m_menuPopup = new MythDialogBox(label, mainStack, "version_dialog");
    if (m_menuPopup->Create())
        mainStack->AddScreen(m_menuPopup);

    m_menuPopup->SetReturnEvent(this, "version");
    m_menuPopup->AddButton(tr("Ok"));

}

void MythThemedMenu::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = (DialogCompletionEvent*)(event);

        QString resultid = dce->GetId();
        //int buttonnum = dce->GetResult();
        QString halt_cmd = GetMythDB()->GetSetting("HaltCommand");
        QString reboot_cmd = GetMythDB()->GetSetting("RebootCommand");

        if (resultid == "popmenu")
        {
            QString action = dce->GetData().toString();
            if (action == "shutdown")
            {
                if (!halt_cmd.isEmpty())
                    myth_system(halt_cmd);
            }
            else if (action == "reboot")
            {
                if (!reboot_cmd.isEmpty())
                    myth_system(reboot_cmd);
            }
            else if (action == "about")
            {
                aboutScreen();
            }
            else if (action == "standby")
            {
                QString arg("standby_mode");
                m_state->m_callback(m_state->m_callbackdata, arg);
            }
            else if (action == "exit")
            {
                QString arg("exiting_app");
                m_state->m_callback(m_state->m_callbackdata, arg);
            }
        }
        else if (resultid == "password")
        {
            QString text = dce->GetResultText();
            MythUIButtonListItem *item = m_buttonList->GetItemCurrent();
            auto button = item->GetData().value<ThemedButton>();
            QString password = GetMythDB()->GetSetting(button.password);
            if (text == password)
            {
                QString timestamp_setting = QString("%1Time").arg(button.password);
                QDateTime curr_time = MythDate::current();
                QString last_time_stamp =
                    MythDate::toString(curr_time, MythDate::kDatabase);
                GetMythDB()->SaveSetting(timestamp_setting, last_time_stamp);
                buttonAction(item, true);
            }
        }

        m_menuPopup = nullptr;
    }
}

/** \brief Parses the element's tags and set the ThemeButton's type,
 *         text, depends, and action, then adds the button.
 *
 *  \param element DOM element describing features of the themeButton
 */
void MythThemedMenu::parseThemeButton(QDomElement &element)
{
    QString type;
    QString text;
    QStringList action;
    QString alttext;
    QString description;
    QString password;

    bool addit = true;

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "type")
            {
                type = getFirstText(info);
            }
            else if (info.tagName() == "text")
            {
                if (text.isEmpty() &&
                    info.attribute("lang","").isEmpty())
                {
                    text = QCoreApplication::translate("ThemeUI",
                                           parseText(info).toUtf8() );
                }
                else if ((info.attribute("lang","").toLower() ==
                          gCoreContext->GetLanguageAndVariant()) ||
                         (info.attribute("lang","").toLower() ==
                          gCoreContext->GetLanguage()))
                {
                    text = parseText(info);
                }
            }
            else if (info.tagName() == "alttext")
            {
                if (alttext.isEmpty() &&
                    info.attribute("lang","").isEmpty())
                {
                    alttext = QCoreApplication::translate("ThemeUI",
                                              parseText(info).toUtf8());
                }
                else if ((info.attribute("lang","").toLower() ==
                          gCoreContext->GetLanguageAndVariant()) ||
                         (info.attribute("lang","").toLower() ==
                          gCoreContext->GetLanguage()))
                {
                    alttext = parseText(info);
                }
            }
            else if (info.tagName() == "action")
            {
                action += getFirstText(info);
            }
            else if (info.tagName() == "depends")
            {
                addit = findDepends(getFirstText(info));
            }
            else if (info.tagName() == "dependsexec")
            {
                addit = findDependsExec(getFirstText(info));
            }
            else if (info.tagName() == "dependssetting")
            {
                addit = GetMythDB()->GetBoolSetting(getFirstText(info));
            }
            else if (info.tagName() == "dependjumppoint")
            {
                addit = GetMythMainWindow()->DestinationExists(
                            getFirstText(info));
            }
            else if (info.tagName() == "dependswindow")
            {
                QString xmlFile = info.attribute("xmlfile", "");
                QString windowName = getFirstText(info);
                if (xmlFile.isEmpty() || windowName.isEmpty())
                    addit = false;
                else
                    addit = XMLParseBase::WindowExists(xmlFile, windowName);
            }
            else if (info.tagName() == "description")
            {
                if (description.isEmpty() &&
                    info.attribute("lang","").isEmpty())
                {
                    description = QCoreApplication::translate("ThemeUI",
                                                  getFirstText(info).toUtf8());
                }
                else if ((info.attribute("lang","").toLower() ==
                          gCoreContext->GetLanguageAndVariant()) ||
                         (info.attribute("lang","").toLower() ==
                          gCoreContext->GetLanguage()))
                {
                    description = getFirstText(info);
                }
            }
            else if (info.tagName() == "password")
            {
                password = getFirstText(info);
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("MythThemedMenu: Unknown tag %1 in button")
                        .arg(info.tagName()));
            }
        }
    }

    if (text.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "MythThemedMenu: Missing 'text' in button");
        return;
    }

    if (action.empty())
    {
        LOG(VB_GENERAL, LOG_ERR, "MythThemedMenu: Missing 'action' in button");
        return;
    }

    if (addit)
        addButton(type, text, alttext, action, description, password);
}

/** \brief Parse the themebuttons to be added based on the name of
 *         the menu file provided.
 *
 *  If the menu to be parsed is the main menu and this fails to find the
 *  XML file this will simply return false. Otherwise if it fails to
 *  find the menu it will pop up an error dialog and then return false.
 *
 *  The idea behind this is that if we can't parse the main menu we
 *  have to exit from the frontend entirely. But in all other cases
 *  we can simply return to the main menu and hope that it is a
 *  non-essential portion of MythTV which the theme does not support.
 *
 */
bool MythThemedMenu::parseMenu(const QString &menuname)
{
    QString filename = findMenuFile(menuname);

    QDomDocument doc;
    QFile f(filename);

    if (!f.exists() || !f.open(QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("MythThemedMenu: Couldn't read "
                                      "menu file %1").arg(menuname));

        if (menuname != "mainmenu.xml")
            ShowOkPopup(tr("MythTV could not locate the menu file %1")
                        .arg(menuname));
        return false;
    }

#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    if (!doc.setContent(&f, false, &errorMsg, &errorLine, &errorColumn))
    {
        LOG(VB_GENERAL, LOG_ERR,
                QString("Error parsing: %1\nat line: %2  column: %3 msg: %4").
                arg(filename).arg(errorLine).arg(errorColumn).arg(errorMsg));
        f.close();

        if (menuname != "mainmenu.xml")
            ShowOkPopup(tr("The menu file %1 is incomplete.")
                        .arg(menuname));
        return false;
    }
#else
    auto parseResult = doc.setContent(&f);
    if (!parseResult)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Error parsing: %1\nat line: %2  column: %3 msg: %4")
            .arg(filename).arg(parseResult.errorLine)
            .arg(parseResult.errorColumn).arg(parseResult.errorMessage));
        f.close();

        if (menuname != "mainmenu.xml")
            ShowOkPopup(tr("The menu file %1 is incomplete.")
                        .arg(menuname));
        return false;
    }
#endif

    f.close();

    LOG(VB_GUI, LOG_INFO, QString("Loading menu theme from %1").arg(filename));

    QDomElement docElem = doc.documentElement();

    m_menumode = docElem.attribute("name", "MAIN");

    QDomNode n = docElem.firstChild();
    while (!n.isNull())
    {
        QDomElement e = n.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "button")
            {
                parseThemeButton(e);
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("MythThemedMenu: Unknown element %1")
                        .arg(e.tagName()));
                return false;
            }
        }
        n = n.nextSibling();
    }

    if (m_buttonList->GetCount() == 0)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("MythThemedMenu: No buttons for menu %1").arg(menuname));
        return false;
    }

    m_buttonList->SetLCDTitles("MYTH-" + m_menumode);

    if (m_titleState)
    {
        m_titleState->EnsureStateLoaded(m_menumode);
        m_titleState->DisplayState(m_menumode);
    }

    m_selection.clear();
    return true;
}

/** \brief Create a new MythThemedButton based on the MythThemedMenuState
 *         m_state and the type, text, alt-text and action provided in
 *         the parameters.
 *
 *  Buttons (or the action of a button) can be locked and requie a
 *  password to be used.  This is indicated by setting the password
 *  parameter.
 *
 *  \param type        type of button to be created
 *  \param text        text to appear on the button
 *  \param alttext     alternate text to appear when required
 *  \param action      actions to be associated with button
 *  \param description XXX
 *  \param password    The name of the setting that stores the
 *                     password.
 */
void MythThemedMenu::addButton(const QString &type, const QString &text,
                                const QString &alttext,
                                const QStringList &action,
                                const QString &description,
                                const QString &password)
{
    ThemedButton newbutton;
    newbutton.type = type;
    newbutton.action = action;
    newbutton.text = text;
    newbutton.alttext = alttext;
    newbutton.description = description;
    newbutton.password = password;

    if (m_watermarkState)
        m_watermarkState->EnsureStateLoaded(type);

    auto *listbuttonitem = new MythUIButtonListItem(m_buttonList, text,
                                                QVariant::fromValue(newbutton));

    listbuttonitem->DisplayState(type, "icon");
    listbuttonitem->SetText(description, "description");
}

void MythThemedMenu::buttonAction(MythUIButtonListItem *item, bool skipPass)
{
    auto button = item->GetData().value<ThemedButton>();

    QString password;
    if (!skipPass)
        password = button.password;

    for (const auto & act : std::as_const(button.action))
    {
        if (handleAction(act, password))
            break;
    }
}

void MythThemedMenu::buttonAction(MythUIButtonListItem *item)
{
    buttonAction(item, false);
}

/** \brief Locates the appropriate menu file from which to parse the menu
 *
 *  \param menuname file name of the menu you want to find
 *  \return the directory in which the menu file is found
 */
QString MythThemedMenu::findMenuFile(const QString &menuname)
{
    QString testdir = GetConfDir() + '/' + menuname;
    QFile file(testdir);
    if (file.exists())
        return testdir;
    LOG(VB_FILE, LOG_DEBUG, "No menu file " + testdir);

    testdir = GetMythUI()->GetMenuThemeDir() + '/' + menuname;
    file.setFileName(testdir);
    if (file.exists())
        return testdir;
    LOG(VB_FILE, LOG_DEBUG, "No menu file " + testdir);

    testdir = GetMythUI()->GetThemeDir() + '/' + menuname;
    file.setFileName(testdir);
    if (file.exists())
        return testdir;
    LOG(VB_FILE, LOG_DEBUG, "No menu file " + testdir);

    testdir = GetShareDir() + menuname;
    file.setFileName(testdir);
    if (file.exists())
        return testdir;
    LOG(VB_FILE, LOG_DEBUG, "No menu file " + testdir);

    testdir = "../mythfrontend/" + menuname;
    file.setFileName(testdir);
    if (file.exists())
        return testdir;
    LOG(VB_FILE, LOG_DEBUG, "No menu file " + testdir);

    testdir = GetShareDir() + "themes/defaultmenu/" + menuname;
    file.setFileName(testdir);
    if (file.exists())
        return testdir;
    LOG(VB_FILE, LOG_DEBUG, "No menu file " + testdir);

    return {};
}

/** \brief Handle a MythTV action for the Menus.
 *
 *  \param action single action to be handled
 *  \param password  password setting to be checked
 *  \return true if the action is not to EXEC another program
 */
bool MythThemedMenu::handleAction(const QString &action, const QString &password)
{
    MythUIMenuCallbacks *cbs = GetMythUI()->GetMenuCBs();

    if (!password.isEmpty() && !checkPinCode(password))
        return true;

    if (action.startsWith("EXEC "))
    {
        QString rest = action.right(action.length() - 5);
        if (cbs && cbs->exec_program)
            cbs->exec_program(rest);

        return false;
    }
    if (action.startsWith("EXECTV "))
    {
        QString rest = action.right(action.length() - 7).trimmed();
        if (cbs && cbs->exec_program_tv)
            cbs->exec_program_tv(rest);
    }
    else if (action.startsWith("MENU "))
    {
        QString menu = action.right(action.length() - 5);

        MythScreenStack *stack = GetScreenStack();

        auto *newmenu = new MythThemedMenu("", menu, stack, menu, false, m_state);
        if (newmenu->foundTheme())
            stack->AddScreen(newmenu);
        else
            delete newmenu;
    }
    else if (action.startsWith("UPMENU"))
    {
        m_wantpop = true;
    }
    else if (action.startsWith("CONFIGPLUGIN"))
    {
        QString rest = action.right(action.length() - 13);
        if (cbs && cbs->configplugin)
            cbs->configplugin(rest);
    }
    else if (action.startsWith("PLUGIN"))
    {
        QString rest = action.right(action.length() - 7);
        if (cbs && cbs->plugin)
            cbs->plugin(rest);
    }
    else if (action.startsWith("SHUTDOWN"))
    {
        if (m_allocedstate)
        {
            m_wantpop = true;
        }
    }
    else if (action.startsWith("EJECT"))
    {
        if (cbs && cbs->eject)
            cbs->eject();
    }
    else if (action.startsWith("JUMP "))
    {
        QString rest = action.right(action.length() - 5);
        GetMythMainWindow()->JumpTo(rest, false);
    }
    else if (action.startsWith("MEDIA "))
    {
        // the format is MEDIA HANDLER URL
        // TODO: allow spaces in the url
        QStringList list = action.simplified().split(' ');
        if (list.size() >= 3)
            GetMythMainWindow()->HandleMedia(list[1], list[2]);
    }
    else
    {
        m_selection = action;
        if (m_state->m_callback)
            m_state->m_callback(m_state->m_callbackdata, m_selection);
        else
            LOG(VB_GENERAL, LOG_ERR, "Unknown menu action: " + action);
    }

    return true;
}

bool MythThemedMenu::findDepends(const QString &fileList)
{
    QStringList files = fileList.split(" ");
    MythPluginManager *pluginManager = gCoreContext->GetPluginManager();

    for (const auto & file : std::as_const(files))
    {
        QString filename = findMenuFile(file);
        if (!filename.isEmpty() && filename.endsWith(".xml"))
            return true;

        // Has plugin by this name been successfully loaded
        MythPlugin *plugin = pluginManager->GetPlugin(file);
        if (plugin)
            return true;
    }

    return false;
}

bool MythThemedMenu::findDependsExec(const QString &filename)
{
    QFileInfo filename_info(filename);

    return filename_info.exists() && filename_info.isFile() && filename_info.isExecutable();
}

/** \brief Queries the user for a password to enter a part of MythTV
 *         restricted by a password.
 *
 *  \param password_setting  password to be checked
 *  \return true if password checks out or is not needed.
 */
bool MythThemedMenu::checkPinCode(const QString &password_setting)
{
    QString timestamp_setting = QString("%1Time").arg(password_setting);
    QDateTime curr_time = MythDate::current();
    QString last_time_stamp = GetMythDB()->GetSetting(timestamp_setting);
    QString password = GetMythDB()->GetSetting(password_setting);

    // Password empty? Then skip asking for it
    if (password.isEmpty())
        return true;

    if (last_time_stamp.length() < 1)
    {
        LOG(VB_GENERAL, LOG_ERR,
                "MythThemedMenu: Could not read password/pin time stamp.\n"
                "This is only an issue if it happens repeatedly.");
    }
    else
    {
        QDateTime last_time = MythDate::fromString(last_time_stamp);
        if (!last_time.isValid() || last_time.secsTo(curr_time) < 120)
        {
            last_time_stamp = MythDate::toString(
                curr_time, MythDate::kDatabase);
            GetMythDB()->SaveSetting(timestamp_setting, last_time_stamp);
            return true;
        }
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Using Password: %1")
                                  .arg(password_setting));

    QString text = tr("Enter password:");
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *dialog = new MythTextInputDialog(popupStack, text, FilterNone, true);

    if (dialog->Create())
    {
        dialog->SetReturnEvent(this, "password");
        popupStack->AddScreen(dialog);
    }
    else
    {
        delete dialog;
    }

    return false;
}

/**
 * \copydoc MythUIType::mediaEvent()
 */
void MythThemedMenu::mediaEvent(MythMediaEvent* event)
{
    if (!event)
        return;

    MythMediaDevice *device = event->getDevice();

    if (!device)
        return;

    MythMediaType type = device->getMediaType();
    MythMediaStatus status = device->getStatus();

    if ((status & ~MEDIASTAT_USEABLE) &&
        (status & ~MEDIASTAT_MOUNTED))
        return;

    switch (type)
    {
        case MEDIATYPE_DVD :
        case MEDIATYPE_BD :
            // DVD or Blu-ray Available
            break;
        default :
            return;
    }
}
