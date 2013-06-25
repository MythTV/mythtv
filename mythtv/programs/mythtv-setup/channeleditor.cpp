// Qt
#include <QCoreApplication>

// MythTV
#include "mythprogressdialog.h"
#include "mythuibuttonlist.h"
#include "channelsettings.h"
#include "transporteditor.h"
#include "mythuicheckbox.h"
#include "mythuitextedit.h"
#include "channeleditor.h"
#include "mythdialogbox.h"
#include "mythuibutton.h"
#include "channelutil.h"
#include "importicons.h"
#include "mythlogging.h"
#include "mythuitext.h"
#include "mythwizard.h"
#include "scanwizard.h"
#include "sourceutil.h"
#include "cardutil.h"
#include "settings.h"
#include "mythdirs.h"
#include "mythdb.h"

ChannelWizard::ChannelWizard(int id, int default_sourceid)
    : ConfigurationWizard()
{
    setLabel(QObject::tr("Channel Options"));

    // Must be first.
    addChild(cid = new ChannelID());
    cid->setValue(id);

    ChannelOptionsCommon *common =
        new ChannelOptionsCommon(*cid, default_sourceid);
    addChild(common);

    ChannelOptionsFilters *filters =
        new ChannelOptionsFilters(*cid);
    addChild(filters);

    QStringList cardtypes = ChannelUtil::GetCardTypes(cid->getValue().toUInt());
    bool all_v4l = !cardtypes.empty();
    bool all_asi = !cardtypes.empty();
    for (uint i = 0; i < (uint) cardtypes.size(); i++)
    {
        all_v4l &= CardUtil::IsV4L(cardtypes[i]);
        all_asi &= cardtypes[i] == "ASI";
    }

    if (all_v4l)
        addChild(new ChannelOptionsV4L(*cid));
    else if (all_asi)
        addChild(new ChannelOptionsRawTS(*cid));
}

/////////////////////////////////////////////////////////

ChannelEditor::ChannelEditor(MythScreenStack *parent)
              : MythScreenType(parent, "channeleditor"),
    m_sourceFilter(FILTER_ALL),
    m_currentSortMode(QObject::tr("Channel Name")),
    m_currentHideMode(false),
    m_channelList(NULL), m_sourceList(NULL), m_preview(NULL),
    m_channame(NULL), m_channum(NULL), m_callsign(NULL),
    m_chanid(NULL), m_sourcename(NULL), m_compoundname(NULL)
{
}

bool ChannelEditor::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("config-ui.xml", "channeloverview", this);

    if (!foundtheme)
        return false;

    MythUIButtonList *sortList = dynamic_cast<MythUIButtonList *>(GetChild("sorting"));
    m_sourceList = dynamic_cast<MythUIButtonList *>(GetChild("source"));
    m_channelList = dynamic_cast<MythUIButtonList *>(GetChild("channels"));
    m_preview = dynamic_cast<MythUIImage *>(GetChild("preview"));
    m_channame = dynamic_cast<MythUIText *>(GetChild("name"));
    m_channum = dynamic_cast<MythUIText *>(GetChild("channum"));
    m_callsign = dynamic_cast<MythUIText *>(GetChild("callsign"));
    m_chanid = dynamic_cast<MythUIText *>(GetChild("chanid"));
    m_sourcename = dynamic_cast<MythUIText *>(GetChild("sourcename"));
    m_compoundname = dynamic_cast<MythUIText *>(GetChild("compoundname"));

    MythUIButton *deleteButton = dynamic_cast<MythUIButton *>(GetChild("delete"));
    MythUIButton *scanButton = dynamic_cast<MythUIButton *>(GetChild("scan"));
    MythUIButton *importIconButton = dynamic_cast<MythUIButton *>(GetChild("importicons"));
    MythUIButton *transportEditorButton = dynamic_cast<MythUIButton *>(GetChild("edittransport"));

    MythUICheckBox *hideCheck = dynamic_cast<MythUICheckBox *>(GetChild("nochannum"));

    if (!sortList || !m_sourceList || !m_channelList || !deleteButton ||
        !scanButton || !importIconButton || !transportEditorButton ||
        !hideCheck)
    {

        return false;
    }

    // Delete button help text
    deleteButton->SetHelpText(tr("Delete all channels on currently selected source(s)."));

    // Sort List
    new MythUIButtonListItem(sortList, tr("Channel Name"));
    new MythUIButtonListItem(sortList, tr("Channel Number"));
    connect(m_sourceList, SIGNAL(itemSelected(MythUIButtonListItem *)),
            SLOT(setSourceID(MythUIButtonListItem *)));
    sortList->SetValue(m_currentSortMode);


    // Source List
    new MythUIButtonListItem(m_sourceList,tr("All"),
                             qVariantFromValue((int)FILTER_ALL));
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT name, sourceid FROM videosource");
    if (query.exec())
    {
        while(query.next())
        {
            new MythUIButtonListItem(m_sourceList, query.value(0).toString(),
                                     query.value(1).toInt());
        }
    }
    new MythUIButtonListItem(m_sourceList,tr("(Unassigned)"),
                             qVariantFromValue((int)FILTER_UNASSIGNED));
    connect(sortList, SIGNAL(itemSelected(MythUIButtonListItem *)),
            SLOT(setSortMode(MythUIButtonListItem *)));

    // Hide/Show channels without channum checkbox
    hideCheck->SetCheckState(m_currentHideMode);
    connect(hideCheck, SIGNAL(toggled(bool)),
            SLOT(setHideMode(bool)));

    // Scan Button
    scanButton->SetHelpText(tr("Starts the channel scanner."));
    scanButton->SetEnabled(SourceUtil::IsAnySourceScanable());

    // Import Icons Button
    importIconButton->SetHelpText(tr("Starts the icon downloader"));
    importIconButton->SetEnabled(true);
    connect(importIconButton,  SIGNAL(Clicked()), SLOT(channelIconImport()));

    // Transport Editor Button
    transportEditorButton->SetHelpText(
        tr("Allows you to edit the transports directly. "
            "This is rarely required unless you are using "
            "a satellite dish and must enter an initial "
            "frequency to for the channel scanner to try."));
    connect(transportEditorButton, SIGNAL(Clicked()), SLOT(transportEditor()));

    // Other signals
    connect(m_channelList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            SLOT(edit(MythUIButtonListItem *)));
    connect(m_channelList, SIGNAL(itemSelected(MythUIButtonListItem *)),
            SLOT(itemChanged(MythUIButtonListItem *)));
    connect(scanButton, SIGNAL(Clicked()), SLOT(scan()));
    connect(deleteButton,  SIGNAL(Clicked()), SLOT(deleteChannels()));

    fillList();

    BuildFocusList();

    return true;
}

bool ChannelEditor::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
            menu();
        else if (action == "DELETE")
            del();
        else if (action == "EDIT")
            edit();
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void ChannelEditor::itemChanged(MythUIButtonListItem *item)
{
    if (!item)
        return;

    if (m_preview)
    {
        m_preview->Reset();
        QString iconpath = item->GetImageFilename();
        if (!iconpath.isEmpty())
        {
            // mythtv-setup needs direct access to channel icon dir to import.  We
            // also can't rely on the backend to be running, so access the file directly
            QString tmpIcon = GetConfDir() + "/channels/" + iconpath;
            m_preview->SetFilename(tmpIcon);
            m_preview->Load();
        }
    }

    if (m_channame)
        m_channame->SetText(item->GetText("name"));

    if (m_channum)
        m_channum->SetText(item->GetText("channum"));

    if (m_callsign)
        m_callsign->SetText(item->GetText("callsign"));

    if (m_chanid)
        m_chanid->SetText(item->GetText("chanid"));

    if (m_sourcename)
        m_sourcename->SetText(item->GetText("sourcename"));

    if (m_compoundname)
        m_compoundname->SetText(item->GetText("compoundname"));
}

void ChannelEditor::fillList(void)
{
    QString currentValue = m_channelList->GetValue();
    uint    currentIndex = qMax(m_channelList->GetCurrentPos(), 0);
    m_channelList->Reset();
    QString newchanlabel = tr("(Add New Channel)");
    MythUIButtonListItem *item = new MythUIButtonListItem(m_channelList, "");
    item->SetText(newchanlabel, "compoundname");
    item->SetText(newchanlabel, "name");

    bool fAllSources = true;

    QString querystr = "SELECT channel.name,channum,chanid,callsign,icon,"
                       "visible ,videosource.name FROM channel "
                       "LEFT JOIN videosource ON "
                       "(channel.sourceid = videosource.sourceid) ";

    if (m_sourceFilter == FILTER_ALL)
    {
        fAllSources = true;
    }
    else
    {
        querystr += QString(" WHERE channel.sourceid='%1' ")
                           .arg(m_sourceFilter);
        fAllSources = false;
    }

    if (m_currentSortMode == tr("Channel Name"))
    {
        querystr += " ORDER BY channel.name";
    }
    else if (m_currentSortMode == tr("Channel Number"))
    {
        querystr += " ORDER BY channum + 0";
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(querystr);

    uint selidx = 0, idx = 1;
    if (query.exec() && query.size() > 0)
    {
        for (; query.next() ; idx++)
        {
            QString name = query.value(0).toString();
            QString channum = query.value(1).toString();
            QString chanid = query.value(2).toString();
            QString callsign = query.value(3).toString();
            QString icon = query.value(4).toString();
            bool visible =  query.value(5).toBool();
            QString sourceid = "Unassigned";

            QString state = "normal";

            if (!visible)
                state = "disabled";

            if (!query.value(6).toString().isEmpty())
            {
                sourceid = query.value(6).toString();
                if (fAllSources && m_sourceFilter == FILTER_UNASSIGNED)
                    continue;
            }
            else
                state = "warning";

            if (channum.isEmpty() && m_currentHideMode)
                continue;

            if (name.isEmpty())
                name = "(Unnamed : " + chanid + ")";

            QString compoundname = name;

            if (m_currentSortMode == tr("Channel Name"))
            {
                if (!channum.isEmpty())
                    compoundname += " (" + channum + ")";
            }
            else if (m_currentSortMode == tr("Channel Number"))
            {
                if (!channum.isEmpty())
                    compoundname = channum + ". " + compoundname;
                else
                    compoundname = "???. " + compoundname;
            }

            if (m_sourceFilter == FILTER_ALL)
                compoundname += " (" + sourceid  + ")";

            bool sel = (chanid == currentValue);
            selidx = (sel) ? idx : selidx;
            item = new MythUIButtonListItem(m_channelList, "",
                                                     qVariantFromValue(chanid));
            item->SetText(compoundname, "compoundname");
            item->SetText(name, "name");
            item->SetText(channum, "channum");
            item->SetText(chanid, "chanid");
            item->SetText(callsign, "callsign");
            item->SetText(sourceid, "sourcename");

            // mythtv-setup needs direct access to channel icon dir to import.  We
            // also can't rely on the backend to be running, so access the file directly
            QString tmpIcon = GetConfDir() + "/channels/" + icon;
            item->SetImage(tmpIcon);
            item->SetImage(tmpIcon, "icon");

            item->DisplayState(state, "status");
        }
    }

    // Make sure we select the current item, or the following one after
    // deletion, with wrap around to "(New Channel)" after deleting last item.
    m_channelList->SetItemCurrent((!selidx && currentIndex < idx) ? currentIndex : selidx);
}

void ChannelEditor::setSortMode(MythUIButtonListItem *item)
{
    if (!item)
        return;

    QString sortName = item->GetText();

    if (m_currentSortMode != sortName)
    {
        m_currentSortMode = sortName;
        fillList();
    }
}

void ChannelEditor::setSourceID(MythUIButtonListItem *item)
{
    if (!item)
        return;

    QString sourceName = item->GetText();
    int sourceID = item->GetData().toInt();

    if (m_sourceFilter != sourceID)
    {
        m_sourceFilterName = sourceName;
        m_sourceFilter = sourceID;
        fillList();
    }
}

void ChannelEditor::setHideMode(bool hide)
{
    if (m_currentHideMode != hide)
    {
        m_currentHideMode = hide;
        fillList();
    }
}

void ChannelEditor::del()
{
    MythUIButtonListItem *item = m_channelList->GetItemCurrent();

    if (!item)
        return;

    QString message = tr("Delete channel '%1'?").arg(item->GetText("name"));

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythConfirmationDialog *dialog = new MythConfirmationDialog(popupStack, message, true);

    if (dialog->Create())
    {
        dialog->SetData(qVariantFromValue(item));
        dialog->SetReturnEvent(this, "delsingle");
        popupStack->AddScreen(dialog);
    }
    else
        delete dialog;

}

void ChannelEditor::deleteChannels(void)
{
    const QString currentLabel = m_sourceList->GetValue();

    bool del_all = m_sourceFilter == FILTER_ALL;
    bool del_nul = m_sourceFilter == FILTER_UNASSIGNED;

    QString message =
        (del_all) ? tr("Delete ALL channels?") :
        ((del_nul) ? tr("Delete all unassigned channels?") :
            tr("Delete all channels on %1?").arg(currentLabel));

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythConfirmationDialog *dialog = new MythConfirmationDialog(popupStack, message, true);

    if (dialog->Create())
    {
        dialog->SetReturnEvent(this, "delall");
        popupStack->AddScreen(dialog);
    }
    else
        delete dialog;
}

void ChannelEditor::edit(MythUIButtonListItem *item)
{
    if (!item)
        item = m_channelList->GetItemCurrent();

    if (!item)
        return;

    int chanid = item->GetData().toInt();
    ChannelWizard cw(chanid, m_sourceFilter);
    cw.exec();

    fillList();
}

void ChannelEditor::menu()
{
    MythUIButtonListItem *item = m_channelList->GetItemCurrent();

    if (!item)
        return;

    int chanid = item->GetData().toInt();
    if (chanid == 0)
       edit(item);
    else
    {
        QString label = tr("Channel Options");

        MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

        MythDialogBox *menu = new MythDialogBox(label, popupStack, "chanoptmenu");

        if (menu->Create())
        {
            menu->SetReturnEvent(this, "channelopts");

            menu->AddButton(tr("Edit"));
//             if ()
//                 menu->AddButton(tr("Set Hidden"));
//             else
//                 menu->AddButton(tr("Set Visible"));
            menu->AddButton(tr("Delete"));

            popupStack->AddScreen(menu);
        }
        else
        {
            delete menu;
            return;
        }
    }
}

void ChannelEditor::scan(void)
{
#ifdef USING_BACKEND
    ScanWizard *scanwizard = new ScanWizard(m_sourceFilter);
    scanwizard->exec(false, true);
    scanwizard->deleteLater();

    fillList();
#else
    LOG(VB_GENERAL, LOG_ERR,
        "You must compile the backend to be able to scan for channels");
#endif
}

void ChannelEditor::transportEditor(void)
{
    TransportListEditor *editor = new TransportListEditor(m_sourceFilter);
    editor->exec();
    editor->deleteLater();

    fillList();
}

void ChannelEditor::channelIconImport(void)
{
    if (m_channelList->GetCount() == 0)
    {
        ShowOkPopup(tr("Add some channels first!"));
        return;
    }

    int channelID = 0;
    MythUIButtonListItem *item = m_channelList->GetItemCurrent();
    if (item)
        channelID = item->GetData().toInt();

    // Get selected channel name from database
    QString querystr = QString("SELECT channel.name FROM channel "
                               "WHERE chanid='%1'").arg(channelID);
    QString channelname;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(querystr);

    if (query.exec() && query.next())
    {
        channelname = query.value(0).toString();
    }

    QString label = tr("Icon Import Options");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythDialogBox *menu = new MythDialogBox(label, popupStack, "iconoptmenu");

    if (menu->Create())
    {
        menu->SetReturnEvent(this, "iconimportopt");

        menu->AddButton(tr("Download all icons..."));
        menu->AddButton(tr("Rescan for missing icons..."));
        if (!channelname.isEmpty())
            menu->AddButton(tr("Download icon for %1").arg(channelname),
                            channelname);

        popupStack->AddScreen(menu);
    }
    else
    {
        delete menu;
        return;
    }
}

void ChannelEditor::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        QString resultid= dce->GetId();
        int buttonnum  = dce->GetResult();

        if (resultid == "channelopts")
        {
            switch (buttonnum)
            {
                case 0 :
                    edit(m_channelList->GetItemCurrent());
                    break;
                case 1 :
                    del();
                    break;
            }
        }
        else if (resultid == "delsingle" && buttonnum == 1)
        {
            MythUIButtonListItem *item =
                    qVariantValue<MythUIButtonListItem *>(dce->GetData());
            if (!item)
                return;
            uint chanid = item->GetData().toUInt();
            if (chanid && ChannelUtil::DeleteChannel(chanid))
                m_channelList->RemoveItem(item);
        }
        else if (resultid == "delall" && buttonnum == 1)
        {
            bool del_all = m_sourceFilter == FILTER_ALL;
            bool del_nul = m_sourceFilter == FILTER_UNASSIGNED;

            MSqlQuery query(MSqlQuery::InitCon());
            if (del_all)
            {
                query.prepare("TRUNCATE TABLE channel");
            }
            else if (del_nul)
            {
                query.prepare("SELECT sourceid "
                "FROM videosource "
                "GROUP BY sourceid");

                if (!query.exec() || !query.isActive())
                {
                    MythDB::DBError("ChannelEditor Delete Channels", query);
                    return;
                }

                QString tmp = "";
                while (query.next())
                    tmp += "'" + query.value(0).toString() + "',";

                if (tmp.isEmpty())
                {
                    query.prepare("TRUNCATE TABLE channel");
                }
                else
                {
                    tmp = tmp.left(tmp.length() - 1);
                    query.prepare(QString("DELETE FROM channel "
                    "WHERE sourceid NOT IN (%1)").arg(tmp));
                }
            }
            else
            {
                query.prepare("DELETE FROM channel "
                "WHERE sourceid = :SOURCEID");
                query.bindValue(":SOURCEID", m_sourceFilter);
            }

            if (!query.exec())
                MythDB::DBError("ChannelEditor Delete Channels", query);

            fillList();
        }
        else if (resultid == "iconimportopt")
        {
            MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

            ImportIconsWizard *iconwizard;

            QString channelname = dce->GetData().toString();

            switch (buttonnum)
            {
                case 0 : // Import all icons
                    iconwizard = new ImportIconsWizard(mainStack, false);
                    break;
                case 1 : // Rescan for missing
                    iconwizard = new ImportIconsWizard(mainStack, true);
                    break;
                case 2 : // Import a single channel icon
                    iconwizard = new ImportIconsWizard(mainStack, true,
                                                       channelname);
                    break;
                default:
                    return;
            }

            if (iconwizard->Create())
            {
                connect(iconwizard, SIGNAL(Exiting()), SLOT(fillList()));
                mainStack->AddScreen(iconwizard);
            }
            else
            {
                delete iconwizard;

                if (buttonnum == 2)
                {
                    // Reload the list since ImportIconsWizard::Create() will return false
                    // if it automatically downloaded an icon for the selected channel
                    fillList();
                }
            }
        }
    }
}
