#include <cstdlib>

// Qt
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QDomDocument>

// Myth
#include <mythcontext.h>
#include <mythdbcon.h>
#include <mythuihelper.h>
#include <mythmainwindow.h>
#include <mythuitext.h>
#include <mythuitextedit.h>
#include <mythuibutton.h>
#include <mythuibuttonlist.h>
#include <mythdialogbox.h>
#include <mythsystem.h>
#include <exitcodes.h>
#include <mythdate.h>

// mytharchive
#include "importnative.h"
#include "archiveutil.h"
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
        QString type, dbVersion;

        if (itemNodeList.count() < 1)
        {
            LOG(VB_GENERAL, LOG_ERR,
                "Couldn't find an 'item' element in XML file");
            return false;
        }

        QDomNode n = itemNodeList.item(0);
        QDomElement e = n.toElement();
        type = e.attribute("type");
        dbVersion = e.attribute("databaseversion");
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
        else if (type == "video")
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
    FileSelector(parent, NULL, FSTYPE_FILE, "", "*.xml"),
    m_nextButton(NULL),
    m_prevButton(NULL),
    m_progTitle(NULL),
    m_progSubtitle(NULL),
    m_progStartTime(NULL)
{
    m_curDirectory = gCoreContext->GetSetting("MythNativeLoadFilename", "/");
}

ArchiveFileSelector::~ArchiveFileSelector(void)
{
    gCoreContext->SaveSetting("MythNativeLoadFilename", m_curDirectory);
}

bool ArchiveFileSelector::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("mythnative-ui.xml", "archivefile_selector", this);

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

    connect(m_nextButton, SIGNAL(Clicked()), this, SLOT(nextPressed()));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(cancelPressed()));
    connect(m_prevButton, SIGNAL(Clicked()), this, SLOT(prevPressed()));

    connect(m_locationEdit, SIGNAL(LosingFocus()),
            this, SLOT(locationEditLostFocus()));
    m_locationEdit->SetText(m_curDirectory);

    connect(m_backButton, SIGNAL(Clicked()), this, SLOT(backPressed()));
    connect(m_homeButton, SIGNAL(Clicked()), this, SLOT(homePressed()));

    connect(m_fileButtonList, SIGNAL(itemSelected(MythUIButtonListItem *)),
            this, SLOT(itemSelected(MythUIButtonListItem *)));

    connect(m_fileButtonList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            this, SLOT(itemClicked(MythUIButtonListItem *)));

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

    FileData *fileData = qVariantValue<FileData*>(item->GetData());
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

        ImportNative *native = new ImportNative(mainStack, this, m_xmlFile, m_details);

        if (native->Create())
            mainStack->AddScreen(native);
    }
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

ImportNative::ImportNative(
    MythScreenStack *parent, MythScreenType *previousScreen,
    const QString &xmlFile, FileDetails details) :
    MythScreenType(parent, "ImportNative"),
    m_xmlFile(xmlFile),
    m_details(details),
    m_previousScreen(previousScreen),
    m_progTitle_text(NULL),
    m_progDateTime_text(NULL),
    m_progDescription_text(NULL),
    m_chanID_text(NULL),
    m_chanNo_text(NULL),
    m_chanName_text(NULL),
    m_callsign_text(NULL),
    m_localChanID_text(NULL),
    m_localChanNo_text(NULL),
    m_localChanName_text(NULL),
    m_localCallsign_text(NULL),
    m_searchChanID_button(NULL),
    m_searchChanNo_button(NULL),
    m_searchChanName_button(NULL),
    m_searchCallsign_button(NULL),
    m_finishButton(NULL),
    m_prevButton(NULL),
    m_cancelButton(NULL),
    m_isValidXMLSelected(false)
{
}

ImportNative::~ImportNative()
{
}

bool ImportNative::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("mythnative-ui.xml", "importnative", this);

    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_progTitle_text, "progtitle", &err);
    UIUtilE::Assign(this, m_progDateTime_text, "progdatetime", &err);
    UIUtilE::Assign(this, m_progDescription_text, "progdescription", &err);

    UIUtilE::Assign(this, m_chanID_text, "chanid", &err);
    UIUtilE::Assign(this, m_chanNo_text, "channo", &err);
    UIUtilE::Assign(this, m_chanName_text, "name", &err);
    UIUtilE::Assign(this, m_callsign_text, "callsign", &err);

    UIUtilE::Assign(this, m_localChanID_text, "local_chanid", &err);
    UIUtilE::Assign(this, m_localChanNo_text, "local_channo", &err);
    UIUtilE::Assign(this, m_localChanName_text, "local_name", &err);
    UIUtilE::Assign(this, m_localCallsign_text, "local_callsign", &err);

    UIUtilE::Assign(this, m_searchChanID_button, "searchchanid_button", &err);
    UIUtilE::Assign(this, m_searchChanNo_button, "searchchanno_button", &err);
    UIUtilE::Assign(this, m_searchChanName_button, "searchname_button", &err);
    UIUtilE::Assign(this, m_searchCallsign_button ,"searchcallsign_button", &err);

    UIUtilE::Assign(this, m_finishButton, "finish_button", &err);
    UIUtilE::Assign(this, m_prevButton, "prev_button", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel_button", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'importarchive'");
        return false;
    }

    connect(m_finishButton, SIGNAL(Clicked()), this, SLOT(finishedPressed()));
    connect(m_prevButton, SIGNAL(Clicked()), this, SLOT(prevPressed()));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(cancelPressed()));

    connect(m_searchChanID_button, SIGNAL(Clicked()), this, SLOT(searchChanID()));
    connect(m_searchChanNo_button, SIGNAL(Clicked()), this, SLOT(searchChanNo()));
    connect(m_searchChanName_button, SIGNAL(Clicked()), this, SLOT(searchName()));
    connect(m_searchCallsign_button, SIGNAL(Clicked()), this, SLOT(searchCallsign()));

    m_progTitle_text->SetText(m_details.title);

    m_progDateTime_text->SetText(m_details.startTime.toLocalTime()
                                 .toString("dd MMM yy (hh:mm)"));
    m_progDescription_text->SetText(
        (m_details.subtitle == "" ? m_details.subtitle + "\n" : "") + m_details.description);

    m_chanID_text->SetText(m_details.chanID);
    m_chanNo_text->SetText(m_details.chanNo);
    m_chanName_text->SetText(m_details.chanName);
    m_callsign_text->SetText(m_details.callsign);

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

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
        {
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void ImportNative::finishedPressed()

{
    if (m_details.chanID != "N/A" && m_localChanID_text->GetText() == "")
    {
        ShowOkPopup(tr("You need to select a valid channel id!"));
        return;
    }

    QString commandline;
    QString tempDir = gCoreContext->GetSetting("MythArchiveTempDir", "");
    QString chanID = m_localChanID_text->GetText();

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
        m_localChanID_text->SetText(query.value(0).toString());
        m_localChanNo_text->SetText(query.value(1).toString());
        m_localChanName_text->SetText(query.value(2).toString());
        m_localCallsign_text->SetText(query.value(3).toString());
        return;
    }

    // try to find callsign
    query.prepare("SELECT chanid, channum, name, callsign FROM channel "
            "WHERE callsign = :CALLSIGN;");
    query.bindValue(":CALLSIGN", callsign);

    if (query.exec() && query.next())
    {
        // got match
        m_localChanID_text->SetText(query.value(0).toString());
        m_localChanNo_text->SetText(query.value(1).toString());
        m_localChanName_text->SetText(query.value(2).toString());
        m_localCallsign_text->SetText(query.value(3).toString());
        return;
    }

    // try to find name
    query.prepare("SELECT chanid, channum, name, callsign FROM channel "
            "WHERE name = :NAME;");
    query.bindValue(":NAME", callsign);

    if (query.exec() && query.next())
    {
        // got match
        m_localChanID_text->SetText(query.value(0).toString());
        m_localChanNo_text->SetText(query.value(1).toString());
        m_localChanName_text->SetText(query.value(2).toString());
        m_localCallsign_text->SetText(query.value(3).toString());
        return;
    }

    // give up
    m_localChanID_text->Reset();
    m_localChanNo_text->Reset();
    m_localChanName_text->Reset();
    m_localCallsign_text->Reset();
}

void ImportNative::showList(const QString &caption, QString &value,
                            const char *slot)
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythUISearchDialog *searchDialog = new
            MythUISearchDialog(popupStack, caption, m_searchList, true, value);

    if (!searchDialog->Create())
    {
        delete searchDialog;
        searchDialog = NULL;
        return;
    }

    connect(searchDialog, SIGNAL(haveResult(QString)), this, slot);

    popupStack->AddScreen(searchDialog);
}

void ImportNative::fillSearchList(const QString &field)
{

    m_searchList.clear();

    QString querystr;
    querystr = QString("SELECT %1 FROM channel ORDER BY %2").arg(field).arg(field);

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

    s = m_chanID_text->GetText();
    showList(tr("Select a channel id"), s, SLOT(gotChanID(QString)));
}

void ImportNative::gotChanID(QString value)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT chanid, channum, name, callsign "
            "FROM channel WHERE chanid = :CHANID;");
    query.bindValue(":CHANID", value);

    if (query.exec() && query.next())
    {
        m_localChanID_text->SetText(query.value(0).toString());
        m_localChanNo_text->SetText(query.value(1).toString());
        m_localChanName_text->SetText(query.value(2).toString());
        m_localCallsign_text->SetText(query.value(3).toString());
    }
}

void ImportNative::searchChanNo()
{
    QString s;

    fillSearchList("channum");

    s = m_chanNo_text->GetText();
    showList(tr("Select a channel number"), s, SLOT(gotChanNo(QString)));
}

void ImportNative::gotChanNo(QString value)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT chanid, channum, name, callsign "
            "FROM channel WHERE channum = :CHANNUM;");
    query.bindValue(":CHANNUM", value);

    if (query.exec() && query.next())
    {
        m_localChanID_text->SetText(query.value(0).toString());
        m_localChanNo_text->SetText(query.value(1).toString());
        m_localChanName_text->SetText(query.value(2).toString());
        m_localCallsign_text->SetText(query.value(3).toString());
    }
}

void ImportNative::searchName()
{
    QString s;

    fillSearchList("name");

    s = m_chanName_text->GetText();
    showList(tr("Select a channel name"), s, SLOT(gotName(QString)));
}

void ImportNative::gotName(QString value)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT chanid, channum, name, callsign "
            "FROM channel WHERE name = :NAME;");
    query.bindValue(":NAME", value);

    if (query.exec() && query.next())
    {
        m_localChanID_text->SetText(query.value(0).toString());
        m_localChanNo_text->SetText(query.value(1).toString());
        m_localChanName_text->SetText(query.value(2).toString());
        m_localCallsign_text->SetText(query.value(3).toString());
    }
}

void ImportNative::searchCallsign()
{
    QString s;

    fillSearchList("callsign");

    s = m_callsign_text->GetText();
    showList(tr("Select a Callsign"), s, SLOT(gotCallsign(QString)));
}

void ImportNative::gotCallsign(QString value)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT chanid, channum, name, callsign "
            "FROM channel WHERE callsign = :CALLSIGN;");
    query.bindValue(":CALLSIGN", value);

    if (query.exec() && query.next())
    {
        m_localChanID_text->SetText(query.value(0).toString());
        m_localChanNo_text->SetText(query.value(1).toString());
        m_localChanName_text->SetText(query.value(2).toString());
        m_localCallsign_text->SetText(query.value(3).toString());
    }
}
