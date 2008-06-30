#include <unistd.h>

#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <cstdlib>
#include <sys/wait.h>  // for WIFEXITED and WEXITSTATUS

// qt
#include <qdir.h>
#include <qapplication.h>
#include <QKeyEvent>
#include <QTextStream>

// myth
#include <mythtv/mythcontext.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/libmythtv/remoteutil.h>
#include <mythtv/libmythtv/programinfo.h>
#include <mythtv/mythdirs.h>
#include <mythtv/libmythui/mythuihelper.h>

// mytharchive
#include "archiveutil.h"
#include "mythburnwizard.h"
#include "editmetadata.h"
#include "fileselector.h"
#include "thumbfinder.h"
#include "recordingselector.h"
#include "videoselector.h"

// last page in wizard
const int LAST_PAGE = 4;

//Max size of a DVD-R in Mbytes
const int MAX_DVDR_SIZE_SL = 4482;
const int MAX_DVDR_SIZE_DL = 8964;

MythburnWizard::MythburnWizard(MythMainWindow *parent, QString window_name,
                               QString theme_filename, const char *name)
                : MythThemedDialog(parent, window_name, theme_filename, name, true)
{
    themeDir = GetShareDir() + "mytharchive/themes/";

    // remove any old thumb images
    QString thumbDir = getTempDirectory() + "/config/thumbs";
    QDir dir(thumbDir);
    if (dir.exists())
        system("rm -rf " + thumbDir);

    archiveList = NULL;
    popupMenu = NULL;
    profileList = NULL;
    setContext(1);
    wireUpTheme();
    assignFirstFocus();
    updateForeground();
    bReordering = false;

    bCreateISO = false;
    bDoBurn = false;
    bEraseDvdRw = false;
    saveFilename = "";

    loadConfiguration();

    updateSizeBar();
}

MythburnWizard::~MythburnWizard(void)
{
    saveConfiguration();

    if (archiveList)
        delete archiveList;
    if (profileList)
        delete profileList;
}

void MythburnWizard::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Archive", e, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
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
            else if (getCurrentFocusWidget() == selected_list)
            {
                if (bReordering)
                {
                    UIListBtnTypeItem *item = selected_list->GetItemCurrent();
                    item->moveUpDown(false);
                    reloadSelectedList();
                }
                else
                    selected_list->MoveDown(UIListBtnType::MoveItem);

                selected_list->refresh();
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
            else  if (getCurrentFocusWidget() == selected_list)
            {
                if (bReordering)
                {
                    UIListBtnTypeItem *item = selected_list->GetItemCurrent();
                    item->moveUpDown(true);
                    reloadSelectedList();
                }
                else
                    selected_list->MoveUp(UIListBtnType::MoveItem);

                selected_list->refresh();
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
            if (bReordering)
            {
                UIListBtnTypeItem *item = selected_list->GetItemCurrent();
                if (item)
                    item->setPixmap(NULL);
                bReordering = false;
                selected_list->refresh();
            }

            if (getCurrentFocusWidget() == theme_selector)
                theme_selector->push(false);
            else if (getCurrentFocusWidget() == profile_selector)
                profile_selector->push(false);
            else if (getCurrentFocusWidget() == destination_selector)
                destination_selector->push(false);
            else
                nextPrevWidgetFocus(false);
        }
        else if (action == "RIGHT")
        {
            if (bReordering)
            {
                UIListBtnTypeItem *item = selected_list->GetItemCurrent();
                if (item)
                    item->setPixmap(NULL);

                bReordering = false;
                selected_list->refresh();
            }

            if (getCurrentFocusWidget() == theme_selector)
                theme_selector->push(true);
            else if (getCurrentFocusWidget() == profile_selector)
                profile_selector->push(true);
            else if (getCurrentFocusWidget() == destination_selector)
                destination_selector->push(true);
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "SELECT")
        {
            if (getCurrentFocusWidget() == selected_list)
                toggleReorderState();
            else
                activateCurrent();
        }
        else if (action == "MENU")
        {
            showMenu();
        }
        else if (action == "INFO")
        {
            if (getContext() == LAST_PAGE)
            {
                UIListBtnTypeItem *item = selected_list->GetItemCurrent();

                if (!item)
                    return;

                ArchiveItem *a = (ArchiveItem *) item->getData();

                ThumbFinder finder(a, theme_list[theme_no], gContext->GetMainWindow(), 
                                   "thumbfinder", "mythburn-", "thumb finder");
                finder.exec();
            }
        }
        else if (action == "TOGGLECUT")
        {
            UIListBtnTypeItem *item = archive_list->GetItemCurrent();
            ArchiveItem *a = (ArchiveItem *) item->getData();

            if (usecutlist_check && a->hasCutlist)
            {
                usecutlist_check->setState(!usecutlist_check->getState()); 
                toggleUseCutlist(usecutlist_check->getState());
            }
        }
        else
            handled = false;
    }

    if (!handled)
            MythThemedDialog::keyPressEvent(e);
}

void MythburnWizard::toggleReorderState()
{
    UIListBtnTypeItem *item = selected_list->GetItemCurrent();
    if (bReordering)
    {
        bReordering = false;
        item->setPixmap(NULL);
    }
    else
    {
        bReordering = true;
        item->setPixmap(movePixmap);
    }

    selected_list->refresh();
}


void MythburnWizard::reloadSelectedList()
{
    archiveList->clear();

    for (int x = 0; x < selected_list->GetCount(); x++)
    {
        UIListBtnTypeItem *item = selected_list->GetItemAt(x);
        if (item)
            archiveList->push_back((ArchiveItem *) item->getData());
    }
}

void MythburnWizard::updateSizeBar(void)
{
    bool show = (getContext() == 2 || getContext() == 4);

    if (show)
    {
        maxsize_text->show();
        minsize_text->show();
        size_bar->show();
        currentsize_error_text->show();
        currentsize_text->show();
    }
    else
    {
        maxsize_text->hide();
        minsize_text->hide();
        size_bar->hide();
        currentsize_error_text->hide();
        currentsize_text->hide();
    }

    long long size = 0;
    ArchiveItem *a;
    vector<ArchiveItem *>::iterator i = archiveList->begin();
    for ( ; i != archiveList->end(); i++)
    {
        a = *i;
        size += a->newsize; 
    }

    usedSpace = size / 1024 / 1024;

    QString tmpSize;

    if (size_bar)
    {
        size_bar->SetTotal(freeSpace);
        size_bar->SetUsed(usedSpace);
    }

    tmpSize.sprintf("%0d Mb", freeSpace);

    maxsize_text->SetText(tmpSize);

    minsize_text->SetText("0 Mb");

    tmpSize.sprintf("%0d Mb", usedSpace);

    if (usedSpace > freeSpace)
    {
        currentsize_text->hide();

        currentsize_error_text->SetText(tmpSize);
        if (show)
            currentsize_error_text->show();
    }
    else
    {
        currentsize_error_text->hide();

        currentsize_text->SetText(tmpSize);
        if (show)
            currentsize_text->show();
    }

    size_bar->refresh();

    if (show)
        selectedChanged(selected_list->GetItemCurrent());
}

void MythburnWizard::wireUpTheme()
{
    // load pixmap
    movePixmap = GetMythUI()->LoadScalePixmap("ma_updown.png");

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

    // theme preview images
    intro_image = getUIImageType("intro_image");
    mainmenu_image = getUIImageType("mainmenu_image");
    chapter_image = getUIImageType("chapter_image");
    details_image = getUIImageType("details_image");

    // menu theme
    themedesc_text = getUITextType("themedescription");
    theme_image = getUIImageType("theme_image");
    theme_selector = getUISelectorType("theme_selector");
    if (theme_selector)
    {
        getThemeList();
        connect(theme_selector, SIGNAL(pushed(int)),
                this, SLOT(setTheme(int)));
    }

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
    usecutlist_text = getUITextType("usecutlist_text");
    nocutlist_text = getUITextType("nocutlist_text");;
    nofiles_text = getUITextType("nofiles");

    usecutlist_check = getUICheckBoxType("usecutlist_check");
    if (usecutlist_check)
    {
        connect(usecutlist_check, SIGNAL(pushed(bool)),
                this, SLOT(toggleUseCutlist(bool)));
    }

    selected_list = getUIListBtnType("selectedlist");
    if (selected_list)
    {
        connect(selected_list, SIGNAL(itemSelected(UIListBtnTypeItem *)),
                this, SLOT(selectedChanged(UIListBtnTypeItem *)));
    }

    archive_list = getUIListBtnType("archivelist");
    if (archive_list)
    {
        connect(archive_list, SIGNAL(itemSelected(UIListBtnTypeItem *)),
                this, SLOT(titleChanged(UIListBtnTypeItem *)));
    }

    // add recording button
    addrecording_button = getUITextButtonType("addrecording_button");
    if (addrecording_button)
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

    // add file button
    addfile_button = getUITextButtonType("addfile_button");
    if (addfile_button)
    {
        addfile_button->setText(tr("Add File"));
        connect(addfile_button, SIGNAL(pushed()), this, SLOT(handleAddFile()));
    }

    maxsize_text = getUITextType("maxsize");
    minsize_text =  getUITextType("minsize");
    currentsize_error_text = getUITextType("currentsize_error");
    currentsize_text = getUITextType("currentsize");

    size_bar = getUIStatusBarType("size_bar");

    profile_selector = getUISelectorType("profile_selector");
    if (profile_selector)
    {
        connect(profile_selector, SIGNAL(pushed(int)),
                this, SLOT(setProfile(int)));
    }

    profile_text = getUITextType("profile_text");
    loadEncoderProfiles();

    oldsize_text = getUITextType("oldfilesize");
    newsize_text =  getUITextType("newfilesize");

    getArchiveList();

    buildFocusList();
}

void MythburnWizard::loadEncoderProfiles()
{
    profileList = new vector<EncoderProfile*>;

    profile_selector->addItem(0, tr("Don't re-encode"));
    EncoderProfile *item = new EncoderProfile;
    item->name = "NONE";
    item->description = "";
    item->bitrate = 0.0f;
    profileList->push_back(item);

    // find the encoding profiles
    // first look in the ConfDir (~/.mythtv)
    QString filename = GetConfDir() + 
            "/MythArchive/ffmpeg_dvd_" + 
            ((gContext->GetSetting("MythArchiveVideoFormat", "pal")
                .lower() == "ntsc") ? "ntsc" : "pal") + ".xml";

    if (!QFile::exists(filename))
    {
        // not found yet so use the default profiles
        filename = GetShareDir() + 
            "mytharchive/encoder_profiles/ffmpeg_dvd_" + 
            ((gContext->GetSetting("MythArchiveVideoFormat", "pal")
                .lower() == "ntsc") ? "ntsc" : "pal") + ".xml";
    }

    VERBOSE(VB_IMPORTANT, QString("MythArchive: Loading encoding profiles from %1")
            .arg(filename));

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
        profile_selector->addItem(x + 1, name);

        EncoderProfile *item = new EncoderProfile;
        item->name = name;
        item->description = desc;
        item->bitrate = bitrate.toFloat();
        profileList->push_back(item);
    }

    profile_selector->setToItem(0);
}

void MythburnWizard::setProfile(int itemNo)
{
    EncoderProfile *profile = profileList->at(itemNo);
    UIListBtnTypeItem *item = selected_list->GetItemCurrent();
    if (item)
    {
        ArchiveItem *a = (ArchiveItem *) item->getData();
        setProfile(profile, a);
    }
}

void MythburnWizard::recalcItemSize(ArchiveItem *item)
{
    if (!item)
        return;

    if (!item->encoderProfile)
        return;

    if (item->encoderProfile->name == "NONE")
    {
        if (item->hasCutlist && item->useCutlist)
            item->newsize = (long long) (item->size /
                    ((float)item->duration / (float)item->cutDuration));
        else
            item->newsize = item->size;
    }
    else
        item->newsize = recalcSize(item->encoderProfile, item);

    if (newsize_text)
    {
        newsize_text->SetText(tr("New Size ") + formatSize(item->newsize / 1024, 2));
    }

    updateSizeBar();
}

void MythburnWizard::setProfile(EncoderProfile *profile, ArchiveItem *item)
{
    if (profile)
    {
        profile_text->SetText(profile->description);
        if (item)
        {
            item->encoderProfile = profile;
            recalcItemSize(item);

            if (newsize_text)
            {
                newsize_text->SetText(tr("New Size ") + formatSize(item->newsize / 1024, 2));
            }

            updateSizeBar();
        }
    }
}

long long MythburnWizard::recalcSize(EncoderProfile *profile, ArchiveItem *a)
{
    if (a->duration == 0)
        return 0;

    int length;

    if (a->hasCutlist && a->useCutlist)
        length = a->cutDuration;
    else
        length = a->duration;

    float len = (float) length / 3600;
    return (long long) (len * profile->bitrate * 1024 * 1024);
}

void MythburnWizard::toggleUseCutlist(bool state)
{
    UIListBtnTypeItem *item = archive_list->GetItemCurrent();
    ArchiveItem *a = (ArchiveItem *) item->getData();

    if (!a)
        return; 

    if (!a->hasCutlist)
        return;

    a->useCutlist = state;

    recalcItemSize(a);

    updateSelectedArchiveList();
}

void MythburnWizard::titleChanged(UIListBtnTypeItem *item)
{
    ArchiveItem *a;

    a = (ArchiveItem *) item->getData();

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

    if (a->hasCutlist)
    {
        // program has a cut list
        if (usecutlist_text) 
            usecutlist_text->show();

        if (usecutlist_check)
        {
            usecutlist_check->show();
            // have we already set the useCutlist flag for this program
            usecutlist_check->setState(a->useCutlist);
        }

        if (nocutlist_text)
            nocutlist_text->hide();
    }
    else
    {
        // no cut list found
        if (usecutlist_text) 
            usecutlist_text->hide();
        if (usecutlist_check)
            usecutlist_check->hide();
        if (nocutlist_text)
            nocutlist_text->show();
    }

    buildFocusList();
}

void MythburnWizard::selectedChanged(UIListBtnTypeItem *item)
{
    if (!item)
        return;

    ArchiveItem *a;

    a = (ArchiveItem *) item->getData();

    if (!a)
        return;

    if (oldsize_text)
    {
        oldsize_text->SetText(tr("Original Size ") + formatSize(a->size / 1024, 2));
    }

    if (newsize_text)
    {
        newsize_text->SetText(tr("New Size ") + formatSize(a->newsize / 1024, 2));
    }

    if (a->encoderProfile->name == "NONE")
        profile_selector->setToItem(tr("Don't re-encode"));
    else
        profile_selector->setToItem(a->encoderProfile->name);

    profile_text->SetText(a->encoderProfile->description);
}

void MythburnWizard::handleNextPage()
{
    if (getContext() == 2 && archiveList->size() == 0)
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

    updateSizeBar();
    updateForeground();
    buildFocusList();
}

void MythburnWizard::handlePrevPage()
{
    if (getContext() == 1)
        done(Rejected);

    if (getContext() > 1)
        setContext(getContext() - 1);

    if (next_button)
        next_button->setText(tr("Next"));

    updateSizeBar();
    updateForeground();
    buildFocusList();
}

void MythburnWizard::handleCancel()
{
    done(Rejected);
}

void MythburnWizard::getThemeList(void)
{
    theme_list.clear();
    QDir d;

    d.setPath(themeDir);
    if (d.exists())
    {
        QFileInfoList list = d.entryInfoList("*", QDir::Dirs, QDir::Name);

        int count = 0;
        for (int x = 0; x < list.size(); x++)
        {
            QFileInfo fi = list.at(x);
            if (QFile::exists(themeDir + fi.fileName() + "/preview.png"))
            {
                theme_list.append(fi.fileName());
                if (theme_selector)
                    theme_selector->addItem(count, fi.fileName()
                            .replace(QString("_"), QString(" ")));
                ++count;
            }
        }

        if (theme_selector)
            theme_selector->setToItem(0);

        setTheme(0);
    }
    else
        VERBOSE(VB_IMPORTANT, "MythArchive:  Theme directory does not exist!");
}

void MythburnWizard::setTheme(int item)
{
    if (item < 0 || item > (int)theme_list.count() - 1)
        item = 0;

    theme_no = item;
    if (theme_image)
    {
        if (QFile::exists(themeDir + theme_list[item] + "/preview.png"))
            theme_image->SetImage(themeDir + theme_list[item] + "/preview.png");
        else
            theme_image->SetImage("blank.png");
        theme_image->LoadImage();
    }

    if (intro_image)
    {
        if (QFile::exists(themeDir + theme_list[item] + "/intro_preview.png"))
            intro_image->SetImage(themeDir + theme_list[item] + "/intro_preview.png");
        else
            intro_image->SetImage("blank.png");
        intro_image->LoadImage();
    }

    if (mainmenu_image)
    {
        if (QFile::exists(themeDir + theme_list[item] + "/mainmenu_preview.png"))
            mainmenu_image->SetImage(themeDir + theme_list[item] + "/mainmenu_preview.png");
        else
            mainmenu_image->SetImage("blank.png");
        mainmenu_image->LoadImage();
    }

    if (chapter_image)
    {
        if (QFile::exists(themeDir + theme_list[item] + "/chaptermenu_preview.png"))
            chapter_image->SetImage(themeDir + theme_list[item] + "/chaptermenu_preview.png");
        else
            chapter_image->SetImage("blank.png");
        chapter_image->LoadImage();
    }

    if (details_image)
    {
        if (QFile::exists(themeDir + theme_list[item] + "/details_preview.png"))
            details_image->SetImage(themeDir + theme_list[item] + "/details_preview.png");
        else
            details_image->SetImage("blank.png");
        details_image->LoadImage();
    }

    if (description_text)
    {
        if (QFile::exists(themeDir + theme_list[item] + "/description.txt"))
        {
            QString desc = loadFile(themeDir + theme_list[item] + "/description.txt");
            themedesc_text->SetText(desc);
        }
        else
            themedesc_text->SetText("No description found!");
    }
}

QString MythburnWizard::loadFile(const QString &filename)
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

void MythburnWizard::updateArchiveList(void)
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

        if (usecutlist_text) 
            usecutlist_text->hide();

        if (usecutlist_check)
            usecutlist_check->hide();

        if (nocutlist_text)
            nocutlist_text->hide();
    }
    else
    {
        ArchiveItem *a;
        vector<ArchiveItem *>::iterator i = archiveList->begin();
        for ( ; i != archiveList->end(); i++)
        {
            a = *i;

            // get duration of this file
            if (a->duration == 0)
                getFileDetails(a);

            // get default encoding profile if needed
            if (a->encoderProfile == NULL)
            {
                a->encoderProfile = getDefaultProfile(a);
                setProfile(a->encoderProfile, a);
            }

            UIListBtnTypeItem* item = new UIListBtnTypeItem(archive_list, a->title);
            item->setCheckable(false);
            item->setData(a);
        }

        if (nofiles_text)
            nofiles_text->hide();

        archive_list->SetItemCurrent(archive_list->GetItemFirst());
        titleChanged(archive_list->GetItemCurrent());
    }

    archive_list->refresh();
    updateSizeBar();
    updateSelectedArchiveList();
}

bool MythburnWizard::isArchiveItemValid(const QString &type, const QString &filename)
{
    if (type == "Recording")
    {
        QString baseName = getBaseName(filename);

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT title FROM recorded WHERE basename = :FILENAME");
        query.bindValue(":FILENAME", baseName);
        query.exec();
        if (query.isActive() && query.size())
            return true;
        else
        {
            doRemoveArchiveItem(filename);
            VERBOSE(VB_IMPORTANT, QString("MythArchive: Recording not found (%1)").arg(filename));
        }
    }
    else if (type == "Video")
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT title FROM videometadata WHERE filename = :FILENAME");
        query.bindValue(":FILENAME", filename);
        query.exec();
        if (query.isActive() && query.size())
            return true;
        else
        {
            doRemoveArchiveItem(filename);
            VERBOSE(VB_IMPORTANT, QString("MythArchive: Video not found (%1)").arg(filename));
        }
    }
    else if (type == "File")
    {
        if (QFile::exists(filename))
            return true;
        else
        {
            doRemoveArchiveItem(filename);
            VERBOSE(VB_IMPORTANT, QString("MythArchive: File not found (%1)").arg(filename));
        }
    }

    VERBOSE(VB_IMPORTANT, "MythArchive: Archive item removed from list");

    return false;
}

bool MythburnWizard::hasCutList(QString &type, QString &filename)
{
    bool res = false;

    if (type.lower() == "recording")
    {
        QString chanID, startTime;

        if (extractDetailsFromFilename(filename, chanID, startTime))
        {
            ProgramInfo *pinfo = ProgramInfo::GetProgramFromRecorded(chanID, startTime);

            if (pinfo)
            {
                res = (pinfo->programflags & FL_CUTLIST);
                delete pinfo;
            }
        }
    }

    return res;
}

void MythburnWizard::getArchiveListFromDB(void)
{
    if (!archiveList)
        archiveList = new vector<ArchiveItem*>;

    archiveList->clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT intid, type, title, subtitle, description, size, "
                  "startdate, starttime, filename, hascutlist "
                  "FROM archiveitems ORDER BY intid");
    query.exec();
    if (query.isActive() && query.size())
    {
        while (query.next())
        {
            // check this item is still available
            QString type = query.value(1).toString();
            QString filename = query.value(8).toString();
            if (isArchiveItemValid(type, filename))
            {
                ArchiveItem *item = new ArchiveItem;

                item->id = query.value(0).toInt();
                item->type = type;
                item->title = query.value(2).toString();
                item->subtitle = query.value(3).toString();
                item->description = query.value(4).toString();
                item->size = query.value(5).toLongLong();
                item->newsize = query.value(5).toLongLong();
                item->encoderProfile = NULL;
                item->startDate = query.value(6).toString();
                item->startTime =query.value(7).toString();
                item->filename = filename;
                item->hasCutlist = hasCutList(type, filename);
                item->useCutlist = false;
                item->editedDetails = false;
                item->duration = 0;
                item->cutDuration = 0;
                item->fileCodec = "";
                item->videoCodec = "";
                item->videoWidth = 0;
                item->videoHeight = 0;
                archiveList->push_back(item);
            }
        }
    }
}

EncoderProfile *MythburnWizard::getDefaultProfile(ArchiveItem *item)
{
    if (!item)
        return profileList->at(0);

    EncoderProfile *profile = NULL;

    // is the file an mpeg2 file?
    if (item->videoCodec.lower() == "mpeg2video")
    {
        // does the file already have a valid DVD resolution?
        if (gContext->GetSetting("MythArchiveVideoFormat", "pal").lower() == "ntsc")
        {
            if ((item->videoWidth == 720 && item->videoHeight == 480) ||
                (item->videoWidth == 704 && item->videoHeight == 480) ||
                (item->videoWidth == 352 && item->videoHeight == 480) ||
                (item->videoWidth == 352 && item->videoHeight == 240))
            {
                // don't need to re-encode
                profile = profileList->at(0);
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
                profile = profileList->at(0);
            }
        }
    }

    if (!profile)
    {
        // file needs re-encoding - use default profile setting
        QString defaultProfile = 
                gContext->GetSetting("MythArchiveDefaultEncProfile", "SP");

        for (uint x = 0; x < profileList->size(); x++)
            if (profileList->at(x)->name == defaultProfile)
                profile = profileList->at(x);
    }

    return profile;
}

void MythburnWizard::getArchiveList(void)
{
    getArchiveListFromDB();
    updateArchiveList();
}

void MythburnWizard::createConfigFile(const QString &filename)
{
    QDomDocument doc("mythburn");

    QDomElement root = doc.createElement("mythburn");
    doc.appendChild(root);

    QDomElement job = doc.createElement("job");
    job.setAttribute("theme", theme_list[theme_no]);
    root.appendChild(job);

    QDomElement media = doc.createElement("media");
    job.appendChild(media);

    // now loop though selected archive items and add them to the xml file
    ArchiveItem *a;

    vector<ArchiveItem *>::iterator i = archiveList->begin();
    for ( ; i != archiveList->end(); i++)
    {
        a = *i;

        QDomElement file = doc.createElement("file");
        file.setAttribute("type", a->type.lower() );
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
    options.setAttribute("createiso", bCreateISO);
    options.setAttribute("doburn", bDoBurn);
    options.setAttribute("mediatype", archiveDestination.type);
    options.setAttribute("dvdrsize", freeSpace);
    options.setAttribute("erasedvdrw", bEraseDvdRw);
    options.setAttribute("savefilename", saveFilename);
    job.appendChild(options);

    // finally save the xml to the file
    QFile f(filename);
    if (!f.open(QIODevice::WriteOnly))
    {
        VERBOSE(VB_IMPORTANT, QString("MythburnWizard::createConfigFile: "
                "Failed to open file for writing - %1")
                .arg(filename.toLocal8Bit().constData()));
        return;
    }

    QTextStream t(&f);
    t << doc.toString(4);
    f.close();
}

void MythburnWizard::loadConfiguration(void)
{
    theme_selector->setToItem(
            gContext->GetSetting("MythBurnMenuTheme", ""));
    setTheme(theme_list.findIndex(theme_selector->getCurrentString()
            .replace(QString(" "), QString("_"))));

    bCreateISO = (gContext->GetSetting("MythBurnCreateISO", "0") == "1");
    createISO_check->setState(bCreateISO);

    bDoBurn = (gContext->GetSetting("MythBurnBurnDVDr", "1") == "1");
    doBurn_check->setState(bDoBurn);

    bEraseDvdRw = (gContext->GetSetting("MythBurnEraseDvdRw", "0") == "1");
    eraseDvdRw_check->setState(bEraseDvdRw);
}

void MythburnWizard::saveConfiguration(void)
{
    gContext->SaveSetting("MythBurnMenuTheme", 
                theme_selector->getCurrentString());
    gContext->SaveSetting("MythBurnCreateISO", 
                          (createISO_check->getState() ? "1" : "0"));
    gContext->SaveSetting("MythBurnBurnDVDr", 
                          (doBurn_check->getState() ? "1" : "0"));
    gContext->SaveSetting("MythBurnEraseDvdRw", 
                          (eraseDvdRw_check->getState() ? "1" : "0"));
}

void MythburnWizard::updateSelectedArchiveList(void)
{
    selected_list->Reset();

    ArchiveItem *a;

    vector<ArchiveItem *>::iterator i = archiveList->begin();
    for ( ; i != archiveList->end(); i++)
    {
        a = *i;

        QString s = a->title;
        if (a->subtitle != "")
            s += " ~ " + a->subtitle;
        if (a->type == "Recording" && a->startDate != "")
            s += " ~ " + a->startDate + " " + a->startTime;
        UIListBtnTypeItem* item = new UIListBtnTypeItem(selected_list, s);
        item->setCheckable(true);
        if (a->useCutlist) 
            item->setChecked(UIListBtnTypeItem::FullChecked);
        else
            item->setChecked(UIListBtnTypeItem::NotChecked);
        item->setData(a);
    }
}

void MythburnWizard::showMenu()
{
    if (popupMenu || getContext() != 2 || archiveList->size() == 0)
        return;

    popupMenu = new MythPopupBox(gContext->GetMainWindow(),
                                      "popupMenu");

    QAbstractButton *button;
    button = popupMenu->addButton(tr("Edit Details"), this, SLOT(editDetails()));
    button->setFocus();

    popupMenu->addButton(tr("Remove Item"), this, SLOT(removeItem()));
    popupMenu->addButton(tr("Cancel"), this, SLOT(closePopupMenu()));

    popupMenu->ShowPopup(this, SLOT(closePopupMenu()));
}

void MythburnWizard::closePopupMenu(void)
{
    if (popupMenu)
    {
        popupMenu->deleteLater();
        popupMenu = NULL;
    }
}

void MythburnWizard::editDetails()
{
    if (!popupMenu)
        return;

    showEditMetadataDialog();
    closePopupMenu();
}

void MythburnWizard::removeItem()
{
    if (!popupMenu)
        return;

    UIListBtnTypeItem *item = archive_list->GetItemCurrent();
    ArchiveItem *curItem = (ArchiveItem *) item->getData();

    if (!curItem)
        return;

    if (doRemoveArchiveItem(curItem->filename))
        getArchiveList();

    closePopupMenu();
}

bool MythburnWizard::doRemoveArchiveItem(const QString &filename)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM archiveitems WHERE filename = :FILENAME;");
    query.bindValue(":FILENAME", filename);
    query.exec();

    return (query.isActive() && query.numRowsAffected());
}

void MythburnWizard::showEditMetadataDialog()
{
    UIListBtnTypeItem *item = archive_list->GetItemCurrent();
    ArchiveItem *curItem = (ArchiveItem *) item->getData();

    if (!curItem)
        return;

    EditMetadataDialog editDialog(curItem, gContext->GetMainWindow(),
                                  "edit_metadata", "mythburn-", "edit metadata");
    if (kDialogCodeRejected != editDialog.exec())
    {
        // update widgets to reflect any changes
        titleChanged(item);
        item->setText(curItem->title);
    }
}

void MythburnWizard::setDestination(int item)
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

void MythburnWizard::handleFind(void)
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

void MythburnWizard::filenameEditLostFocus()
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

void MythburnWizard::runScript()
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
    commandline = "python " + GetShareDir() + "mytharchive/scripts/mythburn.py";
    commandline += " -j " + configDir + "/mydata.xml";             // job file
    commandline += " -l " + logDir + "/progress.log";              // progress log
    commandline += " > "  + logDir + "/mythburn.log 2>&1 &";       //Logs

    gContext->SaveSetting("MythArchiveLastRunStatus", "Running");

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

void MythburnWizard::handleAddRecording()
{
    RecordingSelector selector(gContext->GetMainWindow(),
                               "recording_selector", "mytharchive-", "recording selector");
    selector.exec();

    getArchiveList();
}

void MythburnWizard::handleAddVideo()
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

void MythburnWizard::handleAddFile()
{
    QString filter = gContext->GetSetting("MythArchiveFileFilter",
                                          "*.mpg *.mpeg *.mov *.avi *.nuv");

    FileSelector selector(FSTYPE_FILELIST, "/", filter, gContext->GetMainWindow(),
                          "file_selector", "mytharchive-", "file selector");
    selector.exec();

    getArchiveList();
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
