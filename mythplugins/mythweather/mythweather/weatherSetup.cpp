// QT headers
#include <QApplication>
#include <QSqlError>
#include <QVariant>

// MythTV headers
#include <libmythbase/mythdb.h>
#include <libmythui/mythprogressdialog.h>

// MythWeather headers
#include "sourceManager.h"
#include "weatherScreen.h"
#include "weatherSetup.h"
#include "weatherSource.h"

bool GlobalSetup::Create()
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("weather-ui.xml", "global-setup", this);
    if (!foundtheme)
        return false;

    m_timeoutSpinbox = dynamic_cast<MythUISpinBox *> (GetChild("timeout_spinbox"));

    m_backgroundCheckbox = dynamic_cast<MythUICheckBox *> (GetChild("backgroundcheck"));
    m_finishButton = dynamic_cast<MythUIButton *> (GetChild("finishbutton"));

    if (!m_timeoutSpinbox || !m_finishButton || !m_backgroundCheckbox)
    {
        LOG(VB_GENERAL, LOG_ERR, "Theme is missing required elements.");
        return false;
    }

    BuildFocusList();

    m_finishButton->SetText(tr("Finish"));
    connect(m_finishButton, &MythUIButton::Clicked, this, &GlobalSetup::saveData);

    loadData();

    return true;
}

void GlobalSetup::loadData()
{
    int setting = gCoreContext->GetNumSetting("weatherbackgroundfetch", 0);
    if (setting == 1)
        m_backgroundCheckbox->SetCheckState(MythUIStateType::Full);

    m_timeout = gCoreContext->GetNumSetting("weatherTimeout", 10);
    m_timeoutSpinbox->SetRange(5, 120, 5);
    m_timeoutSpinbox->SetValue(m_timeout);
}

void GlobalSetup::saveData()
{
    int timeout = m_timeoutSpinbox->GetIntValue();
    gCoreContext->SaveSetting("weatherTimeout", timeout);

    int checkstate = 0;
    if (m_backgroundCheckbox->GetCheckState() == MythUIStateType::Full)
        checkstate = 1;
    gCoreContext->SaveSetting("weatherbackgroundfetch", checkstate);
    Close();
}

///////////////////////////////////////////////////////////////////////

ScreenSetup::ScreenSetup(MythScreenStack *parent, const QString &name,
                         SourceManager *srcman)
    : MythScreenType(parent, name),
      m_sourceManager(srcman ? srcman : new SourceManager()),
      m_createdSrcMan(srcman == nullptr)
{
    m_sourceManager->clearSources();
    m_sourceManager->findScripts();
}

ScreenSetup::~ScreenSetup()
{
    if (m_createdSrcMan)
        delete m_sourceManager;
    m_sourceManager = nullptr;

    // Deallocate the ScreenListInfo objects created for the inactive screen list.
    for (int i=0; i < m_inactiveList->GetCount(); i++)
    {
        MythUIButtonListItem *item = m_inactiveList->GetItemAt(i);
        if (item->GetData().isValid())
            delete item->GetData().value<ScreenListInfo *>();
    }

    // Deallocate the ScreenListInfo objects created for the active screen list.
    for (int i=0; i < m_activeList->GetCount(); i++)
    {
        MythUIButtonListItem *item = m_activeList->GetItemAt(i);
        if (item->GetData().isValid())
            delete item->GetData().value<ScreenListInfo *>();
    }
}

bool ScreenSetup::Create()
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("weather-ui.xml", "screen-setup", this);
    if (!foundtheme)
        return false;

    m_helpText = dynamic_cast<MythUIText *> (GetChild("helptxt"));

    m_activeList = dynamic_cast<MythUIButtonList *> (GetChild("activelist"));
    m_inactiveList = dynamic_cast<MythUIButtonList *> (GetChild("inactivelist"));

    m_finishButton = dynamic_cast<MythUIButton *> (GetChild("finishbutton"));

    MythUIText *activeheader = dynamic_cast<MythUIText *> (GetChild("activehdr"));
    if (activeheader)
        activeheader->SetText(tr("Active Screens"));

    MythUIText *inactiveheader = dynamic_cast<MythUIText *> (GetChild("inactivehdr"));
    if (inactiveheader)
        inactiveheader->SetText(tr("Inactive Screens"));

    if (!m_activeList || !m_inactiveList || !m_finishButton || !m_helpText)
    {
        LOG(VB_GENERAL, LOG_ERR, "Theme is missing required elements.");
        return false;
    }

    BuildFocusList();

    connect(m_activeList, &MythUIButtonList::itemSelected,
            this, &ScreenSetup::updateHelpText);
    connect(m_activeList, &MythUIButtonList::itemClicked,
            this, &ScreenSetup::doListSelect);
    connect(m_inactiveList, &MythUIButtonList::itemSelected,
            this, &ScreenSetup::updateHelpText);
    connect(m_inactiveList, &MythUIButtonList::itemClicked,
            this, &ScreenSetup::doListSelect);

    SetFocusWidget(m_inactiveList);

    m_finishButton->SetText(tr("Finish"));
    connect(m_finishButton, &MythUIButton::Clicked, this, &ScreenSetup::saveData);

    loadData();

    return true;
}

bool ScreenSetup::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Weather", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "DELETE")
        {
            if (GetFocusWidget() == m_activeList)
                deleteScreen();
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

void ScreenSetup::updateHelpText()
{
    MythUIType *list = GetFocusWidget();
    QString text;
    if (!list)
        return;

    if (list == m_inactiveList)
    {

        MythUIButtonListItem *item = m_inactiveList->GetItemCurrent();
        if (!item)
            return;

        auto *si = item->GetData().value<ScreenListInfo *>();
        if (!si)
            return;

        QStringList sources = si->m_sources;

        text = tr("Add desired screen to the Active Screens list "
            "by pressing SELECT.") + "\n";
        text += si->m_title + "\n";
        text += QString("%1: %2").arg(tr("Sources"), sources.join(", "));
    }
    else if (list == m_activeList)
    {
        MythUIButtonListItem *item = m_activeList->GetItemCurrent();
        if (!item)
            return;

        auto *si = item->GetData().value<ScreenListInfo *>();
        if (!si)
            return;

        text += si->m_title + "\n";
        if (si->m_hasUnits)
        {
            text += tr("Units: ");
            text += (ENG_UNITS == si->m_units) ?
                tr("English Units") : tr("SI Units");
            text += "   ";
        }
        if (!si->m_multiLoc && !si->m_types.empty())
        {
            TypeListInfo ti = *si->m_types.begin();
            text += tr("Location: ");
            text += (ti.m_location.isEmpty()) ? tr("Not Defined") : ti.m_location;
            text += "\n";
            text += tr("Source: " );
            text += (ti.m_src) ? ti.m_src->name : tr("Not Defined");
            text += "\n";
        }
        text += "\n" + tr("Press SELECT to ");
        if (!si->m_multiLoc)
            text += tr("change location; ");
        if (si->m_hasUnits)
            text += tr("change units; ");
        text += tr("move screen up or down; or remove screen.");
    }

    m_helpText->SetText(text);
}

void ScreenSetup::loadData()
{
    QStringList types;

    ScreenListMap screenListMap = loadScreens();

    // Fill the inactive screen button list.
    ScreenListMap::const_iterator i = screenListMap.constBegin();
    while (i != screenListMap.constEnd())
    {
        ScreenListInfo *si = &screenListMap[i.key()];
        types = si->m_dataTypes;
        si->m_units = ENG_UNITS;

        QStringList type_strs;
        for (const QString& type : std::as_const(types))
        {
            TypeListInfo ti(type);
            si->m_types.insert(type, ti);
            type_strs << type;
        }

        QList<ScriptInfo *> scriptList;
        // Only add a screen to the list if we have a source
        // available to satisfy the requirements.
        if (m_sourceManager->findPossibleSources(type_strs, scriptList))
        {
            for (const auto *script : std::as_const(scriptList))
                si->m_sources.append(script->name);
            auto *item = new MythUIButtonListItem(m_inactiveList, si->m_title);
            item->SetData(QVariant::fromValue(new ScreenListInfo(*si)));
        }

        ++i;
    }

    QMap<long, ScreenListInfo*> active_screens;

    MSqlQuery db(MSqlQuery::InitCon());
    QString query = "SELECT weatherscreens.container, weatherscreens.units, "
        "weatherdatalayout.dataitem, weatherdatalayout.location, "
        "weathersourcesettings.source_name, weatherscreens.draworder "
        "FROM weatherscreens, weatherdatalayout, weathersourcesettings "
        "WHERE weatherscreens.hostname = :HOST "
        "AND weatherscreens.screen_id = weatherdatalayout.weatherscreens_screen_id "
        "AND weathersourcesettings.sourceid = weatherdatalayout.weathersourcesettings_sourceid "
        "ORDER BY weatherscreens.draworder;";
    db.prepare(query);
    db.bindValue(":HOST", gCoreContext->GetHostName());
    if (!db.exec())
    {
        LOG(VB_GENERAL, LOG_ERR, db.lastError().text());
        return;
    }

    // Fill the active screen button list.
    while (db.next())
    {
        QString name = db.value(0).toString();
        units_t units = db.value(1).toUInt();
        QString dataitem = db.value(2).toString();
        QString location = db.value(3).toString();
        QString src = db.value(4).toString();
        uint draworder = db.value(5).toUInt();

        types = screenListMap[name].m_dataTypes;

        TypeListInfo ti(dataitem, location,
                        m_sourceManager->getSourceByName(src));

        if (active_screens.find(draworder) == active_screens.end())
        {
            auto *si = new ScreenListInfo(screenListMap[name]);
            // Clear types first as we will re-insert the values from the database
            si->m_types.clear();
            si->m_units = units;
            
            auto *item = new MythUIButtonListItem(m_activeList, si->m_title);

            // Only insert types meant for this screen
            for (const auto & type : std::as_const(types))
            {
                if (type == dataitem)
                    si->m_types.insert(dataitem, ti);
            }

            item->SetData(QVariant::fromValue(si));
            active_screens.insert(draworder, si);
        }
        else
        {
            ScreenListInfo *si = active_screens[draworder];
            for (const auto & type : std::as_const(types))
            {
                if (type == dataitem)
                {
                    si->m_types.insert(dataitem, ti);
                }
            }
        }
    }
}

void ScreenSetup::saveData()
{
    // check if all active screens have sources/locations defined
    QStringList notDefined;

    for (int i=0; i < m_activeList->GetCount(); i++)
    {
        MythUIButtonListItem *item = m_activeList->GetItemAt(i);
        auto *si = item->GetData().value<ScreenListInfo *>();
        for (const auto & type : std::as_const(si->m_types))
        {
            if (type.m_src)
                continue;

            notDefined << type.m_name;
            LOG(VB_GENERAL, LOG_ERR, QString("Not defined %1").arg(type.m_name));
        }
    }

    if (!notDefined.empty())
    {
        LOG(VB_GENERAL, LOG_ERR, "A Selected screen has data items with no "
                                 "sources defined.");
        return;
    }

    MSqlQuery db(MSqlQuery::InitCon());
    MSqlQuery db2(MSqlQuery::InitCon());
    QString query = "DELETE FROM weatherscreens WHERE hostname=:HOST";
    db.prepare(query);
    db.bindValue(":HOST", gCoreContext->GetHostName());
    if (!db.exec())
        MythDB::DBError("ScreenSetup::saveData - delete weatherscreens", db);

    query = "INSERT into weatherscreens (draworder, container, units, hostname) "
            "VALUES (:DRAW, :CONT, :UNITS, :HOST);";
    db.prepare(query);

    int draworder = 0;
    for (int i=0; i < m_activeList->GetCount(); i++)
    {
        MythUIButtonListItem *item = m_activeList->GetItemAt(i);
        auto *si = item->GetData().value<ScreenListInfo *>();
        db.bindValue(":DRAW", draworder);
        db.bindValue(":CONT", si->m_name);
        db.bindValue(":UNITS", si->m_units);
        db.bindValue(":HOST", gCoreContext->GetHostName());
        if (db.exec())
        {
            // TODO see comment in dbcheck.cpp for way to improve
            QString query2 = "SELECT screen_id FROM weatherscreens "
                    "WHERE draworder = :DRAW AND hostname = :HOST;";
            db2.prepare(query2);
            db2.bindValue(":DRAW", draworder);
            db2.bindValue(":HOST", gCoreContext->GetHostName());
            if (!db2.exec() || !db2.next())
            {
                LOG(VB_GENERAL, LOG_ERR, db2.executedQuery());
                LOG(VB_GENERAL, LOG_ERR, db2.lastError().text());
                return;
            }
            
            int screen_id = db2.value(0).toInt();

            query2 = "INSERT INTO weatherdatalayout (location, dataitem, "
                    "weatherscreens_screen_id, weathersourcesettings_sourceid) "
                    "VALUES (:LOC, :ITEM, :SCREENID, :SRCID);";
            db2.prepare(query2);
            for (const auto & type : std::as_const(si->m_types))
            {
                db2.bindValue(":LOC",      type.m_location);
                db2.bindValue(":ITEM",     type.m_name);
                db2.bindValue(":SCREENID", screen_id);
                db2.bindValue(":SRCID",    type.m_src->id);
                if (!db2.exec())
                {
                    LOG(VB_GENERAL, LOG_ERR, db2.executedQuery());
                    LOG(VB_GENERAL, LOG_ERR, db2.lastError().text());
                    return;
                }
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, db.executedQuery());
            LOG(VB_GENERAL, LOG_ERR, db.lastError().text());
            return;
        }

        ++draworder;
    }

    Close();
}

void ScreenSetup::doListSelect(MythUIButtonListItem *selected)
{
    if (!selected)
        return;

    if (GetFocusWidget() == m_activeList)
    {
        auto *si = selected->GetData().value<ScreenListInfo *>();

        QString label = tr("Manipulate Screen");

        MythScreenStack *popupStack =
                                GetMythMainWindow()->GetStack("popup stack");

        auto *menuPopup = new MythDialogBox(label, popupStack,
                                            "screensetupmenupopup");

        if (menuPopup->Create())
        {
            popupStack->AddScreen(menuPopup);

            menuPopup->SetReturnEvent(this, "options");

            menuPopup->AddButtonV(tr("Move Up"), QVariant::fromValue(selected));
            menuPopup->AddButtonV(tr("Move Down"), QVariant::fromValue(selected));
            menuPopup->AddButtonV(tr("Remove"), QVariant::fromValue(selected));
            menuPopup->AddButtonV(tr("Change Location"), QVariant::fromValue(selected));
            if (si->m_hasUnits)
                menuPopup->AddButtonV(tr("Change Units"), QVariant::fromValue(selected));
            menuPopup->AddButtonV(tr("Cancel"), QVariant::fromValue(selected));
        }
        else
        {
            delete menuPopup;
        }

    }
    else if (GetFocusWidget() == m_inactiveList)
    {
        auto *si = selected->GetData().value<ScreenListInfo *>();
        QStringList type_strs;

        TypeListMap types;
        // NOLINTNEXTLINE(modernize-loop-convert)
        for (auto it = si->m_types.begin(); it != si->m_types.end(); ++it)
        {
            types.insert(it.key(), *it);
            type_strs << it.key();
        }
        bool hasUnits = si->m_hasUnits;

        QList<ScriptInfo *> tmp;
        if (m_sourceManager->findPossibleSources(type_strs, tmp))
        {
            if (!m_inactiveList->GetCount())
            {
                //m_inactiveList->SetActive(false);
                NextPrevWidgetFocus(true);
            }
            if (hasUnits)
                showUnitsPopup(selected->GetText(), si);
            else
                doLocationDialog(si);
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "Screen cannot be used, not all required "
                                     "data is supplied by existing sources");
        }
    }
}

void ScreenSetup::doLocationDialog(ScreenListInfo *si)
{
    MythScreenStack *mainStack =
                            GetMythMainWindow()->GetMainStack();

    auto *locdialog = new LocationDialog(mainStack, "locationdialog",
                                         this, si, m_sourceManager);

    if (locdialog->Create())
        mainStack->AddScreen(locdialog);
    else
        delete locdialog;
}

void ScreenSetup::showUnitsPopup(const QString &name, ScreenListInfo *si)
{
    if (!si)
        return;

    QString label = QString("%1 %2").arg(name, tr("Change Units"));

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *menuPopup = new MythDialogBox(label, popupStack, "weatherunitspopup");

    if (menuPopup->Create())
    {
        popupStack->AddScreen(menuPopup);

        menuPopup->SetReturnEvent(this, "units");

        menuPopup->AddButtonV(tr("English Units"), QVariant::fromValue(si));
        menuPopup->AddButtonV(tr("SI Units"), QVariant::fromValue(si));
    }
    else
    {
        delete menuPopup;
    }
}

void ScreenSetup::deleteScreen()
{
    MythUIButtonListItem *item = m_activeList->GetItemCurrent();
    if (item)
    {
        if (item->GetData().isValid())
            delete item->GetData().value<ScreenListInfo *>();

        delete item;
    }

    if (!m_activeList->GetCount())
    {
        NextPrevWidgetFocus(false);
        m_activeList->SetEnabled(false);
    }
}

void ScreenSetup::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = dynamic_cast<DialogCompletionEvent*>(event);
        if (dce == nullptr)
            return;

        QString resultid  = dce->GetId();
        int     buttonnum = dce->GetResult();

        if (resultid == "options")
        {
            if (buttonnum > -1)
            {
                auto *item = dce->GetData().value<MythUIButtonListItem *>();
                auto *si = item->GetData().value<ScreenListInfo *>();

                if (buttonnum == 0)
                {
                    m_activeList->MoveItemUpDown(item, true);
                }
                else if (buttonnum == 1)
                {
                    m_activeList->MoveItemUpDown(item, false);
                }
                else if (buttonnum == 2)
                {
                    deleteScreen();
                }
                else if (buttonnum == 3)
                {
                    si->m_updating = true;
                    doLocationDialog(si);
                }
                else if (si->m_hasUnits && buttonnum == 4)
                {
                    si->m_updating = true;
                    showUnitsPopup(item->GetText(), si);
                    updateHelpText();
                }
            }
        }
        else if (resultid == "units")
        {
            if (buttonnum > -1)
            {
                auto *si = dce->GetData().value<ScreenListInfo *>();

                if (buttonnum == 0)
                {
                    si->m_units = ENG_UNITS;
                }
                else if (buttonnum == 1)
                {
                    si->m_units = SI_UNITS;
                }

                updateHelpText();

                if (si->m_updating)
                    si->m_updating = false;
                else
                    doLocationDialog(si);
            }
        }
        else if (resultid == "location")
        {
            auto *si = dce->GetData().value<ScreenListInfo *>();

            auto emptyloc = [](const auto & type)
                { return type.m_location.isEmpty(); };
            if (std::any_of(si->m_types.cbegin(), si->m_types.cend(), emptyloc))
                return;

            if (si->m_updating)
            {
                si->m_updating = false;
                MythUIButtonListItem *item = m_activeList->GetItemCurrent();
                if (item)
                    item->SetData(QVariant::fromValue(si));
            }
            else
            {
                auto *item = new MythUIButtonListItem(m_activeList, si->m_title);
                item->SetData(QVariant::fromValue(si));
            }

            if (m_activeList->GetCount())
                m_activeList->SetEnabled(true);
        }
    }
}

///////////////////////////////////////////////////////////////////////

SourceSetup::~SourceSetup()
{
    for (int i=0; i < m_sourceList->GetCount(); i++)
    {
        MythUIButtonListItem *item = m_sourceList->GetItemAt(i);
        if (item->GetData().isValid())
            delete item->GetData().value<SourceListInfo *>();
    }
}

bool SourceSetup::Create()
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("weather-ui.xml", "source-setup", this);
    if (!foundtheme)
        return false;

    m_sourceList = dynamic_cast<MythUIButtonList *> (GetChild("srclist"));
    m_updateSpinbox = dynamic_cast<MythUISpinBox *> (GetChild("update_spinbox"));
    m_retrieveSpinbox = dynamic_cast<MythUISpinBox *> (GetChild("retrieve_spinbox"));
    m_finishButton = dynamic_cast<MythUIButton *> (GetChild("finishbutton"));
    m_sourceText = dynamic_cast<MythUIText *> (GetChild("srcinfo"));

    if (!m_sourceList || !m_updateSpinbox || !m_retrieveSpinbox
        || !m_finishButton || !m_sourceText)
    {
        LOG(VB_GENERAL, LOG_ERR, "Theme is missing required elements.");
        return false;
    }

    BuildFocusList();
    SetFocusWidget(m_sourceList);

    connect(m_sourceList, &MythUIButtonList::itemSelected,
            this, qOverload<MythUIButtonListItem *>(&SourceSetup::sourceListItemSelected));
#if 0
    connect(m_sourceList, &MythUIButtonList::TakingFocus,
            this, qOverload<>(&SourceSetup::sourceListItemSelected));
#endif

    // 12 Hour max interval
    m_updateSpinbox->SetRange(10, 720, 10);
    connect(m_updateSpinbox, &MythUIType::LosingFocus,
            this, &SourceSetup::updateSpinboxUpdate);

    // 2 Minute retrieval timeout max
    m_retrieveSpinbox->SetRange(10, 120, 5);
    connect(m_retrieveSpinbox, &MythUIType::LosingFocus,
            this, &SourceSetup::retrieveSpinboxUpdate);

    m_finishButton->SetText(tr("Finish"));
    connect(m_finishButton, &MythUIButton::Clicked, this, &SourceSetup::saveData);

    loadData();

    return true;
}

bool SourceSetup::loadData()
{
    MSqlQuery db(MSqlQuery::InitCon());
    QString query =
     "SELECT DISTINCT sourceid, source_name, update_timeout, retrieve_timeout, "
         "author, email, version FROM weathersourcesettings, weatherdatalayout "
         "WHERE weathersourcesettings.sourceid = weatherdatalayout.weathersourcesettings_sourceid "
         "AND hostname=:HOST;";
    db.prepare(query);
    db.bindValue(":HOST", gCoreContext->GetHostName());
    if (!db.exec())
    {
        LOG(VB_GENERAL, LOG_ERR, db.lastError().text());
        return false;
    }

    if (!db.size())
    {
        return false;
    }

    while (db.next())
    {
        auto *si = new SourceListInfo;
        si->id = db.value(0).toUInt();
        si->name = db.value(1).toString();
        si->update_timeout = std::chrono::minutes(db.value(2).toUInt() / 60);
        si->retrieve_timeout = std::chrono::seconds(db.value(3).toUInt());
        si->author = db.value(4).toString();
        si->email = db.value(5).toString();
        si->version = db.value(6).toString();

        new MythUIButtonListItem(m_sourceList, si->name, QVariant::fromValue(si));
    }

    return true;
}

void SourceSetup::saveData()
{
    MythUIButtonListItem *curritem = m_sourceList->GetItemCurrent();

    if (curritem)
    {
        auto *si = curritem->GetData().value<SourceListInfo *>();
        si->update_timeout = m_updateSpinbox->GetDuration<std::chrono::minutes>();
        si->retrieve_timeout = m_retrieveSpinbox->GetDuration<std::chrono::seconds>();
    }

    MSqlQuery db(MSqlQuery::InitCon());
    QString query = "UPDATE weathersourcesettings "
            "SET update_timeout = :UPDATE, retrieve_timeout = :RETRIEVE "
            "WHERE sourceid = :ID;";
    db.prepare(query);

    for (int i=0; i < m_sourceList->GetCount(); i++)
    {
        MythUIButtonListItem *item = m_sourceList->GetItemAt(i);
        auto *si = item->GetData().value<SourceListInfo *>();
        db.bindValue(":ID", si->id);
        db.bindValue(":UPDATE", (int)duration_cast<std::chrono::seconds>(si->update_timeout).count());
        db.bindValue(":RETRIEVE", (int)si->retrieve_timeout.count());
        if (!db.exec())
        {
            LOG(VB_GENERAL, LOG_ERR, db.lastError().text());
            return;
        }
    }

    Close();
}

void SourceSetup::updateSpinboxUpdate()
{
    if (m_sourceList->GetItemCurrent())
    {
        auto *si = m_sourceList->GetItemCurrent()->GetData().value<SourceListInfo *>();
        si->update_timeout = m_updateSpinbox->GetDuration<std::chrono::minutes>();
    }
}

void SourceSetup::retrieveSpinboxUpdate()
{
    if (m_sourceList->GetItemCurrent())
    {
        auto *si = m_sourceList->GetItemCurrent()->GetData().value<SourceListInfo *>();
        si->retrieve_timeout = m_retrieveSpinbox->GetDuration<std::chrono::seconds>();
    }
}

void SourceSetup::sourceListItemSelected(MythUIButtonListItem *item)
{
    if (!item)
        item = m_sourceList->GetItemCurrent();

    if (!item)
        return;

    auto *si = item->GetData().value<SourceListInfo *>();
    if (!si)
        return;

    m_updateSpinbox->SetDuration<std::chrono::minutes>(si->update_timeout);
    m_retrieveSpinbox->SetDuration<std::chrono::seconds>(si->retrieve_timeout);
    QString txt = tr("Author: ");
    txt += si->author;
    txt += "\n" + tr("Email: ") + si->email;
    txt += "\n" + tr("Version: ") + si->version;
    m_sourceText->SetText(txt);
}

///////////////////////////////////////////////////////////////////////

LocationDialog::LocationDialog(MythScreenStack *parent, const QString &name,
                               MythScreenType *retScreen, ScreenListInfo *si,
                               SourceManager *srcman)
    : MythScreenType(parent, name),
      m_screenListInfo(new ScreenListInfo(*si)),   m_sourceManager(srcman),
      m_retScreen(retScreen)
{
    for (const auto & type : std::as_const(si->m_types))
        m_types << type.m_name;
}

LocationDialog::~LocationDialog()
{
  if(m_locationList)
    clearResults();
    
  delete m_screenListInfo;
}

bool LocationDialog::Create()
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("weather-ui.xml", "setup-location", this);
    if (!foundtheme)
        return false;

    m_sourceText = dynamic_cast<MythUIText *> (GetChild("source"));
    m_resultsText = dynamic_cast<MythUIText *> (GetChild("numresults"));
    m_locationEdit = dynamic_cast<MythUITextEdit *> (GetChild("loc-edit"));
    m_locationList = dynamic_cast<MythUIButtonList *> (GetChild("results"));
    m_searchButton = dynamic_cast<MythUIButton *> (GetChild("searchbtn"));


    if (!m_sourceText || !m_resultsText || !m_locationEdit || !m_locationList
        || !m_searchButton)
    {
        LOG(VB_GENERAL, LOG_ERR, "Theme is missing required elements.");
        return false;
    }

    BuildFocusList();
    SetFocusWidget(m_locationEdit);

    connect(m_searchButton, &MythUIButton::Clicked, this, &LocationDialog::doSearch);
    m_searchButton->SetText(tr("Search"));
    connect(m_locationList, &MythUIButtonList::itemSelected,
            this, &LocationDialog::itemSelected);
    connect(m_locationList, &MythUIButtonList::itemClicked,
            this, &LocationDialog::itemClicked);

    return true;
}

void LocationDialog::doSearch()
{
    QString busymessage = tr("Searching...");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *busyPopup = new MythUIBusyDialog(busymessage, popupStack,
                                           "mythweatherbusydialog");

    if (busyPopup->Create())
    {
        popupStack->AddScreen(busyPopup, false);
    }
    else
    {
        delete busyPopup;
        busyPopup = nullptr;
    }
       

    QHash<ScriptInfo *, QStringList> result_cache;
    int numresults = 0;
    clearResults();

    QString searchingresults = tr("Searching... Results: %1");

    m_resultsText->SetText(searchingresults.arg(0));
    QCoreApplication::processEvents();

    QList<ScriptInfo *> sources;
    // if a screen makes it this far, theres at least one source for it
    m_sourceManager->findPossibleSources(m_types, sources);
    QString search = m_locationEdit->GetText();
    for (auto *si : std::as_const(sources))
    {
        if (!result_cache.contains(si))
        {
            QStringList results = m_sourceManager->getLocationList(si, search);
            result_cache[si] = results;
            numresults += results.size();
            m_resultsText->SetText(searchingresults.arg(numresults));
            QCoreApplication::processEvents();
        }
    }

    for (auto it = result_cache.begin(); it != result_cache.end(); ++it)
    {
        ScriptInfo *si = it.key();
        QStringList results = it.value();
        QString name = si->name;
        QStringList::iterator rit;
        for (rit = results.begin(); rit != results.end(); ++rit)
        {
            QStringList tmp = (*rit).split("::");
            if (tmp.size() < 2)
            {
                LOG(VB_GENERAL, LOG_WARNING,
                        QString("Invalid line in Location Search reponse "
                                "from %1: %2").arg(name, *rit));
                continue;
            }
            QString resultstring = QString("%1 (%2)").arg(tmp[1], name);
            auto *item = new MythUIButtonListItem(m_locationList, resultstring);
            auto *ri = new ResultListInfo;
            ri->idstr = tmp[0];
            ri->src = si;
            item->SetData(QVariant::fromValue(ri));
            QCoreApplication::processEvents();
        }
    }

    if (busyPopup)
    {
        busyPopup->Close();
        busyPopup = nullptr;
    }

    m_resultsText->SetText(tr("Search Complete. Results: %1").arg(numresults));
    if (numresults)
        SetFocusWidget(m_locationList);
}

void LocationDialog::clearResults()
{
    for (int i=0; i < m_locationList->GetCount(); i++)
    {
        MythUIButtonListItem *item = m_locationList->GetItemAt(i);
        if (item->GetData().isValid())
            delete item->GetData().value<ResultListInfo *>();
    }
    
    m_locationList->Reset();
}

void LocationDialog::itemSelected(MythUIButtonListItem *item)
{
    auto *ri = item->GetData().value<ResultListInfo *>();
    if (ri)
        m_sourceText->SetText(tr("Source: %1").arg(ri->src->name));
}

void LocationDialog::itemClicked(MythUIButtonListItem *item)
{
    auto *ri = item->GetData().value<ResultListInfo *>();
    if (ri)
    {
        // NOLINTNEXTLINE(modernize-loop-convert)
        for (auto it = m_screenListInfo->m_types.begin();
             it != m_screenListInfo->m_types.end(); ++it)
        {
            (*it).m_location = ri->idstr;
            (*it).m_src      = ri->src;
        }
    }

    auto *dce = new DialogCompletionEvent("location", 0, "",
                      QVariant::fromValue(new ScreenListInfo(*m_screenListInfo)));
    QApplication::postEvent(m_retScreen, dce);

    Close();
}
