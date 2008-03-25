#include <mythtv/mythdbcon.h>
#include <mythtv/uilistbtntype.h>
#include <qapplication.h>
//Added by qt3to4:
#include <QKeyEvent>
#include <Q3PtrList>

#include "weatherScreen.h"
#include "weatherSource.h"
#include "sourceManager.h"

#include "defs.h"

#include "weatherSetup.h"

#define GLBL_SCREEN 0
#define SCREEN_SETUP_SCREEN 1
#define SRC_SCREEN 2

GlobalSetup::GlobalSetup(MythMainWindow *parent)
    : MythThemedDialog(parent, "global-setup", "weather-", "Global Setup")
{
    wireUI();
    loadData();

    buildFocusList();
    assignFirstFocus();
}

GlobalSetup::~GlobalSetup()
{
    delete m_timeout_spinbox;
    delete m_hold_spinbox;
}

void GlobalSetup::wireUI()
{
    UIBlackHoleType *blckhl = getUIBlackHoleType("pgto_spinbox");
    if (!blckhl)
    {
        VERBOSE(VB_IMPORTANT, "error loading pgto_spinbox");
    }
    else
    {
        m_timeout_spinbox = new WeatherSpinBox(this);
        m_timeout_spinbox->setRange(1, 1000);
        m_timeout_spinbox->setLineStep(1);
        m_timeout_spinbox->setFont(gContext->GetMediumFont());
        m_timeout_spinbox->setFocusPolicy(Qt::NoFocus);
        m_timeout_spinbox->setGeometry(blckhl->getScreenArea());
        blckhl->allowFocus(true);
        connect(blckhl, SIGNAL(takingFocus()), m_timeout_spinbox,
                SLOT(setFocus()));
        /* loosing focus? */
        connect(blckhl, SIGNAL(loosingFocus()), m_timeout_spinbox,
                SLOT(clearFocus()));
    }

    blckhl = getUIBlackHoleType("hold_spinbox");
    if (!blckhl)
    {
        VERBOSE(VB_IMPORTANT, "error loading hold_spinbox");
    }
    else
    {
        m_hold_spinbox = new WeatherSpinBox(this);
        m_hold_spinbox->setRange(1, 1000);
        m_hold_spinbox->setLineStep(1);
        m_hold_spinbox->setFont(gContext->GetMediumFont());
        m_hold_spinbox->setFocusPolicy(Qt::NoFocus);
        m_hold_spinbox->setGeometry(blckhl->getScreenArea());
        blckhl->allowFocus(true);
        connect(blckhl, SIGNAL(takingFocus()), m_hold_spinbox,
                SLOT(setFocus()));
        connect(blckhl, SIGNAL(loosingFocus()), m_hold_spinbox,
                SLOT(clearFocus()));
    }

    m_background_check = getUICheckBoxType("backgroundcheck");
    if (!m_background_check)
    {
        VERBOSE(VB_IMPORTANT, "error loading backgroundcheck");
    }
    else
    {
        int setting = gContext->GetNumSetting("weatherbackgroundfetch", 0);
        m_background_check->setState((bool) setting);
    }

//     m_skip_check = getUICheckBoxType("skipcheck");
//     if (!m_skip_check)
//     {
//         VERBOSE(VB_IMPORTANT, "error loading skipcheck");
//     }

    m_finish_btn = getUITextButtonType("finishbutton");
    if (m_finish_btn)
    {
        m_finish_btn->setText(tr("Finish"));
        connect(m_finish_btn, SIGNAL(pushed()), this, SLOT(saveData()));
    }
}

void GlobalSetup::loadData()
{
    m_timeout = gContext->GetNumSetting("weatherTimeout", 10);
    m_hold_timeout = gContext->GetNumSetting("weatherHoldTimeout", 20);

    m_timeout_spinbox->setValue(m_timeout);
    m_hold_spinbox->setValue(m_hold_timeout);
}

void GlobalSetup::saveData()
{
    gContext->SaveSetting("weatherTimeout", m_timeout_spinbox->value());
    gContext->SaveSetting("weatherHoldTimeout", m_hold_spinbox->value());
    gContext->SaveSetting("weatherbackgroundfetch",
                          m_background_check->getState() ? 1 : 0);
    accept();
}

void GlobalSetup::keyPressEvent(QKeyEvent *e)
{
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Weather", e, actions);
    UIType *curr = getCurrentFocusWidget();
    bool handled = false;

    for (uint i = 0; i < actions.size() && !handled; ++i)
    {
        handled = true;
        QString action = actions[i];
        if (action == "DOWN")
        {
            nextPrevWidgetFocus(true);
        }
        else if (action == "UP")
        {
            nextPrevWidgetFocus(false);
        }
        else if (action == "SELECT")
        {
            UICheckBoxType *check = dynamic_cast<UICheckBoxType *>(curr);
            if (check)
            {
                check->push();
            }
            if (curr == m_finish_btn)
                m_finish_btn->push();
        }
        else
            handled = false;
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

///////////////////////////////////////////////////////////////////////

ScreenSetup::ScreenSetup(MythMainWindow *parent, SourceManager *srcman) :
    MythThemedDialog(parent, "screen-setup", "weather-", "Screen Setup")
{
    m_src_man = srcman;
    wireUI();
    loadData();

    buildFocusList();
    assignFirstFocus();
}

void ScreenSetup::wireUI()
{
    m_help_txt = getUITextType("helptxt");
    if (!m_help_txt)
    {
        VERBOSE(VB_IMPORTANT, "Missing textarea helptxt");
    }

    UITextType *activeheader = getUITextType("activehdr");
    if (activeheader)
        activeheader->SetText(tr("Active Screens"));

    UITextType *inactiveheader = getUITextType("inactivehdr");
    if (inactiveheader)
        inactiveheader->SetText(tr("Inactive Screens"));

    m_active_list = getUIListBtnType("activelist");
    if (!m_active_list)
    {
        VERBOSE(VB_IMPORTANT, "Missing listbtntype activelist");
    }
    else
    {
        m_active_list->calculateScreenArea();
        connect(m_active_list, SIGNAL(itemSelected(UIListBtnTypeItem *)),
                this, SLOT(activeListItemSelected(UIListBtnTypeItem *)));
        connect(m_active_list, SIGNAL(takingFocus()), this,
                SLOT(updateHelpText()));
        connect(m_active_list, SIGNAL(itemSelected(UIListBtnTypeItem *)),
                this, SLOT(updateHelpText()));
    }

    m_inactive_list = getUIListBtnType("inactivelist");
    if (!m_inactive_list)
    {
        VERBOSE(VB_IMPORTANT, "Missing listbtntype inactivelist");
    }
    else
    {
        m_inactive_list->calculateScreenArea();
        connect(m_inactive_list, SIGNAL(takingFocus()), this,
                SLOT(updateHelpText()));
        connect(m_inactive_list, SIGNAL(itemSelected(UIListBtnTypeItem *)),
                this, SLOT(updateHelpText()));
    }

//     m_type_list = getUIListBtnType("typelist");
//     if (!m_type_list)
//     {
//         VERBOSE(VB_IMPORTANT, "error loading typelist");
//     }
//     else
//     {
//         m_type_list->calculateScreenArea();
//         m_type_list->allowFocus(false);
//         // connect(m_active_list, SIGNAL(takingFocus()), m_type_list,
//         // SLOT(show()));
//         connect(m_type_list, SIGNAL(takingFocus()), m_type_list, SLOT(show()));
//         connect(m_active_list, SIGNAL(loosingFocus()), m_type_list,
//                 SLOT(hide()));
//         connect(m_active_list, SIGNAL(takingFocus()),
//                 this, SLOT(activeListItemSelected()));
//         connect(m_type_list, SIGNAL(takingFocus()), this,
//                 SLOT(updateHelpText()));
//         connect(m_type_list, SIGNAL(itemSelected(UIListBtnTypeItem *)), this,
//                 SLOT(updateHelpText()));
//     }

//     UITextType *txt = getUITextType("typelbl");
//     if (!txt)
//     {
//         VERBOSE(VB_IMPORTANT, "error loading typelbl");
//     }
//     else
//     {
//         //connect(m_active_list, SIGNAL(takingFocus()), txt, SLOT(show()));
//         connect(m_type_list, SIGNAL(takingFocus()), txt, SLOT(show()));
//         connect(m_active_list, SIGNAL(loosingFocus()), txt, SLOT(hide()));
//         txt->hide();
//         txt->SetText(tr("Data Types"));
//     }

    m_finish_btn = getUITextButtonType("finishbutton");
    if (m_finish_btn)
    {
        m_finish_btn->setText(tr("Finish"));
        connect(m_finish_btn, SIGNAL(pushed()), this, SLOT(saveData()));
    }

}

void ScreenSetup::keyPressEvent(QKeyEvent *e)
{
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Weather", e, actions);
    UIType *curr = getCurrentFocusWidget();
    UIListBtnType *list;
    bool handled = false;

    for (uint i = 0; i < actions.size() && !handled; ++i)
    {
        handled = true;
        QString action = actions[i];
        if (action == "DOWN")
            cursorDown(curr);
        else if (action == "UP")
            cursorUp(curr);
        else if (action == "SELECT")
            cursorSelect(curr);
        else if (action == "RIGHT")
        {
            m_active_list->allowFocus(m_active_list->GetCount() > 0);
            nextPrevWidgetFocus(true);
        }
        else if (action == "LEFT")
        {
            m_active_list->allowFocus(m_active_list->GetCount() > 0);
            nextPrevWidgetFocus(false);
        }
        else if (action == "DELETE")
        {
            if (curr == m_active_list)
            {
                UIListBtnType *list = dynamic_cast<UIListBtnType *>(curr);
                deleteScreen(list);
            }
        }
        else if (action == "SEARCH" &&
                 (list = dynamic_cast<UIListBtnType *>(curr)))
        {
            list->incSearchStart();
            updateForeground(list->getScreenArea());
        }
        else if (action == "NEXTSEARCH" &&
                 (list = dynamic_cast<UIListBtnType *>(curr)))
        {
            list->incSearchNext();
            updateForeground(list->getScreenArea());
        }
        else
            handled = false;
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

void ScreenSetup::updateHelpText()
{
    UIType *itm = getCurrentFocusWidget();
    QString text;
    if (!itm) return;

    if (itm == m_inactive_list)
    {

        UIListBtnTypeItem *lbt = m_inactive_list->GetItemCurrent();
        if (!lbt)
            return;

        ScreenListInfo *si = (ScreenListInfo *) lbt->getData();
        if (!si)
            return;

        QStringList sources = si->sources;

        text = tr("Add desired screen to the Active Screens list "
            "by pressing SELECT.") + "\n";
        text += lbt->text() + "\n";
        text += QString("%1: %2").arg(tr("Sources"))
                                 .arg(sources.join(", "));
    }
    else if (itm == m_active_list)
    {
        UIListBtnTypeItem *lbt = m_active_list->GetItemCurrent();
        if (!lbt)
            return;

        ScreenListInfo *si = (ScreenListInfo *) lbt->getData();
        if (!si)
            return;

        Q3DictIterator<TypeListInfo> it(si->types);
        TypeListInfo *ti = it.current();
        text += lbt->text() + "\n";
        if (si->hasUnits)
            text += tr("Units: ") +
                    (si->units == ENG_UNITS ? tr("English Units") :
                     tr("SI Units")) + "   ";
        if (!si->multiLoc && ti)
        {
            text += tr("Location: ") + (ti->location != "" ?
                    ti->location : tr("Not Defined")) + "\n";
            text += tr("Source: " ) +
                    (ti->src ? ti->src->name : tr("Not Defined")) + "\n";
        }
        text += "\n" + tr("Press SELECT to ");
        if (!si->multiLoc)
            text += tr("change location; ");
        if (si->hasUnits)
            text += tr("change units; ");
        text += tr("move screen up or down; or remove screen.");
    }
//     else if (itm == m_type_list)
//     {
//         UIListBtnTypeItem *btnitm = m_type_list->GetItemCurrent();
//         if (btnitm)
//         {
//             TypeListInfo *ti = (TypeListInfo *) btnitm->getData();
//             text = tr("%1\nLocation: %2\nSource: %3\n\nPress SELECT to change Location settings")
//                 .arg(btnitm->text()).arg(ti->location != "" ? ti->location :
//                                          tr("Not Defined"))
//                 .arg(ti->src ? ti->src->name : tr("Not Defined"));
//         }
//     }

    m_help_txt->SetText(text);
}

void ScreenSetup::loadData()
{
    Q3IntDict<ScreenListInfo> active_screens;
    ScreenListInfo *si;
    TypeListInfo *ti;

    /*
     * Basic jist of this is taken from XMLParse, since we're doing some of the
     * same stuff to get a list of container elements
     */
    QString uifile = gContext->GetThemeDir() + "/weather-ui.xml";
    if (!QFile::exists(uifile))
        uifile = gContext->GetShareDir() + "themes/default/weather-ui.xml";

    if (!QFile::exists(uifile))
    {
        VERBOSE(VB_IMPORTANT, "Error locating weather-ui.xml");
        return;
    }

    QFile xml(uifile);
    QDomDocument doc;
    if (!xml.open(QIODevice::ReadOnly))
        return;
    QString msg;
    int line, col;
    if (!doc.setContent(&xml, false, &msg, &line, &col))
    {
        VERBOSE(VB_IMPORTANT, QString("Error parsing weather-ui.xml at line %1")
                                      .arg(line));
        VERBOSE(VB_IMPORTANT, QString("XML error %1")
                                      .arg(msg));
        return;
    }

    QDomElement *win = 0;
    QDomNode n = doc.documentElement().firstChild();
    while (!n.isNull() && !win)
    {
        QDomElement e = n.toElement();
        if (e.isNull())
            continue;
        if (e.tagName() == "window")
        {
            QString name = e.attribute("name");
            if (!name.isNull() && name == "weather") 
                win = &e;
        }
        n = n.nextSibling();
    }
    if (!win)
    {
        VERBOSE(VB_IMPORTANT, "weather window not found");
        return;
    }
    n = win->firstChild();
    QString tmpname;
    int context;
    QRect area;
    QStringList types;
    while (!n.isNull())
    {
        QDomElement e = n.toElement();
        if (e.tagName() == "font")
        {
            m_weather_screens.parseFont(e);
        }
        else if (e.tagName() == "container")
        {
            QString name = e.attribute("name");
            if (!name.isNull() && !name.isEmpty() && name != "startup"
                    && name != "background")
            {
                m_weather_screens.parseContainer(e, tmpname, context, area);
                types = WeatherScreen::getAllDynamicTypes(m_weather_screens.GetSet(name));
                LayerSet *set = m_weather_screens.GetSet(name);

                si = new ScreenListInfo;
                si->units = ENG_UNITS;
                si->hasUnits = !(bool) set->GetType("nounits");
                si->multiLoc = (bool) set->GetType("multilocation");
                si->types.setAutoDelete(true);

                QStringList type_strs;
                for (uint i = 0; i < types.size(); ++i)
                {
                    ti = new TypeListInfo;
                    ti->name =  types[i];
                    ti->src = 0;
                    si->types.insert(types[i], ti);
                    type_strs << types[i];
                }

                Q3PtrList<ScriptInfo> scriptList;
                // Only add a screen to the list if we have a source
                // available to satisfy the requirements.
                if (m_src_man->findPossibleSources(type_strs, scriptList))
                {
                    ScriptInfo *script;
                    for (script = scriptList.first(); script;
                                                script = scriptList.next())
                    {
                        si->sources.append(script->name);
                    }
                    UIListBtnTypeItem *itm =
                            new UIListBtnTypeItem(m_inactive_list, name);
                    itm->setData(si);
                }

            }
        }
        n = n.nextSibling();
    }

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
    db.bindValue(":HOST", gContext->GetHostName());
    if (!db.exec())
    {
        VERBOSE(VB_IMPORTANT, db.lastError().text());
        return;
    }

    while (db.next())
    {
        QString name = db.value(0).toString();
        units_t units = db.value(1).toUInt();
        QString dataitem = db.value(2).toString();
        QString location = db.value(3).toString();
        QString src = db.value(4).toString();
        uint draworder = db.value(5).toUInt();
        LayerSet *set = m_weather_screens.GetSet(name);

        ti = new TypeListInfo;
        ti->name = dataitem;
        ti->location = location;
        ti->src = m_src_man->getSourceByName(src);
        types = WeatherScreen::getAllDynamicTypes(m_weather_screens.GetSet(name));

        if (!active_screens.find(draworder))
        {
            UIListBtnTypeItem *itm = new UIListBtnTypeItem(m_active_list, name);
            si = new ScreenListInfo;
            si->units = units;
            si->types.setAutoDelete(true);

            // Only insert types meant for this container
            for (QStringList::Iterator type_i = types.begin(); type_i != types.end(); ++type_i )
            {
                if(*type_i == dataitem)
                {
                    si->types.insert(dataitem, ti);
                }
            }

            si->hasUnits = !(bool) set->GetType("nounits");
            si->multiLoc = (bool) set->GetType("multilocation");
            itm->setData(si);
            active_screens.insert(draworder, si);
        }
        else
        {
            si = active_screens[draworder];
            for (QStringList::Iterator type_i = types.begin(); type_i != types.end(); ++type_i )
            {
                if(*type_i == dataitem)
                {
                    si->types.insert(dataitem, ti);
                }
            }
        }
    }
}

inline QString format_msg(
    const QStringList &notDefined, uint rows, uint columns)
{
    const QString etc = QObject::tr("etc...");
    uint elen = etc.length();
    QStringList lines;
    lines += "";
    QStringList::iterator oit = lines.begin();
    QStringList::const_iterator iit = notDefined.begin();
    while (iit != notDefined.end())
    {
        QStringList::const_iterator nit = iit;
        nit++;

        uint olen = (*oit).length();
        uint ilen = (*iit).length();

        if (lines.size() >= rows)
        {
            if (((olen + 2 + ilen + 2 + elen) < columns) ||
                (((olen + 2 + ilen) < columns) && (nit == notDefined.end())))
            {
                *oit += ", " + *iit;
            }
            else
            {
                *oit += ", " + etc;
                nit = notDefined.end();
            }
        }
        else
        {
            if ((olen + 2 + ilen) < columns)
            {
                *oit += ", " + *iit;
            }
            else
            {
                *oit += ",";
                lines += "";
                oit++;
                *oit += *iit;
            }
        }

        iit = nit;
    }

    return lines.join("\n").mid(2);
}

void ScreenSetup::saveData()
{
    // check if all active screens have sources/locations defined
    QStringList notDefined;
    Q3PtrListIterator<UIListBtnTypeItem> screens = m_active_list->GetIterator();
    if (!screens)
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), "No Screens",
                tr("No Active Screens are defined.  Define atleast one "
                    "before continuing."));
        return;
    }

    while (screens)
    {
        UIListBtnTypeItem *itm = *screens;
        ScreenListInfo *si = (ScreenListInfo *) itm->getData();
        Q3DictIterator<TypeListInfo> it(si->types);
        for (; it.current(); ++it)
        {
            TypeListInfo *ti = it.current();
            if (!ti->src)
                notDefined << ti->name;
        }
        ++screens;
    }

    if (notDefined.size())
    {
        QString msg = tr("Can not proceed, the following data "
                         "items do not have sources defined:\n");
        msg += format_msg(notDefined, 1, 400);
        MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                  "Undefined Sources", msg);
        return;
    }

    MSqlQuery db(MSqlQuery::InitCon());
    MSqlQuery db2(MSqlQuery::InitCon());
    QString query = "DELETE FROM weatherscreens WHERE hostname=:HOST";
    db.prepare(query);
    db.bindValue(":HOST", gContext->GetHostName());
    db.exec();

    query = "INSERT into weatherscreens (draworder, container, units, hostname) "
            "VALUES (:DRAW, :CONT, :UNITS, :HOST);";
    db.prepare(query);
    Q3PtrListIterator<UIListBtnTypeItem> an_it = m_active_list->GetIterator();
    int draworder = 0;
    while (an_it)
    {
        UIListBtnTypeItem *itm = *an_it;
        ScreenListInfo *si = (ScreenListInfo *) itm->getData();
        db.bindValue(":DRAW", draworder);
        db.bindValue(":CONT", itm->text());
        db.bindValue(":UNITS", si->units);
        db.bindValue(":HOST", gContext->GetHostName());
        if (db.exec())
        {
            // TODO see comment in dbcheck.cpp for way to improve
            QString query2 = "SELECT screen_id FROM weatherscreens "
                    "WHERE draworder = :DRAW AND hostname = :HOST;";
            db2.prepare(query2);
            db2.bindValue(":DRAW", draworder);
            db2.bindValue(":HOST", gContext->GetHostName());
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
            Q3DictIterator<TypeListInfo> it(si->types);
            TypeListInfo *ti;
            for (; it.current(); ++it)
            {
                ti = it.current();
                db2.bindValue(":LOC", ti->location);
                db2.bindValue(":ITEM", ti->name);
                db2.bindValue(":SCREENID", screen_id);
                db2.bindValue(":SRCID", ti->src->id);
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

        ++an_it;
        ++draworder;
    }

    accept();
}

typedef QMap<DialogCode, QString> CommandMap;

static DialogCode add_button(QStringList   &buttons,
                             CommandMap    &commands,
                             const QString &button_text,
                             const QString &command)
{
    int idx = buttons.size();
    buttons += button_text;
    commands[(DialogCode)((int)kDialogCodeButton0 + idx)] = command;

    return (DialogCode)((int)kDialogCodeButton0 + idx);
}

void ScreenSetup::doListSelect(UIListBtnType *list,
                               UIListBtnTypeItem *selected)
{
    if (!selected)
        return;

    QString txt = selected->text();
    if (list == m_active_list)
    {
        ScreenListInfo *si = (ScreenListInfo *) selected->getData();
        QStringList buttons;
        CommandMap commands;

        if (!si->multiLoc)
            add_button(buttons, commands, tr("Change Location"), "change_loc");

        if (si->hasUnits)
            add_button(buttons, commands, tr("Change Units"), "change_units");

        add_button(buttons, commands, tr("Move Up"),   "move_up");
        add_button(buttons, commands, tr("Move Down"), "move_down");
        add_button(buttons, commands, tr("Remove"),    "remove");

        DialogCode cancelbtn =
            add_button(buttons, commands, tr("Cancel"), "cancel");
        commands[kDialogCodeRejected] = "cancel";

        DialogCode res = MythPopupBox::ShowButtonPopup(
            gContext->GetMainWindow(), "Manipulate Screen",
            tr("Action to take on screen ") + selected->text(),
            buttons, cancelbtn);

        QString cmd = commands[res];
        if (cmd == "change_loc")
        {
            doLocationDialog(si, true);
        }
        else if (cmd == "change_units")
        {
            showUnitsPopup(selected->text(),
                           (ScreenListInfo *) selected->getData());
            updateHelpText();
        }
        else if (cmd == "move_up")
        {
            list->MoveItemUpDown(selected, true);
        }
        else if (cmd == "move_down")
        {
            list->MoveItemUpDown(selected, false);
        }
        else if (cmd == "remove")
        {
            deleteScreen(list);
        }
    }
    else if (list == m_inactive_list)
    {
        ScreenListInfo *si = (ScreenListInfo *) selected->getData();
        QStringList type_strs;
        Q3Dict<TypeListInfo> types;
        Q3DictIterator<TypeListInfo> it(si->types);
        for (; it.current(); ++it)
        {
            TypeListInfo *newti = new TypeListInfo(*it.current());
            types.insert(it.currentKey(), newti);
            type_strs << it.currentKey();
        }
        bool hasUnits = si->hasUnits;
        bool multiLoc = si->multiLoc;

        Q3PtrList<ScriptInfo> tmp;
        if (m_src_man->findPossibleSources(type_strs, tmp))
        {
            ScreenListInfo *newsi = new ScreenListInfo(*si);
            // FIXME: this seems bogus
            newsi->types.setAutoDelete(true);

            if (!list->GetCount())
            {
                list->allowFocus(false);
                nextPrevWidgetFocus(true);
            }
            if (hasUnits)
                showUnitsPopup(selected->text(), newsi);

            if (!doLocationDialog(newsi, true))
                return;

            UIListBtnTypeItem *itm = new UIListBtnTypeItem(m_active_list, txt);
            itm->setDrawArrow(multiLoc);
            itm->setData(newsi);
            if (m_active_list->GetCount())
                m_active_list->allowFocus(true);
        }
        else
        {
            MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                "Add Screen Error",
                tr("Screen cannot be used, not all required data "
                   "is supplied by existing sources"));
        }
    }
//     else if (list == m_type_list)
//     {
//         doLocationDialog((ScreenListInfo *) m_active_list->GetItemCurrent()->getData(), false);
//     }
}

bool ScreenSetup::doLocationDialog(ScreenListInfo *si,  bool alltypes)
{
    /*
     * If its alltypes, we round up all types for this screen,
     * if its not, just the seleted item in m_type_list
     */
    QStringList types;
    Q3PtrList<TypeListInfo> infos;
    if (alltypes)
    {
        Q3DictIterator<TypeListInfo> it(si->types);
        for (; it.current(); ++it)
        {
            TypeListInfo *ti = it.current();
            infos.append(ti);
            types << ti->name;
        }
    }
//     else
//     {
//         TypeListInfo *ti =
//                 (TypeListInfo *) m_type_list->GetItemCurrent()->getData();
//         infos.append(ti);
//         types << ti->name;
//     }
    QString loc;
    ScriptInfo *src = 0;
    if (showLocationPopup(types, loc, src))
    {
        for (TypeListInfo *ti = infos.first(); ti; ti = infos.next())
        {
            ti->location = loc;
            ti->src = src;
        }
        updateHelpText();

        return true;
    }
    else
        return false;
}

void ScreenSetup::activeListItemSelected(UIListBtnTypeItem *itm)
{
    if (!itm)
        itm = m_active_list->GetItemCurrent();
    if (!itm)
        return;

    ScreenListInfo *si = (ScreenListInfo *) itm->getData();
    if (!si)
        return;

    Q3Dict<TypeListInfo> types = si->types;
//     m_type_list->Reset();
//    UITextType *txt = getUITextType("typelbl");
    if (si->multiLoc)
    {
//         QDictIterator<TypeListInfo> it(si->types);
//         for (; it.current(); ++it)
//         {
//             UIListBtnTypeItem *item = new UIListBtnTypeItem(m_type_list,
//                                                            it.current()->name);
//             item->setData(it.current());
//         }
// 
//         if (txt) txt->show();
//         m_type_list->show();
//         m_type_list->allowFocus(true);
    }
    else
    {
//         if (txt) txt->hide();
//        m_type_list->hide();
//        m_type_list->allowFocus(false);
    }
    updateForeground();
}

bool ScreenSetup::showUnitsPopup(const QString &name, ScreenListInfo *si)
{
    if (!si) return false;

    units_t *units = &si->units;
    QStringList unitsBtns;
    unitsBtns << tr("English Units") << tr("SI Units");
    DialogCode ret = MythPopupBox::ShowButtonPopup(
        gContext->GetMainWindow(), "Change Units",
        tr("Select units for screen ") + name, unitsBtns,
        *units == ENG_UNITS ? kDialogCodeButton0 : kDialogCodeButton1);

    switch (ret)
    {
        case kDialogCodeButton0:
            *units = ENG_UNITS;
            break;
        case kDialogCodeButton1:
            *units = SI_UNITS;
            break;
        default:
            return false;
    }
    return true;
}

bool ScreenSetup::showLocationPopup(QStringList types, QString &loc,
                                    ScriptInfo *&src)
{
    LocationDialog dlg(gContext->GetMainWindow(), types, m_src_man);
    if (dlg.exec() == QDialog::Accepted)
    {
        loc = dlg.getLocation();
        src = dlg.getSource();
        return true;
    }

    loc = QString();
    src = NULL;
    return false;
}

void ScreenSetup::cursorUp(UIType *curr)
{
    UIListBtnType *list = dynamic_cast<UIListBtnType *>(curr);
    if (list)
    {
        int index = list->GetItemPos(list->GetItemCurrent());
        if (index > 0)
        {
            list->MoveUp(UIListBtnType::MoveItem);
            updateForeground();
        }
        else
            nextPrevWidgetFocus(false);
    }
    else
        nextPrevWidgetFocus(false);
}

void ScreenSetup::cursorDown(UIType *curr)
{
    UIListBtnType *list = dynamic_cast<UIListBtnType *>(curr);
    if (list)
    {
        int index = list->GetItemPos(list->GetItemCurrent());
        if (index != list->GetCount() - 1)
        {
            list->MoveDown(UIListBtnType::MoveItem);
            updateForeground();
        }
        else
            nextPrevWidgetFocus(true);
    }
    else
        nextPrevWidgetFocus(true);
}

void ScreenSetup::deleteScreen(UIListBtnType *list)
{

    if (list->GetItemCurrent())
        delete list->GetItemCurrent();

    if (!list->GetCount())
    {
        nextPrevWidgetFocus(false);
        list->allowFocus(false);
//         m_type_list->hide();
//         m_type_list->allowFocus(false);
//         UITextType *txt = getUITextType("typelbl");
//         if (txt) txt->hide();
    }

}

void ScreenSetup::cursorSelect(UIType *curr)
{
    UIListBtnType *list = dynamic_cast<UIListBtnType *>(curr);
    if (list)
    {
        doListSelect(list, list->GetItemCurrent());
        updateForeground();
    }

    if (curr == m_finish_btn)
        m_finish_btn->push();
}

void ScreenSetup::cursorRight(UIType *curr)
{
    if (curr == m_active_list)
    {
        UIListBtnTypeItem *itm = m_active_list->GetItemCurrent();
        if (((ScreenListInfo *) itm->getData())->multiLoc)
        {
            buildFocusList();
            nextPrevWidgetFocus(true);
        }
    }
}

void ScreenSetup::cursorLeft(UIType *curr)
{
//     if (curr == m_type_list)
//     {
//         nextPrevWidgetFocus(false);
//     }
}

///////////////////////////////////////////////////////////////////////

SourceSetup::SourceSetup(MythMainWindow *parent) :
    MythThemedDialog(parent, "source-setup", "weather-", "Source Setup")
{
    wireUI();

    buildFocusList();
    assignFirstFocus();
}

SourceSetup::~SourceSetup()
{
    delete m_update_spinbox;
    delete m_retrieve_spinbox;
    Q3PtrListIterator<UIListBtnTypeItem> it = m_src_list->GetIterator();
    UIListBtnTypeItem *itm;
    while ((itm = it.current()))
    {
        if (itm->getData())
            delete (SourceListInfo *) itm->getData();
        ++it;
    }
}

void SourceSetup::wireUI()
{
    m_src_list = getUIListBtnType("srclist");
    if (!m_src_list)
    {
        VERBOSE(VB_IMPORTANT, "error loading srclist");
    }
    else
    {
        connect(m_src_list, SIGNAL(itemSelected(UIListBtnTypeItem *)),
                this, SLOT(sourceListItemSelected(UIListBtnTypeItem *)));
        connect(m_src_list, SIGNAL(takingFocus()),
                this, SLOT(sourceListItemSelected()));
    }

    UIBlackHoleType *blckhl = getUIBlackHoleType("update_spinbox");
    if (!blckhl)
    {
        VERBOSE(VB_IMPORTANT, "error loading update_spinbox");
    }
    else
    {
        blckhl->allowFocus(true);
        m_update_spinbox = new WeatherSpinBox(this);
        m_update_spinbox->setRange(10, 600);
        m_update_spinbox->setLineStep(1);
        m_update_spinbox->setFont(gContext->GetMediumFont());
        m_update_spinbox->setFocusPolicy(Qt::NoFocus);
        m_update_spinbox->setGeometry(blckhl->getScreenArea());
        connect(blckhl, SIGNAL(takingFocus()), m_update_spinbox,
                SLOT(setFocus()));
        connect(blckhl, SIGNAL(loosingFocus()), m_update_spinbox,
                SLOT(clearFocus()));
        connect(blckhl, SIGNAL(loosingFocus()), this,
                SLOT(updateSpinboxUpdate()));
    }

    blckhl = getUIBlackHoleType("retrieve_spinbox");
    if (!blckhl)
    {
        VERBOSE(VB_IMPORTANT, "error loading retrieve_spinbox");
    }
    else
    {
        blckhl->allowFocus(true);
        m_retrieve_spinbox = new WeatherSpinBox(this);
        m_retrieve_spinbox->setRange(10, 1000);
        m_retrieve_spinbox->setLineStep(1);
        m_retrieve_spinbox->setFont(gContext->GetMediumFont());
        m_retrieve_spinbox->setFocusPolicy(Qt::NoFocus);
        m_retrieve_spinbox->setGeometry(blckhl->getScreenArea());

        connect(blckhl, SIGNAL(takingFocus()), m_retrieve_spinbox,
                SLOT(setFocus()));
        connect(blckhl, SIGNAL(loosingFocus()), m_retrieve_spinbox,
                SLOT(clearFocus()));
        connect(blckhl, SIGNAL(loosingFocus()), this,
                SLOT(retrieveSpinboxUpdate()));
    }

    m_finish_btn = getUITextButtonType("finishbutton");
    if (m_finish_btn)
    {
        m_finish_btn->setText(tr("Finish"));
        connect(m_finish_btn, SIGNAL(pushed()), this, SLOT(saveData()));
    }
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
    db.bindValue(":HOST", gContext->GetHostName());
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

        UIListBtnTypeItem *item =
            new UIListBtnTypeItem(m_src_list, tr(si->name));
        item->setData(si);
    }

    m_src_list->SetItemCurrent(0);

    return true;
}

void SourceSetup::saveData()
{
    SourceListInfo *si =
            (SourceListInfo *) m_src_list->GetItemCurrent()->getData();
    si->retrieve_timeout = m_update_spinbox->value();
    si->update_timeout = m_retrieve_spinbox->value();

    MSqlQuery db(MSqlQuery::InitCon());
    QString query = "UPDATE weathersourcesettings "
            "SET update_timeout = :UPDATE, retrieve_timeout = :RETRIEVE "
            "WHERE sourceid = :ID;";
    db.prepare(query);

    Q3PtrListIterator<UIListBtnTypeItem> an_it = m_src_list->GetIterator();

    while (an_it)
    {
        si = (SourceListInfo *) (*an_it)->getData();
        db.bindValue(":ID", si->id);
        db.bindValue(":UPDATE", si->update_timeout * 60);
        db.bindValue(":RETRIEVE", si->retrieve_timeout);
        if (!db.exec())
        {
            VERBOSE(VB_IMPORTANT, db.lastError().text());
            return;
        }

        ++an_it;
    }

    accept();
}

void SourceSetup::updateSpinboxUpdate()
{
    SourceListInfo *si =
            (SourceListInfo *) m_src_list->GetItemCurrent()->getData();
    si->retrieve_timeout = m_update_spinbox->value();
}

void SourceSetup::retrieveSpinboxUpdate()
{
    SourceListInfo *si =
            (SourceListInfo *) m_src_list->GetItemCurrent()->getData();
    si->update_timeout = m_retrieve_spinbox->value();
}

void SourceSetup::keyPressEvent(QKeyEvent *e)
{
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Weather", e, actions);
    UIType *curr = getCurrentFocusWidget();
    bool handled = false;

    for (uint i = 0; i < actions.size() && !handled; ++i)
    {
        handled = true;
        QString action = actions[i];
        if (action == "DOWN" )
        {
            UIListBtnType *list;
            if (curr && (list = dynamic_cast<UIListBtnType *>(curr)))
            {
                int index = list->GetItemPos(list->GetItemCurrent());
                if (index != list->GetCount() - 1)
                {
                    list->MoveDown(UIListBtnType::MoveItem);
                    updateForeground();
                }
            }
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "UP")
        {
            UIListBtnType *list;
            if (curr && (list = dynamic_cast<UIListBtnType *>(curr)))
            {
                int index = list->GetItemPos(list->GetItemCurrent());
                if (index > 0)
                {
                    list->MoveUp(UIListBtnType::MoveItem);
                    updateForeground();
                }
            }
            else
                nextPrevWidgetFocus(false);
        }
        else if (action == "SELECT")
        {
            if (curr == m_finish_btn)
                m_finish_btn->push();
        }
        else if (action == "RIGHT")
        {
            if (curr == m_src_list)
            {
                nextPrevWidgetFocus(true);
            }
        }
        else if (action == "LEFT")
        {
            if (curr == m_src_list)
            {
                nextPrevWidgetFocus(false);
            }
        }
        else
            handled = false;
    }
    if (!handled)
        MythDialog::keyPressEvent(e);
}

void SourceSetup::sourceListItemSelected(UIListBtnTypeItem *itm)
{
    if (!itm)
        itm = m_src_list->GetItemCurrent();

    if (!itm)
        return;

    SourceListInfo *si = (SourceListInfo *) itm->getData();
    if (!si)
        return;

    m_update_spinbox->setValue(si->retrieve_timeout);
    m_retrieve_spinbox->setValue(si->update_timeout);
    QString txt = tr("Author: ");
    txt += si->author;
    txt += "\n" + tr("Email: ") + si->email;
    txt += "\n" + tr("Version: ") + si->version;
    getUITextType("srcinfo")->SetText(txt);
}

///////////////////////////////////////////////////////////////////////

LocationDialog::LocationDialog(MythMainWindow *parent, QStringList types,
                               SourceManager *srcman) :
    MythThemedDialog(parent, "setup-location", "weather-", "Location Selection")
{

    m_types = types;
    m_src_man = srcman;

    wireUI();

    assignFirstFocus();
}

void LocationDialog::wireUI()
{
    m_edit = getUIRemoteEditType("loc-edit");
    m_edit->createEdit(this);
    m_list = getUIListBtnType("results");
    m_list->allowFocus(true);
    connect(m_list, SIGNAL(itemSelected(UIListBtnTypeItem *)),
            this, SLOT(itemSelected(UIListBtnTypeItem *)));
    m_btn = getUITextButtonType("searchbtn");
    connect(m_btn, SIGNAL(pushed()), this, SLOT(doSearch()));
    m_btn->setText(tr("Search"));
}

void LocationDialog::doSearch()
{
    QMap<ScriptInfo *, QStringList> result_cache;
    int numresults = 0;
    m_list->Reset();
    UITextType *resultslbl = getUITextType("numresults");

    QString searchingresults = tr("Searching ... Results: %1");

    resultslbl->SetText(searchingresults.arg(numresults));
    qApp->processEvents();

    Q3PtrList<ScriptInfo> sources;
    // if a screen makes it this far, theres at least one source for it
    m_src_man->findPossibleSources(m_types, sources);
    QString search = m_edit->getText();
    ScriptInfo *si;
    for (si = sources.first(); si; si = sources.next())
    {
        if (!result_cache.contains(si))
        {
            QStringList results = m_src_man->getLocationList(si, search);
            result_cache[si] = results;
            numresults += results.size();
            resultslbl->SetText(searchingresults.arg(numresults));
            qApp->processEvents();
        }
    }

    for (uint i = 0; i < result_cache.keys().size(); ++i)
    {
        si = result_cache.keys()[i];
        QStringList results = result_cache[si];
        QString name = si->name;
        for (uint ii = 0; ii < results.size(); ++ii)
        {
            QStringList tmp = QStringList::split("::", results[ii]);
            UIListBtnTypeItem *itm = new UIListBtnTypeItem(m_list, tmp[1]);
            ResultListInfo *ri = new ResultListInfo;
            ri->idstr = tmp[0];
            ri->src = si;
            itm->setData(ri);
        }
    }
    resultslbl->SetText(tr("Search Complete. Results: %1").arg(numresults));
    if (numresults)
    {
        m_list->allowFocus(true);
        nextPrevWidgetFocus(true);
        itemSelected(m_list->GetItemAt(0));
    }
    update();
}

void LocationDialog::keyPressEvent(QKeyEvent *e)
{
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Weather", e, actions);
    UIType *curr = getCurrentFocusWidget();
    bool handled = false;

    for (uint i = 0; i < actions.size() && !handled; ++i)
    {
        handled = true;
        QString action = actions[i];
        if (action == "DOWN")
        {
            if (curr == m_list)
            {
                if (m_list->GetItemPos(m_list->GetItemCurrent()) !=
                        m_list->GetCount() - 1)
                    m_list->MoveDown(UIListBtnType::MoveItem);
                else
                    nextPrevWidgetFocus(true);
                updateForeground(m_list->getScreenArea());
            }
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "UP")
        {
            if (curr == m_list)
            {
                if (m_list->GetItemPos(m_list->GetItemCurrent()) > 0)
                    m_list->MoveUp(UIListBtnType::MoveItem);
                else nextPrevWidgetFocus(false);
                updateForeground(m_list->getScreenArea());
            }
            else
                nextPrevWidgetFocus(false);
        }
        else if (action == "PAGEUP" && curr == m_list)
        {
            m_list->MoveUp(UIListBtnType::MovePage);
            updateForeground(m_list->getScreenArea());
        }
        else if (action == "PAGEDOWN" && curr == m_list)
        {
            m_list->MoveDown(UIListBtnType::MovePage);
            updateForeground(m_list->getScreenArea());
        }
        else if (action == "PREVVIEW" && curr == m_list)
        {
            m_list->MoveUp(UIListBtnType::MoveMax);
            updateForeground(m_list->getScreenArea());
        }
        else if (action == "NEXTVIEW" && curr == m_list)
        {
            m_list->MoveDown(UIListBtnType::MoveMax);
            updateForeground(m_list->getScreenArea());
        }
        else if (action == "SEARCH" && curr == m_list)
        {
            m_list->incSearchStart();
            updateForeground(m_list->getScreenArea());
        }
        else if (action == "NEXTSEARCH" && curr == m_list)
        {
            m_list->incSearchNext();
            updateForeground(m_list->getScreenArea());
        }
        else if (action == "SELECT")
        {
            if (curr == m_btn)
                m_btn->push();
            else if (curr == m_list)
                accept();
        }
        else
            handled = false;
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

void LocationDialog::itemSelected(UIListBtnTypeItem *itm)
{
    UITextType *txt = getUITextType("source");
    ResultListInfo *ri = (ResultListInfo *)itm->getData();
    if (ri)
        txt->SetText(tr("Source: %1").arg(ri->src->name));
}

QString LocationDialog::getLocation()
{
    UIListBtnTypeItem *itm = m_list->GetItemCurrent();
    if (!itm)
        return NULL;

    ResultListInfo *ri = (ResultListInfo *) itm->getData();

    if (!ri)
        return NULL;

    return ri->idstr;
}

ScriptInfo *LocationDialog::getSource()
{
    UIListBtnTypeItem *itm = m_list->GetItemCurrent();
    if (!itm)
        return NULL;

    ResultListInfo *ri = (ResultListInfo *) itm->getData();

    if (!ri)
        return NULL;

    return ri->src;
}
