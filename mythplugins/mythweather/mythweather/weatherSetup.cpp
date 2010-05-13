
// QT headers
#include <QApplication>
#include <QSqlError>

// MythTV headers
//#include <mythdbcon.h>
#include <mythdb.h>
#include <mythprogressdialog.h>

// MythWeather headers
#include "weatherScreen.h"
#include "weatherSource.h"
#include "sourceManager.h"
#include "weatherSetup.h"

#define GLBL_SCREEN 0
#define SCREEN_SETUP_SCREEN 1
#define SRC_SCREEN 2

GlobalSetup::GlobalSetup(MythScreenStack *parent, const QString &name)
    : MythScreenType(parent, name),
      m_backgroundCheckbox(NULL), m_timeoutSpinbox(NULL),
      m_timeout(0),               m_hold_timeout(0),
      m_finishButton(NULL)
{
}

GlobalSetup::~GlobalSetup()
{
}

bool GlobalSetup::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("weather-ui.xml", "global-setup", this);

    if (!foundtheme)
        return false;

    m_timeoutSpinbox = dynamic_cast<MythUISpinBox *> (GetChild("timeout_spinbox"));

    m_backgroundCheckbox = dynamic_cast<MythUICheckBox *> (GetChild("backgroundcheck"));
    m_finishButton = dynamic_cast<MythUIButton *> (GetChild("finishbutton"));

    if (!m_timeoutSpinbox || !m_finishButton || !m_backgroundCheckbox)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing required elements.");
        return false;
    }

    BuildFocusList();

    m_finishButton->SetText(tr("Finish"));
    connect(m_finishButton, SIGNAL(Clicked()), this, SLOT(saveData()));

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
      m_createdSrcMan(srcman ? false : true),
      m_helpText(NULL),     m_activeList(NULL),
      m_inactiveList(NULL), m_finishButton(NULL)
{
    m_sourceManager->clearSources();
    m_sourceManager->findScripts();
}

ScreenSetup::~ScreenSetup()
{
    if (m_createdSrcMan)
    {
        if (m_sourceManager)
            delete m_sourceManager;
    }
    else
    {
        m_sourceManager->clearSources();
        m_sourceManager->findScriptsDB();
        m_sourceManager->setupSources();
    }

    // Deallocate the ScreenListInfo objects created for the inactive screen list.
    for (int i=0; i < m_inactiveList->GetCount(); i++)
    {
        MythUIButtonListItem *item = m_inactiveList->GetItemAt(i);
        if (item->GetData().isValid())
            delete qVariantValue<ScreenListInfo *>(item->GetData());
    }

    // Deallocate the ScreenListInfo objects created for the active screen list.
    for (int i=0; i < m_activeList->GetCount(); i++)
    {
        MythUIButtonListItem *item = m_activeList->GetItemAt(i);
        if (item->GetData().isValid())
            delete qVariantValue<ScreenListInfo *>(item->GetData());
    }
}

bool ScreenSetup::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("weather-ui.xml", "screen-setup", this);

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
        VERBOSE(VB_IMPORTANT, "Theme is missing required elements.");
        return false;
    }

    BuildFocusList();

    connect(m_activeList, SIGNAL(itemSelected(MythUIButtonListItem *)),
            this, SLOT(updateHelpText()));
    connect(m_activeList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            this, SLOT(doListSelect(MythUIButtonListItem *)));
    connect(m_inactiveList, SIGNAL(itemSelected(MythUIButtonListItem *)),
            this, SLOT(updateHelpText()));
    connect(m_inactiveList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            this, SLOT(doListSelect(MythUIButtonListItem *)));

    SetFocusWidget(m_inactiveList);

    m_finishButton->SetText(tr("Finish"));
    connect(m_finishButton, SIGNAL(Clicked()), this, SLOT(saveData()));

    loadData();

    return true;
}

bool ScreenSetup::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Weather", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "DELETE")
        {
            if (GetFocusWidget() == m_activeList)
                deleteScreen();
        }
        else
            handled = false;
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

        ScreenListInfo *si = qVariantValue<ScreenListInfo *>(item->GetData());
        if (!si)
            return;

        QStringList sources = si->sources;

        text = tr("Add desired screen to the Active Screens list "
            "by pressing SELECT.") + "\n";
        text += si->title + "\n";
        text += QString("%1: %2").arg(tr("Sources"))
                                 .arg(sources.join(", "));
    }
    else if (list == m_activeList)
    {
        MythUIButtonListItem *item = m_activeList->GetItemCurrent();
        if (!item)
            return;

        ScreenListInfo *si = qVariantValue<ScreenListInfo *>(item->GetData());
        if (!si)
            return;

        text += si->title + "\n";
        if (si->hasUnits)
        {
            text += tr("Units: ");
            text += (ENG_UNITS == si->units) ?
                tr("English Units") : tr("SI Units");
            text += "   ";
        }
        if (!si->multiLoc && !si->types.empty())
        {
            TypeListInfo ti = *si->types.begin();
            text += tr("Location: ");
            text += (ti.location.isEmpty()) ? tr("Not Defined") : ti.location;
            text += "\n";
            text += tr("Source: " );
            text += (ti.src) ? ti.src->name : tr("Not Defined");
            text += "\n";
        }
        text += "\n" + tr("Press SELECT to ");
        if (!si->multiLoc)
            text += tr("change location; ");
        if (si->hasUnits)
            text += tr("change units; ");
        text += tr("move screen up or down; or remove screen.");
    }

    m_helpText->SetText(text);
}

void ScreenSetup::loadData()
{
    ScreenListInfo *si;

    QStringList types;

    ScreenListMap screenListMap = loadScreens();

    // Fill the inactive screen button list.
    ScreenListMap::const_iterator i = screenListMap.constBegin();
    while (i != screenListMap.constEnd())
    {

        si = &screenListMap[i.key()];
        types = si->dataTypes;
        si->units = ENG_UNITS;

        QStringList type_strs;
        for (int typei = 0; typei < types.size(); ++typei)
        {
            TypeListInfo ti(types[typei]);
            si->types.insert(types[typei], ti);
            type_strs << types[typei];
        }

        QList<ScriptInfo *> scriptList;
        // Only add a screen to the list if we have a source
        // available to satisfy the requirements.
        if (m_sourceManager->findPossibleSources(type_strs, scriptList))
        {
            ScriptInfo *script;
            for (int x = 0; x < scriptList.size(); x++)
            {
                script = scriptList.at(x);
                si->sources.append(script->name);
            }
            MythUIButtonListItem *item =
                        new MythUIButtonListItem(m_inactiveList, si->title);
            item->SetData(qVariantFromValue(new ScreenListInfo(*si)));
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
        VERBOSE(VB_IMPORTANT, db.lastError().text());
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

        types = screenListMap[name].dataTypes;

        TypeListInfo ti(dataitem, location,
                        m_sourceManager->getSourceByName(src));

        if (active_screens.find(draworder) == active_screens.end())
        {
            si = new ScreenListInfo(screenListMap[name]);
            // Clear types first as we will re-insert the values from the database
            si->types.clear();
            si->units = units;
            
            MythUIButtonListItem *item =
                new MythUIButtonListItem(m_activeList, si->title);
            

            // Only insert types meant for this screen
            for (QStringList::Iterator type_i = types.begin();
                 type_i != types.end(); ++type_i )
            {
                if (*type_i == dataitem)
                    si->types.insert(dataitem, ti);
            }

            item->SetData(qVariantFromValue(si));
            active_screens.insert(draworder, si);
        }
        else
        {
            si = active_screens[draworder];
            for (QStringList::Iterator type_i = types.begin();
                 type_i != types.end(); ++type_i )
            {
                if (*type_i == dataitem)
                {
                    si->types.insert(dataitem, ti);
                }
            }
        }
    }
}

void ScreenSetup::saveData()
{
    if (m_activeList->GetCount() <= 0)
    {
        VERBOSE(VB_IMPORTANT, "No Active Screens are defined. Nothing Saved.");
        return;
    }

    // check if all active screens have sources/locations defined
    QStringList notDefined;

    for (int i=0; i < m_activeList->GetCount(); i++)
    {
        MythUIButtonListItem *item = m_activeList->GetItemAt(i);
        ScreenListInfo *si = qVariantValue<ScreenListInfo *>(item->GetData());
        TypeListMap::iterator it = si->types.begin();
        for (; it != si->types.end(); ++it)
        {
            if ((*it).src)
                continue;

            notDefined << (*it).name;
            VERBOSE(VB_IMPORTANT, QString("Not defined %1").arg((*it).name));
        }
    }

    if (!notDefined.empty())
    {
        VERBOSE(VB_IMPORTANT, "A Selected screen has data items with no "
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
        ScreenListInfo *si = qVariantValue<ScreenListInfo *>(item->GetData());
        db.bindValue(":DRAW", draworder);
        db.bindValue(":CONT", si->name);
        db.bindValue(":UNITS", si->units);
        db.bindValue(":HOST", gCoreContext->GetHostName());
        if (db.exec())
        {
            // TODO see comment in dbcheck.cpp for way to improve
            QString query2 = "SELECT screen_id FROM weatherscreens "
                    "WHERE draworder = :DRAW AND hostname = :HOST;";
            db2.prepare(query2);
            db2.bindValue(":DRAW", draworder);
            db2.bindValue(":HOST", gCoreContext->GetHostName());
            if (!db2.exec())
            {
                VERBOSE(VB_IMPORTANT, db2.executedQuery());
                VERBOSE(VB_IMPORTANT, db2.lastError().text());
                return;
            }

            db2.next();
            int screen_id = db2.value(0).toInt();

            query2 = "INSERT INTO weatherdatalayout (location, dataitem, "
                    "weatherscreens_screen_id, weathersourcesettings_sourceid) "
                    "VALUES (:LOC, :ITEM, :SCREENID, :SRCID);";
            db2.prepare(query2);
            TypeListMap::iterator it = si->types.begin();
            for (; it != si->types.end(); ++it)
            {
                db2.bindValue(":LOC",      (*it).location);
                db2.bindValue(":ITEM",     (*it).name);
                db2.bindValue(":SCREENID", screen_id);
                db2.bindValue(":SRCID",    (*it).src->id);
                if (!db2.exec())
                {
                    VERBOSE(VB_IMPORTANT, db2.executedQuery());
                    VERBOSE(VB_IMPORTANT, db2.lastError().text());
                    return;
                }
            }
        }
        else
        {
            VERBOSE(VB_IMPORTANT, db.executedQuery());
            VERBOSE(VB_IMPORTANT, db.lastError().text());
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

    QString txt = selected->GetText();
    if (GetFocusWidget() == m_activeList)
    {
        ScreenListInfo *si = qVariantValue<ScreenListInfo *>(selected->GetData());

        QString label = tr("Manipulate Screen");

        MythScreenStack *popupStack =
                                GetMythMainWindow()->GetStack("popup stack");

        MythDialogBox *menuPopup = new MythDialogBox(label, popupStack,
                                                    "screensetupmenupopup");

        if (menuPopup->Create())
        {
            popupStack->AddScreen(menuPopup);

            menuPopup->SetReturnEvent(this, "options");

            menuPopup->AddButton(tr("Move Up"), qVariantFromValue(selected));
            menuPopup->AddButton(tr("Move Down"), qVariantFromValue(selected));
            menuPopup->AddButton(tr("Remove"), qVariantFromValue(selected));
            menuPopup->AddButton(tr("Change Location"), qVariantFromValue(selected));
            if (si->hasUnits)
                menuPopup->AddButton(tr("Change Units"), qVariantFromValue(selected));
            menuPopup->AddButton(tr("Cancel"), qVariantFromValue(selected));
        }
        else
        {
            delete menuPopup;
        }

    }
    else if (GetFocusWidget() == m_inactiveList)
    {
        ScreenListInfo *si = qVariantValue<ScreenListInfo *>(selected->GetData());
        QStringList type_strs;

        TypeListMap::iterator it = si->types.begin();
        TypeListMap types;
        for (; it != si->types.end(); ++it)
        {
            types.insert(it.key(), TypeListInfo(*it));
            type_strs << it.key();
        }
        bool hasUnits = si->hasUnits;

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
            VERBOSE(VB_IMPORTANT, "Screen cannot be used, not all required "
                                  " data is supplied by existing sources");
    }
}

void ScreenSetup::doLocationDialog(ScreenListInfo *si)
{
    MythScreenStack *mainStack =
                            GetMythMainWindow()->GetMainStack();

    LocationDialog *locdialog = new LocationDialog(mainStack, "locationdialog",
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

    QString label = QString("%1 %2").arg(name).arg(tr("Change Units"));

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythDialogBox *menuPopup = new MythDialogBox(label, popupStack,
                                                "weatherunitspopup");

    if (menuPopup->Create())
    {
        popupStack->AddScreen(menuPopup);

        menuPopup->SetReturnEvent(this, "units");

        menuPopup->AddButton(tr("English Units"), qVariantFromValue(si));
        menuPopup->AddButton(tr("SI Units"), qVariantFromValue(si));
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
            delete qVariantValue<ScreenListInfo *>(item->GetData());

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
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        QString resultid  = dce->GetId();
        int     buttonnum = dce->GetResult();

        if (resultid == "options")
        {
            if (buttonnum > -1)
            {
                MythUIButtonListItem *item =
                    qVariantValue<MythUIButtonListItem *>(dce->GetData());
                        
                ScreenListInfo *si =
                    qVariantValue<ScreenListInfo *>(item->GetData());

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
                    si->updating = true;
                    doLocationDialog(si);
                }
                else if (si->hasUnits && buttonnum == 4)
                {
                    si->updating = true;
                    showUnitsPopup(item->GetText(), si);
                    updateHelpText();
                }
            }
        }
        else if (resultid == "units")
        {
            if (buttonnum > -1)
            {
                ScreenListInfo *si =
                    qVariantValue<ScreenListInfo *>(dce->GetData());

                if (buttonnum == 0)
                {
                    si->units = ENG_UNITS;
                }
                else if (buttonnum == 1)
                {
                    si->units = SI_UNITS;
                }

                updateHelpText();

                if (si->updating)
                    si->updating = false;
                else
                    doLocationDialog(si);
            }
        }
        else if (resultid == "location")
        {
            ScreenListInfo *si =
                qVariantValue<ScreenListInfo *>(dce->GetData());

            TypeListMap::iterator it = si->types.begin();
            for (; it != si->types.end(); ++it)
            {
                if ((*it).location.isEmpty())
                    return;
            }

            if (si->updating)
            {
                si->updating = false;
                MythUIButtonListItem *item = m_activeList->GetItemCurrent();
                if (item)
                    item->SetData(qVariantFromValue(si));
            }
            else
            {
                QString txt = si->title; txt.detach();
                MythUIButtonListItem *item = 
                        new MythUIButtonListItem(m_activeList, txt);
                item->SetData(qVariantFromValue(si));
            }

            if (m_activeList->GetCount())
                m_activeList->SetEnabled(true);
        }
    }
}

///////////////////////////////////////////////////////////////////////

SourceSetup::SourceSetup(MythScreenStack *parent, const QString &name)
    : MythScreenType(parent, name)
{
    m_sourceList = NULL;
    m_updateSpinbox = m_retrieveSpinbox = NULL;
    m_finishButton = NULL;
    m_sourceText = NULL;
}

SourceSetup::~SourceSetup()
{
    for (int i=0; i < m_sourceList->GetCount(); i++)
    {
        MythUIButtonListItem *item = m_sourceList->GetItemAt(i);
        if (item->GetData().isValid())
            delete qVariantValue<SourceListInfo *>(item->GetData());
    }
}

bool SourceSetup::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("weather-ui.xml", "source-setup", this);

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
        VERBOSE(VB_IMPORTANT, "Theme is missing required elements.");
        return false;
    }

    BuildFocusList();
    SetFocusWidget(m_sourceList);

    connect(m_sourceList, SIGNAL(itemSelected(MythUIButtonListItem *)),
            SLOT(sourceListItemSelected(MythUIButtonListItem *)));
//     connect(m_sourceList, SIGNAL(TakingFocus()),
//             this, SLOT(sourceListItemSelected()));

    // 12 Hour max interval
    m_updateSpinbox->SetRange(10, 720, 10);
    connect(m_updateSpinbox, SIGNAL(LosingFocus()),
            SLOT(updateSpinboxUpdate()));

    // 2 Minute retrieval timeout max
    m_retrieveSpinbox->SetRange(10, 120, 5);
    connect(m_retrieveSpinbox, SIGNAL(LosingFocus()),
            SLOT(retrieveSpinboxUpdate()));

    m_finishButton->SetText(tr("Finish"));
    connect(m_finishButton, SIGNAL(Clicked()), SLOT(saveData()));

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
        VERBOSE(VB_IMPORTANT, db.lastError().text());
        return false;
    }

    if (!db.size())
    {
        return false;
    }

    while (db.next())
    {
        SourceListInfo *si = new SourceListInfo;
        si->id = db.value(0).toUInt();
        si->name = db.value(1).toString();
        si->update_timeout = db.value(2).toUInt() / 60;
        si->retrieve_timeout = db.value(3).toUInt();
        si->author = db.value(4).toString();
        si->email = db.value(5).toString();
        si->version = db.value(6).toString();

        new MythUIButtonListItem(m_sourceList, si->name, qVariantFromValue(si));
    }

    return true;
}

void SourceSetup::saveData()
{
    MythUIButtonListItem *curritem = m_sourceList->GetItemCurrent();

    if (curritem)
    {
        SourceListInfo *si = qVariantValue<SourceListInfo *>(curritem->GetData());
        si->update_timeout = m_updateSpinbox->GetIntValue();
        si->retrieve_timeout = m_retrieveSpinbox->GetIntValue();
    }

    MSqlQuery db(MSqlQuery::InitCon());
    QString query = "UPDATE weathersourcesettings "
            "SET update_timeout = :UPDATE, retrieve_timeout = :RETRIEVE "
            "WHERE sourceid = :ID;";
    db.prepare(query);

    for (int i=0; i < m_sourceList->GetCount(); i++)
    {
        MythUIButtonListItem *item = m_sourceList->GetItemAt(i);
        SourceListInfo *si = qVariantValue<SourceListInfo *>(item->GetData());
        db.bindValue(":ID", si->id);
        db.bindValue(":UPDATE", si->update_timeout * 60);
        db.bindValue(":RETRIEVE", si->retrieve_timeout);
        if (!db.exec())
        {
            VERBOSE(VB_IMPORTANT, db.lastError().text());
            return;
        }
    }

    Close();
}

void SourceSetup::updateSpinboxUpdate()
{
    if (m_sourceList->GetItemCurrent())
    {
        SourceListInfo *si = qVariantValue<SourceListInfo *>
                                    (m_sourceList->GetItemCurrent()->GetData());
        si->update_timeout = m_updateSpinbox->GetIntValue();
    }
}

void SourceSetup::retrieveSpinboxUpdate()
{
    if (m_sourceList->GetItemCurrent())
    {
        SourceListInfo *si = qVariantValue<SourceListInfo *>
                                    (m_sourceList->GetItemCurrent()->GetData());
        si->retrieve_timeout = m_retrieveSpinbox->GetIntValue();
    }
}

void SourceSetup::sourceListItemSelected(MythUIButtonListItem *item)
{
    if (!item)
        item = m_sourceList->GetItemCurrent();

    if (!item)
        return;

    SourceListInfo *si = qVariantValue<SourceListInfo *>(item->GetData());
    if (!si)
        return;

    m_updateSpinbox->SetValue(si->update_timeout);
    m_retrieveSpinbox->SetValue(si->retrieve_timeout);
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
      m_retScreen(retScreen), m_locationList(NULL),
      m_locationEdit(NULL),   m_searchButton(NULL),
      m_resultsText(NULL),    m_sourceText(NULL)
{
    TypeListMap::iterator it = si->types.begin();
    for (; it != si->types.end(); ++it)
        m_types << (*it).name;
    m_types.detach();
}

LocationDialog::~LocationDialog()
{
  if(m_locationList)
    clearResults();
    
  delete m_screenListInfo;
}

bool LocationDialog::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("weather-ui.xml", "setup-location", this);

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
        VERBOSE(VB_IMPORTANT, "Theme is missing required elements.");
        return false;
    }

    BuildFocusList();
    SetFocusWidget(m_locationEdit);

    connect(m_searchButton, SIGNAL(Clicked()), this, SLOT(doSearch()));
    m_searchButton->SetText(tr("Search"));
    connect(m_locationList, SIGNAL(itemSelected(MythUIButtonListItem *)),
            this, SLOT(itemSelected(MythUIButtonListItem *)));
    connect(m_locationList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            this, SLOT(itemClicked(MythUIButtonListItem *)));

    return true;
}

void LocationDialog::doSearch()
{
    QString busymessage = tr("Searching ...");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythUIBusyDialog *busyPopup = new MythUIBusyDialog(busymessage, popupStack,
                                                       "mythweatherbusydialog");

    if (busyPopup->Create())
    {
        popupStack->AddScreen(busyPopup, false);
    }
    else
    {
        delete busyPopup;
        busyPopup = NULL;
    }
       

    QMap<ScriptInfo *, QStringList> result_cache;
    int numresults = 0;
    clearResults();

    QString searchingresults = tr("Searching ... Results: %1");

    m_resultsText->SetText(searchingresults.arg(0));
    qApp->processEvents();

    QList<ScriptInfo *> sources;
    // if a screen makes it this far, theres at least one source for it
    m_sourceManager->findPossibleSources(m_types, sources);
    QString search = m_locationEdit->GetText();
    ScriptInfo *si;
    for (int x = 0; x < sources.size(); x++)
    {
        si = sources.at(x);
        if (!result_cache.contains(si))
        {
            QStringList results = m_sourceManager->getLocationList(si, search);
            result_cache[si] = results;
            numresults += results.size();
            m_resultsText->SetText(searchingresults.arg(numresults));
            qApp->processEvents();
        }
    }

    for (int i = 0; i < result_cache.keys().size(); ++i)
    {
        si = result_cache.keys()[i];
        QStringList results = result_cache[si];
        QString name = si->name;
        for (int ii = 0; ii < results.size(); ++ii)
        {
            QStringList tmp = results[ii].split("::");
            QString resultstring = QString("%1 (%2)").arg(tmp[1]).arg(name);
            MythUIButtonListItem *item =
                new MythUIButtonListItem(m_locationList, resultstring);
            ResultListInfo *ri = new ResultListInfo;
            ri->idstr = tmp[0];
            ri->src = si;
            item->SetData(qVariantFromValue(ri));
            qApp->processEvents();
        }
    }

    if (busyPopup)
    {
        busyPopup->Close();
        busyPopup = NULL;
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
            delete qVariantValue<ResultListInfo *>(item->GetData());
    }
    
    m_locationList->Reset();
}

void LocationDialog::itemSelected(MythUIButtonListItem *item)
{
    ResultListInfo *ri = qVariantValue<ResultListInfo *>(item->GetData());
    if (ri)
        m_sourceText->SetText(tr("Source: %1").arg(ri->src->name));
}

void LocationDialog::itemClicked(MythUIButtonListItem *item)
{
    ResultListInfo *ri = qVariantValue<ResultListInfo *>(item->GetData());

    if (ri)
    {
        TypeListMap::iterator it = m_screenListInfo->types.begin();
        for (; it != m_screenListInfo->types.end(); ++it)
        {
            (*it).location = ri->idstr;
            (*it).location.detach();
            (*it).src      = ri->src;
        }
    }

    DialogCompletionEvent *dce =
        new DialogCompletionEvent("location", 0, "",
                                  qVariantFromValue(new ScreenListInfo(*m_screenListInfo)));
    QApplication::postEvent(m_retScreen, dce);

    Close();
}
