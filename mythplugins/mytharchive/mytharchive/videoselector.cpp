// c
#include <chrono>
#include <cstdlib>
#include <unistd.h>

// qt
#include <QDir>
#include <QTimer>

// mythtv
#include <libmyth/mythcontext.h>
#include <libmythbase/mythdb.h>
#include <libmythbase/remotefile.h>
#include <libmythbase/stringutil.h>
#include <libmythmetadata/videoutils.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuiimage.h>
#include <libmythui/mythuitext.h>

// mytharchive
#include "archiveutil.h"
#include "videoselector.h"

VideoSelector::VideoSelector(MythScreenStack *parent, QList<ArchiveItem *> *archiveList)
              :MythScreenType(parent, "VideoSelector"),
               m_parentalLevelChecker(new ParentalLevelChangeChecker()),
               m_archiveList(archiveList)
{
    connect(m_parentalLevelChecker, &ParentalLevelChangeChecker::SigResultReady,
            this, &VideoSelector::parentalLevelChanged);
}

VideoSelector::~VideoSelector(void)
{
    delete m_videoList;
    while (!m_selectedList.isEmpty())
         delete m_selectedList.takeFirst();
    m_selectedList.clear();
    delete m_parentalLevelChecker;
}

bool VideoSelector::Create(void)
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("mytharchive-ui.xml", "video_selector", this);
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

    connect(m_okButton, &MythUIButton::Clicked, this, &VideoSelector::OKPressed);
    connect(m_cancelButton, &MythUIButton::Clicked, this, &VideoSelector::cancelPressed);

    connect(m_categorySelector, &MythUIButtonList::itemSelected,
            this, &VideoSelector::setCategory);

    getVideoList();
    connect(m_videoButtonList, &MythUIButtonList::itemSelected,
            this, &VideoSelector::titleChanged);
    connect(m_videoButtonList, &MythUIButtonList::itemClicked,
            this, &VideoSelector::toggleSelected);

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

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "MENU")
        {
            ShowMenu();
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
        {
            handled = false;
        }
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void VideoSelector::ShowMenu()
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *menuPopup = new MythDialogBox(tr("Menu"), popupStack, "actionmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);

    menuPopup->SetReturnEvent(this, "action");

    menuPopup->AddButton(tr("Clear All"), &VideoSelector::clearAll);
    menuPopup->AddButton(tr("Select All"), &VideoSelector::selectAll);
}

void VideoSelector::selectAll()
{
    while (!m_selectedList.isEmpty())
         m_selectedList.takeFirst();
    m_selectedList.clear();

    for (auto *v : *m_videoList)
        m_selectedList.append(v);

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
        int index = m_selectedList.indexOf(item->GetData().value<VideoInfo *>());
        if (index != -1)
            m_selectedList.takeAt(index);
        item->setChecked(MythUIButtonListItem::NotChecked);
    }
    else
    {
        int index = m_selectedList.indexOf(item->GetData().value<VideoInfo *>());
        if (index == -1)
            m_selectedList.append(item->GetData().value<VideoInfo *>());

        item->setChecked(MythUIButtonListItem::FullChecked);
    }
}

void VideoSelector::titleChanged(MythUIButtonListItem *item)
{
    auto *v = item->GetData().value<VideoInfo *>();

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
            struct stat fileinfo {};

            bool bExists = RemoteFile::Exists(v->filename, &fileinfo);
            if (bExists)
                v->size = (uint64_t)fileinfo.st_size;

            if (!bExists)
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("VideoSelector: Cannot find file: %1")
                        .arg(v->filename));
            }
        }

        m_filesizeText->SetText(StringUtil::formatKBytes(v->size / 1024));
    }
}

void VideoSelector::OKPressed()
{
    // loop though selected videos and add them to the list
    // remove any items that have been removed from the list
    QList<ArchiveItem *> tempAList;
    for (auto *a : std::as_const(*m_archiveList))
    {
        bool found = false;

        for (const auto *v : std::as_const(m_selectedList))
        {
            if (a->type != "Video" || a->filename == v->filename)
            {
                found = true;
                break;
            }
        }

        if (!found)
            tempAList.append(a);
    }

    for (auto *x : std::as_const(tempAList))
        m_archiveList->removeAll(x);

    // remove any items that are already in the list
    QList<VideoInfo *> tempSList;
    for (auto *v : std::as_const(m_selectedList))
    {
        for (const auto *a : std::as_const(*m_archiveList))
        {
            if (a->filename == v->filename)
            {
                tempSList.append(v);
                break;
            }
        }
    }

    for (auto *x : std::as_const(tempSList))
        m_selectedList.removeAll(x);

    // add all that are left
    for (const auto *v : std::as_const(m_selectedList))
    {
        auto *a = new ArchiveItem;
        a->type = "Video";
        a->title = v->title;
        a->subtitle = "";
        a->description = v->plot;
        a->startDate = "";
        a->startTime = "";
        a->size = v->size;
        a->newsize = v->size;
        a->filename = v->filename;
        a->hasCutlist = false;
        a->useCutlist = false;
        a->duration = 0;
        a->cutDuration = 0;
        a->videoWidth = 0;
        a->videoHeight = 0;
        a->fileCodec = "";
        a->videoCodec = "";
        a->encoderProfile = nullptr;
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
        for (auto *v : *m_videoList)
        {
            if (v->category == m_categorySelector->GetValue() ||
                m_categorySelector->GetValue() == tr("All Videos"))
            {
                if (v->parentalLevel <= m_currentParentalLevel)
                {
                    auto* item = new MythUIButtonListItem(m_videoButtonList,
                                                          v->title);
                    item->setCheckable(true);
                    if (m_selectedList.indexOf(v) != -1)
                    {
                        item->setChecked(MythUIButtonListItem::FullChecked);
                    }
                    else
                    {
                        item->setChecked(MythUIButtonListItem::NotChecked);
                    }

                    item->SetData(QVariant::fromValue(v));
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

std::vector<VideoInfo *> *VideoSelector::getVideoListFromDB(void)
{
    // get a list of category's
    using CategoryMap = QMap<int, QString>;
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
                  "category, showlevel, subtitle, season, episode, host "
                  "FROM videometadata ORDER BY title,season,episode");

    if (query.exec() && query.size())
    {
        auto *videoList = new std::vector<VideoInfo*>;
        QString episode;
        while (query.next())
        {
            // Exclude iso images as they aren't supported
            QString filename = query.value(4).toString();
            if (filename.endsWith(".iso") || filename.endsWith(".ISO"))
                continue;

            auto *info = new VideoInfo;

            info->id = query.value(0).toInt();
            if (query.value(9).toInt() > 0)
            {
                episode = query.value(10).toString();
                if (episode.size() < 2)
                        episode.prepend("0");
                    info->title = QString("%1 %2x%3 - %4")
                                .arg(query.value(1).toString(),
                                     query.value(9).toString(),
                                     episode,
                                     query.value(8).toString());
            }
            else
            {
                info->title = query.value(1).toString();
            }

            info->plot = query.value(2).toString();
            info->size = 0; //query.value(3).toInt();
            QString host = query.value(11).toString();

            // try to find the file locally
            if (host.isEmpty())
            {
                // must already be a local filename?
                info->filename = filename;
            }
            else
            {
                // if the file is on this host then we should be able to find it locally
                if (host == gCoreContext->GetHostName())
                {
                    StorageGroup videoGroup("Videos", gCoreContext->GetHostName(), false);
                    info->filename = videoGroup.FindFile(filename);

                    // sanity check the file exists
                    if (!QFile::exists(info->filename))
                    {
                        LOG(VB_GENERAL, LOG_ERR,
                            QString("VideoSelector: Failed to find local file '%1'").arg(info->filename));
                        info->filename.clear();
                    }
                }

                if (info->filename.isEmpty())
                {
                    // file must not be local or doesn't exist
                    info->filename = generate_file_url("Videos", host, filename);
                }
            }

            LOG(VB_FILE, LOG_INFO,
                QString("VideoSelector: found file '%1'").arg(info->filename));

            info->coverfile = query.value(5).toString();
            info->category = categoryMap[query.value(6).toInt()];
            info->parentalLevel = query.value(7).toInt();
            if (info->category.isEmpty())
                info->category = "(None)";
            videoList->push_back(info);
        }

        return videoList;
    }

    LOG(VB_GENERAL, LOG_ERR, "VideoSelector: Failed to get any videos");

    return nullptr;
}

void VideoSelector::getVideoList(void)
{
    m_videoList = getVideoListFromDB();
    QStringList categories;

    if (m_videoList && !m_videoList->empty())
    {
        for (auto *v : *m_videoList)
        {
            if (categories.indexOf(v->category) == -1)
                categories.append(v->category);
        }
    }
    else
    {
        QTimer::singleShot(100ms, this, &VideoSelector::cancelPressed);
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

    setCategory(nullptr);
}

void VideoSelector::setCategory([[maybe_unused]] MythUIButtonListItem *item)
{
    updateVideoList();
}

void VideoSelector::updateSelectedList()
{
    if (!m_videoList)
        return;

    m_selectedList.clear();

    for (const auto *a : std::as_const(*m_archiveList))
    {
        for (auto *v : std::as_const(*m_videoList))
        {
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
    {
        ShowOkPopup(tr("You need to enter a valid password for this parental level"));
    }
}

