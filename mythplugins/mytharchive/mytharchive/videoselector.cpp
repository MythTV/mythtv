// c
#include <cstdlib>
#include <stdlib.h>
#include <unistd.h>

// qt
#include <QDir>
#include <QTimer>

// mythtv
#include <mythcontext.h>
#include <dialogbox.h>
#include <mythdb.h>
#include <mythuitext.h>
#include <mythuibutton.h>
#include <mythuiimage.h>
#include <mythuibuttonlist.h>
#include <mythmainwindow.h>
#include <mythdialogbox.h>

// mytharchive
#include "videoselector.h"
#include "archiveutil.h"

using namespace std;

VideoSelector::VideoSelector(MythScreenStack *parent, QList<ArchiveItem *> *archiveList)
              :MythScreenType(parent, "VideoSelector"),
               m_archiveList(archiveList), m_videoList(NULL),
               m_currentParentalLevel(ParentalLevel::plNone),
               m_plText(NULL), m_videoButtonList(NULL), m_warningText(),
               m_okButton(NULL), m_cancelButton(NULL), m_categorySelector(NULL),
               m_titleText(NULL), m_filesizeText(NULL), m_plotText(NULL),
               m_coverImage(NULL)
{
    m_parentalLevelChecker = new ParentalLevelChangeChecker();
    connect(m_parentalLevelChecker, SIGNAL(SigResultReady(bool, ParentalLevel::Level)),
            this, SLOT(parentalLevelChanged(bool, ParentalLevel::Level)));
}

VideoSelector::~VideoSelector(void)
{
    if (m_videoList)
        delete m_videoList;

    while (!m_selectedList.isEmpty())
         delete m_selectedList.takeFirst();
    m_selectedList.clear();

    delete m_parentalLevelChecker;
}

bool VideoSelector::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("mytharchive-ui.xml", "video_selector", this);

    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_okButton, "ok_button", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel_button", &err);
    UIUtilE::Assign(this, m_categorySelector, "category_selector", &err);
    UIUtilE::Assign(this, m_videoButtonList, "videolist", &err);
    UIUtilE::Assign(this, m_titleText, "videotitle", &err);
    UIUtilE::Assign(this, m_plotText, "videoplot", &err);
    UIUtilE::Assign(this, m_filesizeText, "filesize", &err);
    UIUtilE::Assign(this, m_coverImage, "cover_image", &err);
    UIUtilE::Assign(this, m_warningText, "warning_text", &err);
    UIUtilE::Assign(this, m_plText, "parentallevel_text", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'video_selector'");
        return false;
    }

    connect(m_okButton, SIGNAL(Clicked()), SLOT(OKPressed()));
    connect(m_cancelButton, SIGNAL(Clicked()), SLOT(cancelPressed()));

    connect(m_categorySelector, SIGNAL(itemSelected(MythUIButtonListItem *)),
            SLOT(setCategory(MythUIButtonListItem *)));

    getVideoList();
    connect(m_videoButtonList, SIGNAL(itemSelected(MythUIButtonListItem *)),
            SLOT(titleChanged(MythUIButtonListItem *)));
    connect(m_videoButtonList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            SLOT(toggleSelected(MythUIButtonListItem *)));

    BuildFocusList();

    SetFocusWidget(m_videoButtonList);

    setParentalLevel(ParentalLevel::plLowest);

    updateSelectedList();
    updateVideoList();

    return true;
}

bool VideoSelector::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
        {
            showMenu();
        }
        else if (action == "1")
        {
            setParentalLevel(ParentalLevel::plLowest);
        }
        else if (action == "2")
        {
            setParentalLevel(ParentalLevel::plLow);
        }
        else if (action == "3")
        {
            setParentalLevel(ParentalLevel::plMedium);
        }
        else if (action == "4")
        {
            setParentalLevel(ParentalLevel::plHigh);
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void VideoSelector::showMenu()
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythDialogBox *menuPopup = new MythDialogBox(tr("Menu"), popupStack, "actionmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);

    menuPopup->SetReturnEvent(this, "action");

    menuPopup->AddButton(tr("Clear All"), SLOT(clearAll()));
    menuPopup->AddButton(tr("Select All"), SLOT(selectAll()));
}

void VideoSelector::selectAll()
{
    while (!m_selectedList.isEmpty())
         m_selectedList.takeFirst();
    m_selectedList.clear();

    VideoInfo *v;
    vector<VideoInfo *>::iterator i = m_videoList->begin();
    for ( ; i != m_videoList->end(); ++i)
    {
        v = *i;
        m_selectedList.append(v);
    }

    updateVideoList();
}

void VideoSelector::clearAll()
{
    while (!m_selectedList.isEmpty())
         m_selectedList.takeFirst();
    m_selectedList.clear();

    updateVideoList();
}

void VideoSelector::toggleSelected(MythUIButtonListItem *item)
{
    if (item->state() == MythUIButtonListItem::FullChecked)
    {
        int index = m_selectedList.indexOf(
                                qVariantValue<VideoInfo *>(item->GetData()));
        if (index != -1)
            m_selectedList.takeAt(index);
        item->setChecked(MythUIButtonListItem::NotChecked);
    }
    else
    {
        int index = m_selectedList.indexOf(
                                qVariantValue<VideoInfo *>(item->GetData()));
        if (index == -1)
            m_selectedList.append(qVariantValue<VideoInfo *>(item->GetData()));

        item->setChecked(MythUIButtonListItem::FullChecked);
    }
}

void VideoSelector::titleChanged(MythUIButtonListItem *item)
{
    VideoInfo *v;

    v = qVariantValue<VideoInfo *>(item->GetData());

    if (!v)
        return;

    if (m_titleText)
        m_titleText->SetText(v->title);

    if (m_plotText)
        m_plotText->SetText(v->plot);

    if (m_coverImage)
    {
        if (v->coverfile != "" && v->coverfile != "No Cover")
        {
            m_coverImage->SetFilename(v->coverfile);
            m_coverImage->Load();
        }
        else
        {
            m_coverImage->SetFilename("blank.png");
            m_coverImage->Load();
        }
    }

    if (m_filesizeText)
    {
        if (v->size == 0)
        {
            QFile file(v->filename);
            if (file.exists())
                v->size = (uint64_t)file.size();
            else
                LOG(VB_GENERAL, LOG_ERR,
                    QString("VideoSelector: Cannot find file: %1")
                        .arg(v->filename));
        }

        m_filesizeText->SetText(formatSize(v->size / 1024));
    }
}

void VideoSelector::OKPressed()
{
    // loop though selected videos and add them to the list
    VideoInfo *v;
    ArchiveItem *a;

    // remove any items that have been removed from the list
    QList<ArchiveItem *> tempAList;
    for (int x = 0; x < m_archiveList->size(); x++)
    {
        a = m_archiveList->at(x);
        bool found = false;

        for (int y = 0; y < m_selectedList.size(); y++)
        {
            v = m_selectedList.at(y);
            if (a->type != "Video" || a->filename == v->filename)
            {
                found = true;
                break;
            }
        }

        if (!found)
            tempAList.append(a);
    }

    for (int x = 0; x < tempAList.size(); x++)
        m_archiveList->removeAll(tempAList.at(x));

    // remove any items that are already in the list
    QList<VideoInfo *> tempSList;
    for (int x = 0; x < m_selectedList.size(); x++)
    {
        v = m_selectedList.at(x);

        for (int y = 0; y < m_archiveList->size(); y++)
        {
            a = m_archiveList->at(y);
            if (a->filename == v->filename)
            {
                tempSList.append(v);
                break;
            }
        }
    }

    for (int x = 0; x < tempSList.size(); x++)
        m_selectedList.removeAll(tempSList.at(x));

    // add all that are left
    for (int x = 0; x < m_selectedList.size(); x++)
    {
        v = m_selectedList.at(x);
        a = new ArchiveItem;
        a->type = "Video";
        a->title = v->title;
        a->subtitle = "";
        a->description = v->plot;
        a->startDate = "";
        a->startTime = "";
        a->size = v->size;
        a->filename = v->filename;
        a->hasCutlist = false;
        a->useCutlist = false;
        a->duration = 0;
        a->cutDuration = 0;
        a->videoWidth = 0;
        a->videoHeight = 0;
        a->fileCodec = "";
        a->videoCodec = "";
        a->encoderProfile = NULL;
        a->editedDetails = false;
        m_archiveList->append(a);
    }

    emit haveResult(true);
    Close();
}

void VideoSelector::cancelPressed()
{
    emit haveResult(false);
    Close();
}

void VideoSelector::updateVideoList(void)
{
    if (!m_videoList)
        return;

    m_videoButtonList->Reset();

    if (m_categorySelector)
    {
        VideoInfo *v;
        vector<VideoInfo *>::iterator i = m_videoList->begin();
        for ( ; i != m_videoList->end(); ++i)
        {
            v = *i;

            if (v->category == m_categorySelector->GetValue() ||
                m_categorySelector->GetValue() == tr("All Videos"))
            {
                if (v->parentalLevel <= m_currentParentalLevel)
                {
                    MythUIButtonListItem* item = new MythUIButtonListItem(
                            m_videoButtonList, v->title);
                    item->setCheckable(true);
                    if (m_selectedList.indexOf((VideoInfo *) v) != -1)
                    {
                        item->setChecked(MythUIButtonListItem::FullChecked);
                    }
                    else
                    {
                        item->setChecked(MythUIButtonListItem::NotChecked);
                    }

                    item->SetData(qVariantFromValue(v));
                }
            }
        }
    }

    if (m_videoButtonList->GetCount() > 0)
    {
        m_videoButtonList->SetItemCurrent(m_videoButtonList->GetItemFirst());
        titleChanged(m_videoButtonList->GetItemCurrent());
        m_warningText->Hide();
    }
    else
    {
        m_warningText->Show();
        m_titleText->Reset();
        m_plotText->Reset();
        m_coverImage->SetFilename("blank.png");
        m_coverImage->Load();
        m_filesizeText->Reset();
    }
}

vector<VideoInfo *> *VideoSelector::getVideoListFromDB(void)
{
    // get a list of category's
    typedef QMap<int, QString> CategoryMap;
    CategoryMap categoryMap;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT intid, category FROM videocategory");

    if (query.exec())
    {
        while (query.next())
        {
            int id = query.value(0).toInt();
            QString category = query.value(1).toString();
            categoryMap.insert(id, category);
        }
    }

    query.prepare("SELECT intid, title, plot, length, filename, coverfile, "
                  "category, showlevel, subtitle, season, episode "
                  "FROM videometadata ORDER BY title,season,episode");

    if (query.exec() && query.size())
    {
        vector<VideoInfo*> *videoList = new vector<VideoInfo*>;
        QString artist, genre, episode;
        while (query.next())
        {
            VideoInfo *info = new VideoInfo;

            info->id = query.value(0).toInt();
            if (query.value(9).toInt() > 0)
            {
                episode = query.value(10).toString();
                if (episode.size() < 2)
                        episode.prepend("0");
                    info->title = QString("%1 %2x%3 - %4")
                                .arg(query.value(1).toString())
                                .arg(query.value(9).toString())
                                .arg(episode)
                                .arg(query.value(8).toString());
            }
            else
                info->title = query.value(1).toString();

            info->plot = query.value(2).toString();
            info->size = 0; //query.value(3).toInt();
            info->filename = query.value(4).toString();
            info->coverfile = query.value(5).toString();
            info->category = categoryMap[query.value(6).toInt()];
            info->parentalLevel = query.value(7).toInt();
            if (info->category.isEmpty())
                info->category = "(None)";
            videoList->push_back(info);
        }
        return videoList;
    }
    
    
    LOG(VB_GENERAL, LOG_ERR, "VideoSelector: Failed to get any video's");
    return NULL;
}

void VideoSelector::getVideoList(void)
{
    VideoInfo *v;
    m_videoList = getVideoListFromDB();
    QStringList categories;

    if (m_videoList && !m_videoList->empty())
    {
        vector<VideoInfo *>::iterator i = m_videoList->begin();
        for ( ; i != m_videoList->end(); ++i)
        {
            v = *i;

            if (categories.indexOf(v->category) == -1)
                categories.append(v->category);
        }
    }
    else
    {
        QTimer::singleShot(100, this, SLOT(cancelPressed()));
        return;
    }

    // sort and add categories to selector
    categories.sort();

    if (m_categorySelector)
    {
        new MythUIButtonListItem(m_categorySelector, tr("All Videos"));
        m_categorySelector->SetItemCurrent(0);

        for (int x = 0; x < categories.count(); x++)
        {
            new MythUIButtonListItem(m_categorySelector, categories[x]);
        }
    }

    setCategory(0);
}

void VideoSelector::setCategory(MythUIButtonListItem *item)
{
    (void)item;
    updateVideoList();
}

void VideoSelector::updateSelectedList()
{
    if (!m_videoList)
        return;

    m_selectedList.clear();

    ArchiveItem *a;
    VideoInfo *v;
    for (int x = 0; x < m_archiveList->size(); x++)
    {
        a = m_archiveList->at(x);
        for (uint y = 0; y < m_videoList->size(); y++)
        {
            v = m_videoList->at(y);
            if (v->filename == a->filename)
            {
                if (m_selectedList.indexOf(v) == -1)
                    m_selectedList.append(v);
                break;
            }
        }
    }
}

void VideoSelector::setParentalLevel(ParentalLevel::Level level)
{
    m_parentalLevelChecker->Check(m_currentParentalLevel, level);
}

void VideoSelector::parentalLevelChanged(bool passwordValid, ParentalLevel::Level newLevel)
{
    if (passwordValid)
    {
        m_currentParentalLevel = newLevel;
        updateVideoList();
        m_plText->SetText(QString::number(newLevel));
    }
    else
        ShowOkPopup(tr("You need to enter a valid password for this parental level"));
}

