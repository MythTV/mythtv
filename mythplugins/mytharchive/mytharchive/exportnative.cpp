// C++
#include <cstdlib>
#include <iostream>
#include <unistd.h>

// qt
#include <QFile>
#include <QKeyEvent>
#include <QTextStream>
#include <QDomDocument>

// myth
#include <libmyth/mythcontext.h>
#include <libmythbase/exitcodes.h>
#include <libmythbase/mythdb.h>
#include <libmythbase/mythsystemlegacy.h>
#include <libmythbase/programinfo.h>
#include <libmythbase/remoteutil.h>
#include <libmythbase/stringutil.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuicheckbox.h>
#include <libmythui/mythuiprogressbar.h>
#include <libmythui/mythuitext.h>

// mytharchive
#include "archiveutil.h"
#include "exportnative.h"
#include "fileselector.h"
#include "logviewer.h"
#include "recordingselector.h"
#include "videoselector.h"

ExportNative::~ExportNative(void)
{
    saveConfiguration();

    while (!m_archiveList.isEmpty())
         delete m_archiveList.takeFirst();
    m_archiveList.clear();
}

bool ExportNative::Create(void)
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("mythnative-ui.xml", "exportnative", this);
    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_nextButton, "next_button", &err);
    UIUtilE::Assign(this, m_prevButton, "prev_button", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel_button", &err);

    UIUtilE::Assign(this, m_titleText, "progtitle", &err);
    UIUtilE::Assign(this, m_datetimeText, "progdatetime", &err);
    UIUtilE::Assign(this, m_descriptionText, "progdescription", &err);
    UIUtilE::Assign(this, m_filesizeText, "filesize", &err);
    UIUtilE::Assign(this, m_nofilesText, "nofiles", &err);
    UIUtilE::Assign(this, m_sizeBar, "size_bar", &err);
    UIUtilE::Assign(this, m_archiveButtonList, "archivelist", &err);
    UIUtilE::Assign(this, m_addrecordingButton, "addrecording_button", &err);
    UIUtilE::Assign(this, m_addvideoButton, "addvideo_button", &err);

    UIUtilW::Assign(this, m_maxsizeText, "maxsize");
    UIUtilW::Assign(this, m_minsizeText, "minsize");
    UIUtilW::Assign(this, m_currsizeText, "currentsize");
    UIUtilW::Assign(this, m_currsizeErrText, "currentsize_error");

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'exportnative'");
        return false;
    }

    connect(m_nextButton, &MythUIButton::Clicked, this, &ExportNative::handleNextPage);
    connect(m_prevButton, &MythUIButton::Clicked, this, &ExportNative::handlePrevPage);
    connect(m_cancelButton, &MythUIButton::Clicked, this, &ExportNative::handleCancel);


    getArchiveList();
    connect(m_archiveButtonList, &MythUIButtonList::itemSelected,
            this, &ExportNative::titleChanged);

    connect(m_addrecordingButton, &MythUIButton::Clicked, this, &ExportNative::handleAddRecording);
    connect(m_addvideoButton, &MythUIButton::Clicked, this, &ExportNative::handleAddVideo);

    BuildFocusList();

    SetFocusWidget(m_nextButton);

    loadConfiguration();

    return true;
}

bool ExportNative::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Archive", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "MENU")
        {
            ShowMenu();
        }
        else if (action == "DELETE")
        {
            removeItem();
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

void ExportNative::updateSizeBar()
{
    int64_t size = 0;

    for (const auto *a : std::as_const(m_archiveList))
        size += a->size;

    m_usedSpace = size / 1024 / 1024;
    uint freeSpace = m_archiveDestination.freeSpace / 1024;

    m_sizeBar->SetTotal(freeSpace);
    m_sizeBar->SetUsed(m_usedSpace);

    QString tmpSize = QString("%1 Mb").arg(freeSpace);

    if (m_maxsizeText)
        m_maxsizeText->SetText(tmpSize);

    if (m_minsizeText)
        m_minsizeText->SetText("0 Mb");

    tmpSize = QString("%1 Mb").arg(m_usedSpace);

    if (m_usedSpace > freeSpace)
    {
        if (m_currsizeText)
            m_currsizeText->Hide();

        if (m_currsizeErrText)
        {
            m_currsizeErrText->Show();
            m_currsizeErrText->SetText(tmpSize);
        }
    }
    else
    {
        if (m_currsizeErrText)
            m_currsizeErrText->Hide();

        if (m_currsizeText)
        {
            m_currsizeText->Show();
            m_currsizeText->SetText(tmpSize);
        }
    }
}

void ExportNative::titleChanged(MythUIButtonListItem *item)
{
    auto *a = item->GetData().value<ArchiveItem *>();
    if (!a)
        return;

    m_titleText->SetText(a->title);

    m_datetimeText->SetText(a->startDate + " " + a->startTime);

    m_descriptionText->SetText(
                (a->subtitle != "" ? a->subtitle + "\n" : "") + a->description);

    m_filesizeText->SetText(StringUtil::formatKBytes(a->size / 1024, 2));
}

void ExportNative::handleNextPage()
{
    if (m_archiveList.empty())
    {
        ShowOkPopup(tr("You need to add at least one item to archive!"));
        return;
    }

    runScript();

    m_previousScreen->Close();
    Close();
}

void ExportNative::handlePrevPage()
{
    Close();
}

void ExportNative::handleCancel()
{
    m_previousScreen->Close();
    Close();
}

void ExportNative::updateArchiveList(void)
{
    m_archiveButtonList->Reset();

    if (m_archiveList.empty())
    {
        m_titleText->Reset();
        m_datetimeText->Reset();
        m_descriptionText->Reset();
        m_filesizeText->Reset();
        m_nofilesText->Show();
    }
    else
    {
        for (auto *a : std::as_const(m_archiveList))
        {
            auto* item = new MythUIButtonListItem(m_archiveButtonList, a->title);
            item->SetData(QVariant::fromValue(a));
        }

        m_archiveButtonList->SetItemCurrent(m_archiveButtonList->GetItemFirst());
        titleChanged(m_archiveButtonList->GetItemCurrent());
        m_nofilesText->Hide();
    }

    updateSizeBar();
}

void ExportNative::getArchiveListFromDB(void)
{
    while (!m_archiveList.isEmpty())
         delete m_archiveList.takeFirst();
    m_archiveList.clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT intid, type, title, subtitle, description, size, "
                  "startdate, starttime, filename, hascutlist "
                  "FROM archiveitems WHERE type = 'Recording' OR type = 'Video' "
                  "ORDER BY title, subtitle");

    if (query.exec())
    {
        while (query.next())
        {
            auto *item = new ArchiveItem;

            item->id = query.value(0).toInt();
            item->type = query.value(1).toString();
            item->title = query.value(2).toString();
            item->subtitle = query.value(3).toString();
            item->description = query.value(4).toString();
            item->size = query.value(5).toLongLong();
            item->startDate = query.value(6).toString();
            item->startTime = query.value(7).toString();
            item->filename = query.value(8).toString();
            item->hasCutlist = (query.value(9).toInt() > 0);
            item->useCutlist = false;
            item->editedDetails = false;

            m_archiveList.append(item);
        }
    }
}

void ExportNative::getArchiveList(void)
{
    getArchiveListFromDB();
    updateArchiveList();
}

void ExportNative::loadConfiguration(void)
{
    m_bCreateISO = (gCoreContext->GetSetting("MythNativeCreateISO", "0") == "1");
    m_bDoBurn = (gCoreContext->GetSetting("MythNativeBurnDVDr", "1") == "1");
    m_bEraseDvdRw = (gCoreContext->GetSetting("MythNativeEraseDvdRw", "0") == "1");
    m_saveFilename = gCoreContext->GetSetting("MythNativeSaveFilename", "");
}

void ExportNative::saveConfiguration(void)
{
    // remove all old archive items from DB
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM archiveitems;");
    if (!query.exec())
        MythDB::DBError("ExportNative::saveConfiguration - "
                        "deleting archiveitems", query);

    // save new list of archive items to DB
    query.prepare("INSERT INTO archiveitems (type, title, subtitle, "
                    "description, startdate, starttime, size, filename, hascutlist, "
                    "duration, cutduration, videowidth, videoheight, filecodec,"
                    "videocodec, encoderprofile) "
                    "VALUES(:TYPE, :TITLE, :SUBTITLE, :DESCRIPTION, :STARTDATE, "
                    ":STARTTIME, :SIZE, :FILENAME, :HASCUTLIST, :DURATION, "
                    ":CUTDURATION, :VIDEOWIDTH, :VIDEOHEIGHT, :FILECODEC, "
                    ":VIDEOCODEC, :ENCODERPROFILE);");
    for (const auto * a : std::as_const(m_archiveList))
    {
        query.bindValue(":TYPE", a->type);
        query.bindValue(":TITLE", a->title);
        query.bindValue(":SUBTITLE", a->subtitle);
        query.bindValue(":DESCRIPTION", a->description);
        query.bindValue(":STARTDATE", a->startDate);
        query.bindValue(":STARTTIME", a->startTime);
        query.bindValue(":SIZE", 0);
        query.bindValue(":FILENAME", a->filename);
        query.bindValue(":HASCUTLIST", a->hasCutlist);
        query.bindValue(":DURATION", 0);
        query.bindValue(":CUTDURATION", 0);
        query.bindValue(":VIDEOWIDTH", 0);
        query.bindValue(":VIDEOHEIGHT", 0);
        query.bindValue(":FILECODEC", "");
        query.bindValue(":VIDEOCODEC", "");
        query.bindValue(":ENCODERPROFILE", "");

        if (!query.exec())
            MythDB::DBError("archive item insert", query);
    }
}

void ExportNative::ShowMenu()
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *menuPopup = new MythDialogBox(tr("Menu"), popupStack, "actionmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);

    menuPopup->SetReturnEvent(this, "action");

    menuPopup->AddButton(tr("Remove Item"), &ExportNative::removeItem);
}

void ExportNative::removeItem()
{
    MythUIButtonListItem *item = m_archiveButtonList->GetItemCurrent();
    auto *curItem = item->GetData().value<ArchiveItem *>();

    if (!curItem)
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM archiveitems WHERE filename = :FILENAME;");
    query.bindValue(":FILENAME", curItem->filename);
    if (query.exec() && query.numRowsAffected())
    {
        getArchiveList();
    }
}

void ExportNative::createConfigFile(const QString &filename)
{
    QDomDocument doc("NATIVEARCHIVEJOB");

    QDomElement root = doc.createElement("nativearchivejob");
    doc.appendChild(root);

    QDomElement job = doc.createElement("job");
    root.appendChild(job);

    QDomElement media = doc.createElement("media");
    job.appendChild(media);

    // now loop though selected archive items and add them to the xml file
    for (const auto * a : std::as_const(m_archiveList))
    {
        QDomElement file = doc.createElement("file");
        file.setAttribute("type", a->type.toLower() );
        file.setAttribute("title", a->title);
        file.setAttribute("filename", a->filename);
        file.setAttribute("delete", "0");
        media.appendChild(file);
    }

    // add the options to the xml file
    QDomElement options = doc.createElement("options");
    options.setAttribute("createiso", static_cast<int>(m_bCreateISO));
    options.setAttribute("doburn", static_cast<int>(m_bDoBurn));
    options.setAttribute("mediatype", m_archiveDestination.type);
    options.setAttribute("dvdrsize", (qint64)m_archiveDestination.freeSpace);
    options.setAttribute("erasedvdrw", static_cast<int>(m_bEraseDvdRw));
    options.setAttribute("savedirectory", m_saveFilename);
    job.appendChild(options);

    // finally save the xml to the file
    QFile f(filename);
    if (!f.open(QIODevice::WriteOnly))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("ExportNative::createConfigFile: "
                    "Failed to open file for writing - %1") .arg(filename));
        return;
    }

    QTextStream t(&f);
    t << doc.toString(4);
    f.close();
}

void ExportNative::runScript()
{
    QString tempDir = getTempDirectory();
    QString logDir = tempDir + "logs";
    QString configDir = tempDir + "config";
    QString commandline;

    // remove any existing logs
    myth_system("rm -f " + logDir + "/*.log");

    // remove cancel flag file if present
    if (QFile::exists(logDir + "/mythburncancel.lck"))
        QFile::remove(logDir + "/mythburncancel.lck");

    createConfigFile(configDir + "/mydata.xml");
    commandline = "mytharchivehelper --logpath " + logDir + " --nativearchive "
                  "--outfile " + configDir + "/mydata.xml";  // job file

    uint flags = kMSRunBackground | kMSDontBlockInputDevs |
                 kMSDontDisableDrawing;
    uint retval = myth_system(commandline, flags);
    if (retval != GENERIC_EXIT_RUNNING && retval != GENERIC_EXIT_OK)
    {
        ShowOkPopup(tr("It was not possible to create the DVD. "
                       "An error occured when running the scripts") );
        return;
    }

    showLogViewer();
}

void ExportNative::handleAddRecording()
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *selector = new RecordingSelector(mainStack, &m_archiveList);

    connect(selector, &RecordingSelector::haveResult,
            this, &ExportNative::selectorClosed);

    if (selector->Create())
        mainStack->AddScreen(selector);
}

void ExportNative::selectorClosed(bool ok)
{
    if (ok)
        updateArchiveList();
}

void ExportNative::handleAddVideo()
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT title FROM videometadata");
    if (query.exec() && query.size())
    {
    }
    else
    {
        ShowOkPopup(tr("You don't have any videos!"));
        return;
    }

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *selector = new VideoSelector(mainStack, &m_archiveList);

    connect(selector, &VideoSelector::haveResult,
            this, &ExportNative::selectorClosed);

    if (selector->Create())
        mainStack->AddScreen(selector);
}
