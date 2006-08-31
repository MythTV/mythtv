/*
	recordingselector.cpp

	
*/

#include <unistd.h>
#include <cstdlib>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>

using namespace std;

// qt
#include <qdir.h>
#include <qdom.h>

// mythtv
#include <mythtv/mythcontext.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/libmythtv/programinfo.h>
#include <mythtv/libmythtv/remoteutil.h>

// mytharchive
#include "recordingselector.h"
#include "archiveutil.h"

RecordingSelector::RecordingSelector(MythMainWindow *parent, QString window_name,
                                 QString theme_filename, const char *name)
                : MythThemedDialog(parent, window_name, theme_filename, name, true)
{
    recordingList = NULL;
    wireUpTheme();
    assignFirstFocus();
    updateForeground();
    popupMenu = NULL;
}

RecordingSelector::~RecordingSelector(void)
{
    if (recordingList)
        delete recordingList;
}

void RecordingSelector::keyPressEvent(QKeyEvent *e)
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
            done(0);
        }
        else if (action == "DOWN")
        {
            if (getCurrentFocusWidget() == recording_list)
            {
                recording_list->MoveDown(UIListBtnType::MoveItem);
                recording_list->refresh();
            }
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "UP")
        {
            if (getCurrentFocusWidget() == recording_list)
            {
                recording_list->MoveUp(UIListBtnType::MoveItem);
                recording_list->refresh();
            }
            else
                nextPrevWidgetFocus(false);
        }
        else if (action == "PAGEDOWN")
        {
            if (getCurrentFocusWidget() == recording_list)
            {
                recording_list->MoveDown(UIListBtnType::MovePage);
                recording_list->refresh();
            }
        }
        else if (action == "PAGEUP")
        {
            if (getCurrentFocusWidget() == recording_list)
            {
                recording_list->MoveUp(UIListBtnType::MovePage);
                recording_list->refresh();
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
            if (getCurrentFocusWidget() == recording_list)
                toggleSelectedState();
            else
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

void RecordingSelector::showMenu()
{
    if (popupMenu)
        return;

    popupMenu = new MythPopupBox(gContext->GetMainWindow(),
                                 "popupMenu");

    QButton *button;
    button = popupMenu->addButton(tr("Clear All"), this, SLOT(clearAll()));
    button->setFocus();
    popupMenu->addButton(tr("Select All"), this, SLOT(selectAll()));
    popupMenu->addButton(tr("Cancel"), this, SLOT(closePopupMenu()));

    popupMenu->ShowPopup(this, SLOT(closePopupMenu()));
}

void RecordingSelector::closePopupMenu()
{
    if (!popupMenu)
        return;

    popupMenu->hide();
    delete popupMenu;
    popupMenu = NULL;
}

void RecordingSelector::selectAll()
{
    if (!popupMenu)
        return;

    selectedList.clear();

    ProgramInfo *p;
    vector<ProgramInfo *>::iterator i = recordingList->begin();
    for ( ; i != recordingList->end(); i++)
    {
        p = *i;
        selectedList.append(p);
    }

    updateRecordingList();
    closePopupMenu();
}

void RecordingSelector::clearAll()
{
    if (!popupMenu)
        return;

    selectedList.clear();

    updateRecordingList();
    closePopupMenu();
}

void RecordingSelector::toggleSelectedState()
{
    UIListBtnTypeItem *item = recording_list->GetItemCurrent();
    if (item->state() == UIListBtnTypeItem:: FullChecked)
    {
        if (selectedList.find((ProgramInfo *) item->getData()) != -1)
            selectedList.remove((ProgramInfo *) item->getData());
        item->setChecked(UIListBtnTypeItem:: NotChecked);
    }
    else
    {
        if (selectedList.find((ProgramInfo *) item->getData()) == -1)
            selectedList.append((ProgramInfo *) item->getData());

        item->setChecked(UIListBtnTypeItem:: FullChecked);
    }

    recording_list->refresh();
}

void RecordingSelector::wireUpTheme()
{
    // ok button
    ok_button = getUITextButtonType("ok_button");
    if (ok_button)
    {
        ok_button->setText(tr("OK"));
        connect(ok_button, SIGNAL(pushed()), this, SLOT(OKPressed()));
    }

    // cancel button
    cancel_button = getUITextButtonType("cancel_button");
    if (cancel_button)
    {
        cancel_button->setText(tr("Cancel"));
        connect(cancel_button, SIGNAL(pushed()), this, SLOT(cancelPressed()));
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
    preview_image = getUIImageType("preview_image");
    cutlist_image = getUIImageType("cutlist_image");


    recording_list = getUIListBtnType("recordinglist");
    if (recording_list)
    {
        getRecordingList();
        connect(recording_list, SIGNAL(itemSelected(UIListBtnTypeItem *)),
                this, SLOT(titleChanged(UIListBtnTypeItem *)));
    }

    updateSelectedList();
    updateRecordingList();

    buildFocusList();
}

void RecordingSelector::titleChanged(UIListBtnTypeItem *item)
{
    ProgramInfo *p;

    p = (ProgramInfo *) item->getData();

    if (!p)
        return;

    if (title_text)
        title_text->SetText(p->title);

    if (datetime_text)
        datetime_text->SetText(p->startts.toString("dd MMM yy (hh:mm)"));

    if (description_text)
        description_text->SetText(
                (p->subtitle != "" ? p->subtitle + "\n" : "") + p->description);

    if (filesize_text)
    {
        filesize_text->SetText(formatSize(p->filesize / 1024));
    }

    if (cutlist_image)
    {
        if (p->programflags & FL_CUTLIST)
            cutlist_image->show();
        else
            cutlist_image->hide();
    }

    if (preview_image)
    {
        // try to locate a preview image
        if (QFile::exists(p->pathname + ".png"))
        {
            preview_image->SetImage(p->pathname + ".png");
            preview_image->LoadImage();
        }
        else
        {
            preview_image->SetImage("blank.png");
            preview_image->LoadImage();
        }
    }

    buildFocusList();
}

void RecordingSelector::OKPressed()
{
    if (selectedList.count() == 0)
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), tr("Myth Archive"),
                                  tr("You need to select at least one recording!"));
        return;
    }

    // remove all recordings from archivelist
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM archiveitems WHERE type = 'Recording'");
    query.exec();

    // loop though selected recordings and add them to the archiveitems table
    ProgramInfo *p;

    for (p = selectedList.first(); p; p = selectedList.next())
    {
        query.prepare("INSERT INTO archiveitems (type, title, subtitle,"
                "description, startdate, starttime, size, filename, hascutlist) "
                "VALUES(:TYPE, :TITLE, :SUBTITLE, :DESCRIPTION, :STARTDATE, "
                ":STARTTIME, :SIZE, :FILENAME, :HASCUTLIST);");
        query.bindValue(":TYPE", "Recording");
        query.bindValue(":TITLE", p->title.utf8());
        query.bindValue(":SUBTITLE", p->subtitle.utf8());
        query.bindValue(":DESCRIPTION", p->description.utf8());
        query.bindValue(":STARTDATE", p->startts.toString("dd MMM yy"));
        query.bindValue(":STARTTIME", p->startts.toString("(hh:mm)"));
        query.bindValue(":SIZE", p->filesize);
        query.bindValue(":FILENAME", p->GetRecordBasename());
        query.bindValue(":HASCUTLIST", (p->programflags & FL_CUTLIST));
        if (!query.exec())
            MythContext::DBError("archive item insert", query);
    }

    done(Accepted);
}

void RecordingSelector::cancelPressed()
{
    reject();
}

void RecordingSelector::updateRecordingList(void)
{
    if (!recordingList)
        return;

    recording_list->Reset();

    if (category_selector)
    {
        ProgramInfo *p;
        vector<ProgramInfo *>::iterator i = recordingList->begin();
        for ( ; i != recordingList->end(); i++)
        {
            p = *i;

            if (p->title == category_selector->getCurrentString() || 
                category_selector->getCurrentString() == tr("All Recordings"))
            {
                UIListBtnTypeItem* item = new UIListBtnTypeItem(recording_list, 
                        p->title + " ~ " + 
                        p->startts.toString("dd MMM yy (hh:mm)"));
                item->setCheckable(true);
                if (selectedList.find((ProgramInfo *) p) != -1) 
                {
                    item->setChecked(UIListBtnTypeItem::FullChecked);
                }
                else 
                {
                    item->setChecked(UIListBtnTypeItem::NotChecked);
                }

                item->setData(p);
            }
        }
    }

    recording_list->SetItemCurrent(recording_list->GetItemFirst());
    titleChanged(recording_list->GetItemCurrent());
    recording_list->refresh();
}

void RecordingSelector::getRecordingList(void)
{
    ProgramInfo *p;
    recordingList = RemoteGetRecordedList(true);
    QStringList categories;

    if (recordingList && recordingList->size() > 0)
    {
        vector<ProgramInfo *>::iterator i = recordingList->begin();
        for ( ; i != recordingList->end(); i++)
        {
            p = *i;

            if (categories.find(p->title) == categories.end())
                categories.append(p->title);
        }
    }
    else
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), tr("Myth Burn"),
                                  tr("You don't have any recordings!\n\nClick OK"));
        QTimer::singleShot(100, this, SLOT(cancelPressed()));
        return;
    }

    // sort and add categories to selector
    categories.sort();

    if (category_selector)
    {
        category_selector->addItem(0, tr("All Recordings"));
        category_selector->setToItem(0);
    }

    for (uint x = 1; x <= categories.count(); x++)
    {
        if (category_selector)
            category_selector->addItem(x, categories[x-1]); 
    }

    setCategory(0);
}

void RecordingSelector::setCategory(int item)
{
    item = item;
    updateRecordingList();
}

void RecordingSelector::updateSelectedList()
{
    if (!recordingList)
        return;

    selectedList.clear();
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT filename FROM archiveitems WHERE type = 'Recording'");
    query.exec();
    if (query.isActive() && query.numRowsAffected())
    {
        while (query.next())
        {
            QString filename = QString::fromUtf8(query.value(0).toString()); 

            ProgramInfo *p;
            vector<ProgramInfo *>::iterator i = recordingList->begin();
            for ( ; i != recordingList->end(); i++)
            {
                p = *i;
                if (p->GetRecordBasename() == filename) // is this unique ??
                {
                    if (selectedList.find(p) == -1)
                        selectedList.append(p);
                    break;
                }
            }
        }
    }
}

