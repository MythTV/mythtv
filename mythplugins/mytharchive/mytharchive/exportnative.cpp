#include <unistd.h>
#include <stdlib.h>

#include <iostream>
#include <cstdlib>

// qt
#include <QFile>
#include <QKeyEvent>
#include <QTextStream>
#include <QDomDocument>

// myth
#include <mythtv/mythcontext.h>
#include <mythtv/libmythtv/remoteutil.h>
#include <mythtv/libmythtv/programinfo.h>
#include <mythtv/libmythdb/mythdb.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythuitext.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuicheckbox.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuiprogressbar.h>
#include <libmythui/mythmainwindow.h>

// mytharchive
#include "exportnative.h"
#include "fileselector.h"
#include "archiveutil.h"
#include "recordingselector.h"
#include "videoselector.h"
#include "logviewer.h"

ExportNative::ExportNative(MythScreenStack *parent, MythScreenType *previousScreen,
                           ArchiveDestination archiveDestination, QString name)
             : MythScreenType(parent, name)
{
    m_previousScreen = previousScreen;
    m_archiveDestination = archiveDestination;
}

ExportNative::~ExportNative(void)
{
    saveConfiguration();

    while (!m_archiveList.isEmpty())
         delete m_archiveList.takeFirst();
    m_archiveList.clear();
}

bool ExportNative::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("mythnative-ui.xml", "exportnative", this);

    if (!foundtheme)
        return false;

    try
    {
        m_nextButton = GetMythUIButton("next_button");
        m_prevButton = GetMythUIButton("prev_button");
        m_cancelButton = GetMythUIButton("cancel_button");

        m_titleText = GetMythUIText("progtitle");
        m_datetimeText = GetMythUIText("progdatetime");
        m_descriptionText = GetMythUIText("progdescription");
        m_filesizeText = GetMythUIText("filesize");
        m_nofilesText = GetMythUIText("nofiles");
        m_sizeBar = GetMythUIProgressBar("size_bar");
        m_archiveButtonList = GetMythUIButtonList("archivelist");
        m_addrecordingButton = GetMythUIButton("addrecording_button");
        m_addvideoButton = GetMythUIButton("addvideo_button");

        m_maxsizeText = GetMythUIText("maxsize", true);
        m_minsizeText = GetMythUIText("minsize", true);
        m_currsizeText = GetMythUIText("currentsize", true);
        m_currsizeErrText = GetMythUIText("currentsize_error", true);

    }
    catch (QString &error)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'exportnative'\n\t\t\t"
                              "Error was: " + error);
        return false;
    }

    m_nextButton->SetText(tr("Finish"));
    connect(m_nextButton, SIGNAL(Clicked()), this, SLOT(handleNextPage()));

    m_prevButton->SetText(tr("Previous"));
    connect(m_prevButton, SIGNAL(Clicked()), this, SLOT(handlePrevPage()));

    m_cancelButton->SetText(tr("Cancel"));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(handleCancel()));


    getArchiveList();
    connect(m_archiveButtonList, SIGNAL(itemSelected(MythUIButtonListItem *)),
            this, SLOT(titleChanged(MythUIButtonListItem *)));

    m_addrecordingButton->SetText(tr("Add Recording"));
    connect(m_addrecordingButton, SIGNAL(Clicked()), this, SLOT(handleAddRecording()));

    m_addvideoButton->SetText(tr("Add Video"));
    connect(m_addvideoButton, SIGNAL(Clicked()), this, SLOT(handleAddVideo()));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    SetFocusWidget(m_nextButton);

    loadConfiguration();

    return true;
}

bool ExportNative::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Archive", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
        {
            showMenu();
        }
        else if (action == "DELETEITEM")
        {
            removeItem();
        }

        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void ExportNative::updateSizeBar()
{
    long long size = 0;
    ArchiveItem *a;

    for (int x = 0; x < m_archiveList.size(); x++)
    {
        a = m_archiveList.at(x);
        size += a->size; 
    }

    m_usedSpace = size / 1024 / 1024;
    uint freeSpace = m_archiveDestination.freeSpace / 1024;

    QString tmpSize;

    m_sizeBar->SetTotal(freeSpace);
    m_sizeBar->SetUsed(m_usedSpace);

    tmpSize.sprintf("%0d Mb", freeSpace);

    if (m_maxsizeText)
        m_maxsizeText->SetText(tmpSize);

    if (m_minsizeText)
        m_minsizeText->SetText("0 Mb");

    tmpSize.sprintf("%0d Mb", m_usedSpace);

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
    ArchiveItem *a;

    a = (ArchiveItem *) item->getData();

    if (!a)
        return;

    m_titleText->SetText(a->title);

    m_datetimeText->SetText(a->startDate + " " + a->startTime);

    m_descriptionText->SetText(
                (a->subtitle != "" ? a->subtitle + "\n" : "") + a->description);

    m_filesizeText->SetText(formatSize(a->size / 1024, 2));
}

void ExportNative::handleNextPage()
{
    if (m_archiveList.size() == 0)
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

    if (m_archiveList.size() == 0)
    {
        m_titleText->SetText("");
        m_datetimeText->SetText("");
        m_descriptionText->SetText("");
        m_filesizeText->SetText("");
        m_nofilesText->Show();
    }
    else
    {
        ArchiveItem *a;
        for (int x = 0;  x < m_archiveList.size(); x++)
        {
            a = m_archiveList.at(x);

            MythUIButtonListItem* item = new MythUIButtonListItem(m_archiveButtonList, a->title);
            item->setData(a);
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
    query.exec();
    if (query.isActive() && query.size())
    {
        while (query.next())
        {
            ArchiveItem *item = new ArchiveItem;

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
    m_bCreateISO = (gContext->GetSetting("MythNativeCreateISO", "0") == "1");
    m_bDoBurn = (gContext->GetSetting("MythNativeBurnDVDr", "1") == "1");
    m_bEraseDvdRw = (gContext->GetSetting("MythNativeEraseDvdRw", "0") == "1");
    m_saveFilename = gContext->GetSetting("MythNativeSaveFilename", "");
}

void ExportNative::saveConfiguration(void)
{
    // remove all old archive items from DB
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM archiveitems;");
    query.exec();

    // save new list of archive items to DB
    ArchiveItem *a;
    for (int x = 0; x < m_archiveList.size(); x++)
    {
        a = m_archiveList.at(x);

        query.prepare("INSERT INTO archiveitems (type, title, subtitle, "
                "description, startdate, starttime, size, filename, hascutlist, "
                "duration, cutduration, videowidth, videoheight, filecodec,"
                "videocodec, encoderprofile) "
                "VALUES(:TYPE, :TITLE, :SUBTITLE, :DESCRIPTION, :STARTDATE, "
                ":STARTTIME, :SIZE, :FILENAME, :HASCUTLIST, :DURATION, "
                ":CUTDURATION, :VIDEOWIDTH, :VIDEOHEIGHT, :FILECODEC, "
                ":VIDEOCODEC, :ENCODERPROFILE);");
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

void ExportNative::showMenu()
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythDialogBox *menuPopup = new MythDialogBox("Menu", popupStack, "actionmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);

    menuPopup->SetReturnEvent(this, "action");

    menuPopup->AddButton(tr("Remove Item"), SLOT(removeItem()));
    menuPopup->AddButton(tr("Cancel"), NULL);
}

void ExportNative::removeItem()
{
    MythUIButtonListItem *item = m_archiveButtonList->GetItemCurrent();
    ArchiveItem *curItem = (ArchiveItem *) item->getData();

    if (!curItem)
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM archiveitems WHERE filename = :FILENAME;");
    query.bindValue(":FILENAME", curItem->filename);
    query.exec();
    if (query.isActive() && query.numRowsAffected())
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
    ArchiveItem *a;
    for (int x = 0; x < m_archiveList.size(); x++)
    {
        a = m_archiveList.at(x);

        QDomElement file = doc.createElement("file");
        file.setAttribute("type", a->type.lower() );
        file.setAttribute("title", a->title);
        file.setAttribute("filename", a->filename);
        file.setAttribute("delete", "0");
        media.appendChild(file);
    }

    // add the options to the xml file
    QDomElement options = doc.createElement("options");
    options.setAttribute("createiso", m_bCreateISO);
    options.setAttribute("doburn", m_bDoBurn);
    options.setAttribute("mediatype", m_archiveDestination.type);
    options.setAttribute("dvdrsize", m_archiveDestination.freeSpace);
    options.setAttribute("erasedvdrw", m_bEraseDvdRw);
    options.setAttribute("savedirectory", m_saveFilename);
    job.appendChild(options);

    // finally save the xml to the file
    QFile f(filename);
    if (!f.open(QIODevice::WriteOnly))
    {
        VERBOSE(VB_IMPORTANT, QString("ExportNative::createConfigFile: "
                "Failed to open file for writing - %1")
                .arg(filename.toLocal8Bit().constData()));
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

    // remove existing progress.log if present
    if (QFile::exists(logDir + "/progress.log"))
        QFile::remove(logDir + "/progress.log");

    // remove cancel flag file if present
    if (QFile::exists(logDir + "/mythburncancel.lck"))
        QFile::remove(logDir + "/mythburncancel.lck");

    createConfigFile(configDir + "/mydata.xml");
    commandline = "mytharchivehelper -n " + configDir + "/mydata.xml";  // job file
    commandline += " > "  + logDir + "/progress.log 2>&1 &";            // Logs

    int state = system(commandline);

    if (state != 0)
    {
        ShowOkPopup(QObject::tr("It was not possible to create the DVD. "
                                "An error occured when running the scripts") );
    }
    else
    {
        showLogViewer();
    }
}

void ExportNative::handleAddRecording()
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    RecordingSelector *selector = new RecordingSelector(mainStack, &m_archiveList);

    connect(selector, SIGNAL(haveResult(bool)),
            this, SLOT(selectorClosed(bool)));

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
    query.exec();
    if (query.isActive() && query.size())
    {
    }
    else
    {
        ShowOkPopup(tr("You don't have any videos!"));
        return;
    }

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    VideoSelector *selector = new VideoSelector(mainStack, &m_archiveList);

    connect(selector, SIGNAL(haveResult(bool)),
            this, SLOT(selectorClosed(bool)));

    if (selector->Create())
        mainStack->AddScreen(selector);
}
