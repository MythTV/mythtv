#include <unistd.h>
#include <stdlib.h>

#include <iostream>
#include <cstdlib>

// qt
#include <QFile>
#include <QKeyEvent>
#include <QTextStream>

// myth
#include <mythtv/mythcontext.h>
#include <mythtv/libmythtv/remoteutil.h>
#include <mythtv/libmythtv/programinfo.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythuitext.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuicheckbox.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuiprogressbar.h>

// mytharchive
#include "exportnative.h"
#include "fileselector.h"
#include "archiveutil.h"
#include "recordingselector.h"
#include "videoselector.h"

ExportNative::ExportNative(MythScreenStack *parent, MythScreenType *previousScreen,
                           ArchiveDestination archiveDestination, QString name)
             : MythScreenType(parent, name)
{
    m_previousScreen = previousScreen;
    m_archiveDestination = archiveDestination;
    archiveList = NULL;
}

ExportNative::~ExportNative(void)
{
    if (archiveList)
        delete archiveList;
}

MythUIText* ExportNative::GetMythUIText(const QString &name, bool optional)
{
    MythUIText *text = dynamic_cast<MythUIText *> (GetChild(name));

    if (!optional && !text)
        throw name;

    return text;
}

MythUIButton* ExportNative::GetMythUIButton(const QString &name, bool optional)
{
    MythUIButton *button = dynamic_cast<MythUIButton *> (GetChild(name));

    if (!optional && !button)
        throw name;

    return button;
}

MythUIButtonList* ExportNative::GetMythUIButtonList(const QString &name, bool optional)
{
    MythUIButtonList *list = dynamic_cast<MythUIButtonList *> (GetChild(name));

    if (!optional && !list)
        throw name;

    return list;
}

MythUIProgressBar* ExportNative::GetMythUIProgressBar(const QString &name, bool optional)
{
    MythUIProgressBar *bar = dynamic_cast<MythUIProgressBar *> (GetChild(name));

    if (!optional && !bar)
        throw name;

    return bar;
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
        m_archive_list = GetMythUIButtonList("archivelist");
        m_addrecordingButton = GetMythUIButton("addrecording_button");
        m_addvideoButton = GetMythUIButton("addvideo_button");

        m_maxsizeText = GetMythUIText("maxsize", true);
        m_minsizeText = GetMythUIText("minsize", true);
        m_currsizeText = GetMythUIText("currentsize", true);
        m_currsizeErrText = GetMythUIText("currentsize_error", true);

    }
    catch (const QString name)
    {
        VERBOSE(VB_IMPORTANT, QString("Theme is missing a critical theme element ('%1')")
                                      .arg(name));
        return false;
    }

    m_nextButton->SetText(tr("Finish"));
    connect(m_nextButton, SIGNAL(Clicked()), this, SLOT(handleNextPage()));

    m_prevButton->SetText(tr("Previous"));
    connect(m_prevButton, SIGNAL(Clicked()), this, SLOT(handlePrevPage()));

    m_cancelButton->SetText(tr("Cancel"));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(handleCancel()));


    getArchiveList();
    connect(m_archive_list, SIGNAL(itemSelected(MythUIButtonListItem *)),
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
    gContext->GetMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
        {
            showMenu();
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
    NativeItem *a;

    vector<NativeItem *>::iterator i = archiveList->begin();
    for ( ; i != archiveList->end(); i++)
    {
        a = *i;
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
    NativeItem *a;

    a = (NativeItem *) item->getData();

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
    if (archiveList->size() == 0)
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
    m_archive_list->Reset();

    if (archiveList->size() == 0)
    {
        m_titleText->SetText("");
        m_datetimeText->SetText("");
        m_descriptionText->SetText("");
        m_filesizeText->SetText("");
        m_nofilesText->Show();
    }
    else
    {
        NativeItem *a;
        vector<NativeItem *>::iterator i = archiveList->begin();
        for ( ; i != archiveList->end(); i++)
        {
            a = *i;

            MythUIButtonListItem* item = new MythUIButtonListItem(m_archive_list, a->title);
            item->setData(a);
        }

        m_archive_list->SetItemCurrent(m_archive_list->GetItemFirst());
        titleChanged(m_archive_list->GetItemCurrent());
        m_nofilesText->Hide();
    }

    updateSizeBar();
}

void ExportNative::getArchiveListFromDB(void)
{
    if (!archiveList)
        archiveList = new vector<NativeItem*>;

    archiveList->clear();

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
            NativeItem *item = new NativeItem;

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

            archiveList->push_back(item);
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
    MythUIButtonListItem *item = m_archive_list->GetItemCurrent();
    NativeItem *curItem = (NativeItem *) item->getData();

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
    NativeItem *a;

    vector<NativeItem *>::iterator i = archiveList->begin();
    for ( ; i != archiveList->end(); i++)
    {
        a = *i;

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
}

void ExportNative::handleAddRecording()
{
    RecordingSelector selector(gContext->GetMainWindow(),
                               "recording_selector", "mytharchive-", "recording selector");
    selector.exec();

    getArchiveList();
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
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), QObject::tr("Video Selector"),
                                  QObject::tr("You don't have any videos!"));
        return;
    }

    VideoSelector selector(gContext->GetMainWindow(),
                           "video_selector", "mytharchive-", "video selector");
    selector.exec();

    getArchiveList();
}
