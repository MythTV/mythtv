#include <cstdlib>

// Qt
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QDomDocument>

// Myth
#include <libmyth/mythcontext.h>
#include <libmythbase/exitcodes.h>
#include <libmythbase/mythdate.h>
#include <libmythbase/mythdbcon.h>
#include <libmythbase/mythsystemlegacy.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuihelper.h>
#include <libmythui/mythuitext.h>
#include <libmythui/mythuitextedit.h>

// mytharchive
#include "archiveutil.h"
#include "importnative.h"
#include "logviewer.h"


////////////////////////////////////////////////////////////////

static bool loadDetailsFromXML(const QString &filename, FileDetails *details)
{
    QDomDocument doc("mydocument");
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    if (!doc.setContent(&file))
    {
        file.close();
        return false;
    }
    file.close();

    QString docType = doc.doctype().name();

    if (docType == "MYTHARCHIVEITEM")
    {
        QDomNodeList itemNodeList = doc.elementsByTagName("item");
        QString type;
//      QString dbVersion;

        if (itemNodeList.count() < 1)
        {
            LOG(VB_GENERAL, LOG_ERR,
                "Couldn't find an 'item' element in XML file");
            return false;
        }

        QDomNode n = itemNodeList.item(0);
        QDomElement e = n.toElement();
        type = e.attribute("type");
//      dbVersion = e.attribute("databaseversion");
        if (type == "recording")
        {
            QDomNodeList nodeList = e.elementsByTagName("recorded");
            if (nodeList.count() < 1)
            {
                LOG(VB_GENERAL, LOG_ERR,
                    "Couldn't find a 'recorded' element in XML file");
                return false;
            }

            n = nodeList.item(0);
            e = n.toElement();
            n = e.firstChild();
            while (!n.isNull())
            {
                e = n.toElement();
                if (!e.isNull())
                {
                    if (e.tagName() == "title")
                        details->title = e.text();

                    if (e.tagName() == "subtitle")
                        details->subtitle = e.text();

                    if (e.tagName() == "starttime")
                        details->startTime = MythDate::fromString(e.text());

                    if (e.tagName() == "description")
                        details->description = e.text();
                }
                n = n.nextSibling();
            }

            // get channel info
            n = itemNodeList.item(0);
            e = n.toElement();
            nodeList = e.elementsByTagName("channel");
            if (nodeList.count() < 1)
            {
                LOG(VB_GENERAL, LOG_ERR,
                    "Couldn't find a 'channel' element in XML file");
                details->chanID = "";
                details->chanNo = "";
                details->chanName = "";
                details->callsign =  "";
                return false;
            }

            n = nodeList.item(0);
            e = n.toElement();
            details->chanID = e.attribute("chanid");
            details->chanNo = e.attribute("channum");
            details->chanName = e.attribute("name");
            details->callsign =  e.attribute("callsign");
            return true;
        }
        if (type == "video")
        {
            QDomNodeList nodeList = e.elementsByTagName("videometadata");
            if (nodeList.count() < 1)
            {
                LOG(VB_GENERAL, LOG_ERR,
                    "Couldn't find a 'videometadata' element in XML file");
                return false;
            }

            n = nodeList.item(0);
            e = n.toElement();
            n = e.firstChild();
            while (!n.isNull())
            {
                e = n.toElement();
                if (!e.isNull())
                {
                    if (e.tagName() == "title")
                    {
                        details->title = e.text();
                        details->subtitle = "";
                        details->startTime = QDateTime();
                    }

                    if (e.tagName() == "plot")
                    {
                        details->description = e.text();
                    }
                }
                n = n.nextSibling();
            }

            details->chanID = "N/A";
            details->chanNo = "N/A";
            details->chanName = "N/A";
            details->callsign = "N/A";

            return true;
        }
    }

    return false;
}

////////////////////////////////////////////////////////////////

ArchiveFileSelector::ArchiveFileSelector(MythScreenStack *parent) :
    FileSelector(parent, nullptr, FSTYPE_FILE, "", "*.xml")
{
    m_curDirectory = gCoreContext->GetSetting("MythNativeLoadFilename", "/");
}

ArchiveFileSelector::~ArchiveFileSelector(void)
{
    gCoreContext->SaveSetting("MythNativeLoadFilename", m_curDirectory);
}

bool ArchiveFileSelector::Create(void)
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("mythnative-ui.xml", "archivefile_selector", this);
    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilW::Assign(this, m_titleText, "title");
    UIUtilE::Assign(this, m_fileButtonList, "filelist", &err);
    UIUtilE::Assign(this, m_locationEdit, "location_edit", &err);
    UIUtilE::Assign(this, m_backButton, "back_button", &err);
    UIUtilE::Assign(this, m_homeButton, "home_button", &err);
    UIUtilE::Assign(this, m_nextButton, "next_button", &err);
    UIUtilE::Assign(this, m_prevButton, "prev_button", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel_button", &err);
    UIUtilE::Assign(this, m_progTitle, "title_text", &err);
    UIUtilE::Assign(this, m_progSubtitle, "subtitle_text", &err);
    UIUtilE::Assign(this, m_progStartTime, "starttime_text", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'archivefile_selector'");
        return false;
    }

    if (m_titleText)
        m_titleText->SetText(tr("Find File To Import"));

    connect(m_nextButton, &MythUIButton::Clicked, this, &ArchiveFileSelector::nextPressed);
    connect(m_cancelButton, &MythUIButton::Clicked, this, &ArchiveFileSelector::cancelPressed);
    connect(m_prevButton, &MythUIButton::Clicked, this, &ArchiveFileSelector::prevPressed);

    connect(m_locationEdit, &MythUIType::LosingFocus,
            this, &ArchiveFileSelector::locationEditLostFocus);
    m_locationEdit->SetText(m_curDirectory);

    connect(m_backButton, &MythUIButton::Clicked, this, &ArchiveFileSelector::backPressed);
    connect(m_homeButton, &MythUIButton::Clicked, this, &ArchiveFileSelector::homePressed);

    connect(m_fileButtonList, &MythUIButtonList::itemSelected,
            this, &ArchiveFileSelector::itemSelected);

    connect(m_fileButtonList, &MythUIButtonList::itemClicked,
            this, &ArchiveFileSelector::itemClicked);

    BuildFocusList();

    SetFocusWidget(m_fileButtonList);

    updateSelectedList();
    updateFileList();

    return true;
}

void ArchiveFileSelector::itemSelected(MythUIButtonListItem *item)
{
    m_xmlFile.clear();

    if (!item)
        return;

    auto *fileData = item->GetData().value<FileData*>();
    if (!fileData)
        return;

    if (loadDetailsFromXML(m_curDirectory + "/" + fileData->filename, &m_details))
    {
        m_xmlFile = m_curDirectory + "/" + fileData->filename;
        m_progTitle->SetText(m_details.title);
        m_progSubtitle->SetText(m_details.subtitle);
        m_progStartTime->SetText(m_details.startTime.toLocalTime()
                                 .toString("dd MMM yy (hh:mm)"));
    }
    else
    {
        m_progTitle->Reset();
        m_progSubtitle->Reset();
        m_progStartTime->Reset();
    }
}

void ArchiveFileSelector::nextPressed()
{
    if (m_xmlFile == "")
    {
        ShowOkPopup(tr("The selected item is not a valid archive file!"));
    }
    else
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

        auto *native = new ImportNative(mainStack, this, m_xmlFile, m_details);

        if (native->Create())
            mainStack->AddScreen(native);
    }
    // No memory leak. ImportNative (via MythUIType) adds the new item
    // onto the parent mainStack.
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
}

void ArchiveFileSelector::prevPressed()
{
    Close();
}

void ArchiveFileSelector::cancelPressed()
{
    Close();
}

////////////////////////////////////////////////////////////////

bool ImportNative::Create(void)
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("mythnative-ui.xml", "importnative", this);
    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_progTitleText, "progtitle", &err);
    UIUtilE::Assign(this, m_progDateTimeText, "progdatetime", &err);
    UIUtilE::Assign(this, m_progDescriptionText, "progdescription", &err);

    UIUtilE::Assign(this, m_chanIDText, "chanid", &err);
    UIUtilE::Assign(this, m_chanNoText, "channo", &err);
    UIUtilE::Assign(this, m_chanNameText, "name", &err);
    UIUtilE::Assign(this, m_callsignText, "callsign", &err);

    UIUtilE::Assign(this, m_localChanIDText, "local_chanid", &err);
    UIUtilE::Assign(this, m_localChanNoText, "local_channo", &err);
    UIUtilE::Assign(this, m_localChanNameText, "local_name", &err);
    UIUtilE::Assign(this, m_localCallsignText, "local_callsign", &err);

    UIUtilE::Assign(this, m_searchChanIDButton, "searchchanid_button", &err);
    UIUtilE::Assign(this, m_searchChanNoButton, "searchchanno_button", &err);
    UIUtilE::Assign(this, m_searchChanNameButton, "searchname_button", &err);
    UIUtilE::Assign(this, m_searchCallsignButton ,"searchcallsign_button", &err);

    UIUtilE::Assign(this, m_finishButton, "finish_button", &err);
    UIUtilE::Assign(this, m_prevButton, "prev_button", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel_button", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'importarchive'");
        return false;
    }

    connect(m_finishButton, &MythUIButton::Clicked, this, &ImportNative::finishedPressed);
    connect(m_prevButton, &MythUIButton::Clicked, this, &ImportNative::prevPressed);
    connect(m_cancelButton, &MythUIButton::Clicked, this, &ImportNative::cancelPressed);

    connect(m_searchChanIDButton, &MythUIButton::Clicked, this, &ImportNative::searchChanID);
    connect(m_searchChanNoButton, &MythUIButton::Clicked, this, &ImportNative::searchChanNo);
    connect(m_searchChanNameButton, &MythUIButton::Clicked, this, &ImportNative::searchName);
    connect(m_searchCallsignButton, &MythUIButton::Clicked, this, &ImportNative::searchCallsign);

    m_progTitleText->SetText(m_details.title);

    m_progDateTimeText->SetText(m_details.startTime.toLocalTime()
                                 .toString("dd MMM yy (hh:mm)"));
    m_progDescriptionText->SetText(
        (m_details.subtitle == "" ? m_details.subtitle + "\n" : "") + m_details.description);

    m_chanIDText->SetText(m_details.chanID);
    m_chanNoText->SetText(m_details.chanNo);
    m_chanNameText->SetText(m_details.chanName);
    m_callsignText->SetText(m_details.callsign);

    findChannelMatch(m_details.chanID, m_details.chanNo,
                     m_details.chanName, m_details.callsign);

    BuildFocusList();

    SetFocusWidget(m_finishButton);

    return true;
}

bool ImportNative::keyPressEvent(QKeyEvent *event)
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
        {
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

void ImportNative::finishedPressed()

{
    if (m_details.chanID != "N/A" && m_localChanIDText->GetText() == "")
    {
        ShowOkPopup(tr("You need to select a valid channel id!"));
        return;
    }

    QString commandline;
    QString tempDir = gCoreContext->GetSetting("MythArchiveTempDir", "");
    QString chanID = m_localChanIDText->GetText();

    if (chanID == "")
        chanID = m_details.chanID;

    if (tempDir == "")
        return;

    if (!tempDir.endsWith("/"))
        tempDir += "/";

    QString logDir = tempDir + "logs";

    // remove any existing logs
    myth_system("rm -f " + logDir + "/*.log");

    commandline = "mytharchivehelper --logpath " + logDir + " --importarchive "
                  "--infile \"" + m_xmlFile +
                  "\" --chanid " + chanID;

    uint flags = kMSRunBackground | kMSDontBlockInputDevs |
                 kMSDontDisableDrawing;
    uint retval = myth_system(commandline, flags);
    if (retval != GENERIC_EXIT_RUNNING && retval != GENERIC_EXIT_OK)
    {
        ShowOkPopup(tr("It was not possible to import the Archive. "
                       " An error occured when running 'mytharchivehelper'") );
        return;
    }

    showLogViewer();

    m_previousScreen->Close();
    Close();
}

void ImportNative::prevPressed()
{
    Close();
}

void ImportNative::cancelPressed()
{
    m_previousScreen->Close();
    Close();
}

void ImportNative::findChannelMatch(const QString &chanID, const QString &chanNo,
                                          const QString &name, const QString &callsign)
{
    // find best match in channel table for this recording

    // look for an exact match - maybe the user is importing back an old recording
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT chanid, channum, name, callsign FROM channel "
            "WHERE chanid = :CHANID AND channum = :CHANNUM AND name = :NAME "
            "AND callsign = :CALLSIGN;");
    query.bindValue(":CHANID", chanID);
    query.bindValue(":CHANNUM", chanNo);
    query.bindValue(":NAME", name);
    query.bindValue(":CALLSIGN", callsign);

    if (query.exec() && query.next())
    {
        // got match
        m_localChanIDText->SetText(query.value(0).toString());
        m_localChanNoText->SetText(query.value(1).toString());
        m_localChanNameText->SetText(query.value(2).toString());
        m_localCallsignText->SetText(query.value(3).toString());
        return;
    }

    // try to find callsign
    query.prepare("SELECT chanid, channum, name, callsign FROM channel "
            "WHERE callsign = :CALLSIGN;");
    query.bindValue(":CALLSIGN", callsign);

    if (query.exec() && query.next())
    {
        // got match
        m_localChanIDText->SetText(query.value(0).toString());
        m_localChanNoText->SetText(query.value(1).toString());
        m_localChanNameText->SetText(query.value(2).toString());
        m_localCallsignText->SetText(query.value(3).toString());
        return;
    }

    // try to find name
    query.prepare("SELECT chanid, channum, name, callsign FROM channel "
            "WHERE name = :NAME;");
    query.bindValue(":NAME", callsign);

    if (query.exec() && query.next())
    {
        // got match
        m_localChanIDText->SetText(query.value(0).toString());
        m_localChanNoText->SetText(query.value(1).toString());
        m_localChanNameText->SetText(query.value(2).toString());
        m_localCallsignText->SetText(query.value(3).toString());
        return;
    }

    // give up
    m_localChanIDText->Reset();
    m_localChanNoText->Reset();
    m_localChanNameText->Reset();
    m_localCallsignText->Reset();
}

void ImportNative::showList(const QString &caption, QString &value,
                            const INSlot slot)
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *searchDialog = new
            MythUISearchDialog(popupStack, caption, m_searchList, true, value);

    if (!searchDialog->Create())
    {
        delete searchDialog;
        searchDialog = nullptr;
        return;
    }

    connect(searchDialog, &MythUISearchDialog::haveResult, this, slot);

    popupStack->AddScreen(searchDialog);
}

void ImportNative::fillSearchList(const QString &field)
{

    m_searchList.clear();

    QString querystr;
    querystr = QString("SELECT %1 FROM channel ORDER BY %2").arg(field, field);

    MSqlQuery query(MSqlQuery::InitCon());

    if (query.exec(querystr))
    {
        while (query.next())
        {
            m_searchList << query.value(0).toString();
        }
    }
}

void ImportNative::searchChanID()
{
    QString s;

    fillSearchList("chanid");

    s = m_chanIDText->GetText();
    showList(tr("Select a channel id"), s, &ImportNative::gotChanID);
}

void ImportNative::gotChanID(const QString& value)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT chanid, channum, name, callsign "
            "FROM channel WHERE chanid = :CHANID;");
    query.bindValue(":CHANID", value);

    if (query.exec() && query.next())
    {
        m_localChanIDText->SetText(query.value(0).toString());
        m_localChanNoText->SetText(query.value(1).toString());
        m_localChanNameText->SetText(query.value(2).toString());
        m_localCallsignText->SetText(query.value(3).toString());
    }
}

void ImportNative::searchChanNo()
{
    QString s;

    fillSearchList("channum");

    s = m_chanNoText->GetText();
    showList(tr("Select a channel number"), s, &ImportNative::gotChanNo);
}

void ImportNative::gotChanNo(const QString& value)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT chanid, channum, name, callsign "
            "FROM channel WHERE channum = :CHANNUM;");
    query.bindValue(":CHANNUM", value);

    if (query.exec() && query.next())
    {
        m_localChanIDText->SetText(query.value(0).toString());
        m_localChanNoText->SetText(query.value(1).toString());
        m_localChanNameText->SetText(query.value(2).toString());
        m_localCallsignText->SetText(query.value(3).toString());
    }
}

void ImportNative::searchName()
{
    QString s;

    fillSearchList("name");

    s = m_chanNameText->GetText();
    showList(tr("Select a channel name"), s, &ImportNative::gotName);
}

void ImportNative::gotName(const QString& value)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT chanid, channum, name, callsign "
            "FROM channel WHERE name = :NAME;");
    query.bindValue(":NAME", value);

    if (query.exec() && query.next())
    {
        m_localChanIDText->SetText(query.value(0).toString());
        m_localChanNoText->SetText(query.value(1).toString());
        m_localChanNameText->SetText(query.value(2).toString());
        m_localCallsignText->SetText(query.value(3).toString());
    }
}

void ImportNative::searchCallsign()
{
    QString s;

    fillSearchList("callsign");

    s = m_callsignText->GetText();
    showList(tr("Select a Callsign"), s, &ImportNative::gotCallsign);
}

void ImportNative::gotCallsign(const QString& value)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT chanid, channum, name, callsign "
            "FROM channel WHERE callsign = :CALLSIGN;");
    query.bindValue(":CALLSIGN", value);

    if (query.exec() && query.next())
    {
        m_localChanIDText->SetText(query.value(0).toString());
        m_localChanNoText->SetText(query.value(1).toString());
        m_localChanNameText->SetText(query.value(2).toString());
        m_localCallsignText->SetText(query.value(3).toString());
    }
}
