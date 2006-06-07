#include <unistd.h>

#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <cstdlib>

// qt
#include <qdir.h>
#include <qdom.h>

// myth
#include <mythtv/mythcontext.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/libmythtv/remoteutil.h>

// mytharchive
#include "mytharchivewizard.h"
#include "mythburnwizard.h"
#include "editmetadata.h"
#include "advancedoptions.h"

// last page in wizard
const int LAST_PAGE = 3;

//Max size of a DVD-R in Mbytes
const int MAX_DVDR_SIZE_SL = 4489;
const int MAX_DVDR_SIZE_DL = 8978;

MythburnWizard::MythburnWizard(ArchiveDestination destination,
                               MythMainWindow *parent, QString window_name,
                               QString theme_filename, const char *name)
                : MythThemedDialog(parent, window_name, theme_filename, name, true)
{
    archiveDestination = destination;
    themeDir = gContext->GetShareDir() + "mytharchive/themes/";

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

    freeSpace = destination.freeSpace / 1024;
    updateSizeBar();
}

MythburnWizard::~MythburnWizard(void)
{
    saveConfiguration();

    if (archiveList)
        delete archiveList;
}

void MythburnWizard::keyPressEvent(QKeyEvent *e)
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
            else if (getCurrentFocusWidget() == selected_list)
            {
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
            if (getCurrentFocusWidget() == theme_selector)
                theme_selector->push(false);
            else if (getCurrentFocusWidget() == category_selector)
                category_selector->push(false);
            else
                nextPrevWidgetFocus(false);
        }
        else if (action == "RIGHT")
        {
            if (getCurrentFocusWidget() == theme_selector)
                theme_selector->push(true);
            else if (getCurrentFocusWidget() == category_selector)
                category_selector->push(true);
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "SELECT")
        {
            if (getCurrentFocusWidget() == archive_list)
                toggleSelectedState();
            else
                activateCurrent();
        }
        else if (action == "MENU")
        {
            showMenu();
        }
        else if (action == "INFO")
        {
            if (freeSpace == MAX_DVDR_SIZE_SL)
                freeSpace = MAX_DVDR_SIZE_DL;
            else
                freeSpace = MAX_DVDR_SIZE_SL;
            updateSizeBar();
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

void MythburnWizard::toggleSelectedState()
{
    UIListBtnTypeItem *item = archive_list->GetItemCurrent();
    if (item->state() == UIListBtnTypeItem:: FullChecked)
    {
        if (selectedList.find((ArchiveItem *) item->getData()) != -1)
            selectedList.remove((ArchiveItem *) item->getData());
        item->setChecked(UIListBtnTypeItem:: NotChecked);
    }
    else
    {
        if (selectedList.find((ArchiveItem *) item->getData()) == -1)
            selectedList.append((ArchiveItem *) item->getData());

        item->setChecked(UIListBtnTypeItem:: FullChecked);
    }

    archive_list->refresh();
    updateSizeBar();

    updateSelectedArchiveList();
}

void MythburnWizard::updateSizeBar()
{
    long long size = 0;
    ArchiveItem *a;

    for (a = selectedList.first(); a; a = selectedList.next())
    {
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

void MythburnWizard::wireUpTheme()
{
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

    // advanced button
    advanced_button = getUITextButtonType("advanced_button");
    if (advanced_button)
    {
        advanced_button->setText(tr("Advanced Options"));
        connect(advanced_button, SIGNAL(pushed()), this, SLOT(advancedPressed()));
    }

    // recordings selector
    category_selector = getUISelectorType("category_selector");
    if (category_selector)
    {
        connect(category_selector, SIGNAL(pushed(int)),
                this, SLOT(setCategory(int)));
    }

    title_text = getUITextType("progtitle");
    datetime_text = getUITextType("progdatetime");
    description_text = getUITextType("progdescription");
    filesize_text = getUITextType("filesize");
    usecutlist_text = getUITextType("usecutlist_text");
    nocutlist_text = getUITextType("nocutlist_text");;

    usecutlist_check = getUICheckBoxType("usecutlist_check");
    if (usecutlist_check)
    {
        connect(usecutlist_check, SIGNAL(pushed(bool)),
                this, SLOT(toggleUseCutlist(bool)));
    }

    selected_list = getUIListBtnType("selectedlist");

    archive_list = getUIListBtnType("archivelist");
    if (archive_list)
    {
        getArchiveList();
        connect(archive_list, SIGNAL(itemSelected(UIListBtnTypeItem *)),
                this, SLOT(titleChanged(UIListBtnTypeItem *)));
    }

    size_bar = getUIStatusBarType("size_bar");
    if (size_bar)
    {
        updateSizeBar();
    }

    buildFocusList();
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

void MythburnWizard::handleNextPage()
{
    if (getContext() == 1 && selectedList.count() == 0)
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), tr("Myth Archive"),
            tr("You need to select at least one item to archive!"));
        return;
    }

    if (getContext() == LAST_PAGE)
        done(Accepted);
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

void MythburnWizard::handlePrevPage()
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
        const QFileInfoList *list = d.entryInfoList("*", QDir::Dirs, QDir::Name);
        QFileInfoListIterator it(*list);
        QFileInfo *fi;

        int count = 0;
        while ( (fi = it.current()) != 0 )
        {
            // only include theme directory's with a preview image
            if (QFile::exists(themeDir + fi->fileName() + "/preview.png"))
            {
                theme_list.append(fi->fileName());
                if (theme_selector)
                    theme_selector->addItem(count, fi->fileName()); 
                ++count;
            }
            ++it;
        }

        if (theme_selector)
            theme_selector->setToItem(0);

        setTheme(0);
    }
    else
        cout << "MythArchive:  Theme directory does not exist!" << endl;
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

    if (file.open( IO_ReadOnly ))
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

    if (category_selector)
    {
        ArchiveItem *a;
        vector<ArchiveItem *>::iterator i = archiveList->begin();
        for ( ; i != archiveList->end(); i++)
        {
            a = *i;

            if (a->type == category_selector->getCurrentString() || 
                category_selector->getCurrentString() == tr("All Archive Items"))
            {
                UIListBtnTypeItem* item = new UIListBtnTypeItem(
                        archive_list, a->title);
                item->setCheckable(true);
                if (selectedList.find((ArchiveItem *) a) != -1) 
                {
                    item->setChecked(UIListBtnTypeItem::FullChecked);
                }
                else 
                {
                    item->setChecked(UIListBtnTypeItem::NotChecked);
                }

                item->setData(a);
            }
        }
    }

    archive_list->SetItemCurrent(archive_list->GetItemFirst());
    titleChanged(archive_list->GetItemCurrent());
    archive_list->refresh();
}

bool MythburnWizard::isArchiveItemValid(QString &type, QString &filename)
{
    if (type == "Recording")
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT title FROM recorded WHERE basename = :FILENAME");
        query.bindValue(":FILENAME", filename);
        query.exec();
        if (query.isActive() && query.numRowsAffected())
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
        if (query.isActive() && query.numRowsAffected())
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

bool MythburnWizard::extractDetailsFromFilename(const QString &inFile,
                                QString &chanID, QString &startTime)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT chanid, starttime FROM recorded "
                  "WHERE basename = :BASENAME");
    query.bindValue(":BASENAME", inFile);

    query.exec();
    if (query.isActive() && query.numRowsAffected())
    {
        query.first();
        chanID = query.value(0).toString();
        startTime= query.value(1).toString();
    }
    else
    {
        VERBOSE(VB_IMPORTANT, 
                QString("MythArchive: Cannot find details for %1").arg(inFile));
        return false;
    }

    return true;
}

vector<ArchiveItem *> *MythburnWizard::getArchiveListFromDB(void)
{
    vector<ArchiveItem*> *archiveList = new vector<ArchiveItem*>;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT intid, type, title, subtitle, description, size, "
                  "startdate, starttime, filename, hascutlist "
                  "FROM archiveitems ORDER BY title, subtitle");
    query.exec();
    if (query.isActive() && query.numRowsAffected())
    {
        while (query.next())
        {
            // check this item is still available
            QString type = query.value(1).toString();
            QString filename = QString::fromUtf8(query.value(8).toString());
            if (isArchiveItemValid(type, filename))
            {
                ArchiveItem *item = new ArchiveItem;

                item->id = query.value(0).toInt();
                item->type = type;
                item->title = QString::fromUtf8(query.value(2).toString());
                item->subtitle = QString::fromUtf8(query.value(3).toString());
                item->description = QString::fromUtf8(query.value(4).toString());
                item->size = query.value(5).toLongLong();
                item->startDate = QString::fromUtf8(query.value(6).toString());
                item->startTime = QString::fromUtf8(query.value(7).toString());
                item->filename = filename;
                item->hasCutlist = hasCutList(type, filename);
                item->useCutlist = false;
                item->editedDetails = false;

                archiveList->push_back(item);
            }
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "MythburnWizard: Failed to get any archive items");
        return NULL;
    }

    return archiveList;
}

void MythburnWizard::getArchiveList(void)
{
    ArchiveItem *a;
    archiveList = getArchiveListFromDB();
    QStringList categories;

    if (archiveList && archiveList->size() > 0)
    {
        vector<ArchiveItem *>::iterator i = archiveList->begin();
        for ( ; i != archiveList->end(); i++)
        {
            a = *i;

            if (categories.find(a->type) == categories.end())
                categories.append(a->type);
        }
    }
    else
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), tr("Myth Burn"),
                                  tr("You don't have any items to archive!\n\nClick OK"));
        QTimer::singleShot(100, this, SLOT(handleCancel()));
        return;
    }

    // sort and add categories to selector
    categories.sort();

    if (category_selector)
    {
        category_selector->addItem(0, "All Archive Items");
        category_selector->setToItem(0);
    }

    for (uint x = 1; x <= categories.count(); x++)
    {
        if (category_selector)
            category_selector->addItem(x, categories[x-1]); 
    }

    setCategory(0);
}

void MythburnWizard::setCategory(int item)
{
    (void) item;
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

    for (a = selectedList.first(); a; a = selectedList.next())
    {
        QDomElement file = doc.createElement("file");
        file.setAttribute("type", a->type.lower() );
        file.setAttribute("usecutlist", a->useCutlist);
        file.setAttribute("filename", a->filename);

        if (a->editedDetails)
        {
            QDomElement details = doc.createElement("details");
            file.appendChild(details);
            details.setAttribute("title", a->title.utf8());
            details.setAttribute("subtitle", a->subtitle.utf8());
            details.setAttribute("startdate", a->startDate.utf8());
            details.setAttribute("starttime", a->startTime.utf8());
            QDomText desc = doc.createTextNode(a->description.utf8());
            details.appendChild(desc);
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
    if (!f.open(IO_WriteOnly))
    {
        cout << "MythburnWizard::createConfigFile: Failed to open file for writing - "
             << filename << endl;
        return;
    }

    QTextStream t(&f);
    t << doc.toString(4);
    f.close();
}

void MythburnWizard::loadConfiguration(void)
{
    theme_selector->setToItem(
            gContext->GetSetting("MythArchiveMenuTheme", ""));
    setTheme(theme_list.findIndex(theme_selector->getCurrentString()));

    bCreateISO = (gContext->GetSetting("MythArchiveCreateISO", "0") == "1");
    bDoBurn = (gContext->GetSetting("MythArchiveBurnDVDr", "1") == "1");
    bEraseDvdRw = (gContext->GetSetting("MythArchiveEraseDvdRw", "0") == "1");
}

void MythburnWizard::saveConfiguration(void)
{
    gContext->SaveSetting("MythArchiveMenuTheme", 
                theme_selector->getCurrentString());
}

void MythburnWizard::updateSelectedArchiveList(void)
{
    selected_list->Reset();

    ArchiveItem *a;

    for (a = selectedList.first(); a; a = selectedList.next())
    {
        QString s = a->title;
        UIListBtnTypeItem* item = new UIListBtnTypeItem(selected_list, s);
        item->setCheckable(true);
        if (a->useCutlist) 
            item->setChecked(UIListBtnTypeItem::FullChecked);
        else
            item->setChecked(UIListBtnTypeItem::NotChecked);
    }
}

void MythburnWizard::showMenu()
{
    if (popupMenu)
        return;

    popupMenu = new MythPopupBox(gContext->GetMainWindow(),
                                      "popupMenu");

    QButton *button;
    button = popupMenu->addButton(tr("Edit Details"), this, SLOT(editDetails()));
    button->setFocus();

    popupMenu->addButton(tr("Remove Item"), this, SLOT(removeItem()));

    QLabel *splitter = popupMenu->addLabel(" ", MythPopupBox::Small);
    splitter->setLineWidth(2);
    splitter->setFrameShape(QFrame::HLine);
    splitter->setFrameShadow(QFrame::Sunken);
    splitter->setMinimumHeight((int) (25 * hmult));
    splitter->setMaximumHeight((int) (25 * hmult));

    popupMenu->addButton(tr("Use SL DVD (4489Mb)"), this, SLOT(useSLDVD()));
    popupMenu->addButton(tr("Use DL DVD (8978Mb)"), this, SLOT(useDLDVD()));

    popupMenu->ShowPopup(this, SLOT(closePopupMenu()));
}

void MythburnWizard::closePopupMenu()
{
    if (!popupMenu)
        return;

    popupMenu->hide();
    delete popupMenu;
    popupMenu = NULL;
}

void MythburnWizard::editDetails()
{
    if (!popupMenu)
        return;

    showEditMetadataDialog();
    closePopupMenu();
}

void MythburnWizard::useSLDVD()
{
    if (!popupMenu)
        return;

    freeSpace = MAX_DVDR_SIZE_SL;
    updateSizeBar();
    closePopupMenu();
}

void MythburnWizard::useDLDVD()
{
    if (!popupMenu)
        return;

    freeSpace = MAX_DVDR_SIZE_DL;
    updateSizeBar();
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

bool MythburnWizard::doRemoveArchiveItem(QString &filename)
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
                                  "edit_metadata", "mytharchive-", "edit metadata");
    if (editDialog.exec())
    {
        // update widgets to reflect any changes
        titleChanged(item);
        item->setText(curItem->title);
    }
}

void MythburnWizard::advancedPressed()
{
    AdvancedOptions *dialog = new AdvancedOptions(gContext->GetMainWindow(),
                           "advanced_options", "mytharchive-", "advanced options");
    int res = dialog->exec();
    delete dialog;

    if (res)
    {
        // need to reload our copy of setting in case they have changed
        loadConfiguration();
    }
}
