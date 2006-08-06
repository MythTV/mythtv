/*
	videoselector.cpp

	
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
#include <mythtv/mythdbcon.h>

// mytharchive
#include "videoselector.h"
#include "archiveutil.h"

VideoSelector::VideoSelector(MythMainWindow *parent, QString window_name,
                                 QString theme_filename, const char *name)
                : MythThemedDialog(parent, window_name, theme_filename, name, true)
{
    currentParentalLevel = 1;
    videoList = NULL;
    wireUpTheme();
    assignFirstFocus();
    updateForeground();
}

VideoSelector::~VideoSelector(void)
{
    if (videoList)
        delete videoList;
}

void VideoSelector::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Burn", e, actions);

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
            if (getCurrentFocusWidget() == video_list)
            {
                video_list->MoveDown(UIListBtnType::MoveItem);
                video_list->refresh();
            }
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "UP")
        {
            if (getCurrentFocusWidget() == video_list)
            {
                video_list->MoveUp(UIListBtnType::MoveItem);
                video_list->refresh();
            }
            else
                nextPrevWidgetFocus(false);
        }
        else if (action == "PAGEDOWN")
        {
            if (getCurrentFocusWidget() == video_list)
            {
                video_list->MoveDown(UIListBtnType::MovePage);
                video_list->refresh();
            }
        }
        else if (action == "PAGEUP")
        {
            if (getCurrentFocusWidget() == video_list)
            {
                video_list->MoveUp(UIListBtnType::MovePage);
                video_list->refresh();
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
            if (getCurrentFocusWidget() == video_list)
                toggleSelectedState();
            else
                activateCurrent();
        }
        else if (action == "MENU")
        {
            showMenu();
        }
        else if (action == "1")
        {
            setParentalLevel(1);
        }
        else if (action == "2")
        {
            setParentalLevel(2);
        }
        else if (action == "3")
        {
            setParentalLevel(3);
        }
        else if (action == "4")
        {
            setParentalLevel(4);
        }
        else
            handled = false;
    }

    if (!handled)
            MythThemedDialog::keyPressEvent(e);
}

void VideoSelector::showMenu()
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

void VideoSelector::closePopupMenu()
{
    if (!popupMenu)
        return;

    popupMenu->hide();
    delete popupMenu;
    popupMenu = NULL;
}

void VideoSelector::selectAll()
{
    if (!popupMenu)
        return;

    selectedList.clear();

    VideoInfo *v;
    vector<VideoInfo *>::iterator i = videoList->begin();
    for ( ; i != videoList->end(); i++)
    {
        v = *i;
        selectedList.append(v);
    }

    updateVideoList();
    closePopupMenu();
}

void VideoSelector::clearAll()
{
    if (!popupMenu)
        return;

    selectedList.clear();

    updateVideoList();
    closePopupMenu();
}

void VideoSelector::toggleSelectedState()
{
    UIListBtnTypeItem *item = video_list->GetItemCurrent();

    if (!item)
        return;

    if (item->state() == UIListBtnTypeItem:: FullChecked)
    {
        if (selectedList.find((VideoInfo *) item->getData()) != -1)
            selectedList.remove((VideoInfo *) item->getData());
        item->setChecked(UIListBtnTypeItem:: NotChecked);
    }
    else
    {
        if (selectedList.find((VideoInfo *) item->getData()) == -1)
            selectedList.append((VideoInfo *) item->getData());

        item->setChecked(UIListBtnTypeItem:: FullChecked);
    }

    video_list->refresh();
}

void VideoSelector::wireUpTheme()
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

    // video selector
    category_selector = getUISelectorType("category_selector");
    if (category_selector)
    {
        connect(category_selector, SIGNAL(pushed(int)),
                this, SLOT(setCategory(int)));
    }

    title_text = getUITextType("videotitle");
    plot_text = getUITextType("videoplot");
    filesize_text = getUITextType("filesize");
    cover_image = getUIImageType("cover_image");
    warning_text =  getUITextType("warning_text");
    pl_text =  getUITextType("parentallevel_text");

    video_list = getUIListBtnType("videolist");
    if (video_list)
    {
        getVideoList();
        connect(video_list, SIGNAL(itemSelected(UIListBtnTypeItem *)),
                this, SLOT(titleChanged(UIListBtnTypeItem *)));
    }

    updateSelectedList();
    updateVideoList();
    buildFocusList();
}

void VideoSelector::titleChanged(UIListBtnTypeItem *item)
{
    VideoInfo *v;

    v = (VideoInfo *) item->getData();

    if (!v)
        return;

    if (title_text)
        title_text->SetText(v->title);

    if (plot_text)
        plot_text->SetText(v->plot);

    if (cover_image)
    {
        if (v->coverfile != "" && v->coverfile != "No Cover")
        {
            cover_image->SetImage(v->coverfile);
            cover_image->LoadImage();
        }
        else
        {
            cover_image->SetImage("blank.png");
            cover_image->LoadImage();
        }
    }

    if (filesize_text)
    {
        if (v->size == 0)
        {
            QFile file(v->filename);
            if (file.exists())
                v->size = (long long)file.size();
            else
                cout << "VideoSelector: Cannot find file: " << v->filename << endl;
        }

        filesize_text->SetText(formatSize(v->size / 1024));
    }

    buildFocusList();
}

void VideoSelector::OKPressed()
{
    if (selectedList.count() == 0)
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), tr("Myth Archive"),
            tr("You need to select at least one video file!"));
        return;
    }

    // remove all videos from archivelist
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM archiveitems WHERE type = 'Video'");
    query.exec();

    // loop though selected videos and add them to the archiveitems table
    VideoInfo *v;

    for (v = selectedList.first(); v; v = selectedList.next())
    {
        QFile file(v->filename);
        if (file.exists())
        {
            query.prepare("INSERT INTO archiveitems (type, title, subtitle, "
                    "description, startdate, starttime, size, filename, hascutlist) "
                    "VALUES(:TYPE, :TITLE, :SUBTITLE, :DESCRIPTION, :STARTDATE, "
                    ":STARTTIME, :SIZE, :FILENAME, :HASCUTLIST);");
            query.bindValue(":TYPE", "Video");
            query.bindValue(":TITLE", v->title.utf8());
            query.bindValue(":SUBTITLE", "");
            query.bindValue(":DESCRIPTION", v->plot.utf8());
            query.bindValue(":STARTDATE", "");
            query.bindValue(":STARTTIME", "");
            query.bindValue(":SIZE", (long long)file.size());
            query.bindValue(":FILENAME", v->filename);
            query.bindValue(":HASCUTLIST", 0);
            if (!query.exec())
                MythContext::DBError("archive item insert", query);
        }
    }

    done(Accepted);
}

void VideoSelector::cancelPressed()
{
    reject();
}

void VideoSelector::updateVideoList(void)
{
    if (!videoList)
        return;

    video_list->Reset();

    if (category_selector)
    {
        VideoInfo *v;
        vector<VideoInfo *>::iterator i = videoList->begin();
        for ( ; i != videoList->end(); i++)
        {
            v = *i;

            if (v->category == category_selector->getCurrentString() || 
                category_selector->getCurrentString() == tr("All Videos"))
            {
                if (v->parentalLevel <= currentParentalLevel)
                {
                    UIListBtnTypeItem* item = new UIListBtnTypeItem(
                            video_list, v->title);
                    item->setCheckable(true);
                    if (selectedList.find((VideoInfo *) v) != -1)
                    {
                        item->setChecked(UIListBtnTypeItem::FullChecked);
                    }
                    else 
                    {
                        item->setChecked(UIListBtnTypeItem::NotChecked);
                    }

                    item->setData(v);
                }
            }
        }
    }

    if (video_list->GetCount() > 0)
    {
        video_list->SetItemCurrent(video_list->GetItemFirst());
        titleChanged(video_list->GetItemCurrent());
        warning_text->hide();
    }
    else
    {
        warning_text->show();
        title_text->SetText("");
        plot_text->SetText("");
        cover_image->SetImage("blank.png");
        cover_image->LoadImage();
        filesize_text->SetText("");
    }

    video_list->refresh();
}

vector<VideoInfo *> *VideoSelector::getVideoListFromDB(void)
{
    // get a list of category's
    typedef QMap<int, QString> CategoryMap;
    CategoryMap categoryMap;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT intid, category FROM videocategory");
    query.exec();
    if (query.isActive() && query.numRowsAffected())
    {
        while (query.next())
        {
            int id = query.value(0).toInt();
            QString category = QString::fromUtf8(query.value(1).toString());
            categoryMap.insert(id, category);
        }
    }

    vector<VideoInfo*> *videoList = new vector<VideoInfo*>;

    query.prepare("SELECT intid, title, plot, length, filename, coverfile, "
                  "category, showlevel "
                  "FROM videometadata ORDER BY title");
    query.exec();
    if (query.isActive() && query.numRowsAffected())
    {
        QString artist, genre;
        while (query.next())
        {
            VideoInfo *info = new VideoInfo;

            info->id = query.value(0).toInt();
            info->title = QString::fromUtf8(query.value(1).toString());
            info->plot = QString::fromUtf8(query.value(2).toString());
            info->size = 0; //query.value(3).toInt();
            info->filename = QString::fromUtf8(query.value(4).toString()); // Utf8 ??
            info->coverfile = QString::fromUtf8(query.value(5).toString()); // Utf8 ??
            info->category = categoryMap[query.value(6).toInt()];
            info->parentalLevel = query.value(7).toInt();
            if (info->category == "")
                info->category = "(None)";
            videoList->push_back(info);
        }
    }
    else
    {
        cout << "VideoSelector: Failed to get any video's" << endl;
        return NULL;
    }

    return videoList;
}

void VideoSelector::getVideoList(void)
{
    VideoInfo *v;
    videoList = getVideoListFromDB();
    QStringList categories;

    if (videoList && videoList->size() > 0)
    {
        vector<VideoInfo *>::iterator i = videoList->begin();
        for ( ; i != videoList->end(); i++)
        {
            v = *i;

            if (categories.find(v->category) == categories.end())
                categories.append(v->category);
        }
    }
    else
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), tr("Video Selector"),
                                  tr("You don't have any videos!\n\nClick OK"));
        QTimer::singleShot(100, this, SLOT(cancelPressed()));
        return;
    }

    // sort and add categories to selector
    categories.sort();

    if (category_selector)
    {
        category_selector->addItem(0, "All Videos");
        category_selector->setToItem(0);
    }

    for (uint x = 1; x <= categories.count(); x++)
    {
        if (category_selector)
            category_selector->addItem(x, categories[x-1]); 
    }

    setCategory(0);
}

void VideoSelector::setCategory(int item)
{
    (void) item;
    updateVideoList();
}

void VideoSelector::updateSelectedList()
{
    if (!videoList)
        return;

    selectedList.clear();
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT filename FROM archiveitems WHERE type = 'Video'");
    query.exec();
    if (query.isActive() && query.numRowsAffected())
    {
        while (query.next())
        {
            QString filename = QString::fromUtf8(query.value(0).toString()); 

            VideoInfo *v;
            vector<VideoInfo *>::iterator i = videoList->begin();
            for ( ; i != videoList->end(); i++)
            {
                v = *i;
                if (v->filename == filename)
                {
                    if (selectedList.find(v) == -1)
                        selectedList.append(v);
                    break;
                }
            }
        }
    }
}

void VideoSelector::setParentalLevel(int which_level)
{
    if (which_level < 1)
        which_level = 1;

    if (which_level > 4)
        which_level = 4;

    if ((which_level > currentParentalLevel) && !checkParentPassword())
        which_level = currentParentalLevel;


    if (currentParentalLevel != which_level)
    {
        currentParentalLevel = which_level;
        updateVideoList();
        pl_text->SetText(QString::number(which_level));
    }
}

bool VideoSelector::checkParentPassword()
{
    QDateTime curr_time = QDateTime::currentDateTime();
    QString last_time_stamp = gContext->GetSetting("VideoPasswordTime");
    QString password = gContext->GetSetting("VideoAdminPassword");
    if (password.length() < 1)
    {
        return true;
    }

    //  See if we recently (and succesfully) asked for a password
    if (last_time_stamp.length() < 1)
    {
        //  Probably first time used
    }
    else
    {
        QDateTime last_time = QDateTime::fromString(last_time_stamp, Qt::TextDate);
        if (last_time.secsTo(curr_time) < 120)
        {
            //  Two minute window
            last_time_stamp = curr_time.toString(Qt::TextDate);
            gContext->SetSetting("VideoPasswordTime", last_time_stamp);
            gContext->SaveSetting("VideoPasswordTime", last_time_stamp);
            return true;
        }
    }

    //  See if there is a password set
    if (password.length() > 0)
    {
        bool ok = false;
        MythPasswordDialog *pwd = new MythPasswordDialog(tr("Parental Pin:"),
                &ok,
                password,
                gContext->GetMainWindow());
        pwd->exec();
        delete pwd;
        if (ok)
        {
            //  All is good
            last_time_stamp = curr_time.toString(Qt::TextDate);
            gContext->SetSetting("VideoPasswordTime", last_time_stamp);
            gContext->SaveSetting("VideoPasswordTime", last_time_stamp);
            return true;
        }
    }
    else
    {
        return true;
    }

    return false;
}

