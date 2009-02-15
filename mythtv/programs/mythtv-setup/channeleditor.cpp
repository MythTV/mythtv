#include <qsqldatabase.h>
#include "settings.h"
#include "mythcontext.h"
#include "mythdb.h"
#include "channeleditor.h"

#include <QApplication>

#include <mythdialogs.h>
#include <mythwizard.h>

// MythUI
#include "mythuitext.h"
#include "mythuibutton.h"
#include "mythuibuttonlist.h"
#include "mythuitextedit.h"
#include "mythuicheckbox.h"
#include "mythdialogbox.h"
#include "mythprogressdialog.h"

#include "channelsettings.h"
#include "transporteditor.h"
#include "sourceutil.h"

#include "scanwizard.h"
#include "importicons.h"

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

    int cardtypes = countCardtypes();
    bool hasDVB = cardTypesInclude("DVB");

    // add v4l options if no dvb or if dvb and some other card type
    // present
    QString cardtype = getCardtype();
    if (!hasDVB || cardtypes > 1 || id == 0) {
        ChannelOptionsV4L* v4l = new ChannelOptionsV4L(*cid);
        addChild(v4l);
    }
}

QString ChannelWizard::getCardtype()
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT cardtype"
                  " FROM capturecard, cardinput, channel"
                  " WHERE channel.chanid = :CHID "
                  " AND channel.sourceid = cardinput.sourceid"
                  " AND cardinput.cardid = capturecard.cardid");
    query.bindValue(":CHID", cid->getValue());

    if (query.exec() && query.next())
    {
        return query.value(0).toString();
    }

    return "";
}

bool ChannelWizard::cardTypesInclude(const QString& thecardtype) {
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT count(cardtype)"
        " FROM capturecard, cardinput, channel"
        " WHERE channel.chanid= :CHID "
        " AND channel.sourceid = cardinput.sourceid"
        " AND cardinput.cardid = capturecard.cardid"
        " AND capturecard.cardtype= :CARDTYPE ");
    query.bindValue(":CHID", cid->getValue());
    query.bindValue(":CARDTYPE", thecardtype);

    if (query.exec() && query.next())
    {
        int count = query.value(0).toInt();

        if (count > 0)
            return true;
        else
            return false;
    } else
        return false;
}

int ChannelWizard::countCardtypes()
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT count(DISTINCT cardtype)"
        " FROM capturecard, cardinput, channel"
        " WHERE channel.chanid = :CHID "
        " AND channel.sourceid = cardinput.sourceid"
        " AND cardinput.cardid = capturecard.cardid");
    query.bindValue(":CHID", cid->getValue());

    if (query.exec() && query.next())
    {
        return query.value(0).toInt();
    }
    else
        return 0;
}

/////////////////////////////////////////////////////////

ChannelEditor::ChannelEditor(MythScreenStack *parent)
              : MythScreenType(parent, "channeleditor")
{
    m_currentSourceName = "";
    m_currentSortMode = tr("Channel Name");
    m_currentHideMode = false;
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
    connect(m_sourceList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            SLOT(setSourceID(MythUIButtonListItem *)));
    sortList->SetValue(m_currentSortMode);


    // Source List
    new MythUIButtonListItem(m_sourceList,tr("All"),
                             qVariantFromValue(QString("All")));
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT name, sourceid FROM videosource");
    if (query.exec())
    {
        while(query.next())
        {
            new MythUIButtonListItem(m_sourceList, query.value(0).toString(),
                                     query.value(1).toString());
        }
    }
    new MythUIButtonListItem(m_sourceList,tr("(Unassigned)"),
                             qVariantFromValue(QString("Unassigned")));
    connect(sortList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            SLOT(setSortMode(MythUIButtonListItem *)));
    m_sourceList->SetValue(m_currentSourceName);

    // Hide/Show channels without channum checkbox
    hideCheck->SetCheckState(m_currentHideMode);
    connect(hideCheck, SIGNAL(toggled(bool)),
            SLOT(setHideMode(bool)));

    // Scan Button
    scanButton->SetHelpText(tr("Starts the channel scanner."));
    scanButton->SetEnabled(SourceUtil::IsAnySourceScanable());

    // Import Icons Button
    importIconButton->SetHelpText(tr("Starts the icon downloader"));
    importIconButton->SetEnabled(SourceUtil::IsAnySourceScanable());
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
    gContext->GetMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
        {
            menu();
        }
        else if (action == "DELETE")
        {
            del();
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void ChannelEditor::fillList(void)
{
    QString currentValue = m_channelList->GetValue();
    uint    currentIndex = qMax(m_channelList->GetCurrentPos(), 0);
    m_channelList->Reset();
    new MythUIButtonListItem(m_channelList, tr("(Add New Channel)"));

    bool fAllSources = true;

    QString querystr = "SELECT channel.name,channum,chanid ";

    if ((m_currentSourceName.isEmpty()) ||
        (m_currentSourceName == "Unassigned") ||
        (m_currentSourceName == "All"))
    {
        querystr += ",videosource.name FROM channel "
                    "LEFT JOIN videosource ON "
                    "(channel.sourceid = videosource.sourceid) ";
        fAllSources = true;
    }
    else
    {
        querystr += QString("FROM channel WHERE sourceid='%1' ")
                           .arg(m_currentSourceName);
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
            QString sourceid = "Unassigned";

            if (fAllSources && !query.value(3).toString().isNull())
            {
                sourceid = query.value(3).toString();
                if (m_currentSourceName == "Unassigned")
                    continue;
            }

            if (channum.isEmpty() && m_currentHideMode)
                continue;

            if (name.isEmpty())
                name = "(Unnamed : " + chanid + ")";

            if (m_currentSortMode == tr("Channel Name"))
            {
                if (!channum.isEmpty())
                    name += " (" + channum + ")";
            }
            else if (m_currentSortMode == tr("Channel Number"))
            {
                if (!channum.isEmpty())
                    name = channum + ". " + name;
                else
                    name = "???. " + name;
            }

            if ((m_currentSourceName.isEmpty()) && (m_currentSourceName != "Unassigned"))
                name += " (" + sourceid  + ")";

            bool sel = (chanid == currentValue);
            selidx = (sel) ? idx : selidx;
            new MythUIButtonListItem(m_channelList, name, qVariantFromValue(chanid));
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

    if (m_currentSourceID != sourceID)
    {
        m_currentSourceName = sourceName;
        m_currentSourceID = sourceID;
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

void ChannelEditor::deleteChannels(void)
{
    const QString currentLabel    = m_sourceList->GetValue();

    bool del_all = m_currentSourceName.isEmpty() || m_currentSourceName == "All";
    bool del_nul = m_currentSourceName == "Unassigned";

    QString chan_msg =
        (del_all) ? tr("Are you sure you would like to delete ALL channels?") :
        ((del_nul) ?
         tr("Are you sure you would like to delete all unassigned channels?") :
         tr("Are you sure you would like to delete the channels on %1?")
         .arg(currentLabel));

    DialogCode val = MythPopupBox::Show2ButtonPopup(
        gContext->GetMainWindow(), "", chan_msg,
        tr("Yes, delete the channels"),
        tr("No, don't"), kDialogCodeButton1);

    if (kDialogCodeButton0 != val)
        return;

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
        query.bindValue(":SOURCEID", m_currentSourceName);
    }

    if (!query.exec())
        MythDB::DBError("ChannelEditor Delete Channels", query);

    fillList();
}

void ChannelEditor::edit(MythUIButtonListItem *item)
{
    if (!item)
        return;

    m_id = item->GetData().toInt();
    ChannelWizard cw(m_id, m_currentSourceID);
    cw.exec();

    fillList();
}

void ChannelEditor::del()
{
    MythUIButtonListItem *item = m_channelList->GetItemCurrent();

    if (!item)
        return;

    m_id = item->GetData().toInt();

    DialogCode val = MythPopupBox::Show2ButtonPopup(
        gContext->GetMainWindow(),
        "", tr("Are you sure you would like to delete this channel?"),
        tr("Yes, delete the channel"),
        tr("No, don't"), kDialogCodeButton1);

    if (kDialogCodeButton0 == val)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("DELETE FROM channel WHERE chanid = :CHID ;");
        query.bindValue(":CHID", m_id);
        if (!query.exec() || !query.isActive())
            MythDB::DBError("ChannelEditor Delete Channel", query);

        fillList();
    }
}

void ChannelEditor::menu()
{
    MythUIButtonListItem *item = m_channelList->GetItemCurrent();

    if (!item)
        return;

    m_id = item->GetData().toInt();
    if (m_id == 0)
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
    ScanWizard *scanwizard = new ScanWizard(m_currentSourceID);
    scanwizard->exec(false, true);
    scanwizard->deleteLater();

    fillList();
#else
    VERBOSE(VB_IMPORTANT,  "You must compile the backend "
            "to be able to scan for channels");
#endif
}

void ChannelEditor::transportEditor(void)
{
    TransportListEditor *editor = new TransportListEditor(m_currentSourceID);
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

    QStringList buttons;
    buttons.append(tr("Cancel"));
    buttons.append(tr("Download all icons.."));
    buttons.append(tr("Rescan for missing icons.."));
    if (!channelname.isEmpty())
        buttons.append(tr("Download icon for ") + channelname);

    int val = MythPopupBox::ShowButtonPopup(gContext->GetMainWindow(),
                                             "", "Channel Icon Import", buttons, kDialogCodeButton2);

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    ImportIconsWizard *iconwizard;
    if (val == kDialogCodeButton0) // Cancel pressed
        return;
    else if (val == kDialogCodeButton1) // Import all icons pressed
        iconwizard = new ImportIconsWizard(mainStack, false);
    else if (val == kDialogCodeButton2) // Rescan for missing pressed
        iconwizard = new ImportIconsWizard(mainStack, true);
    else if (val == kDialogCodeButton3) // Import a single channel icon
        iconwizard = new ImportIconsWizard(mainStack, true, channelname);
    else
        return;

    if (iconwizard->Create())
    {
        connect(iconwizard, SIGNAL(Exiting()), SLOT(fillList()));
        mainStack->AddScreen(iconwizard);
    }
    else
        delete iconwizard;
}

void ChannelEditor::customEvent(QEvent *event)
{
    if (event->type() == kMythDialogBoxCompletionEventType)
    {
        DialogCompletionEvent *dce =
                                dynamic_cast<DialogCompletionEvent*>(event);

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
    }
}
