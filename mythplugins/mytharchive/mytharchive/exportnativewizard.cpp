#include <unistd.h>

#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <cstdlib>

// qt
#include <qdir.h>
#include <qapplication.h>
#include <QKeyEvent>
#include <Q3TextStream>

// myth
#include <mythtv/mythcontext.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/libmythtv/remoteutil.h>
#include <mythtv/libmythtv/programinfo.h>

// mytharchive
#include "exportnativewizard.h"
#include "fileselector.h"
#include "archiveutil.h"
#include "recordingselector.h"
#include "videoselector.h"

// last page in wizard
const int LAST_PAGE = 2;

ExportNativeWizard::ExportNativeWizard(MythMainWindow *parent, QString window_name,
                               QString theme_filename, const char *name)
                : MythThemedDialog(parent, window_name, theme_filename, name, true)
{
    archiveList = NULL;
    popupMenu = NULL;
    setContext(1);
    wireUpTheme();
    assignFirstFocus();
    updateForeground();

    bCreateISO = false;
    bDoBurn = false;
    bEraseDvdRw = false;
    saveFilename = "";

    loadConfiguration();
}

ExportNativeWizard::~ExportNativeWizard(void)
{
    saveConfiguration();

    if (archiveList)
        delete archiveList;
}

void ExportNativeWizard::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Archive", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "ESCAPE")
        {
            done(Rejected);
        }
        else if (action == "DOWN")
        {
            if (getCurrentFocusWidget() == archive_list)
            {
                archive_list->MoveDown(UIListBtnType::MoveItem);
                archive_list->refresh();
            }
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "UP")
        {
            if (getCurrentFocusWidget() == archive_list)
            {
                archive_list->MoveUp(UIListBtnType::MoveItem);
                archive_list->refresh();
            }
            else
                nextPrevWidgetFocus(false);
        }
        else if (action == "PAGEDOWN")
        {
            if (getCurrentFocusWidget() == archive_list)
            {
                archive_list->MoveDown(UIListBtnType::MovePage);
                archive_list->refresh();
            }
        }
        else if (action == "PAGEUP")
        {
            if (getCurrentFocusWidget() == archive_list)
            {
                archive_list->MoveUp(UIListBtnType::MovePage);
                archive_list->refresh();
            }
        }
        else if (action == "LEFT")
        {
            if (getCurrentFocusWidget() == destination_selector)
                destination_selector->push(false);
            else
                nextPrevWidgetFocus(false);
        }
        else if (action == "RIGHT")
        {
            if (getCurrentFocusWidget() == destination_selector)
                destination_selector->push(true);
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "SELECT")
        {
            activateCurrent();
        }
        else if (action == "MENU")
        {
            showMenu();
        }
        else
            handled = false;
    }

    if (!handled)
            MythThemedDialog::keyPressEvent(e);
}

void ExportNativeWizard::updateSizeBar()
{
    long long size = 0;
    NativeItem *a;

    vector<NativeItem *>::iterator i = archiveList->begin();
    for ( ; i != archiveList->end(); i++)
    {
        a = *i;
        size += a->size; 
    }

    usedSpace = size / 1024 / 1024;

    UITextType *item;
    QString tmpSize;

    if (size_bar)
    {
        size_bar->SetTotal(freeSpace);
        size_bar->SetUsed(usedSpace);
    }

    tmpSize.sprintf("%0d Mb", freeSpace);

    item = getUITextType("maxsize");
    if (item)
        item->SetText(tr(tmpSize));

    item = getUITextType("minsize");
    if (item)
        item->SetText(tr("0 Mb"));

    tmpSize.sprintf("%0d Mb", usedSpace);

    if (usedSpace > freeSpace)
    {
        item = getUITextType("currentsize");
        if (item)
            item->hide();

        item = getUITextType("currentsize_error");
        if (item)
        {
            item->show();
            item->SetText(tmpSize);
        }
    }
    else
    {
        item = getUITextType("currentsize_error");
        if (item)
            item->hide();

        item = getUITextType("currentsize");
        if (item)
        {
            item->show();
            item->SetText(tmpSize);
        }
    }

    size_bar->refresh();
}

void ExportNativeWizard::wireUpTheme()
{
    // make iso image checkbox
    createISO_check = getUICheckBoxType("makeisoimage_check");
    if (createISO_check)
    {
        connect(createISO_check, SIGNAL(pushed(bool)),
                this, SLOT(toggleCreateISO(bool)));
    }

    // burn dvdr checkbox
    doBurn_check = getUICheckBoxType("burntodvdr_check");
    if (doBurn_check)
    {
        connect(doBurn_check, SIGNAL(pushed(bool)),
                this, SLOT(toggleDoBurn(bool)));
    }
    doBurn_text = getUITextType("burntodvdr_text");

    // erase DVD RW checkbox
    eraseDvdRw_check = getUICheckBoxType("erasedvdrw_check");
    if (eraseDvdRw_check)
    {
        connect(eraseDvdRw_check, SIGNAL(pushed(bool)),
                this, SLOT(toggleEraseDvdRw(bool)));
    }
    eraseDvdRw_text = getUITextType("erasedvdrw_text");

    // finish button
    next_button = getUITextButtonType("next_button");
    if (next_button)
    {
        next_button->setText(tr("Next"));
        connect(next_button, SIGNAL(pushed()), this, SLOT(handleNextPage()));
    }

    // prev button
    prev_button = getUITextButtonType("prev_button");
    if (prev_button)
    {
        prev_button->setText(tr("Previous"));
        connect(prev_button, SIGNAL(pushed()), this, SLOT(handlePrevPage()));
    }

    // cancel button
    cancel_button = getUITextButtonType("cancel_button");
    if (cancel_button)
    {
        cancel_button->setText(tr("Cancel"));
        connect(cancel_button, SIGNAL(pushed()), this, SLOT(handleCancel()));
    }

    // destination selector
    destination_selector = getUISelectorType("destination_selector");
    if (destination_selector)
    {
        connect(destination_selector, SIGNAL(pushed(int)),
                this, SLOT(setDestination(int)));

        for (int x = 0; x < ArchiveDestinationsCount; x++)
            destination_selector->addItem(ArchiveDestinations[x].type,
                                          ArchiveDestinations[x].name);
    }

    destination_text = getUITextType("destination_text");

    // optional destination items

    // find button
    find_button = getUITextButtonType("find_button");
    if (find_button)
    {
        find_button->setText(tr("Choose File..."));
        connect(find_button, SIGNAL(pushed()), this, SLOT(handleFind()));
    }

    filename_edit = getUIRemoteEditType("filename_edit");
    if (filename_edit)
    {
        filename_edit->createEdit(this);
        connect(filename_edit, SIGNAL(loosingFocus()), this, 
                SLOT(filenameEditLostFocus()));
    }

    freespace_text = getUITextType("freespace_text");

    setDestination(0);

    title_text = getUITextType("progtitle");
    datetime_text = getUITextType("progdatetime");
    description_text = getUITextType("progdescription");
    filesize_text = getUITextType("filesize");
    nofiles_text = getUITextType("nofiles");

    size_bar = getUIStatusBarType("size_bar");

    archive_list = getUIListBtnType("archivelist");
    if (archive_list)
    {
        getArchiveList();
        connect(archive_list, SIGNAL(itemSelected(UIListBtnTypeItem *)),
                this, SLOT(titleChanged(UIListBtnTypeItem *)));
    }

    // add recording button
    addrecording_button = getUITextButtonType("addrecording_button");
    if (next_button)
    {
        addrecording_button->setText(tr("Add Recording"));
        connect(addrecording_button, SIGNAL(pushed()), this, SLOT(handleAddRecording()));
    }

    // add video button
    addvideo_button = getUITextButtonType("addvideo_button");
    if (addvideo_button)
    {
        addvideo_button->setText(tr("Add Video"));
        connect(addvideo_button, SIGNAL(pushed()), this, SLOT(handleAddVideo()));
    }

    buildFocusList();
}

void ExportNativeWizard::titleChanged(UIListBtnTypeItem *item)
{
    NativeItem *a;

    a = (NativeItem *) item->getData();

    if (!a)
        return;

    if (title_text)
        title_text->SetText(a->title);

    if (datetime_text)
        datetime_text->SetText(a->startDate + " " + a->startTime);

    if (description_text)
        description_text->SetText(
                (a->subtitle != "" ? a->subtitle + "\n" : "") + a->description);

    if (filesize_text)
    {
        filesize_text->SetText(formatSize(a->size / 1024, 2));
    }

    buildFocusList();
}

void ExportNativeWizard::handleNextPage()
{
    if (getContext() ==  LAST_PAGE && archiveList->size() == 0)
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), tr("Myth Archive"),
            tr("You need to add at least one item to archive!"));
        return;
    }

    if (getContext() == LAST_PAGE)
    {
        runScript();
        done(Accepted);
    }
    else
        setContext(getContext() + 1);

    if (next_button)
    {
        if (getContext() == LAST_PAGE)
            next_button->setText(tr("Finish"));
        else
            next_button->setText(tr("Next"));
    }

    updateForeground();
    buildFocusList();
}

void ExportNativeWizard::handlePrevPage()
{
    if (getContext() == 1)
        done(Rejected);

    if (getContext() > 1)
        setContext(getContext() - 1);

    if (next_button)
        next_button->setText(tr("Next"));

    updateForeground();
    buildFocusList();
}

void ExportNativeWizard::handleCancel()
{
    done(Rejected);
}

void ExportNativeWizard::updateArchiveList(void)
{
    archive_list->Reset();

    if (archiveList->size() == 0)
    {
        if (title_text)
            title_text->SetText("");

        if (datetime_text)
            datetime_text->SetText("");

        if (description_text)
            description_text->SetText("");

        if (filesize_text)
            filesize_text->SetText("");

        if (nofiles_text)
            nofiles_text->show();
    }
    else
    {
        NativeItem *a;
        vector<NativeItem *>::iterator i = archiveList->begin();
        for ( ; i != archiveList->end(); i++)
        {
            a = *i;

            UIListBtnTypeItem* item = new UIListBtnTypeItem(archive_list, a->title);
            item->setCheckable(false);
            item->setData(a);
        }

        archive_list->SetItemCurrent(archive_list->GetItemFirst());
        titleChanged(archive_list->GetItemCurrent());
        if (nofiles_text)
            nofiles_text->hide();
    }

    updateSizeBar();
    archive_list->refresh();
}

void ExportNativeWizard::getArchiveListFromDB(void)
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
    if (query.isActive() && query.numRowsAffected())
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

void ExportNativeWizard::getArchiveList(void)
{
    getArchiveListFromDB();
    updateArchiveList();
}

void ExportNativeWizard::loadConfiguration(void)
{
    bCreateISO = (gContext->GetSetting("MythNativeCreateISO", "0") == "1");
    createISO_check->setState(bCreateISO);

    bDoBurn = (gContext->GetSetting("MythNativeBurnDVDr", "1") == "1");
    doBurn_check->setState(bDoBurn);

    bEraseDvdRw = (gContext->GetSetting("MythNativeEraseDvdRw", "0") == "1");
    eraseDvdRw_check->setState(bEraseDvdRw);
}

void ExportNativeWizard::saveConfiguration(void)
{
    gContext->SaveSetting("MythNativeCreateISO", 
                          (createISO_check->getState() ? "1" : "0"));
    gContext->SaveSetting("MythNativeBurnDVDr", 
                          (doBurn_check->getState() ? "1" : "0"));
    gContext->SaveSetting("MythNativeEraseDvdRw", 
                          (eraseDvdRw_check->getState() ? "1" : "0"));
}

void ExportNativeWizard::showMenu()
{
    if (popupMenu || getContext() != 2 || archiveList->size() == 0)
        return;

    popupMenu = new MythPopupBox(gContext->GetMainWindow(),
                                      "popupMenu");

    QAbstractButton *button;

    button = popupMenu->addButton(tr("Remove Item"), this, SLOT(removeItem()));
    button->setFocus();
    popupMenu->addButton(tr("Cancel"), this, SLOT(closePopupMenu()));

    popupMenu->ShowPopup(this, SLOT(closePopupMenu()));
}

void ExportNativeWizard::closePopupMenu(void)
{
    if (popupMenu)
    {
        popupMenu->deleteLater();
        popupMenu = NULL;
    }
}

void ExportNativeWizard::removeItem()
{
    if (!popupMenu)
        return;

    UIListBtnTypeItem *item = archive_list->GetItemCurrent();
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

    closePopupMenu();
}

void ExportNativeWizard::createConfigFile(const QString &filename)
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
    options.setAttribute("createiso", bCreateISO);
    options.setAttribute("doburn", bDoBurn);
    options.setAttribute("mediatype", archiveDestination.type);
    options.setAttribute("dvdrsize", freeSpace);
    options.setAttribute("erasedvdrw", bEraseDvdRw);
    options.setAttribute("savedirectory", saveFilename);
    job.appendChild(options);

    // finally save the xml to the file
    QFile f(filename);
    if (!f.open(QIODevice::WriteOnly))
    {
        cout << "ExportNativeWizard::createConfigFile: Failed to open file for writing - "
                << filename.toLocal8Bit().constData() << endl;
        return;
    }

    Q3TextStream t(&f);
    t << doc.toString(4);
    f.close();
}

void ExportNativeWizard::setDestination(int item)
{
    if (item < 0 || item > ArchiveDestinationsCount - 1)
        item = 0;

    destination_no = item;
    if (destination_text)
    {
        destination_text->SetText(ArchiveDestinations[item].description);
    }

    archiveDestination = ArchiveDestinations[item];

    switch(item)
    {
        case AD_DVD_SL:
        case AD_DVD_DL:
            filename_edit->hide();
            find_button->hide();
            eraseDvdRw_check->hide();
            eraseDvdRw_text->hide();
            doBurn_check->show();
            doBurn_text->show();
            break;
        case AD_DVD_RW:
            filename_edit->hide();
            find_button->hide();
            eraseDvdRw_check->show();
            eraseDvdRw_text->show();
            doBurn_check->show();
            doBurn_text->show();
            break;
        case AD_FILE:
            long long dummy;
            ArchiveDestinations[item].freeSpace = 
                    getDiskSpace(filename_edit->getText(), dummy, dummy);

            filename_edit->show();
            find_button->show();
            eraseDvdRw_check->hide();
            eraseDvdRw_text->hide();
            doBurn_check->hide();
            doBurn_text->hide();
            break;
    }

    filename_edit->refresh();
    eraseDvdRw_check->refresh();
    eraseDvdRw_text->refresh();
    find_button->refresh();

    // update free space
    if (ArchiveDestinations[item].freeSpace != -1)
    {
        freespace_text->SetText(formatSize(ArchiveDestinations[item].freeSpace, 2));
        freeSpace = ArchiveDestinations[item].freeSpace / 1024;
    }
    else
    {
        freespace_text->SetText("Unknown");
        freeSpace = 0;
    }

    buildFocusList();
}

void ExportNativeWizard::handleFind(void)
{
    FileSelector selector(FSTYPE_FILE, "/", "*.*", gContext->GetMainWindow(),
                          "file_selector", "mytharchive-", "file selector");
    qApp->unlock();
    bool res = (kDialogCodeRejected != selector.exec());

    if (res)
    {
        filename_edit->setText(selector.getSelected());
        filenameEditLostFocus();
    }
    qApp->lock();
}

void ExportNativeWizard::filenameEditLostFocus()
{
    long long dummy;
    ArchiveDestinations[AD_FILE].freeSpace = 
            getDiskSpace(filename_edit->getText(), dummy, dummy);

    saveFilename = filename_edit->getText();

    // if we don't get a valid freespace value it probably means the file doesn't
    // exist yet so try looking up the freespace for the parent directory 
    if (ArchiveDestinations[AD_FILE].freeSpace == -1)
    {
        QString dir = filename_edit->getText();
        int pos = dir.findRev('/');
        if (pos > 0)
            dir = dir.left(pos);
        else
            dir = "/";

        ArchiveDestinations[AD_FILE].freeSpace = getDiskSpace(dir, dummy, dummy);
    }

    if (ArchiveDestinations[AD_FILE].freeSpace != -1)
    {
        freespace_text->SetText(formatSize(ArchiveDestinations[AD_FILE].freeSpace, 2));
        freeSpace = ArchiveDestinations[AD_FILE].freeSpace / 1024;
    }
    else
    {
        freespace_text->SetText("Unknown");
        freeSpace = 0;
    }
}

void ExportNativeWizard::runScript()
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
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), QObject::tr("Myth Archive"),
                                  QObject::tr("It was not possible to create the DVD. "  
                                          " An error occured when running the scripts") );
        done(Rejected);
        return;
    }

    done(Accepted);
}

void ExportNativeWizard::handleAddRecording()
{
    RecordingSelector selector(gContext->GetMainWindow(),
                               "recording_selector", "mytharchive-", "recording selector");
    selector.exec();

    getArchiveList();
}

void ExportNativeWizard::handleAddVideo()
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT title FROM videometadata");
    query.exec();
    if (query.isActive() && query.numRowsAffected())
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
