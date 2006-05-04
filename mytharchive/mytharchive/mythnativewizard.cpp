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
#include <remoteutil.h>

// mytharchive
#include "mytharchivewizard.h"
#include "mythnativewizard.h"
#include "advancedoptions.h"

// last page in wizard
const int LAST_PAGE = 2;

//Max size of a DVD-R in Mbytes
const int MAX_DVDR_SIZE_SL = 4489;
const int MAX_DVDR_SIZE_DL = 8978;

MythNativeWizard::MythNativeWizard(ArchiveDestination destination,
                               MythMainWindow *parent, QString window_name,
                               QString theme_filename, const char *name)
                : MythThemedDialog(parent, window_name, theme_filename, name, true)
{
    archiveDestination = destination;

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

MythNativeWizard::~MythNativeWizard(void)
{
    saveConfiguration();

    if (archiveList)
        delete archiveList;
}

void MythNativeWizard::keyPressEvent(QKeyEvent *e)
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
            if (getCurrentFocusWidget() == category_selector)
                category_selector->push(false);
            else
                nextPrevWidgetFocus(false);
        }
        else if (action == "RIGHT")
        {
            if (getCurrentFocusWidget() == category_selector)
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
        else
            handled = false;
    }

    if (!handled)
            MythThemedDialog::keyPressEvent(e);
}

void MythNativeWizard::toggleSelectedState()
{
    UIListBtnTypeItem *item = archive_list->GetItemCurrent();
    if (item->state() == UIListBtnTypeItem:: FullChecked)
    {
        if (selectedList.find((NativeItem *) item->getData()) != -1)
            selectedList.remove((NativeItem *) item->getData());
        item->setChecked(UIListBtnTypeItem:: NotChecked);
    }
    else
    {
        if (selectedList.find((NativeItem *) item->getData()) == -1)
            selectedList.append((NativeItem *) item->getData());

        item->setChecked(UIListBtnTypeItem:: FullChecked);
    }

    archive_list->refresh();
    updateSizeBar();

    updateSelectedArchiveList();
}

void MythNativeWizard::updateSizeBar()
{
    long long size = 0;
    NativeItem *a;

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

void MythNativeWizard::wireUpTheme()
{
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

    // make iso image checkbox
    createISO_check = getUICheckBoxType("makeisoimage");
    if (createISO_check)
    {
        connect(createISO_check, SIGNAL(pushed(bool)),
                this, SLOT(toggleCreateISO(bool)));
    }

    // burn dvdr checkbox
    doBurn_check = getUICheckBoxType("burntodvdr");
    if (doBurn_check)
    {
        connect(doBurn_check, SIGNAL(pushed(bool)),
                this, SLOT(toggleDoBurn(bool)));
    }

    // erase DVD RW checkbox
    eraseDvdRw_check = getUICheckBoxType("erasedvdrw");
    if (eraseDvdRw_check)
    {
        connect(eraseDvdRw_check, SIGNAL(pushed(bool)),
                this, SLOT(toggleEraseDvdRw(bool)));
    }

    buildFocusList();
}

void MythNativeWizard::titleChanged(UIListBtnTypeItem *item)
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

void MythNativeWizard::handleNextPage()
{
    if (getContext() == 1 && selectedList.count() == 0)
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), tr("Myth Archive"),
            tr("You need to select at least one item to archive!"));
        return;
    }

    if (getContext() == LAST_PAGE)
        done(Accepted); //archiveItems();
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

bool MythNativeWizard::extractDetailsFromFilename(const QString &filename, 
                                               QString &chanID, QString &startTime)
{
    cout << "Extracting details from: " << filename << endl;
    chanID = "";
    startTime = "";
    bool res = false;

    int sep = filename.find('_');
    if (sep != -1)
    {
        chanID = filename.left(sep);
        startTime = filename.mid(sep + 1, filename.length() - sep - 5);
        if (startTime.length() == 14)
        {
            QString year, month, day, hour, minute, second;
            year = startTime.mid(0, 4);
            month = startTime.mid(4, 2);
            day = startTime.mid(6, 2);
            hour = startTime.mid(8, 2);
            minute = startTime.mid(10, 2);
            second = startTime.mid(12, 2);
            startTime = QString("%1-%2-%3T%4:%5:%6").arg(year).arg(month).arg(day)
                .arg(hour).arg(minute).arg(second);
            res = true;
        }
    }
    cout << "chanid: " << chanID << " starttime: " << startTime << endl; 
    return res;
}

void MythNativeWizard::handlePrevPage()
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

void MythNativeWizard::handleCancel()
{
    done(Rejected);
}

QString MythNativeWizard::loadFile(const QString &filename)
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

void MythNativeWizard::updateArchiveList(void)
{
    archive_list->Reset();

    if (category_selector)
    {
        NativeItem *a;
        vector<NativeItem *>::iterator i = archiveList->begin();
        for ( ; i != archiveList->end(); i++)
        {
            a = *i;

            if (a->type == category_selector->getCurrentString() || 
                category_selector->getCurrentString() == tr("All Archive Items"))
            {
                UIListBtnTypeItem* item = new UIListBtnTypeItem(
                        archive_list, a->title);
                item->setCheckable(true);
                if (selectedList.find((NativeItem *) a) != -1) 
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

vector<NativeItem *> *MythNativeWizard::getArchiveListFromDB(void)
{
    // for the moment we only know how to archive recordings
    vector<NativeItem*> *archiveList = new vector<NativeItem*>;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT intid, type, title, subtitle, description, size, "
                  "startdate, starttime, filename, hascutlist "
                  "FROM archiveitems WHERE type = 'Recording' "
                  "ORDER BY title, subtitle");
    query.exec();
    if (query.isActive() && query.numRowsAffected())
    {
        while (query.next())
        {
            NativeItem *item = new NativeItem;

            item->id = query.value(0).toInt();
            item->type = QString::fromUtf8(query.value(1).toString());
            item->title = QString::fromUtf8(query.value(2).toString());
            item->subtitle = QString::fromUtf8(query.value(3).toString());
            item->description = QString::fromUtf8(query.value(4).toString());
            item->size = query.value(5).toLongLong();
            item->startDate = QString::fromUtf8(query.value(6).toString());
            item->startTime = QString::fromUtf8(query.value(7).toString());
            item->filename = QString::fromUtf8(query.value(8).toString()); // Utf8 ??
            item->hasCutlist = (query.value(9).toInt() > 0);
            item->useCutlist = false;
            item->editedDetails = false;

            archiveList->push_back(item);
        }
    }
    else
    {
        cout << "MythNativeWizard: Failed to get any archive items" << endl;
        return NULL;
    }

    return archiveList;
}

void MythNativeWizard::getArchiveList(void)
{
    NativeItem *a;
    archiveList = getArchiveListFromDB();
    QStringList categories;

    if (archiveList && archiveList->size() > 0)
    {
        vector<NativeItem *>::iterator i = archiveList->begin();
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

void MythNativeWizard::setCategory(int item)
{
    (void) item;
    updateArchiveList();
}

void MythNativeWizard::loadConfiguration(void)
{
    bCreateISO = (gContext->GetSetting("MythBurnCreateISO", "0") == "1");
    createISO_check->setState(bCreateISO);

    bDoBurn = (gContext->GetSetting("MythBurnBurnDVDr", "1") == "1");
    doBurn_check->setState(bDoBurn);

    bEraseDvdRw = (gContext->GetSetting("MythBurnEraseDvdRw", "0") == "1");
    eraseDvdRw_check->setState(bEraseDvdRw);
}

void MythNativeWizard::saveConfiguration(void)
{
    gContext->SaveSetting("MythBurnCreateISO", 
                          (createISO_check->getState() ? "1" : "0"));
    gContext->SaveSetting("MythBurnBurnDVDr", 
                          (doBurn_check->getState() ? "1" : "0"));
    gContext->SaveSetting("MythBurnEraseDvdRw", 
                          (eraseDvdRw_check->getState() ? "1" : "0"));
}

void MythNativeWizard::updateSelectedArchiveList(void)
{
    selected_list->Reset();

    NativeItem *a;

    for (a = selectedList.first(); a; a = selectedList.next())
    {
        QString s = a->title;
        UIListBtnTypeItem* item = new UIListBtnTypeItem(selected_list, s);
        item->setCheckable(true);
    }
}

void MythNativeWizard::showMenu()
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

void MythNativeWizard::closePopupMenu()
{
    if (!popupMenu)
        return;

    popupMenu->hide();
    delete popupMenu;
    popupMenu = NULL;
}

void MythNativeWizard::editDetails()
{
    if (!popupMenu)
        return;

    showEditMetadataDialog();
    closePopupMenu();
}

void MythNativeWizard::useSLDVD()
{
    if (!popupMenu)
        return;

    freeSpace = MAX_DVDR_SIZE_SL;
    updateSizeBar();
    closePopupMenu();
}

void MythNativeWizard::useDLDVD()
{
    if (!popupMenu)
        return;

    freeSpace = MAX_DVDR_SIZE_DL;
    updateSizeBar();
    closePopupMenu();
}

void MythNativeWizard::removeItem()
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

void MythNativeWizard::showEditMetadataDialog()
{
#if 0
    UIListBtnTypeItem *item = archive_list->GetItemCurrent();
    NativeItem *curItem = (NativeItem *) item->getData();

    if (!curItem)
        return;

    EditMetadataDialog editDialog(curItem, gContext->GetMainWindow(),
                                  "edit_metadata", "mythburn-", "edit metadata");
    if (editDialog.exec())
    {
        // update widgets to reflect any changes
        titleChanged(item);
        item->setText(curItem->title);
    }
#endif
}

void MythNativeWizard::advancedPressed()
{
    AdvancedOptions *dialog = new AdvancedOptions(gContext->GetMainWindow(),
                           "advanced_options", "mythburn-", "advanced options");
    int res = dialog->exec();
    delete dialog;

    if (res)
    {
        // need to reload our copy of setting in case they have changed
        loadConfiguration();
    }
}

void MythNativeWizard::createConfigFile(const QString &filename)
{
    QDomDocument doc("NATIVEARCHIVEJOB");

    QDomElement root = doc.createElement("nativearchivejob");
    doc.appendChild(root);

    QDomElement job = doc.createElement("job");
    root.appendChild(job);

    QDomElement media = doc.createElement("media");
    job.appendChild(media);

    // now loop though selected archive items and add them to the xml file
    NativeItem *i;

    for (i = selectedList.first(); i; i = selectedList.next())
    {
        QDomElement file = doc.createElement("file");
        file.setAttribute("type", i->type.lower() );
        file.setAttribute("title", i->title);
        file.setAttribute("filename", i->filename);
        file.setAttribute("delete", "0"); //FIXME
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
    if (!f.open(IO_WriteOnly))
    {
        cout << "MythNativeWizard::createConfigFile: Failed to open file for writing - "
                << filename << endl;
        return;
    }

    QTextStream t(&f);
    t << doc.toString(4);
    f.close();
}
