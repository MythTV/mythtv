#include <unistd.h>

#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <cstdlib>
#include <sys/wait.h>  // for WIFEXITED and WEXITSTATUS

// qt
#include <QDir>
#include <QDomDocument>
#include <QApplication>
#include <QKeyEvent>
#include <QTextStream>

// myth
#include <mythcontext.h>
#include <mythdb.h>
#include <mythdirs.h>
#include <mythprogressdialog.h>
#include <mythuihelper.h>
#include <mythdialogbox.h>
#include <mythuitext.h>
#include <mythuibutton.h>
#include <mythuicheckbox.h>
#include <mythuibuttonlist.h>
#include <mythuiprogressbar.h>
#include <mythdate.h>
#include <mythsystemlegacy.h>
#include <mythmiscutil.h>
#include <exitcodes.h>

// mytharchive
#include "archiveutil.h"
#include "mythburn.h"
#include "editmetadata.h"
#include "fileselector.h"
#include "thumbfinder.h"
#include "recordingselector.h"
#include "videoselector.h"
#include "logviewer.h"

using namespace std;

MythBurn::MythBurn(MythScreenStack   *parent,
                   MythScreenType    *destinationScreen,
                   MythScreenType    *themeScreen,
                   ArchiveDestination archiveDestination, QString name) :
    MythScreenType(parent, name),
    m_destinationScreen(destinationScreen),
    m_themeScreen(themeScreen),
    m_archiveDestination(archiveDestination),
    m_bCreateISO(false),
    m_bDoBurn(false),
    m_bEraseDvdRw(false),
    m_saveFilename(""),
    m_theme(),
    m_moveMode(false),
    m_nextButton(NULL),
    m_prevButton(NULL),
    m_cancelButton(NULL),
    m_archiveButtonList(NULL),
    m_nofilesText(NULL),
    m_addrecordingButton(NULL),
    m_addvideoButton(NULL),
    m_addfileButton(NULL),
    m_sizeBar(NULL),
    m_maxsizeText(NULL),
    m_minsizeText(NULL),
    m_currentsizeErrorText(NULL),
    m_currentsizeText(NULL)
{
    // remove any old thumb images
    QString thumbDir = getTempDirectory() + "/config/thumbs";
    QDir dir(thumbDir);
    if (dir.exists() && !MythRemoveDirectory(dir))
        LOG(VB_GENERAL, LOG_ERR, "MythBurn: Failed to clear thumb directory");
}

MythBurn::~MythBurn(void)
{
    saveConfiguration();

    while (!m_profileList.isEmpty())
         delete m_profileList.takeFirst();
    m_profileList.clear();

    while (!m_archiveList.isEmpty())
         delete m_archiveList.takeFirst();
    m_archiveList.clear();
}

bool MythBurn::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("mythburn-ui.xml", "mythburn", this);

    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_nextButton, "next_button", &err);
    UIUtilE::Assign(this, m_prevButton, "prev_button", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel_button", &err);
    UIUtilE::Assign(this, m_nofilesText, "nofiles", &err);
    UIUtilE::Assign(this, m_archiveButtonList, "archivelist", &err);
    UIUtilE::Assign(this, m_addrecordingButton, "addrecording_button", &err);
    UIUtilE::Assign(this, m_addvideoButton, "addvideo_button", &err);
    UIUtilE::Assign(this, m_addfileButton, "addfile_button", &err);
    UIUtilE::Assign(this, m_maxsizeText, "maxsize", &err);
    UIUtilE::Assign(this, m_minsizeText, "minsize", &err);
    UIUtilE::Assign(this, m_currentsizeErrorText, "currentsize_error", &err);
    UIUtilE::Assign(this, m_currentsizeText, "currentsize", &err);
    UIUtilE::Assign(this, m_sizeBar, "size_bar", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'mythburn'");
        return false;
    }

    connect(m_nextButton, SIGNAL(Clicked()), this, SLOT(handleNextPage()));
    connect(m_prevButton, SIGNAL(Clicked()), this, SLOT(handlePrevPage()));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(handleCancel()));


    loadEncoderProfiles();
    loadConfiguration();

    updateArchiveList();

    connect(m_addrecordingButton, SIGNAL(Clicked()),
            this, SLOT(handleAddRecording()));

    connect(m_addvideoButton, SIGNAL(Clicked()), this, SLOT(handleAddVideo()));
    connect(m_addfileButton, SIGNAL(Clicked()), this, SLOT(handleAddFile()));
    connect(m_archiveButtonList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            this, SLOT(itemClicked(MythUIButtonListItem *)));

    BuildFocusList();

    SetFocusWidget(m_nextButton);

    return true;
}

bool MythBurn::keyPressEvent(QKeyEvent *event)
{
    if (!m_moveMode && GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Archive", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        // if we are currently moving an item,
        // we only accept UP/DOWN/SELECT/ESCAPE
        if (m_moveMode)
        {
            MythUIButtonListItem *item = m_archiveButtonList->GetItemCurrent();
            if (!item)
                return false;

            if (action == "SELECT" || action == "ESCAPE")
            {
                m_moveMode = false;
                item->DisplayState("off", "movestate");
            }
            else if (action == "UP")
            {
                item->MoveUpDown(true);
            }
            else if (action == "DOWN")
            {
                item->MoveUpDown(false);
            }

            return true;
        }

        if (action == "MENU")
        {
            showMenu();
        }
        else if (action == "DELETE")
        {
            removeItem();
        }
        else if (action == "INFO")
        {
            editThumbnails();
        }
        else if (action == "TOGGLECUT")
        {
            toggleUseCutlist();
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void MythBurn::updateSizeBar(void)
{
    int64_t size = 0;
    ArchiveItem *a;
    for (int x = 0; x < m_archiveList.size(); x++)
    {
        a = m_archiveList.at(x);
        size += a->newsize;
    }

    uint usedSpace = size / 1024 / 1024;

    QString tmpSize;

    m_sizeBar->SetTotal(m_archiveDestination.freeSpace / 1024);
    m_sizeBar->SetUsed(usedSpace);

    tmpSize = QString("%1 Mb").arg(m_archiveDestination.freeSpace / 1024);

    m_maxsizeText->SetText(tmpSize);

    m_minsizeText->SetText("0 Mb");

    tmpSize = QString("%1 Mb").arg(usedSpace);

    if (usedSpace > m_archiveDestination.freeSpace / 1024)
    {
        m_currentsizeText->Hide();

        m_currentsizeErrorText->SetText(tmpSize);
        m_currentsizeErrorText->Show();
    }
    else
    {
        m_currentsizeErrorText->Hide();

        m_currentsizeText->SetText(tmpSize);
        m_currentsizeText->Show();
    }
}

void MythBurn::loadEncoderProfiles()
{
    EncoderProfile *item = new EncoderProfile;
    item->name = "NONE";
    item->description = "";
    item->bitrate = 0.0f;
    m_profileList.append(item);

    // find the encoding profiles
    // first look in the ConfDir (~/.mythtv)
    QString filename = GetConfDir() +
            "/MythArchive/ffmpeg_dvd_" +
            ((gCoreContext->GetSetting("MythArchiveVideoFormat", "pal")
                .toLower() == "ntsc") ? "ntsc" : "pal") + ".xml";

    if (!QFile::exists(filename))
    {
        // not found yet so use the default profiles
        filename = GetShareDir() +
            "mytharchive/encoder_profiles/ffmpeg_dvd_" +
            ((gCoreContext->GetSetting("MythArchiveVideoFormat", "pal")
                .toLower() == "ntsc") ? "ntsc" : "pal") + ".xml";
    }

    LOG(VB_GENERAL, LOG_NOTICE,
        "MythArchive: Loading encoding profiles from " + filename);

    QDomDocument doc("mydocument");
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
        return;

    if (!doc.setContent( &file ))
    {
        file.close();
        return;
    }
    file.close();

    QDomElement docElem = doc.documentElement();
    QDomNodeList profileNodeList = doc.elementsByTagName("profile");
    QString name, desc, bitrate;

    for (int x = 0; x < (int) profileNodeList.count(); x++)
    {
        QDomNode n = profileNodeList.item(x);
        QDomElement e = n.toElement();
        QDomNode n2 = e.firstChild();
        while (!n2.isNull())
        {
            QDomElement e2 = n2.toElement();
            if(!e2.isNull())
            {
                if (e2.tagName() == "name")
                    name = e2.text();
                if (e2.tagName() == "description")
                    desc = e2.text();
                if (e2.tagName() == "bitrate")
                    bitrate = e2.text();

            }
            n2 = n2.nextSibling();

        }

        EncoderProfile *item = new EncoderProfile;
        item->name = name;
        item->description = desc;
        item->bitrate = bitrate.toFloat();
        m_profileList.append(item);
    }
}

void MythBurn::toggleUseCutlist(void)
{
    MythUIButtonListItem *item = m_archiveButtonList->GetItemCurrent();
    ArchiveItem *a = qVariantValue<ArchiveItem *>(item->GetData());

    if (!a)
        return;

    if (!a->hasCutlist)
        return;

    a->useCutlist = !a->useCutlist;

    if (a->hasCutlist)
    {
        if (a->useCutlist)
        {
            item->SetText(tr("Using Cut List"), "cutlist");
            item->DisplayState("using", "cutliststatus");
        }
        else
        {
            item->SetText(tr("Not Using Cut List"), "cutlist");
            item->DisplayState("notusing", "cutliststatus");
        }
    }
    else
    {
        item->SetText(tr("No Cut List"), "cutlist");
        item->DisplayState("none", "cutliststatus");
    }
    recalcItemSize(a);
    updateSizeBar();
}

void MythBurn::handleNextPage()
{
    if (m_archiveList.size() == 0)
    {
        ShowOkPopup(tr("You need to add at least one item to archive!"));
        return;
    }

    runScript();
}

void MythBurn::handlePrevPage()
{
    Close();
}

void MythBurn::handleCancel()
{
    m_destinationScreen->Close();
    m_themeScreen->Close();
    Close();
}

QString MythBurn::loadFile(const QString &filename)
{
    QString res = "";

    QFile file(filename);

    if (!file.exists())
        return "";

    if (file.open( QIODevice::ReadOnly ))
    {
        QTextStream stream(&file);

        while ( !stream.atEnd() )
        {
            res = res + stream.readLine();
        }
        file.close();
    }
    else
        return "";

    return res;
}

void MythBurn::updateArchiveList(void)
{
    QString message = tr("Retrieving File Information. Please Wait...");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythUIBusyDialog *busyPopup = new
            MythUIBusyDialog(message, popupStack, "mythburnbusydialog");

    if (busyPopup->Create())
        popupStack->AddScreen(busyPopup, false);
    else
    {
        delete busyPopup;
        busyPopup = NULL;
    }

    qApp->processEvents();

    m_archiveButtonList->Reset();

    if (m_archiveList.size() == 0)
    {
        m_nofilesText->Show();
    }
    else
    {
        ArchiveItem *a;
        for (int x = 0; x < m_archiveList.size(); x++)
        {
            qApp->processEvents();
            a = m_archiveList.at(x);

            // get duration of this file
            if (a->duration == 0)
            {
                if (!getFileDetails(a))
                    LOG(VB_GENERAL, LOG_ERR,
                        QString("MythBurn: failed to get file details for: %1").arg(a->filename));
             }

            // get default encoding profile if needed

            if (a->encoderProfile == NULL)
                a->encoderProfile = getDefaultProfile(a);

            recalcItemSize(a);

            MythUIButtonListItem* item = new
                    MythUIButtonListItem(m_archiveButtonList, a->title);
            item->SetData(qVariantFromValue(a));
            item->SetText(a->subtitle, "subtitle");
            item->SetText(a->startDate + " " + a->startTime, "date");
            item->SetText(formatSize(a->newsize / 1024, 2), "size");
            if (a->hasCutlist)
            {
                if (a->useCutlist)
                {
                    item->SetText(tr("Using Cut List"), "cutlist");
                    item->DisplayState("using", "cutliststatus");
                }
                else
                {
                    item->SetText(tr("Not Using Cut List"), "cutlist");
                    item->DisplayState("notusing", "cutliststatus");
                }
            }
            else
            {
                item->SetText(tr("No Cut List"), "cutlist");
                item->DisplayState("none", "cutliststatus");
            }
            item->SetText(tr("Encoder: ") + a->encoderProfile->name, "profile");
        }

        m_nofilesText->Hide();

        m_archiveButtonList->SetItemCurrent(
            m_archiveButtonList->GetItemFirst());
    }

    updateSizeBar();

    if (busyPopup)
        busyPopup->Close();
}

bool MythBurn::isArchiveItemValid(const QString &type, const QString &filename)
{
    if (type == "Recording")
    {
        QString baseName = getBaseName(filename);

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT title FROM recorded WHERE basename = :FILENAME");
        query.bindValue(":FILENAME", baseName);
        if (query.exec() && query.size())
            return true;
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("MythArchive: Recording not found (%1)")
                    .arg(filename));
        }
    }
    else if (type == "Video")
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT title FROM videometadata"
                      " WHERE filename = :FILENAME");
        query.bindValue(":FILENAME", filename);
        if (query.exec() && query.size())
            return true;
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("MythArchive: Video not found (%1)").arg(filename));
        }
    }
    else if (type == "File")
    {
        if (QFile::exists(filename))
            return true;
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("MythArchive: File not found (%1)").arg(filename));
        }
    }

    LOG(VB_GENERAL, LOG_NOTICE, "MythArchive: Archive item removed from list");

    return false;
}

EncoderProfile *MythBurn::getDefaultProfile(ArchiveItem *item)
{
    if (!item)
        return m_profileList.at(0);

    EncoderProfile *profile = NULL;

    // is the file an mpeg2 file?
    if (item->videoCodec.toLower() == "mpeg2video (main)")
    {
        // does the file already have a valid DVD resolution?
        if (gCoreContext->GetSetting("MythArchiveVideoFormat", "pal").toLower()
                    == "ntsc")
        {
            if ((item->videoWidth == 720 && item->videoHeight == 480) ||
                (item->videoWidth == 704 && item->videoHeight == 480) ||
                (item->videoWidth == 352 && item->videoHeight == 480) ||
                (item->videoWidth == 352 && item->videoHeight == 240))
            {
                // don't need to re-encode
                profile = m_profileList.at(0);
            }
        }
        else
        {
            if ((item->videoWidth == 720 && item->videoHeight == 576) ||
                (item->videoWidth == 704 && item->videoHeight == 576) ||
                (item->videoWidth == 352 && item->videoHeight == 576) ||
                (item->videoWidth == 352 && item->videoHeight == 288))
            {
                // don't need to re-encode
                profile = m_profileList.at(0);
            }
        }
    }

    if (!profile)
    {
        // file needs re-encoding - use default profile setting
        QString defaultProfile =
                gCoreContext->GetSetting("MythArchiveDefaultEncProfile", "SP");

        for (int x = 0; x < m_profileList.size(); x++)
            if (m_profileList.at(x)->name == defaultProfile)
                profile = m_profileList.at(x);
    }

    return profile;
}

void MythBurn::createConfigFile(const QString &filename)
{
    QDomDocument doc("mythburn");

    QDomElement root = doc.createElement("mythburn");
    doc.appendChild(root);

    QDomElement job = doc.createElement("job");
    job.setAttribute("theme", m_theme);
    root.appendChild(job);

    QDomElement media = doc.createElement("media");
    job.appendChild(media);

    // now loop though selected archive items and add them to the xml file
    ArchiveItem *a;
    MythUIButtonListItem *item;
    for (int x = 0; x < m_archiveButtonList->GetCount(); x++)
    {
        item = m_archiveButtonList->GetItemAt(x);
        if (!item)
            continue;

        a = qVariantValue<ArchiveItem *>(item->GetData());
        if (!a)
            continue;

        QDomElement file = doc.createElement("file");
        file.setAttribute("type", a->type.toLower() );
        file.setAttribute("usecutlist", a->useCutlist);
        file.setAttribute("filename", a->filename);
        file.setAttribute("encodingprofile", a->encoderProfile->name);
        if (a->editedDetails)
        {
            QDomElement details = doc.createElement("details");
            file.appendChild(details);
            details.setAttribute("title", a->title);
            details.setAttribute("subtitle", a->subtitle);
            details.setAttribute("startdate", a->startDate);
            details.setAttribute("starttime", a->startTime);
            QDomText desc = doc.createTextNode(a->description);
            details.appendChild(desc);
        }

        if (a->thumbList.size() > 0)
        {
            QDomElement thumbs = doc.createElement("thumbimages");
            file.appendChild(thumbs);

            for (int x = 0; x < a->thumbList.size(); x++)
            {
                QDomElement thumb = doc.createElement("thumb");
                thumbs.appendChild(thumb);
                ThumbImage *thumbImage = a->thumbList.at(x);
                thumb.setAttribute("caption", thumbImage->caption);
                thumb.setAttribute("filename", thumbImage->filename);
                thumb.setAttribute("frame", (int) thumbImage->frame);
            }
        }

        media.appendChild(file);
    }

    // add the options to the xml file
    QDomElement options = doc.createElement("options");
    options.setAttribute("createiso", m_bCreateISO);
    options.setAttribute("doburn", m_bDoBurn);
    options.setAttribute("mediatype", m_archiveDestination.type);
    options.setAttribute("dvdrsize", (qint64)m_archiveDestination.freeSpace);
    options.setAttribute("erasedvdrw", m_bEraseDvdRw);
    options.setAttribute("savefilename", m_saveFilename);
    job.appendChild(options);

    // finally save the xml to the file
    QFile f(filename);
    if (!f.open(QIODevice::WriteOnly))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("MythBurn::createConfigFile: "
                    "Failed to open file for writing - %1") .arg(filename));
        return;
    }

    QTextStream t(&f);
    t << doc.toString(4);
    f.close();
}

void MythBurn::loadConfiguration(void)
{
    m_theme = gCoreContext->GetSetting("MythBurnMenuTheme", "");
    m_bCreateISO = (gCoreContext->GetSetting("MythBurnCreateISO", "0") == "1");
    m_bDoBurn = (gCoreContext->GetSetting("MythBurnBurnDVDr", "1") == "1");
    m_bEraseDvdRw = (gCoreContext->GetSetting("MythBurnEraseDvdRw", "0") == "1");
    m_saveFilename = gCoreContext->GetSetting("MythBurnSaveFilename", "");

    while (!m_archiveList.isEmpty())
         delete m_archiveList.takeFirst();
    m_archiveList.clear();

    // load selected file list
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT type, title, subtitle, description, startdate, "
                  "starttime, size, filename, hascutlist, duration, "
                  "cutduration, videowidth, videoheight, filecodec, "
                  "videocodec, encoderprofile FROM archiveitems "
                  "ORDER BY intid;");

    if (!query.exec())
    {
        MythDB::DBError("archive item insert", query);
        return;
    }

    while (query.next())
    {
        ArchiveItem *a = new ArchiveItem;
        a->type = query.value(0).toString();
        a->title = query.value(1).toString();
        a->subtitle = query.value(2).toString();
        a->description = query.value(3).toString();
        a->startDate = query.value(4).toString();
        a->startTime = query.value(5).toString();
        a->size = query.value(6).toLongLong();
        a->filename = query.value(7).toString();
        a->hasCutlist = (query.value(8).toInt() == 1);
        a->useCutlist = false;
        a->duration = query.value(9).toInt();
        a->cutDuration = query.value(10).toInt();
        a->videoWidth = query.value(11).toInt();
        a->videoHeight = query.value(12).toInt();
        a->fileCodec = query.value(13).toString();
        a->videoCodec = query.value(14).toString();
        a->encoderProfile = getProfileFromName(query.value(15).toString());
        a->editedDetails = false;
        m_archiveList.append(a);
    }
}

EncoderProfile *MythBurn::getProfileFromName(const QString &profileName)
{
    for (int x = 0; x < m_profileList.size(); x++)
        if (m_profileList.at(x)->name == profileName)
            return m_profileList.at(x);

    return NULL;
}

void MythBurn::saveConfiguration(void)
{
    // remove all old archive items from DB
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM archiveitems;");
    if (!query.exec())
        MythDB::DBError("MythBurn::saveConfiguration - deleting archiveitems",
                        query);

    // save new list of archive items to DB
    ArchiveItem *a;
    MythUIButtonListItem *item;
    for (int x = 0; x < m_archiveButtonList->GetCount(); x++)
    {
        item = m_archiveButtonList->GetItemAt(x);
        if (!item)
            continue;

        a = qVariantValue<ArchiveItem *>(item->GetData());
        if (!a)
            continue;

        query.prepare("INSERT INTO archiveitems (type, title, subtitle, "
                "description, startdate, starttime, size, filename, "
                "hascutlist, duration, cutduration, videowidth, "
                "videoheight, filecodec, videocodec, encoderprofile) "
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
        query.bindValue(":SIZE", (qint64)a->size);
        query.bindValue(":FILENAME", a->filename);
        query.bindValue(":HASCUTLIST", a->hasCutlist);
        query.bindValue(":DURATION", a->duration);
        query.bindValue(":CUTDURATION", a->cutDuration);
        query.bindValue(":VIDEOWIDTH", a->videoWidth);
        query.bindValue(":VIDEOHEIGHT", a->videoHeight);
        query.bindValue(":FILECODEC", a->fileCodec);
        query.bindValue(":VIDEOCODEC", a->videoCodec);
        query.bindValue(":ENCODERPROFILE", a->encoderProfile->name);

        if (!query.exec())
            MythDB::DBError("archive item insert", query);
    }
}

void MythBurn::showMenu()
{
    if (m_archiveList.size() == 0)
        return;

    MythUIButtonListItem *item = m_archiveButtonList->GetItemCurrent();
    ArchiveItem *curItem = qVariantValue<ArchiveItem *>(item->GetData());

    if (!curItem)
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythDialogBox *menuPopup = new MythDialogBox(tr("Menu"),
                                                 popupStack, "actionmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);

    menuPopup->SetReturnEvent(this, "action");

    if (curItem->hasCutlist)
    {
        if (curItem->useCutlist)
            menuPopup->AddButton(tr("Don't Use Cut List"),
                                 SLOT(toggleUseCutlist()));
        else
            menuPopup->AddButton(tr("Use Cut List"),
                                 SLOT(toggleUseCutlist()));
    }

    menuPopup->AddButton(tr("Remove Item"), SLOT(removeItem()));
    menuPopup->AddButton(tr("Edit Details"), SLOT(editDetails()));
    menuPopup->AddButton(tr("Change Encoding Profile"), SLOT(changeProfile()));
    menuPopup->AddButton(tr("Edit Thumbnails"), SLOT(editThumbnails()));
}

void MythBurn::removeItem()
{
    MythUIButtonListItem *item = m_archiveButtonList->GetItemCurrent();
    ArchiveItem *curItem = qVariantValue<ArchiveItem *>(item->GetData());

    if (!curItem)
        return;

    m_archiveList.removeAll(curItem);

    updateArchiveList();
}

void MythBurn::editDetails()
{
    MythUIButtonListItem *item = m_archiveButtonList->GetItemCurrent();
    ArchiveItem *curItem = qVariantValue<ArchiveItem *>(item->GetData());

    if (!curItem)
        return;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    EditMetadataDialog *editor = new EditMetadataDialog(mainStack, curItem);

    connect(editor, SIGNAL(haveResult(bool, ArchiveItem *)),
            this, SLOT(editorClosed(bool, ArchiveItem *)));

    if (editor->Create())
        mainStack->AddScreen(editor);
}

void MythBurn::editThumbnails()
{
    MythUIButtonListItem *item = m_archiveButtonList->GetItemCurrent();
    ArchiveItem *curItem = qVariantValue<ArchiveItem *>(item->GetData());

    if (!curItem)
        return;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    ThumbFinder *finder = new ThumbFinder(mainStack, curItem, m_theme);

    if (finder->Create())
        mainStack->AddScreen(finder);
}

void MythBurn::editorClosed(bool ok, ArchiveItem *item)
{
    MythUIButtonListItem *gridItem = m_archiveButtonList->GetItemCurrent();

    if (ok && item && gridItem)
    {
        // update the grid to reflect any changes
        gridItem->SetText(item->title);
        gridItem->SetText(item->subtitle, "subtitle");
        gridItem->SetText(item->startDate + " " + item->startTime, "date");
    }
}

void MythBurn::changeProfile()
{
    MythUIButtonListItem *item = m_archiveButtonList->GetItemCurrent();
    ArchiveItem *curItem = qVariantValue<ArchiveItem *>(item->GetData());

    if (!curItem)
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    ProfileDialog *profileDialog = new ProfileDialog(popupStack,
                                                     curItem, m_profileList);

    if (profileDialog->Create())
    {
        popupStack->AddScreen(profileDialog, false);
        connect(profileDialog, SIGNAL(haveResult(int)),
                this, SLOT(profileChanged(int)));
    }
}

void MythBurn::profileChanged(int profileNo)
{
    if (profileNo > m_profileList.size() - 1)
        return;

    EncoderProfile *profile = m_profileList.at(profileNo);

    MythUIButtonListItem *item = m_archiveButtonList->GetItemCurrent();
    if (!item)
        return;

    ArchiveItem *archiveItem = qVariantValue<ArchiveItem *>(item->GetData());
    if (!archiveItem)
        return;

    archiveItem->encoderProfile = profile;

    item->SetText(profile->name, "profile");
    item->SetText(formatSize(archiveItem->newsize / 1024, 2), "size");

    updateSizeBar();
}

void MythBurn::runScript()
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
    commandline = "python " + GetShareDir() + "mytharchive/scripts/mythburn.py";
    commandline += " -j " + configDir + "/mydata.xml";          // job file
    commandline += " -l " + logDir + "/progress.log";           // progress log
    commandline += " > "  + logDir + "/mythburn.log 2>&1 &";    // Logs

    gCoreContext->SaveSetting("MythArchiveLastRunStatus", "Running");

    uint flags = kMSRunBackground | kMSDontBlockInputDevs | 
                 kMSDontDisableDrawing;
    uint retval = myth_system(commandline, flags);
    if (retval != GENERIC_EXIT_RUNNING && retval != GENERIC_EXIT_OK)
    {
        ShowOkPopup(tr("It was not possible to create the DVD. "
                       " An error occured when running the scripts"));
    }
    else
    {
        // now show the log viewer
        showLogViewer();
    }

    m_destinationScreen->Close();
    m_themeScreen->Close();
    Close();
}

void MythBurn::handleAddRecording()
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    RecordingSelector *selector = new RecordingSelector(mainStack,
                                                        &m_archiveList);

    connect(selector, SIGNAL(haveResult(bool)),
            this, SLOT(selectorClosed(bool)));

    if (selector->Create())
        mainStack->AddScreen(selector);
}

void MythBurn::selectorClosed(bool ok)
{
    if (ok)
        updateArchiveList();
}

void MythBurn::handleAddVideo()
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT title FROM videometadata");
    if (query.exec() && query.size())
    {
    }
    else
    {
        ShowOkPopup(QObject::tr("You don't have any videos!"));
        return;
    }

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    VideoSelector *selector = new VideoSelector(mainStack, &m_archiveList);

    connect(selector, SIGNAL(haveResult(bool)),
            this, SLOT(selectorClosed(bool)));

    if (selector->Create())
        mainStack->AddScreen(selector);
}

void MythBurn::handleAddFile()
{
    QString filter = gCoreContext->GetSetting("MythArchiveFileFilter",
                                          "*.mpg *.mpeg *.mov *.avi *.nuv");

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    FileSelector *selector = new FileSelector(mainStack, &m_archiveList,
                                             FSTYPE_FILELIST, "/", filter);

    connect(selector, SIGNAL(haveResult(bool)),
            this, SLOT(selectorClosed(bool)));

    if (selector->Create())
        mainStack->AddScreen(selector);
}

void MythBurn::itemClicked(MythUIButtonListItem *item)
{
    m_moveMode = !m_moveMode;

    if (m_moveMode)
        item->DisplayState("on", "movestate");
    else
        item->DisplayState("off", "movestate");
}

///////////////////////////////////////////////////////////////////////////////

ProfileDialog::ProfileDialog(
    MythScreenStack *parent, ArchiveItem *archiveItem,
    QList<EncoderProfile *> profileList) :
    MythScreenType(parent, "functionpopup"),
    m_archiveItem(archiveItem),
    m_profileList(profileList),
    m_captionText(NULL),
    m_descriptionText(NULL),
    m_oldSizeText(NULL),
    m_newSizeText(NULL),
    m_profile_list(NULL),
    m_enabledCheck(NULL),
    m_okButton(NULL)
{
}

bool ProfileDialog::Create()
{
    if (!LoadWindowFromXML("mythburn-ui.xml", "profilepopup", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_captionText, "caption_text", &err);
    UIUtilE::Assign(this, m_descriptionText, "description_text", &err);
    UIUtilE::Assign(this, m_oldSizeText, "oldsize_text", &err);
    UIUtilE::Assign(this, m_newSizeText, "newsize_text", &err);
    UIUtilE::Assign(this, m_profile_list, "profile_list", &err);
    UIUtilE::Assign(this, m_okButton, "ok_button", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'profilepopup'");
        return false;
    }

    for (int x = 0; x < m_profileList.size(); x++)
    {
        MythUIButtonListItem *item = new
                MythUIButtonListItem(m_profile_list, m_profileList.at(x)->name);
        item->SetData(qVariantFromValue(m_profileList.at(x)));
    }

    connect(m_profile_list, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(profileChanged(MythUIButtonListItem*)));


    m_profile_list->MoveToNamedPosition(m_archiveItem->encoderProfile->name);

    m_captionText->SetText(m_archiveItem->title);
    m_oldSizeText->SetText(formatSize(m_archiveItem->size / 1024, 2));

    connect(m_okButton, SIGNAL(Clicked()), this, SLOT(save()));

    BuildFocusList();

    SetFocusWidget(m_profile_list);

    return true;
}

void ProfileDialog::profileChanged(MythUIButtonListItem *item)
{
    if (!item)
        return;

    EncoderProfile *profile = qVariantValue<EncoderProfile *>(item->GetData());
    if (!profile)
        return;

    m_descriptionText->SetText(profile->description);

    m_archiveItem->encoderProfile = profile;

    // calc new size
    recalcItemSize(m_archiveItem);

    m_newSizeText->SetText(formatSize(m_archiveItem->newsize / 1024, 2));
}


void ProfileDialog::save(void)
{
    emit haveResult(m_profile_list->GetCurrentPos());

    Close();
}

///////////////////////////////////////////////////////////////////////////////

BurnMenu::BurnMenu(void)
        :QObject(NULL)
{
    setObjectName("BurnMenu");
}

BurnMenu::~BurnMenu(void)
{
}

void BurnMenu::start(void)
{
    if (!gCoreContext->GetSetting("MythArchiveLastRunStatus").startsWith("Success"))
    {
        showWarningDialog(QObject::tr("Cannot burn a DVD.\n"
                                      "The last run failed to create a DVD."));
        return;
    }

    // ask the user what type of disk to burn to
    QString title = QObject::tr("Burn DVD");
    QString msg   = QObject::tr("\nPlace a blank DVD in the"
                                " drive and select an option below.");
    MythScreenStack *mainStack = GetMythMainWindow()->GetStack("main stack");
    MythDialogBox   *menuPopup = new MythDialogBox(title, msg, mainStack,
                                                   "actionmenu", true);

    if (menuPopup->Create())
        mainStack->AddScreen(menuPopup);

    menuPopup->SetReturnEvent(this, "action");

    menuPopup->AddButton(QObject::tr("Burn DVD"));
    menuPopup->AddButton(QObject::tr("Burn DVD Rewritable"));
    menuPopup->AddButton(QObject::tr("Burn DVD Rewritable (Force Erase)"));
}

void BurnMenu::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);
        if (dce->GetId() == "action")
        {
            doBurn(dce->GetResult());
            deleteLater();
        }
    }
}

void BurnMenu::doBurn(int mode)
{
    if ((mode < 0) || (mode > 2))
        return;

    QString tempDir = getTempDirectory(true);

    if (tempDir == "")
        return;

    QString logDir = tempDir + "logs";
    QString configDir = tempDir + "config";
    QString commandline;

    // remove existing progress.log if present
    if (QFile::exists(logDir + "/progress.log"))
        QFile::remove(logDir + "/progress.log");

    // remove cancel flag file if present
    if (QFile::exists(logDir + "/mythburncancel.lck"))
        QFile::remove(logDir + "/mythburncancel.lck");

    QString sArchiveFormat = QString::number(mode);
    bool bEraseDVDRW = (mode == 2);
    bool bNativeFormat = gCoreContext->GetSetting("MythArchiveLastRunType")
                             .startsWith("Native");

    commandline = "mytharchivehelper --burndvd --mediatype " + sArchiveFormat +
                  (bEraseDVDRW ? " --erasedvdrw" : "") + 
                  (bNativeFormat ? " --nativeformat" : "");
    commandline += logPropagateArgs;
    if (!logPropagateQuiet())
        commandline += " --quiet";
    commandline += " > "  + logDir + "/progress.log 2>&1 &";

    uint flags = kMSRunBackground | kMSDontBlockInputDevs | 
                 kMSDontDisableDrawing;
    uint retval = myth_system(commandline, flags);
    if (retval != GENERIC_EXIT_RUNNING && retval != GENERIC_EXIT_OK)
    {
        showWarningDialog(QObject::tr("It was not possible to run "
                                      "mytharchivehelper to burn the DVD."));
        return;
    }

    // now show the log viewer
    showLogViewer();
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
