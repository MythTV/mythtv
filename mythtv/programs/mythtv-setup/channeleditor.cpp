// Qt
#include <QCoreApplication>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlogging.h"
#include "libmythtv/cardutil.h"
#include "libmythtv/channelsettings.h"
#include "libmythtv/channelutil.h"
#include "libmythtv/restoredata.h"
#include "libmythtv/scanwizard.h"
#include "libmythtv/sourceutil.h"
#include "libmythtv/transporteditor.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythprogressdialog.h"
#include "libmythui/mythuibutton.h"
#include "libmythui/mythuibuttonlist.h"
#include "libmythui/mythuicheckbox.h"
#include "libmythui/mythuiimage.h"
#include "libmythui/mythuitext.h"
#include "libmythui/mythuitextedit.h"

// MythTV Setup
#include "channeleditor.h"
#include "importicons.h"

#define LOC QString("ChannelEditor: ")

ChannelWizard::ChannelWizard(int id, int default_sourceid)
{
    setLabel(tr("Channel Options"));

    // Must be first.
    addChild(m_cid = new ChannelID());
    m_cid->setValue(id);

    QStringList cardtypes = ChannelUtil::GetInputTypes(m_cid->getValue().toUInt());

    // For a new channel the list will be empty so get it this way.
    if (cardtypes.empty())
        cardtypes = CardUtil::GetInputTypeNames(default_sourceid);

    bool all_v4l = !cardtypes.empty();
    bool all_asi = !cardtypes.empty();
    for (const QString& cardtype : std::as_const(cardtypes))
    {
        all_v4l &= CardUtil::IsV4L(cardtype);
        all_asi &= cardtype == "ASI";
    }

    auto *common = new ChannelOptionsCommon(*m_cid, default_sourceid,!all_v4l);
    addChild(common);

    auto *iptv = new ChannelOptionsIPTV(*m_cid);
    addChild(iptv);

    auto *filters = new ChannelOptionsFilters(*m_cid);
    addChild(filters);

    if (all_v4l)
        addChild(new ChannelOptionsV4L(*m_cid));
    else if (all_asi)
        addChild(new ChannelOptionsRawTS(*m_cid));
}

/////////////////////////////////////////////////////////

ChannelEditor::ChannelEditor(MythScreenStack *parent)
              : MythScreenType(parent, "channeleditor"),
    m_currentSortMode(QCoreApplication::translate("(Common)", "Channel Number"))
{
}

bool ChannelEditor::Create()
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("config-ui.xml", "channeloverview", this);
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
    m_frequency = dynamic_cast<MythUIText *>(GetChild("frequency"));
    m_transportid = dynamic_cast<MythUIText *>(GetChild("transportid"));
    m_compoundname = dynamic_cast<MythUIText *>(GetChild("compoundname"));

    MythUIButton *deleteButton = dynamic_cast<MythUIButton *>(GetChild("delete"));
    MythUIButton *scanButton = dynamic_cast<MythUIButton *>(GetChild("scan"));
    MythUIButton *restoreDataButton = dynamic_cast<MythUIButton *>(GetChild("restoredata"));
    MythUIButton *importIconButton = dynamic_cast<MythUIButton *>(GetChild("importicons"));
    MythUIButton *transportEditorButton = dynamic_cast<MythUIButton *>(GetChild("edittransport"));

    MythUICheckBox *hideCheck = dynamic_cast<MythUICheckBox *>(GetChild("nochannum"));

    if (!sortList || !m_sourceList || !m_channelList || !deleteButton ||
        !scanButton || !importIconButton || !transportEditorButton ||
        !hideCheck)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "One or more buttons not found in theme.");
        return false;
    }

    // Delete button help text
    deleteButton->SetHelpText(tr("Delete all channels on currently selected video source."));

    // Sort List
    new MythUIButtonListItem(sortList, tr("Channel Number"));
    new MythUIButtonListItem(sortList, tr("Channel Name"));
    new MythUIButtonListItem(sortList, tr("Service ID"));
    new MythUIButtonListItem(sortList, tr("Frequency"));
    new MythUIButtonListItem(sortList, tr("Transport ID"));
    new MythUIButtonListItem(sortList, tr("Video Source"));
    connect(m_sourceList, &MythUIButtonList::itemSelected,
            this, &ChannelEditor::setSourceID);
    sortList->SetValue(m_currentSortMode);

    // Source List
    new MythUIButtonListItem(m_sourceList,tr("All"),
                             QVariant::fromValue((int)FILTER_ALL));
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
                             QVariant::fromValue((int)FILTER_UNASSIGNED));
    connect(sortList, &MythUIButtonList::itemSelected,
            this, &ChannelEditor::setSortMode);

    // Hide/Show channels without channum checkbox
    hideCheck->SetCheckState(m_currentHideMode);
    connect(hideCheck, &MythUICheckBox::toggled,
            this, &ChannelEditor::setHideMode);

    // Scan Button
    scanButton->SetHelpText(tr("Starts the channel scanner."));
    scanButton->SetEnabled(SourceUtil::IsAnySourceScanable());

    // Restore Data button
    if (restoreDataButton)
    {
        restoreDataButton->SetHelpText(tr("Restore Data from deleted channels."));
        restoreDataButton->SetEnabled(true);
        connect(restoreDataButton, &MythUIButton::Clicked, this, &ChannelEditor::restoreData);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Button \"Restore Data\" not found in theme.");
    }

    // Import Icons Button
    importIconButton->SetHelpText(tr("Starts the icon downloader"));
    importIconButton->SetEnabled(true);
    connect(importIconButton,  &MythUIButton::Clicked, this, &ChannelEditor::channelIconImport);

    // Transport Editor Button
    transportEditorButton->SetHelpText(
        tr("Allows you to edit the transports directly. "
            "This is rarely required unless you are using "
            "a satellite dish and must enter an initial "
            "frequency to for the channel scanner to try."));
    connect(transportEditorButton, &MythUIButton::Clicked, this, &ChannelEditor::transportEditor);

    // Other signals
    connect(m_channelList, &MythUIButtonList::itemClicked,
            this, &ChannelEditor::edit);
    connect(m_channelList, &MythUIButtonList::itemSelected,
            this, &ChannelEditor::itemChanged);
    connect(scanButton, &MythUIButton::Clicked, this, &ChannelEditor::scan);
    connect(deleteButton,  &MythUIButton::Clicked, this, &ChannelEditor::deleteChannels);

    fillList();

    BuildFocusList();

    return true;
}

bool ChannelEditor::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
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
            // also can't rely on the backend to be running, so access the file directly.
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

    if (m_serviceid)
        m_serviceid->SetText(item->GetText("serviceid"));

    if (m_frequency)
        m_frequency->SetText(item->GetText("frequency"));

    if (m_transportid)
        m_transportid->SetText(item->GetText("transportid"));

    if (m_sourcename)
        m_sourcename->SetText(item->GetText("sourcename"));

    if (m_compoundname)
        m_compoundname->SetText(item->GetText("compoundname"));
}

void ChannelEditor::fillList(void)
{
    // Index of current item and offset to the first displayed item
    int currentIndex = std::max(m_channelList->GetCurrentPos(), 0);
    int currentTopItemPos = std::max(m_channelList->GetTopItemPos(), 0);
    int posOffset = currentIndex - currentTopItemPos;

    // Identify the current item in the list with the new sorting order
    MythUIButtonListItem *currentItem = m_channelList->GetItemCurrent();
    QString currentServiceID;
    QString currentTransportID;
    QString currentSourceName;
    if (currentItem)
    {
        currentServiceID = currentItem->GetText("serviceid");
        currentTransportID = currentItem->GetText("transportid");
        currentSourceName = currentItem->GetText("sourcename");
    }

    m_channelList->Reset();
    QString newchanlabel = tr("(Add New Channel)");
    auto *item = new MythUIButtonListItem(m_channelList, "");
    item->SetText(newchanlabel, "compoundname");
    item->SetText(newchanlabel, "name");

    bool fAllSources = true;

    QString querystr = "SELECT channel.name, channum, chanid, callsign, icon, "
                       "channel.visible, videosource.name, serviceid, "
                       "dtv_multiplex.frequency, dtv_multiplex.polarity, "
                       "dtv_multiplex.transportid, dtv_multiplex.mod_sys, "
                       "channel.sourceid FROM channel "
                       "LEFT JOIN videosource ON "
                       "(channel.sourceid = videosource.sourceid) "
                       "LEFT JOIN dtv_multiplex ON "
                       "(channel.mplexid = dtv_multiplex.mplexid) "
                       "WHERE deleted IS NULL ";

    if (m_sourceFilter == FILTER_ALL)
    {
        fAllSources = true;
    }
    else
    {
        querystr += QString("AND channel.sourceid='%1' ")
                           .arg(m_sourceFilter);
        fAllSources = false;
    }

    if (m_currentSortMode == tr("Channel Name"))
    {
        querystr += " ORDER BY channel.name, dtv_multiplex.transportid, serviceid, channel.sourceid";
    }
    else if (m_currentSortMode == tr("Channel Number"))
    {
        querystr += " ORDER BY channum + 0, SUBSTRING_INDEX(channum, '_', -1) + 0, dtv_multiplex.transportid, serviceid, channel.sourceid";
    }
    else if (m_currentSortMode == tr("Service ID"))
    {
        querystr += " ORDER BY serviceid, dtv_multiplex.transportid, channel.sourceid";
    }
    else if (m_currentSortMode == tr("Frequency"))
    {
        querystr += " ORDER BY dtv_multiplex.frequency, dtv_multiplex.transportid, serviceid, channel.sourceid";
    }
    else if (m_currentSortMode == tr("Transport ID"))
    {
        querystr += " ORDER BY dtv_multiplex.transportid, serviceid";
    }
    else if (m_currentSortMode == tr("Video Source"))
    {
        querystr += " ORDER BY channel.sourceid, dtv_multiplex.transportid, serviceid";
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(querystr);

    int selidx = 0;
    int idx = 1;
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
            QString serviceid = query.value(7).toString();
            QString frequency = query.value(8).toString();
            QString polarity = query.value(9).toString().toUpper();
            QString transportid = query.value(10).toString();
            QString mod_sys = query.value(11).toString();
            QString sourcename = "Unassigned";

            // Add polarity for satellite frequencies
            if (mod_sys.startsWith("DVB-S"))
            {
                frequency += polarity;
            }

            QString state = "normal";

            if (!visible)
                state = "disabled";

            if (!query.value(6).toString().isEmpty())
            {
                sourcename = query.value(6).toString();
                if (fAllSources && m_sourceFilter == FILTER_UNASSIGNED)
                    continue;
            }
            else
            {
                state = "warning";
            }

            // Also hide channels that are not visible
            if ((!visible || channum.isEmpty()) && m_currentHideMode)
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
                compoundname += " (" + sourcename  + ")";

            bool sel = ((serviceid   == currentServiceID  ) &&
                        (transportid == currentTransportID) &&
                        (sourcename  == currentSourceName ));
            selidx = (sel) ? idx : selidx;

            item = new MythUIButtonListItem(m_channelList, "", QVariant::fromValue(chanid));
            item->SetText(compoundname, "compoundname");
            item->SetText(chanid, "chanid");
            item->SetText(channum, "channum");
            item->SetText(name, "name");
            item->SetText(callsign, "callsign");
            item->SetText(serviceid, "serviceid");
            item->SetText(frequency, "frequency");
            item->SetText(transportid, "transportid");
            item->SetText(sourcename, "sourcename");

            // mythtv-setup needs direct access to channel icon dir to import.  We
            // also can't rely on the backend to be running, so access the file directly.
            QString tmpIcon = GetConfDir() + "/channels/" + icon;
            item->SetImage(tmpIcon);
            item->SetImage(tmpIcon, "icon");

            item->DisplayState(state, "status");
        }
    }

    // Make sure we select the current item, or the following one after
    // deletion, with wrap around to "(New Channel)" after deleting last item.
    // Preserve the position on the screen of the current item as much as possible
    // when the sorting order is changed.

    // Index of current item
    int newPosition = (!selidx && currentIndex < idx) ? currentIndex : selidx;

    // Index of item on top line
    int newTopPosition = newPosition - posOffset;

    // Correction for number of items from first item to current item.
    newTopPosition = std::max(newTopPosition, 0);

    // Correction for number of items from current item to last item.
    int getCount = m_channelList->GetCount();
    int getVisibleCount = m_channelList->GetVisibleCount();
    newTopPosition = std::min(newTopPosition, getCount - getVisibleCount);

    m_channelList->SetItemCurrent(newPosition, newTopPosition);
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
    auto *dialog = new MythConfirmationDialog(popupStack, message, true);

    if (dialog->Create())
    {
        dialog->SetData(QVariant::fromValue(item));
        dialog->SetReturnEvent(this, "delsingle");
        popupStack->AddScreen(dialog);
    }
    else
    {
        delete dialog;
    }

}

void ChannelEditor::deleteChannels(void)
{
    const QString currentLabel = m_sourceList->GetValue();

    bool del_all = m_sourceFilter == FILTER_ALL;
    bool del_nul = m_sourceFilter == FILTER_UNASSIGNED;

    QString message;
    if (del_all)
        message = tr("Delete ALL channels?");
    else if (del_nul)
        message = tr("Delete all unassigned channels?");
    else
        message =tr("Delete all channels on %1?").arg(currentLabel);

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *dialog = new MythConfirmationDialog(popupStack, message, true);

    if (dialog->Create())
    {
        dialog->SetReturnEvent(this, "delall");
        popupStack->AddScreen(dialog);
    }
    else
    {
        delete dialog;
    }
}

void ChannelEditor::edit(MythUIButtonListItem *item)
{
    if (!item)
        item = m_channelList->GetItemCurrent();

    if (!item)
        return;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    int chanid = item->GetData().toInt();
    auto *cw = new ChannelWizard(chanid, m_sourceFilter);
    auto *ssd = new StandardSettingDialog(mainStack, "channelwizard", cw);
    if (ssd->Create())
    {
        connect(ssd, &MythScreenType::Exiting, this, &ChannelEditor::fillList);
        mainStack->AddScreen(ssd);
    }
    else
    {
        delete ssd;
    }
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

        auto *menu = new MythDialogBox(label, popupStack, "chanoptmenu");

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

// Check that we have a video source and that at least one
// capture card is connected to the video source.
//
static bool check_cardsource(int sourceid, QString &sourcename)
{
    // Check for videosource
    if (sourceid < 1)
    {
        MythConfirmationDialog *md = ShowOkPopup(QObject::tr(
            "Select a video source. 'All' cannot be used. "
            "If there is no video source then create one in the "
            "'Video sources' menu page and connect a capture card."));
        WaitFor(md);
        return false;
    }

    // Check for a connected capture card
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT capturecard.cardid "
        "FROM  capturecard "
        "WHERE capturecard.sourceid = :SOURCEID AND "
        "      capturecard.hostname = :HOSTNAME");
    query.bindValue(":SOURCEID", sourceid);
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("check_capturecard()", query);
        return false;
    }
    uint cardid = 0;
    if (query.next())
        cardid = query.value(0).toUInt();

    if (cardid < 1)
    {
        MythConfirmationDialog *md = ShowOkPopup(QObject::tr(
            "No capture card!"
            "\n"
            "Connect video source '%1' to a capture card "
            "in the 'Input Connections' menu page.")
            .arg(sourcename));
        WaitFor(md);
        return false;
    }

    // At least one capture card connected to the video source
    // must be able to do channel scanning
    if (SourceUtil::IsUnscanable(sourceid))
    {
        MythConfirmationDialog *md = ShowOkPopup(QObject::tr(
            "The capture card(s) connected to video source '%1' "
            "cannot be used for channel scanning.")
            .arg(sourcename));
        WaitFor(md);
        return false;
    }

    return true;
}

void ChannelEditor::scan(void)
{
    // Check that we have a videosource and a connected capture card
    if (!check_cardsource(m_sourceFilter, m_sourceFilterName))
        return;

    // Create the dialog now that we have a video source and a capture card
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *ssd = new StandardSettingDialog(mainStack, "scanwizard",
                                          new ScanWizard(m_sourceFilter));
    if (ssd->Create())
    {
        connect(ssd, &MythScreenType::Exiting, this, &ChannelEditor::fillList);
        mainStack->AddScreen(ssd);
    }
    else
    {
        delete ssd;
    }
}

void ChannelEditor::restoreData(void)
{
    // Check that we have a videosource and a connected capture card
    if (!check_cardsource(m_sourceFilter, m_sourceFilterName))
        return;

    // Create the dialog now that we have a video source and a capture card
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *ssd = new StandardSettingDialog(mainStack, "restoredata",
                                  new RestoreData((uint)m_sourceFilter));
    if (ssd->Create())
    {
        // Reload channel list with fillList after Restore
        connect(ssd, &MythScreenType::Exiting, this, &ChannelEditor::fillList);
        mainStack->AddScreen(ssd);
    }
    else
    {
        delete ssd;
    }
}

void ChannelEditor::transportEditor(void)
{
    // Check that we have a videosource and a connected capture card
    if (!check_cardsource(m_sourceFilter, m_sourceFilterName))
        return;

    // Create the dialog now that we have a video source and a capture card
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *ssd = new StandardSettingDialog(mainStack, "transporteditor",
                                  new TransportListEditor(m_sourceFilter));
    if (ssd->Create())
    {
        connect(ssd, &MythScreenType::Exiting, this, &ChannelEditor::fillList);
        mainStack->AddScreen(ssd);
    }
    else
    {
        delete ssd;
    }
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

    auto *menu = new MythDialogBox(label, popupStack, "iconoptmenu");

    if (menu->Create())
    {
        menu->SetReturnEvent(this, "iconimportopt");

        menu->AddButton(tr("Download all icons..."));
        menu->AddButton(tr("Rescan for missing icons..."));
        if (!channelname.isEmpty())
            menu->AddButtonV(tr("Download icon for %1").arg(channelname),
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
        auto *dce = (DialogCompletionEvent*)(event);

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
            auto *item = dce->GetData().value<MythUIButtonListItem *>();
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
                    query.prepare(QString("UPDATE channel "
                    "SET deleted = NOW() "
                    "WHERE deleted IS NULL AND "
                    "      sourceid NOT IN (%1)").arg(tmp));
                }
            }
            else
            {
                query.prepare("UPDATE channel "
                "SET deleted = NOW() "
                "WHERE deleted IS NULL AND sourceid = :SOURCEID");
                query.bindValue(":SOURCEID", m_sourceFilter);
            }

            if (!query.exec())
                MythDB::DBError("ChannelEditor Delete Channels", query);

            fillList();
        }
        else if (resultid == "iconimportopt")
        {
            MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

            ImportIconsWizard *iconwizard = nullptr;

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
                connect(iconwizard, &MythScreenType::Exiting, this, &ChannelEditor::fillList);
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
