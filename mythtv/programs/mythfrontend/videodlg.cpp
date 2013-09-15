#include <set>
#include <map>

#include <QApplication>
#include <QTimer>
#include <QList>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QUrl>

#include "mythcontext.h"
#include "compat.h"
#include "mythdirs.h"

#include "mythuihelper.h"
#include "mythprogressdialog.h"
#include "mythuitext.h"
#include "mythuibutton.h"
#include "mythuibuttonlist.h"
#include "mythuibuttontree.h"
#include "mythuiimage.h"
#include "mythuistatetype.h"
#include "mythuimetadataresults.h"
#include "mythdialogbox.h"
#include "mythgenerictree.h"
#include "mythsystemlegacy.h"
#include "remotefile.h"
#include "remoteutil.h"
#include "storagegroup.h"

#include "videoscan.h"
#include "globals.h"
#include "videometadatalistmanager.h"
#include "parentalcontrols.h"
#include "videoutils.h"
#include "dbaccess.h"
#include "dirscan.h"
#include "metadatafactory.h"
#include "videofilter.h"
#include "editvideometadata.h"
#include "videopopups.h"
#include "videolist.h"
#include "videoplayercommand.h"
#include "videodlg.h"
#include "videofileassoc.h"
#include "videoplayersettings.h"
#include "videometadatasettings.h"
// for ImageDLFailureEvent
#include "metadataimagedownload.h"

static const QString _Location = "MythVideo";

namespace
{
    bool IsValidDialogType(int num)
    {
        for (int i = 1; i <= VideoDialog::dtLast - 1; i <<= 1)
            if (num == i) return true;
        return false;
    }

    class ParentalLevelNotifyContainer : public QObject
    {
        Q_OBJECT

      signals:
        void SigLevelChanged();

      public:
        ParentalLevelNotifyContainer(QObject *lparent = 0) :
            QObject(lparent), m_level(ParentalLevel::plNone)
        {
            connect(&m_levelCheck,
                    SIGNAL(SigResultReady(bool, ParentalLevel::Level)),
                    SLOT(OnResultReady(bool, ParentalLevel::Level)));
        }

        const ParentalLevel &GetLevel() const { return m_level; }

        void SetLevel(ParentalLevel level)
        {
            m_levelCheck.Check(m_level.GetLevel(), level.GetLevel());
        }

      private slots:
        void OnResultReady(bool passwordValid, ParentalLevel::Level newLevel)
        {
            ParentalLevel lastLevel = m_level;
            if (passwordValid)
            {
                m_level = newLevel;
            }

            if (m_level.GetLevel() == ParentalLevel::plNone)
            {
                m_level = ParentalLevel(ParentalLevel::plLowest);
            }

            if (lastLevel != m_level)
            {
                emit SigLevelChanged();
            }
        }

      private:
        ParentalLevel m_level;
        ParentalLevelChangeChecker m_levelCheck;
    };

    MythGenericTree *GetNodePtrFromButton(MythUIButtonListItem *item)
    {
        if (item)
            return item->GetData().value<MythGenericTree *>();

        return NULL;
    }

    VideoMetadata *GetMetadataPtrFromNode(MythGenericTree *node)
    {
        if (node)
            return node->GetData().value<TreeNodeData>().GetMetadata();

        return NULL;
    }

    bool GetLocalVideoImage(const QString &video_uid, const QString &filename,
                             const QStringList &in_dirs, QString &image,
                             QString title, int season,
                             const QString host, QString sgroup,
                             int episode = 0, bool isScreenshot = false)
    {
        QStringList search_dirs(in_dirs);
        QFileInfo qfi(filename);
        search_dirs += qfi.absolutePath();
        if (title.contains("/"))
            title.replace("/", "-");

        const QString base_name = qfi.completeBaseName();
        QList<QByteArray> image_types = QImageReader::supportedImageFormats();

        typedef std::set<QString> image_type_list;
        image_type_list image_exts;

        QString suffix;

        if (sgroup == "Coverart")
            suffix = "coverart";
        if (sgroup == "Fanart")
            suffix = "fanart";
        if (sgroup == "Screenshots")
            suffix = "screenshot";
        if (sgroup == "Banners")
            suffix = "banner";

        for (QList<QByteArray>::const_iterator it = image_types.begin();
                it != image_types.end(); ++it)
        {
            image_exts.insert(QString(*it).toLower());
        }

        if (!host.isEmpty())
        {
            QStringList hostFiles;

            RemoteGetFileList(host, "", &hostFiles, sgroup, true);
            const QString hntm("%2.%3");

            for (image_type_list::const_iterator ext = image_exts.begin();
                    ext != image_exts.end(); ++ext)
            {
                QStringList sfn;
                if (episode > 0 || season > 0)
                {
                    if (isScreenshot)
                        sfn += hntm.arg(QString("%1 Season %2x%3_%4")
                                 .arg(title).arg(QString::number(season))
                                 .arg(QString::number(episode))
                                 .arg(suffix))
                                 .arg(*ext);
                    else
                        sfn += hntm.arg(QString("%1 Season %2_%3")
                                 .arg(title).arg(QString::number(season))
                                 .arg(suffix))
                                 .arg(*ext);

                }
                else
                {
                sfn += hntm.arg(base_name + "_%1").arg(suffix).arg(*ext);
                sfn += hntm.arg(video_uid + "_%1").arg(suffix).arg(*ext);
                }

                for (QStringList::const_iterator i = sfn.begin();
                        i != sfn.end(); ++i)
                {
                    if (hostFiles.contains(*i))
                    {
                        image = *i;
                        return true;
                    }
                }
            }
        }

        const QString fntm("%1/%2.%3");

        for (QStringList::const_iterator dir = search_dirs.begin();
                dir != search_dirs.end(); ++dir)
        {
            if (!(*dir).length()) continue;

            for (image_type_list::const_iterator ext = image_exts.begin();
                    ext != image_exts.end(); ++ext)
            {
                QStringList sfn;
                if (season > 0 || episode > 0)
                {
                    if (isScreenshot)
                        sfn += fntm.arg(*dir).arg(QString("%1 Season %2x%3_%4")
                                 .arg(title).arg(QString::number(season))
                                 .arg(QString::number(episode))
                                 .arg(suffix))
                                 .arg(*ext);
                    else if (!isScreenshot)
                        sfn += fntm.arg(*dir).arg(QString("%1 Season %2_%3")
                                 .arg(title).arg(QString::number(season))
                                 .arg(suffix))
                                 .arg(*ext);

                }
                if (!isScreenshot)
                {
                sfn += fntm.arg(*dir).arg(QString(base_name + "_%1")
                    .arg(suffix)).arg(*ext);
                sfn += fntm.arg(*dir).arg(QString(video_uid + "_%1")
                    .arg(suffix)).arg(*ext);
                }

                for (QStringList::const_iterator i = sfn.begin();
                        i != sfn.end(); ++i)
                {
                    if (QFile::exists(*i))
                    {
                        image = *i;
                        return true;
                    }
                }
            }
        }

        return false;
    }

    void PlayVideo(const QString &filename,
            const VideoMetadataListManager &video_list, bool useAltPlayer = false)
    {
        const int WATCHED_WATERMARK = 10000; // Less than this and the chain of
                                             // videos will not be followed when
                                             // playing.

        VideoMetadataListManager::VideoMetadataPtr item = video_list.byFilename(filename);

        if (!item) return;

        QTime playing_time;

        do
        {
            playing_time.start();

            if (useAltPlayer)
                VideoPlayerCommand::AltPlayerFor(item.get()).Play();
            else
                VideoPlayerCommand::PlayerFor(item.get()).Play();

            if (item->GetChildID() > 0 && video_list.byID(item->GetChildID()))
                    item = video_list.byID(item->GetChildID());
            else
                break;
        }
        while (item && playing_time.elapsed() > WATCHED_WATERMARK);
    }

    class FanartLoader: public QObject
    {
        Q_OBJECT

      public:
        FanartLoader() : itemsPast(0), m_fanart(NULL), m_bConnected( false )
        {
            // NOTE: Moved call to connect to first call of LoadImage/
            //       Having it here causes a runtime error on windows
        }

       ~FanartLoader()
        {
            m_fanartTimer.stop();
            m_fanartTimer.disconnect(this);
        }

        void LoadImage(const QString &filename, MythUIImage *image)
        {
            if (!m_bConnected)
            {
                connect(&m_fanartTimer, SIGNAL(timeout()), SLOT(fanartLoad()));
                m_bConnected = true;
            }

            bool wasActive = m_fanartTimer.isActive();
            if (filename.isEmpty())
            {
                if (wasActive)
                    m_fanartTimer.stop();

                image->Reset();
                itemsPast++;
            }
            else
            {
                QMutexLocker locker(&m_fanartLock);
                m_fanart = image;
                if (filename != m_fanart->GetFilename())
                {
                    if (wasActive)
                        m_fanartTimer.stop();

                    if (itemsPast > 2)
                        m_fanart->Reset();

                    m_fanart->SetFilename(filename);
                    m_fanartTimer.setSingleShot(true);
                    m_fanartTimer.start(300);

                    if (wasActive)
                        itemsPast++;
                    else
                        itemsPast = 0;
                }
                else
                    itemsPast = 0;
            }
        }

      protected slots:
        void fanartLoad(void)
        {
            QMutexLocker locker(&m_fanartLock);
            m_fanart->Load();
        }

      private:
        int             itemsPast;
        QMutex          m_fanartLock;
        MythUIImage    *m_fanart;
        QTimer          m_fanartTimer;
        bool            m_bConnected;
    };

    FanartLoader fanartLoader;

    struct CopyMetadataDestination
    {
        virtual void handleText(const QString &name, const QString &value) = 0;
        virtual void handleState(const QString &name, const QString &value) = 0;
        virtual void handleImage(const QString &name,
                const QString &filename) = 0;
    };

    class ScreenCopyDest : public CopyMetadataDestination
    {
      public:
        ScreenCopyDest(MythScreenType *screen) : m_screen(screen) {}

        void handleText(const QString &name, const QString &value)
        {
            CheckedSet(m_screen, name, value);
        }

        void handleState(const QString &name, const QString &value)
        {
            handleText(name, value);
        }

        void handleImage(const QString &name, const QString &filename)
        {
            MythUIImage *image = NULL;
            UIUtilW::Assign(m_screen, image, name);
            if (image)
            {
                if (name != "fanart")
                {
                    if (!filename.isEmpty())
                    {
                        image->SetFilename(filename);
                        image->Load();
                    }
                    else
                        image->Reset();
                }
                else
                {
                    fanartLoader.LoadImage(filename, image);
                }
            }
        }

      private:
        MythScreenType *m_screen;
    };

    class MythUIButtonListItemCopyDest : public CopyMetadataDestination
    {
      public:
        MythUIButtonListItemCopyDest(MythUIButtonListItem *item) :
            m_item(item) {}

        void handleText(const QString &name, const QString &value)
        {
            m_item->SetText(value, name);
        }

        void handleState(const QString &name, const QString &value)
        {
            m_item->DisplayState(value, name);
        }

        void handleImage(const QString &name, const QString &filename)
        {
            (void) name;
            (void) filename;
        }

      private:
        MythUIButtonListItem *m_item;
    };

    void CopyMetadataToUI(const VideoMetadata *metadata,
            CopyMetadataDestination &dest)
    {
        typedef std::map<QString, QString> valuelist;
        valuelist tmp;

        if (metadata)
        {
            QString coverfile;
            if ((metadata->IsHostSet()
                && !metadata->GetCoverFile().startsWith("/"))
                && !metadata->GetCoverFile().isEmpty()
                && !IsDefaultCoverFile(metadata->GetCoverFile()))
            {
                coverfile = generate_file_url("Coverart", metadata->GetHost(),
                        metadata->GetCoverFile());
            }
            else
            {
                coverfile = metadata->GetCoverFile();
            }

            if (!IsDefaultCoverFile(coverfile))
                tmp["coverart"] = coverfile;

            tmp["coverfile"] = coverfile;

            QString screenshotfile;
            if (metadata->IsHostSet() && !metadata->GetScreenshot().startsWith("/")
                && !metadata->GetScreenshot().isEmpty())
            {
                screenshotfile = generate_file_url("Screenshots",
                        metadata->GetHost(), metadata->GetScreenshot());
            }
            else
            {
                screenshotfile = metadata->GetScreenshot();
            }

            if (!IsDefaultScreenshot(screenshotfile))
                tmp["screenshot"] = screenshotfile;

            tmp["screenshotfile"] = screenshotfile;

            QString bannerfile;
            if (metadata->IsHostSet() && !metadata->GetBanner().startsWith("/")
                && !metadata->GetBanner().isEmpty())
            {
                bannerfile = generate_file_url("Banners", metadata->GetHost(),
                        metadata->GetBanner());
            }
            else
            {
                bannerfile = metadata->GetBanner();
            }

            if (!IsDefaultBanner(bannerfile))
                tmp["banner"] = bannerfile;

            tmp["bannerfile"] = bannerfile;

            QString fanartfile;
            if (metadata->IsHostSet() && !metadata->GetFanart().startsWith("/")
                && !metadata->GetFanart().isEmpty())
            {
                fanartfile = generate_file_url("Fanart", metadata->GetHost(),
                        metadata->GetFanart());
            }
            else
            {
                fanartfile = metadata->GetFanart();
            }

            if (!IsDefaultFanart(fanartfile))
                tmp["fanart"] = fanartfile;

            tmp["fanartfile"] = fanartfile;

            tmp["trailerstate"] = TrailerToState(metadata->GetTrailer());
            tmp["studiostate"] = metadata->GetStudio();
            tmp["userratingstate"] =
                    QString::number((int)(metadata->GetUserRating()));
            tmp["watchedstate"] = WatchedToState(metadata->GetWatched());

            tmp["videolevel"] = ParentalLevelToState(metadata->GetShowLevel());
        }

        struct helper
        {
            helper(valuelist &values, CopyMetadataDestination &d) :
                m_vallist(values), m_dest(d) {}

            void handleImage(const QString &name)
            {
                m_dest.handleImage(name, m_vallist[name]);
            }

            void handleState(const QString &name)
            {
                m_dest.handleState(name, m_vallist[name]);
            }
          private:
            valuelist &m_vallist;
            CopyMetadataDestination &m_dest;
        };

        helper h(tmp, dest);

        h.handleImage("coverart");
        h.handleImage("screenshot");
        h.handleImage("banner");
        h.handleImage("fanart");

        h.handleState("trailerstate");
        h.handleState("userratingstate");
        h.handleState("watchedstate");
        h.handleState("videolevel");
    }
}

class ItemDetailPopup : public MythScreenType
{
    Q_OBJECT

  public:
    static bool Exists()
    {
        // TODO: Add ability to theme loader to do this a better way.
        return LoadWindowFromXML("video-ui.xml", WINDOW_NAME, NULL);
    }

  public:
    ItemDetailPopup(MythScreenStack *lparent, VideoMetadata *metadata,
            const VideoMetadataListManager &listManager) :
        MythScreenType(lparent, WINDOW_NAME), m_metadata(metadata),
        m_listManager(listManager), m_playButton(NULL), m_doneButton(NULL)
    {
    }

    bool Create()
    {
        if (!LoadWindowFromXML("video-ui.xml", WINDOW_NAME, this))
            return false;

        UIUtilW::Assign(this, m_playButton, "play_button");
        UIUtilW::Assign(this, m_doneButton, "done_button");

        if (m_playButton)
            connect(m_playButton, SIGNAL(Clicked()), SLOT(OnPlay()));

        if (m_doneButton)
            connect(m_doneButton, SIGNAL(Clicked()), SLOT(OnDone()));

        BuildFocusList();

        if (m_playButton || m_doneButton)
            SetFocusWidget(m_playButton ? m_playButton : m_doneButton);

        InfoMap metadataMap;
        m_metadata->toMap(metadataMap);
        SetTextFromMap(metadataMap);

        ScreenCopyDest dest(this);
        CopyMetadataToUI(m_metadata, dest);

        return true;
    }

  private slots:
    void OnPlay()
    {
        PlayVideo(m_metadata->GetFilename(), m_listManager);
    }

    void OnDone()
    {
        // TODO: Close() can do horrible things, this will pop
        // our screen, delete us, and return here.
        Close();
    }

  private:
    bool OnKeyAction(const QStringList &actions)
    {
        bool handled = false;
        for (QStringList::const_iterator key = actions.begin();
                key != actions.end(); ++key)
        {
            handled = true;
            if (*key == "SELECT" || *key == "PLAYBACK")
                OnPlay();
            else
                handled = false;
        }

        return handled;
    }

  protected:
    bool keyPressEvent(QKeyEvent *levent)
    {
        if (!MythScreenType::keyPressEvent(levent))
        {
            QStringList actions;
            bool handled = GetMythMainWindow()->TranslateKeyPress("Video",
                           levent, actions);

            if (!handled && !OnKeyAction(actions))
            {
                handled = GetMythMainWindow()->TranslateKeyPress("TV Frontend",
                        levent, actions);
                OnKeyAction(actions);
            }
        }

        return true;
    }

  private:
    static const char * const WINDOW_NAME;
    VideoMetadata *m_metadata;
    const VideoMetadataListManager &m_listManager;

    MythUIButton *m_playButton;
    MythUIButton *m_doneButton;
};

const char * const ItemDetailPopup::WINDOW_NAME = "itemdetailpopup";

class VideoDialogPrivate
{
  private:
    typedef std::list<std::pair<QString, ParentalLevel::Level> >
            parental_level_map;

    struct rating_to_pl_less :
        public std::binary_function<parental_level_map::value_type,
                                    parental_level_map::value_type, bool>
    {
        bool operator()(const parental_level_map::value_type &lhs,
                    const parental_level_map::value_type &rhs) const
        {
            return lhs.first.length() < rhs.first.length();
        }
    };

    typedef VideoDialog::VideoListPtr VideoListPtr;

  public:
    VideoDialogPrivate(VideoListPtr videoList, VideoDialog::DialogType type,
                       VideoDialog::BrowseType browse) :
        m_switchingLayout(false), m_firstLoadPass(true),
        m_rememberPosition(false), m_videoList(videoList), m_rootNode(0),
        m_currentNode(0), m_treeLoaded(false), m_isFlatList(false),
        m_type(type), m_browse(browse), m_scanner(0)
    {
        if (gCoreContext->GetNumSetting("mythvideo.ParentalLevelFromRating", 0))
        {
            for (ParentalLevel sl(ParentalLevel::plLowest);
                sl.GetLevel() <= ParentalLevel::plHigh && sl.good(); ++sl)
            {
                QString ratingstring =
                        gCoreContext->GetSetting(QString("mythvideo.AutoR2PL%1")
                                .arg(sl.GetLevel()));
                QStringList ratings =
                        ratingstring.split(':', QString::SkipEmptyParts);

                for (QStringList::const_iterator p = ratings.begin();
                    p != ratings.end(); ++p)
                {
                    m_rating_to_pl.push_back(
                        parental_level_map::value_type(*p, sl.GetLevel()));
                }
            }
            m_rating_to_pl.sort(std::not2(rating_to_pl_less()));
        }

        m_rememberPosition =
                gCoreContext->GetNumSetting("mythvideo.VideoTreeRemember", 0);

        m_isFileBrowser = gCoreContext->GetNumSetting("VideoDialogNoDB", 0);
        m_groupType = gCoreContext->GetNumSetting("mythvideo.db_group_type", 0);

        m_altPlayerEnabled =
                    gCoreContext->GetNumSetting("mythvideo.EnableAlternatePlayer");

        m_autoMeta = gCoreContext->GetNumSetting("mythvideo.AutoMetaDataScan", 1);

        m_artDir = gCoreContext->GetSetting("VideoArtworkDir");
        m_sshotDir = gCoreContext->GetSetting("mythvideo.screenshotDir");
        m_fanDir = gCoreContext->GetSetting("mythvideo.fanartDir");
        m_banDir = gCoreContext->GetSetting("mythvideo.bannerDir");
    }

    ~VideoDialogPrivate()
    {
        delete m_scanner;

        if (m_rememberPosition && m_lastTreeNodePath.length())
        {
            gCoreContext->SaveSetting("mythvideo.VideoTreeLastActive",
                    m_lastTreeNodePath);
        }
    }

    void AutomaticParentalAdjustment(VideoMetadata *metadata)
    {
        if (metadata && !m_rating_to_pl.empty())
        {
            QString rating = metadata->GetRating();
            for (parental_level_map::const_iterator p = m_rating_to_pl.begin();
                    !rating.isEmpty() && p != m_rating_to_pl.end(); ++p)
            {
                if (rating.indexOf(p->first) != -1)
                {
                    metadata->SetShowLevel(p->second);
                    break;
                }
            }
        }
    }

    void DelayVideoListDestruction(VideoListPtr videoList)
    {
        m_savedPtr = new VideoListDeathDelay(videoList);
    }

  public:
    ParentalLevelNotifyContainer m_parentalLevel;
    bool m_switchingLayout;

    static VideoDialog::VideoListDeathDelayPtr m_savedPtr;

    bool m_firstLoadPass;

    bool m_rememberPosition;

    VideoListPtr m_videoList;

    MythGenericTree *m_rootNode;
    MythGenericTree *m_currentNode;

    bool m_treeLoaded;

    bool m_isFileBrowser;
    int  m_groupType;
    bool m_isFlatList;
    bool m_altPlayerEnabled;
    VideoDialog::DialogType m_type;
    VideoDialog::BrowseType m_browse;

    bool m_autoMeta;

    QString m_artDir;
    QString m_sshotDir;
    QString m_fanDir;
    QString m_banDir;
    VideoScanner *m_scanner;

    QString m_lastTreeNodePath;
    QMap<QString, int> m_notifications;

  private:
    parental_level_map m_rating_to_pl;
};

VideoDialog::VideoListDeathDelayPtr VideoDialogPrivate::m_savedPtr;

class VideoListDeathDelayPrivate
{
  public:
    VideoListDeathDelayPrivate(VideoDialog::VideoListPtr toSave) :
        m_savedList(toSave)
    {
    }

    VideoDialog::VideoListPtr GetSaved()
    {
        return m_savedList;
    }

  private:
    VideoDialog::VideoListPtr m_savedList;
};

VideoListDeathDelay::VideoListDeathDelay(VideoDialog::VideoListPtr toSave) :
    QObject(qApp)
{
    m_d = new VideoListDeathDelayPrivate(toSave);
    QTimer::singleShot(3000, this, SLOT(OnTimeUp()));
}

VideoListDeathDelay::~VideoListDeathDelay()
{
    delete m_d;
}

VideoDialog::VideoListPtr VideoListDeathDelay::GetSaved()
{
    return m_d->GetSaved();
}

void VideoListDeathDelay::OnTimeUp()
{
    deleteLater();
}

VideoDialog::VideoListDeathDelayPtr &VideoDialog::GetSavedVideoList()
{
    return VideoDialogPrivate::m_savedPtr;
}

VideoDialog::VideoDialog(MythScreenStack *lparent, QString lname,
        VideoListPtr video_list, DialogType type, BrowseType browse)
  : MythScreenType(lparent, lname),
    m_menuPopup(NULL),
    m_busyPopup(NULL),
    m_popupStack(GetMythMainWindow()->GetStack("popup stack")),
    m_mainStack(GetMythMainWindow()->GetMainStack()),
    m_videoButtonList(NULL),
    m_videoButtonTree(NULL),
    m_titleText(NULL),
    m_novideoText(NULL),
    m_positionText(NULL),
    m_crumbText(NULL),
    m_coverImage(NULL),
    m_screenshot(NULL),
    m_banner(NULL),
    m_fanart(NULL),
    m_trailerState(NULL),
    m_parentalLevelState(NULL),
    m_videoLevelState(NULL),
    m_userRatingState(NULL),
    m_watchedState(NULL),
    m_studioState(NULL),
    m_metadataFactory(new MetadataFactory(this)),
    m_d(new VideoDialogPrivate(video_list, type, browse))
{
    m_d->m_videoList->setCurrentVideoFilter(VideoFilterSettings(true,
                    lname));

    m_d->m_parentalLevel.SetLevel(ParentalLevel(gCoreContext->
                    GetNumSetting("VideoDefaultParentalLevel",
                            ParentalLevel::plLowest)));

    StorageGroup::ClearGroupToUseCache();
}

VideoDialog::~VideoDialog()
{
    if (!m_d->m_switchingLayout)
        m_d->DelayVideoListDestruction(m_d->m_videoList);

    SavePosition();

    delete m_d;
}

void VideoDialog::SavePosition(void)
{
    m_d->m_lastTreeNodePath = "";

    if (m_d->m_type == DLG_TREE)
    {
        MythGenericTree *node = m_videoButtonTree->GetCurrentNode();
        if (node)
            m_d->m_lastTreeNodePath = node->getRouteByString().join("\n");
    }
    else if (m_d->m_type == DLG_BROWSER || m_d->m_type == DLG_GALLERY)
    {
        MythUIButtonListItem *item = m_videoButtonList->GetItemCurrent();
        if (item)
        {
            MythGenericTree *node = GetNodePtrFromButton(item);
            if (node)
                 m_d->m_lastTreeNodePath = node->getRouteByString().join("\n");
        }
    }

    gCoreContext->SaveSetting("mythvideo.VideoTreeLastActive", m_d->m_lastTreeNodePath);
}

bool VideoDialog::Create()
{
    if (m_d->m_type == DLG_DEFAULT)
    {
        m_d->m_type = static_cast<DialogType>(
                gCoreContext->GetNumSetting("Default MythVideo View", DLG_GALLERY));
        m_d->m_browse = static_cast<BrowseType>(
                gCoreContext->GetNumSetting("mythvideo.db_group_type", BRS_FOLDER));
    }

    if (!IsValidDialogType(m_d->m_type))
    {
        m_d->m_type = DLG_GALLERY;
    }

    QString windowName = "videogallery";
    int flatlistDefault = 0;

    switch (m_d->m_type)
    {
        case DLG_BROWSER:
            windowName = "browser";
            flatlistDefault = 1;
            break;
        case DLG_GALLERY:
            windowName = "gallery";
            break;
        case DLG_TREE:
            windowName = "tree";
            break;
        case DLG_MANAGER:
            m_d->m_isFlatList =
                    gCoreContext->GetNumSetting("mythvideo.db_folder_view", 1);
            windowName = "manager";
            flatlistDefault = 1;
            break;
        case DLG_DEFAULT:
        default:
            break;
    }

    switch (m_d->m_browse)
    {
        case BRS_GENRE:
            m_d->m_groupType = BRS_GENRE;
            break;
        case BRS_CATEGORY:
            m_d->m_groupType = BRS_CATEGORY;
            break;
        case BRS_YEAR:
            m_d->m_groupType = BRS_YEAR;
            break;
        case BRS_DIRECTOR:
            m_d->m_groupType = BRS_DIRECTOR;
            break;
        case BRS_STUDIO:
            m_d->m_groupType = BRS_STUDIO;
            break;
        case BRS_CAST:
            m_d->m_groupType = BRS_CAST;
            break;
        case BRS_USERRATING:
            m_d->m_groupType = BRS_USERRATING;
            break;
        case BRS_INSERTDATE:
            m_d->m_groupType = BRS_INSERTDATE;
            break;
        case BRS_TVMOVIE:
            m_d->m_groupType = BRS_TVMOVIE;
            break;
        case BRS_FOLDER:
        default:
            m_d->m_groupType = BRS_FOLDER;
            break;
    }

    m_d->m_isFlatList =
            gCoreContext->GetNumSetting(QString("mythvideo.folder_view_%1")
                    .arg(m_d->m_type), flatlistDefault);

    if (!LoadWindowFromXML("video-ui.xml", windowName, this))
        return false;

    bool err = false;
    if (m_d->m_type == DLG_TREE)
        UIUtilE::Assign(this, m_videoButtonTree, "videos", &err);
    else
        UIUtilE::Assign(this, m_videoButtonList, "videos", &err);

    UIUtilW::Assign(this, m_titleText, "title");
    UIUtilW::Assign(this, m_novideoText, "novideos");
    UIUtilW::Assign(this, m_positionText, "position");
    UIUtilW::Assign(this, m_crumbText, "breadcrumbs");

    UIUtilW::Assign(this, m_coverImage, "coverart");
    UIUtilW::Assign(this, m_screenshot, "screenshot");
    UIUtilW::Assign(this, m_banner, "banner");
    UIUtilW::Assign(this, m_fanart, "fanart");

    UIUtilW::Assign(this, m_trailerState, "trailerstate");
    UIUtilW::Assign(this, m_parentalLevelState, "parentallevel");
    UIUtilW::Assign(this, m_watchedState, "watchedstate");
    UIUtilW::Assign(this, m_studioState, "studiostate");

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen '" + windowName + "'");
        return false;
    }

    CheckedSet(m_trailerState, "None");
    CheckedSet(m_parentalLevelState, "None");
    CheckedSet(m_watchedState, "None");
    CheckedSet(m_studioState, "None");

    BuildFocusList();

    if (m_d->m_type == DLG_TREE)
    {
        SetFocusWidget(m_videoButtonTree);

        connect(m_videoButtonTree, SIGNAL(itemClicked(MythUIButtonListItem *)),
                SLOT(handleSelect(MythUIButtonListItem *)));
        connect(m_videoButtonTree, SIGNAL(itemSelected(MythUIButtonListItem *)),
                SLOT(UpdateText(MythUIButtonListItem *)));
        connect(m_videoButtonTree, SIGNAL(nodeChanged(MythGenericTree *)),
                SLOT(SetCurrentNode(MythGenericTree *)));
    }
    else
    {
        SetFocusWidget(m_videoButtonList);

        connect(m_videoButtonList, SIGNAL(itemClicked(MythUIButtonListItem *)),
                SLOT(handleSelect(MythUIButtonListItem *)));
        connect(m_videoButtonList, SIGNAL(itemSelected(MythUIButtonListItem *)),
                SLOT(UpdateText(MythUIButtonListItem *)));
    }

    return true;
}

void VideoDialog::Init()
{
    connect(&m_d->m_parentalLevel, SIGNAL(SigLevelChanged()),
            SLOT(reloadData()));
}

void VideoDialog::Load()
{
    reloadData();
    // We only want to prompt once, on startup, hence this is done in Load()
    if (m_d->m_rootNode->childCount() == 1 &&
        m_d->m_rootNode->getChildAt(0)->getInt() == kNoFilesFound)
        PromptToScan();
}

/** \fn VideoDialog::refreshData()
 *  \brief Reloads the tree without invalidating the data.
 *  \return void.
 */
void VideoDialog::refreshData()
{
    fetchVideos();
    loadData();

    CheckedSet(m_parentalLevelState,
            ParentalLevelToState(m_d->m_parentalLevel.GetLevel()));

    bool noFiles = (m_d->m_rootNode->childCount() == 1 &&
                    m_d->m_rootNode->getChildAt(0)->getInt() == kNoFilesFound);

    if (m_novideoText)
        m_novideoText->SetVisible(noFiles);
}

void VideoDialog::scanFinished(bool dbChanged)
{
    delete m_d->m_scanner;
    m_d->m_scanner = 0;

    if (dbChanged)
        m_d->m_videoList->InvalidateCache();

    m_d->m_currentNode = NULL;
    reloadData();

    if (m_d->m_autoMeta)
        VideoAutoSearch();

    if (m_d->m_rootNode->childCount() == 1 &&
        m_d->m_rootNode->getChildAt(0)->getInt() == kNoFilesFound)
    {
        QString message = tr("The video scan found no files, have you "
                             "configured a video storage group?");
        ShowOkPopup(message);
    }
}

/** \fn VideoDialog::reloadData()
 *  \brief Reloads the tree after having invalidated the data.
 *  \return void.
 */
void VideoDialog::reloadData()
{
    m_d->m_treeLoaded = false;
    refreshData();
}

/** \fn VideoDialog::loadData()
 *  \brief load the data used to build the ButtonTree/List for MythVideo.
 *  \return void.
 */
void VideoDialog::loadData()
{
    if (m_d->m_type == DLG_TREE)
    {
        m_videoButtonTree->AssignTree(m_d->m_rootNode);

        if (m_d->m_firstLoadPass)
        {
            m_d->m_firstLoadPass = false;

            if (m_d->m_rememberPosition)
            {
                QStringList route =
                        gCoreContext->GetSetting("mythvideo.VideoTreeLastActive",
                                "").split("\n");
                m_videoButtonTree->SetNodeByString(route);
            }
        }
    }
    else
    {
        m_videoButtonList->Reset();

        if (!m_d->m_treeLoaded)
            return;

        if (!m_d->m_currentNode)
            SetCurrentNode(m_d->m_rootNode);

        if (!m_d->m_currentNode)
            return;

        MythGenericTree *selectedNode = m_d->m_currentNode->getSelectedChild();

        // restore the last saved position in the video tree if this is the first
        // time this method is called and the option is set in the database
        if (m_d->m_firstLoadPass)
        {
            if (m_d->m_rememberPosition)
            {
                QStringList lastTreeNodePath = gCoreContext->GetSetting("mythvideo.VideoTreeLastActive", "").split("\n");

                if (m_d->m_type == DLG_GALLERY || m_d->m_type == DLG_BROWSER)
                {
                    if (!lastTreeNodePath.isEmpty())
                    {
                        MythGenericTree *node;

                        // go through the path list and set the current node
                        for (int i = 0; i < lastTreeNodePath.size(); i++)
                        {
                            node = m_d->m_currentNode->getChildByName(lastTreeNodePath.at(i));
                            if (node != NULL)
                            {
                                // check if the node name is the same as the currently selected
                                // one in the saved tree list. if yes then we are on the right
                                // way down the video tree to find the last saved position
                                if (node->GetText().compare(lastTreeNodePath.at(i)) == 0)
                                {
                                    // set the folder as the new node so we can travel further down
                                    // dont do this if its the last part of the saved video path tree
                                    if (node->getInt() == kSubFolder &&
                                        node->childCount() > 1 &&
                                        i < lastTreeNodePath.size()-1)
                                    {
                                        SetCurrentNode(node);
                                    }
                                    // in the last run the selectedNode will be the last
                                    // entry of the saved tree node.
                                    if (lastTreeNodePath.at(i) == lastTreeNodePath.last())
                                        selectedNode = node;
                                }
                            }
                        }
                        m_d->m_firstLoadPass = false;
                    }
                }
            }
        }

        typedef QList<MythGenericTree *> MGTreeChildList;
        MGTreeChildList *lchildren = m_d->m_currentNode->getAllChildren();

        for (MGTreeChildList::const_iterator p = lchildren->begin();
                p != lchildren->end(); ++p)
        {
            if (*p != NULL)
            {
                MythUIButtonListItem *item =
                        new MythUIButtonListItem(m_videoButtonList, QString(), 0,
                                true, MythUIButtonListItem::NotChecked);

                item->SetData(qVariantFromValue(*p));

                UpdateItem(item);

                if (*p == selectedNode)
                    m_videoButtonList->SetItemCurrent(item);
            }
        }
    }

    UpdatePosition();
}

/** \fn VideoDialog::UpdateItem(MythUIButtonListItem *item)
 *  \brief Update the visible representation of a MythUIButtonListItem.
 *  \return void.
 */
void VideoDialog::UpdateItem(MythUIButtonListItem *item)
{
    if (!item)
        return;

    MythGenericTree *node = GetNodePtrFromButton(item);

    VideoMetadata *metadata = GetMetadata(item);

    if (metadata)
    {
        InfoMap metadataMap;
        metadata->toMap(metadataMap);
        item->SetTextFromMap(metadataMap);
    }

    MythUIButtonListItemCopyDest dest(item);
    CopyMetadataToUI(metadata, dest);

    MythGenericTree *parent = node->getParent();

    if (parent && metadata && ((QString::compare(parent->GetText(),
                            metadata->GetTitle(), Qt::CaseInsensitive) == 0) ||
                            parent->GetText().startsWith(tr("Season"), Qt::CaseInsensitive)))
        item->SetText(metadata->GetSubtitle());
    else if (metadata && !metadata->GetSubtitle().isEmpty())
        item->SetText(QString("%1: %2").arg(metadata->GetTitle()).arg(metadata->GetSubtitle()));
    else
        item->SetText(metadata ? metadata->GetTitle() : node->GetText());

    QString coverimage = GetCoverImage(node);
    QString screenshot = GetScreenshot(node);
    QString banner     = GetBanner(node);
    QString fanart     = GetFanart(node);

    if (!screenshot.isEmpty() && parent && metadata &&
        ((QString::compare(parent->GetText(),
                            metadata->GetTitle(), Qt::CaseInsensitive) == 0) ||
            parent->GetText().startsWith(tr("Season"), Qt::CaseInsensitive)))
    {
        item->SetImage(screenshot);
    }
    else
    {
        if (coverimage.isEmpty())
            coverimage = GetFirstImage(node, "Coverart");
        item->SetImage(coverimage);
    }

    int nodeInt = node->getInt();

    if (coverimage.isEmpty() && nodeInt == kSubFolder)
        coverimage = GetFirstImage(node, "Coverart");

    item->SetImage(coverimage, "coverart");

    if (screenshot.isEmpty() && nodeInt == kSubFolder)
        screenshot = GetFirstImage(node, "Screenshots");

    item->SetImage(screenshot, "screenshot");

    if (banner.isEmpty() && nodeInt == kSubFolder)
        banner = GetFirstImage(node, "Banners");

    item->SetImage(banner, "banner");

    if (fanart.isEmpty() && nodeInt == kSubFolder)
        fanart = GetFirstImage(node, "Fanart");

    item->SetImage(fanart, "fanart");

    if (nodeInt == kSubFolder)
    {
        item->SetText(QString("%1").arg(node->visibleChildCount()), "childcount");
        item->DisplayState("subfolder", "nodetype");
        item->SetText(node->GetText(), "title");
        item->SetText(node->GetText());
    }
    else if (nodeInt == kUpFolder)
    {
        item->DisplayState("upfolder", "nodetype");
        item->SetText(node->GetText(), "title");
        item->SetText(node->GetText());
    }

    if (item == GetItemCurrent())
        UpdateText(item);
}

/** \fn VideoDialog::fetchVideos()
 *  \brief Build the buttonlist/tree.
 *  \return void.
 */
void VideoDialog::fetchVideos()
{
    MythGenericTree *oldroot = m_d->m_rootNode;
    if (!m_d->m_treeLoaded)
    {
        m_d->m_rootNode = m_d->m_videoList->buildVideoList(m_d->m_isFileBrowser,
                m_d->m_isFlatList, m_d->m_groupType,
                m_d->m_parentalLevel.GetLevel(), true);
    }
    else
    {
        m_d->m_videoList->refreshList(m_d->m_isFileBrowser,
                m_d->m_parentalLevel.GetLevel(),
                m_d->m_isFlatList, m_d->m_groupType);
        m_d->m_rootNode = m_d->m_videoList->GetTreeRoot();
    }

    m_d->m_treeLoaded = true;

    // Move a node down if there is a single directory item here...
    if (m_d->m_rootNode->childCount() == 1)
    {
        MythGenericTree *node = m_d->m_rootNode->getChildAt(0);
        if (node->getInt() == kSubFolder && node->childCount() > 1)
            m_d->m_rootNode = node;
        else if (node->getInt() == kUpFolder)
            m_d->m_treeLoaded = false;
    }
    else if (m_d->m_rootNode->childCount() == 0)
        m_d->m_treeLoaded = false;

    if (!m_d->m_currentNode || m_d->m_rootNode != oldroot)
        SetCurrentNode(m_d->m_rootNode);
}

/** \fn VideoDialog::RemoteImageCheck(QString host, QString filename)
 *  \brief Search for a given (image) filename in the Video SG.
 *  \return A QString of the full myth:// URL to a matching image.
 */
QString VideoDialog::RemoteImageCheck(QString host, QString filename)
{
    QString result = "";
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("RemoteImageCheck(%1)").arg(filename));
#endif

    QStringList dirs = GetVideoDirsByHost(host);

    if (!dirs.isEmpty())
    {
        for (QStringList::const_iterator iter = dirs.begin();
             iter != dirs.end(); ++iter)
        {
            QUrl sgurl = *iter;
            QString path = sgurl.path();

            QString fname = QString("%1/%2").arg(path).arg(filename);

            QStringList list( QString("QUERY_SG_FILEQUERY") );
            list << host;
            list << "Videos";
            list << fname;

            bool ok = gCoreContext->SendReceiveStringList(list);

            if (!ok || list.at(0).startsWith("SLAVE UNREACHABLE"))
            {
                LOG(VB_GENERAL, LOG_WARNING,
                    QString("Backend : %1 currently Unreachable. Skipping "
                            "this one.") .arg(host));
                break;
            }

            if ((!list.isEmpty()) && (list.at(0) == fname))
                result = generate_file_url("Videos", host, filename);

            if (!result.isEmpty())
            {
#if 0
                LOG(VB_GENERAL, LOG_DEBUG,
                    QString("RemoteImageCheck(%1) res :%2: :%3:")
                        .arg(fname).arg(result).arg(*iter));
#endif
                break;
            }

        }
    }

    return result;
}

/** \fn VideoDialog::GetImageFromFolder(VideoMetadata *metadata)
 *  \brief Attempt to find/fallback a cover image for a given metadata item.
 *  \return QString local or myth:// for the first found cover file.
 */
QString VideoDialog::GetImageFromFolder(VideoMetadata *metadata)
{
    QString icon_file;
    QString host = metadata->GetHost();
    QFileInfo fullpath(metadata->GetFilename());
    QDir dir = fullpath.dir();
    QString prefix = QDir::cleanPath(dir.path());

    QString filename = QString("%1/folder").arg(prefix);

    QStringList test_files;
    test_files.append(filename + ".png");
    test_files.append(filename + ".jpg");
    test_files.append(filename + ".gif");
    bool foundCover;

    for (QStringList::const_iterator tfp = test_files.begin();
            tfp != test_files.end(); ++tfp)
    {
        QString imagePath = *tfp;
        foundCover = false;
        if (!host.isEmpty())
        {
            // Strip out any extra /'s
            imagePath.replace("//", "/");
            prefix.replace("//","/");
            imagePath = imagePath.right(imagePath.length() - (prefix.length() + 1));
            QString tmpCover = RemoteImageCheck(host, imagePath);

            if (!tmpCover.isEmpty())
            {
                foundCover = true;
                imagePath = tmpCover;
            }
        }
        else
            foundCover = QFile::exists(imagePath);

        if (foundCover)
        {
            icon_file = imagePath;
            return icon_file;
        }
    }

    // If we found nothing, load something that matches the title.
    // If that fails, load anything we find.
    if (icon_file.isEmpty())
    {
        QStringList imageTypes;
        imageTypes.append(metadata->GetTitle() + ".png");
        imageTypes.append(metadata->GetTitle() + ".jpg");
        imageTypes.append(metadata->GetTitle() + ".gif");
        imageTypes.append("*.png");
        imageTypes.append("*.jpg");
        imageTypes.append("*.gif");

        QStringList fList;

        if (!host.isEmpty())
        {
            // TODO: This can likely get a little cleaner

            QStringList dirs = GetVideoDirsByHost(host);

            if (!dirs.isEmpty())
            {
                for (QStringList::const_iterator iter = dirs.begin();
                     iter != dirs.end(); ++iter)
                {
                    QUrl sgurl = *iter;
                    QString path = sgurl.path();

                    QString subdir = prefix;

                    path = path + "/" + subdir;
                    QStringList tmpList;
                    bool ok = RemoteGetFileList(host, path, &tmpList, "Videos");

                    if (ok)
                    {
                        for (QStringList::const_iterator pattern = imageTypes.begin();
                             pattern != imageTypes.end(); ++pattern)
                        {
                            QRegExp rx(*pattern);
                            rx.setPatternSyntax(QRegExp::Wildcard);
                            QStringList matches = tmpList.filter(rx);
                            if (!matches.isEmpty())
                            {
                                fList.clear();
                                fList.append(subdir + "/" + matches.at(0).split("::").at(1));
                                break;
                            }
                        }

                        break;
                    }
                }
            }
        }
        else
        {
            QDir vidDir(prefix);
            vidDir.setNameFilters(imageTypes);
            fList = vidDir.entryList();
        }

        if (!fList.isEmpty())
        {
            if (host.isEmpty())
                icon_file = QString("%1/%2")
                                .arg(prefix)
                                .arg(fList.at(0));
            else
                icon_file = generate_file_url("Videos", host, fList.at(0));
        }
    }

    if (!icon_file.isEmpty())
        LOG(VB_GENERAL, LOG_DEBUG, QString("Found Image : %1 :")
                .arg(icon_file));
    else
        LOG(VB_GENERAL, LOG_DEBUG, QString("Could not find cover Image : %1 ")
                .arg(prefix));

    if (IsDefaultCoverFile(icon_file))
        icon_file.clear();

    return icon_file;
}

/** \fn VideoDialog::GetCoverImage(MythGenericTree *node)
 *  \brief A "hunt" for cover art to apply for a folder item..
 *  \return QString local or myth:// for the first found cover file.
 */
QString VideoDialog::GetCoverImage(MythGenericTree *node)
{
    int nodeInt = node->getInt();

    QString icon_file;

    if (nodeInt  == kSubFolder)  // subdirectory
    {
        // load folder icon
        QString folder_path = node->GetData().value<TreeNodeData>().GetPath();
        QString host = node->GetData().value<TreeNodeData>().GetHost();
        QString prefix = node->GetData().value<TreeNodeData>().GetPrefix();

        if (folder_path.startsWith("myth://"))
            folder_path = folder_path.right(folder_path.length()
                                - folder_path.lastIndexOf("//") - 1);

        QString filename = QString("%1/folder").arg(folder_path);

#if 0
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("GetCoverImage host : %1  prefix : %2 file : %3")
                .arg(host).arg(prefix).arg(filename));
#endif

        QStringList test_files;
        test_files.append(filename + ".png");
        test_files.append(filename + ".jpg");
        test_files.append(filename + ".gif");
        bool foundCover;

        for (QStringList::const_iterator tfp = test_files.begin();
                tfp != test_files.end(); ++tfp)
        {
            QString imagePath = *tfp;
#if 0
            LOG(VB_GENERAL, LOG_DEBUG, QString("Cover check :%1 : ").arg(*tfp));
#endif

            foundCover = false;
            if (!host.isEmpty())
            {
                // Strip out any extra /'s
                imagePath.replace("//", "/");
                prefix.replace("//","/");
                imagePath = imagePath.right(imagePath.length() - (prefix.length() + 1));
                QString tmpCover = RemoteImageCheck(host, imagePath);

                if (!tmpCover.isEmpty())
                {
                    foundCover = true;
                    imagePath = tmpCover;
                }
            }
            else
                foundCover = QFile::exists(imagePath);

            if (foundCover)
            {
                icon_file = imagePath;
                break;
            }
        }

        // If we found nothing, load the first image we find
        if (icon_file.isEmpty())
        {
            QStringList imageTypes;
            imageTypes.append("*.png");
            imageTypes.append("*.jpg");
            imageTypes.append("*.gif");

            QStringList fList;

            if (!host.isEmpty())
            {
                // TODO: This can likely get a little cleaner

                QStringList dirs = GetVideoDirsByHost(host);

                if (!dirs.isEmpty())
                {
                    for (QStringList::const_iterator iter = dirs.begin();
                         iter != dirs.end(); ++iter)
                    {
                        QUrl sgurl = *iter;
                        QString path = sgurl.path();

                        QString subdir = folder_path.right(folder_path.length() - (prefix.length() + 2));

                        path = path + "/" + subdir;

                        QStringList tmpList;
                        bool ok = RemoteGetFileList(host, path, &tmpList, "Videos");

                        if (ok)
                        {
                            for (QStringList::const_iterator pattern = imageTypes.begin();
                                 pattern != imageTypes.end(); ++pattern)
                            {
                                QRegExp rx(*pattern);
                                rx.setPatternSyntax(QRegExp::Wildcard);
                                QStringList matches = tmpList.filter(rx);
                                if (!matches.isEmpty())
                                {
                                    fList.clear();
                                    fList.append(subdir + "/" + matches.at(0).split("::").at(1));
                                    break;
                                }
                            }

                            break;
                        }
                    }
                }

            }
            else
            {
                QDir vidDir(folder_path);
                vidDir.setNameFilters(imageTypes);
                fList = vidDir.entryList();
            }

            // Take the Coverfile for the first valid node in the dir, if it exists.
            if (icon_file.isEmpty())
            {
                int list_count = node->visibleChildCount();
                if (list_count > 0)
                {
                    for (int i = 0; i < list_count; i++)
                    {
                        MythGenericTree *subnode = node->getVisibleChildAt(i);
                        if (subnode)
                        {
                            VideoMetadata *metadata = GetMetadataPtrFromNode(subnode);
                            if (metadata)
                            {
                                if (!metadata->GetHost().isEmpty() &&
                                    !metadata->GetCoverFile().startsWith("/"))
                                {
                                    QString test_file = generate_file_url("Coverart",
                                                metadata->GetHost(), metadata->GetCoverFile());
                                    if (!test_file.endsWith("/") && !test_file.isEmpty() &&
                                        !IsDefaultCoverFile(test_file))
                                    {
                                        icon_file = test_file;
                                        break;
                                    }
                                }
                                else
                                {
                                    QString test_file = metadata->GetCoverFile();
                                    if (!test_file.isEmpty() &&
                                        !IsDefaultCoverFile(test_file))
                                    {
                                        icon_file = test_file;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (!fList.isEmpty())
            {
                if (host.isEmpty())
                    icon_file = QString("%1/%2")
                                    .arg(folder_path)
                                    .arg(fList.at(0));
                else
                    icon_file = generate_file_url("Videos", host, fList.at(0));
            }
        }

        if (!icon_file.isEmpty())
            LOG(VB_GENERAL, LOG_DEBUG, QString("Found Image : %1 :")
                    .arg(icon_file));
        else
            LOG(VB_GENERAL, LOG_DEBUG,
                QString("Could not find folder cover Image : %1 ")
                    .arg(folder_path));
    }
    else
    {
        const VideoMetadata *metadata = GetMetadataPtrFromNode(node);

        if (metadata)
        {
            if (metadata->IsHostSet() &&
                !metadata->GetCoverFile().startsWith("/") &&
                !IsDefaultCoverFile(metadata->GetCoverFile()))
            {
                icon_file = generate_file_url("Coverart", metadata->GetHost(),
                        metadata->GetCoverFile());
            }
            else
            {
                icon_file = metadata->GetCoverFile();
            }
        }
    }

    if (IsDefaultCoverFile(icon_file))
        icon_file.clear();

    return icon_file;
}

/**
 *  \brief Find the first image of "type" within a folder structure.
 *  \return QString local or myth:// for the image.
 *
 *  Will try immediate children (files) first, if no hits, will continue
 *  through subfolders recursively.  Will only return a value on a
 *  grandchild node if it matches the grandparent title, eg:
 *
 *  Lost->Season 1->Lost
 *
 */
QString VideoDialog::GetFirstImage(MythGenericTree *node, QString type,
                                   QString gpnode, int levels)
{
    if (!node || type.isEmpty())
        return QString();

    QString icon_file;

    int list_count = node->visibleChildCount();
    if (list_count > 0)
    {
        QList<MythGenericTree *> subDirs;
        int maxRecurse = 1;

        for (int i = 0; i < list_count; i++)
        {
            MythGenericTree *subnode = node->getVisibleChildAt(i);
            if (subnode)
            {
                if (subnode->childCount() > 0)
                    subDirs << subnode;

                VideoMetadata *metadata = GetMetadataPtrFromNode(subnode);
                if (metadata)
                {
                    QString test_file;
                    QString host = metadata->GetHost();
                    QString title = metadata->GetTitle();

                    if (type == "Coverart" && !host.isEmpty() &&
                        !metadata->GetCoverFile().startsWith("/"))
                    {
                        test_file = generate_file_url("Coverart",
                                    host, metadata->GetCoverFile());
                    }
                    else if (type == "Coverart")
                        test_file = metadata->GetCoverFile();

                    if (!test_file.endsWith("/") && !test_file.isEmpty() &&
                        !IsDefaultCoverFile(test_file) && (gpnode.isEmpty() ||
                        (QString::compare(gpnode, title, Qt::CaseInsensitive) == 0)))
                    {
                        icon_file = test_file;
                        break;
                    }

                    if (type == "Fanart" && !host.isEmpty() &&
                        !metadata->GetFanart().startsWith("/"))
                    {
                        test_file = generate_file_url("Fanart",
                                    host, metadata->GetFanart());
                    }
                    else if (type == "Fanart")
                        test_file = metadata->GetFanart();

                    if (!test_file.endsWith("/") && !test_file.isEmpty() &&
                        test_file != VIDEO_FANART_DEFAULT && (gpnode.isEmpty() ||
                        (QString::compare(gpnode, title, Qt::CaseInsensitive) == 0)))
                    {
                        icon_file = test_file;
                        break;
                    }

                    if (type == "Banners" && !host.isEmpty() &&
                        !metadata->GetBanner().startsWith("/"))
                    {
                        test_file = generate_file_url("Banners",
                                    host, metadata->GetBanner());
                    }
                    else if (type == "Banners")
                        test_file = metadata->GetBanner();

                    if (!test_file.endsWith("/") && !test_file.isEmpty() &&
                        test_file != VIDEO_BANNER_DEFAULT && (gpnode.isEmpty() ||
                        (QString::compare(gpnode, title, Qt::CaseInsensitive) == 0)))
                    {
                        icon_file = test_file;
                        break;
                    }

                    if (type == "Screenshots" && !host.isEmpty() &&
                        !metadata->GetScreenshot().startsWith("/"))
                    {
                        test_file = generate_file_url("Screenshots",
                                    host, metadata->GetScreenshot());
                    }
                    else if (type == "Screenshots")
                        test_file = metadata->GetScreenshot();

                    if (!test_file.endsWith("/") && !test_file.isEmpty() &&
                       test_file != VIDEO_SCREENSHOT_DEFAULT && (gpnode.isEmpty() ||
                       (QString::compare(gpnode, title, Qt::CaseInsensitive) == 0)))
                    {
                        icon_file = test_file;
                        break;
                    }
                }
            }
        }
        if (icon_file.isEmpty() && !subDirs.isEmpty())
        {
            QString test_file;
            int subDirCount = subDirs.count();
            for (int i = 0; i < subDirCount; i ++)
            {
                if (levels < maxRecurse)
                {
                    test_file = GetFirstImage(subDirs[i], type,
                                     node->GetText(), levels + 1);
                    if (!test_file.isEmpty())
                    {
                        icon_file = test_file;
                        break;
                    }
                }
            }
        }
    }
    return icon_file;
}

/** \fn VideoDialog::GetScreenshot(MythGenericTree *node)
 *  \brief Find the Screenshot for a given node.
 *  \return QString local or myth:// for the screenshot.
 */
QString VideoDialog::GetScreenshot(MythGenericTree *node)
{
    const int nodeInt = node->getInt();

    QString icon_file;

    if (nodeInt  == kSubFolder || nodeInt == kUpFolder)  // subdirectory
    {
        icon_file = VIDEO_SCREENSHOT_DEFAULT;
    }
    else
    {
        const VideoMetadata *metadata = GetMetadataPtrFromNode(node);

        if (metadata)
        {
            if (metadata->IsHostSet() &&
                    !metadata->GetScreenshot().startsWith("/") &&
                    !metadata->GetScreenshot().isEmpty())
            {
                icon_file = generate_file_url("Screenshots", metadata->GetHost(),
                        metadata->GetScreenshot());
            }
            else
            {
                icon_file = metadata->GetScreenshot();
            }
        }
    }

    if (IsDefaultScreenshot(icon_file))
        icon_file.clear();

    return icon_file;
}

/** \fn VideoDialog::GetBanner(MythGenericTree *node)
 *  \brief Find the Banner for a given node.
 *  \return QString local or myth:// for the banner.
 */
QString VideoDialog::GetBanner(MythGenericTree *node)
{
    const int nodeInt = node->getInt();

    if (nodeInt == kSubFolder || nodeInt == kUpFolder)
        return QString();

    QString icon_file;
    const VideoMetadata *metadata = GetMetadataPtrFromNode(node);

    if (metadata)
    {
        if (metadata->IsHostSet() &&
               !metadata->GetBanner().startsWith("/") &&
               !metadata->GetBanner().isEmpty())
        {
            icon_file = generate_file_url("Banners", metadata->GetHost(),
                    metadata->GetBanner());
        }
        else
        {
            icon_file = metadata->GetBanner();
        }

        if (IsDefaultBanner(icon_file))
            icon_file.clear();
    }

    return icon_file;
}

/** \fn VideoDialog::GetFanart(MythGenericTree *node)
 *  \brief Find the Fanart for a given node.
 *  \return QString local or myth:// for the fanart.
 */
QString VideoDialog::GetFanart(MythGenericTree *node)
{
    const int nodeInt = node->getInt();

    if (nodeInt  == kSubFolder || nodeInt == kUpFolder)  // subdirectory
        return QString();

    QString icon_file;
    const VideoMetadata *metadata = GetMetadataPtrFromNode(node);

    if (metadata)
    {
        if (metadata->IsHostSet() &&
                !metadata->GetFanart().startsWith("/") &&
                !metadata->GetFanart().isEmpty())
        {
            icon_file = generate_file_url("Fanart", metadata->GetHost(),
                    metadata->GetFanart());
        }
        else
        {
            icon_file = metadata->GetFanart();
        }

        if (IsDefaultFanart(icon_file))
            icon_file.clear();
    }

    return icon_file;
}

/** \fn VideoDialog::keyPressEvent(QKeyEvent *levent)
 *  \brief Handle keypresses and keybindings.
 *  \return true if the keypress was successfully handled.
 */
bool VideoDialog::keyPressEvent(QKeyEvent *levent)
{
    if (GetFocusWidget()->keyPressEvent(levent))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Video", levent, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "INFO")
        {
            MythUIButtonListItem *item = GetItemCurrent();
            MythGenericTree *node = GetNodePtrFromButton(item);
            if (!m_menuPopup && node->getInt() != kUpFolder)
                VideoMenu();
        }
        else if (action == "INCPARENT")
            shiftParental(1);
        else if (action == "DECPARENT")
            shiftParental(-1);
        else if (action == "1" || action == "2" ||
                 action == "3" || action == "4")
            setParentalLevel((ParentalLevel::Level)action.toInt());
        else if (action == "FILTER")
            ChangeFilter();
        else if (action == "MENU")
        {
            if (!m_menuPopup)
                DisplayMenu();
        }
        else if (action == "PLAYALT")
        {
            if (!m_menuPopup && GetMetadata(GetItemCurrent()) &&
                m_d->m_altPlayerEnabled)
                playVideoAlt();
        }
        else if (action == "DOWNLOADDATA")
        {
            if (!m_menuPopup && GetMetadata(GetItemCurrent()))
                VideoSearch();
        }
        else if (action == "INCSEARCH")
            searchStart();
        else if (action == "ITEMDETAIL")
            DoItemDetailShow();
        else if (action == "DELETE")
        {
            if (!m_menuPopup && GetMetadata(GetItemCurrent()))
                RemoveVideo();
        }
        else if (action == "EDIT" && !m_menuPopup)
            EditMetadata();
        else if (action == "ESCAPE")
        {
            if (m_d->m_type != DLG_TREE
                    && !GetMythMainWindow()->IsExitingToMain()
                    && m_d->m_currentNode != m_d->m_rootNode)
                handled = goBack();
            else
                handled = false;
        }
        else
            handled = false;
    }

    if (!handled)
    {
        handled = GetMythMainWindow()->TranslateKeyPress("TV Frontend", levent,
                                                         actions);

        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            if (action == "PLAYBACK")
            {
                handled = true;
                playVideo();
            }
        }
    }

    if (!handled && MythScreenType::keyPressEvent(levent))
        handled = true;

    return handled;
}

/** \fn VideoDialog::createBusyDialog(QString title)
 *  \brief Create a busy dialog, used during metadata search, etc.
 *  \return void.
 */
void VideoDialog::createBusyDialog(const QString &title)
{
    if (m_busyPopup)
        return;

    QString message = title;

    m_busyPopup = new MythUIBusyDialog(message, m_popupStack,
            "mythvideobusydialog");

    if (m_busyPopup->Create())
        m_popupStack->AddScreen(m_busyPopup);
}

/** \fn VideoDialog::createFetchDialog(VideoMetadata *metadata)
 *  \brief Create a fetch notification, used during metadata search.
 *  \return void.
 */
void VideoDialog::createFetchDialog(VideoMetadata *metadata)
{
    if (m_d->m_notifications.contains(metadata->GetHash()))
        return;

    int id = GetNotificationCenter()->Register(this);
    m_d->m_notifications[metadata->GetHash()] = id;

    QString msg = tr("Fetching details for %1")
                    .arg(metadata->GetTitle());
    QString desc;
    if (metadata->GetSeason() > 0 || metadata->GetEpisode() > 0)
    {
        desc = tr("Season %1, Episode %2")
                .arg(metadata->GetSeason()).arg(metadata->GetEpisode());
    }
    MythBusyNotification n(msg, _Location, desc);
    n.SetId(id);
    n.SetParent(this);
    GetNotificationCenter()->Queue(n);
}

void VideoDialog::dismissFetchDialog(VideoMetadata *metadata, bool ok)
{
    if (!metadata || !m_d->m_notifications.contains(metadata->GetHash()))
        return;

    int id = m_d->m_notifications[metadata->GetHash()];
    m_d->m_notifications.remove(metadata->GetHash());

    QString msg;
    if (ok)
    {
        msg = tr("Retrieved details for %1").arg(metadata->GetTitle());
    }
    else
    {
        msg = tr("Failed to retrieve details for %1").arg(metadata->GetTitle());
    }
    QString desc;
    if (metadata->GetSeason() > 0 || metadata->GetEpisode() > 0)
    {
        desc = tr("Season %1, Episode %2")
                .arg(metadata->GetSeason()).arg(metadata->GetEpisode());
    }
    if (ok)
    {
        MythCheckNotification n(msg, _Location, desc);
        n.SetId(id);
        n.SetParent(this);
        GetNotificationCenter()->Queue(n);
    }
    else
    {
        MythErrorNotification n(msg, _Location, desc);
        n.SetId(id);
        n.SetParent(this);
        GetNotificationCenter()->Queue(n);
    }
    GetNotificationCenter()->UnRegister(this, id);
}

/** \fn VideoDialog::createOkDialog(QString title)
 *  \brief Create a MythUI "OK" Dialog.
 *  \return void.
 */
void VideoDialog::createOkDialog(QString title)
{
    QString message = title;

    MythConfirmationDialog *okPopup =
            new MythConfirmationDialog(m_popupStack, message, false);

    if (okPopup->Create())
        m_popupStack->AddScreen(okPopup);
}

/**
 *  \brief After using incremental search, move to the selected item.
 *  \return void.
 */
void VideoDialog::searchComplete(QString string)
{
    LOG(VB_GENERAL, LOG_DEBUG, QString("Jumping to: %1").arg(string));

    MythGenericTree *parent = m_d->m_currentNode->getParent();
    QStringList childList;
    QList<MythGenericTree*>::iterator it;
    QList<MythGenericTree*> *children;
    QMap<int, QString> idTitle;

    if (parent && m_d->m_type == DLG_TREE)
        children = parent->getAllChildren();
    else
        children = m_d->m_currentNode->getAllChildren();

    for (it = children->begin(); it != children->end(); ++it)
    {
        MythGenericTree *child = *it;
        QString title = child->GetText();
        int id = child->getPosition();
        idTitle.insert(id, title);
    }

    if (m_d->m_type == DLG_TREE)
    {
        MythGenericTree *parent = m_videoButtonTree->GetCurrentNode()->getParent();
        MythGenericTree *new_node = parent->getChildAt(idTitle.key(string));
        if (new_node)
        {
            m_videoButtonTree->SetCurrentNode(new_node);
            m_videoButtonTree->SetActive(true);
        }
    }
    else
        m_videoButtonList->SetItemCurrent(idTitle.key(string));
}

/** \fn VideoDialog::searchStart(void)
 *  \brief Create an incremental search dialog for the current tree level.
 *  \return void.
 */
void VideoDialog::searchStart(void)
{
    MythGenericTree *parent = m_d->m_currentNode->getParent();

    QStringList childList;
    QList<MythGenericTree*>::iterator it;
    QList<MythGenericTree*> *children;
    if (parent && m_d->m_type == DLG_TREE)
        children = parent->getAllChildren();
    else
        children = m_d->m_currentNode->getAllChildren();

    for (it = children->begin(); it != children->end(); ++it)
    {
        MythGenericTree *child = *it;
        childList << child->GetText();
    }

    MythScreenStack *popupStack =
        GetMythMainWindow()->GetStack("popup stack");
    MythUISearchDialog *searchDialog = new MythUISearchDialog(popupStack,
        tr("Video Search"), childList, false, "");

    if (searchDialog->Create())
    {
        connect(searchDialog, SIGNAL(haveResult(QString)),
                SLOT(searchComplete(QString)));

        popupStack->AddScreen(searchDialog);
    }
    else
        delete searchDialog;
}

/** \fn VideoDialog::goBack()
 *  \brief Move one level up in the tree.
 *  \return true if successfully moved upwards.
 */
bool VideoDialog::goBack()
{
    bool handled = false;

    if (m_d->m_currentNode != m_d->m_rootNode)
    {
        MythGenericTree *lparent = m_d->m_currentNode->getParent();
        if (lparent)
        {
            SetCurrentNode(lparent);

            handled = true;
        }
    }

    loadData();

    return handled;
}

/** \fn VideoDialog::SetCurrentNode(MythGenericTree *node)
 *  \brief Switch to a given MythGenericTree node.
 *  \return void.
 */
void VideoDialog::SetCurrentNode(MythGenericTree *node)
{
    if (!node)
        return;

    m_d->m_currentNode = node;
}

/** \fn VideoDialog::UpdatePosition()
 *  \brief Update the "x of y" textarea to curent position.
 *  \return void.
 */
void VideoDialog::UpdatePosition()
{
    MythUIButtonListItem *ci = GetItemCurrent();
    MythUIButtonList *currentList = ci ? ci->parent() : 0;

    if (!currentList)
        return;

    CheckedSet(m_positionText, tr("%1 of %2")
                                    .arg(currentList->IsEmpty() ? 0 : currentList->GetCurrentPos() + 1)
                                    .arg(currentList->GetCount()));
}

/** \fn VideoDialog::UpdateText(MythUIButtonListItem *item)
 *  \brief Update the visible text values for a given ButtonListItem.
 *  \return void.
 */
void VideoDialog::UpdateText(MythUIButtonListItem *item)
{
    if (!item)
        return;

    MythUIButtonList *currentList = item->parent();

    if (!currentList)
        return;

    VideoMetadata *metadata = GetMetadata(item);

    MythGenericTree *node = GetNodePtrFromButton(item);

    if (!node)
        return;

    if (metadata)
    {
        InfoMap metadataMap;
        metadata->toMap(metadataMap);
        SetTextFromMap(metadataMap);
    }
    else
    {
        InfoMap metadataMap;
        ClearMap(metadataMap);
        SetTextFromMap(metadataMap);
    }

    ScreenCopyDest dest(this);
    CopyMetadataToUI(metadata, dest);

    if (node->getInt() == kSubFolder && !metadata)
    {
        QString cover = GetFirstImage(node, "Coverart");
        QString fanart = GetFirstImage(node, "Fanart");
        QString banner = GetFirstImage(node, "Banners");
        QString screenshot = GetFirstImage(node, "Screenshots");
        CheckedSet(m_coverImage, cover);
        CheckedSet(m_fanart, fanart);
        CheckedSet(m_banner, banner);
        CheckedSet(m_screenshot, screenshot);
    }

    if (!metadata)
        CheckedSet(m_titleText, item->GetText());
    UpdatePosition();

    if (m_d->m_currentNode)
    {
        CheckedSet(m_crumbText, m_d->m_currentNode->getRouteByString().join(" > "));
        CheckedSet(this, "foldername", m_d->m_currentNode->GetText());
    }

    if (node && node->getInt() == kSubFolder)
        CheckedSet(this, "childcount",
                   QString("%1").arg(node->visibleChildCount()));

    if (node)
        node->becomeSelectedChild();
}

/** \fn VideoDialog::VideoMenu()
 *  \brief Pop up a MythUI "Playback Menu" for MythVideo.  Bound to INFO.
 *  \return void.
 */
void VideoDialog::VideoMenu()
{
    VideoMetadata *metadata = GetMetadata(GetItemCurrent());
    QString label;

    if (metadata)
    {
        if (!metadata->GetSubtitle().isEmpty())
            label = tr("Video Options\n%1\n%2").arg(metadata->GetTitle())
                                           .arg(metadata->GetSubtitle());
        else
            label = tr("Video Options\n%1").arg(metadata->GetTitle());
    }
    else
        label = tr("Video Options");

    MythMenu *menu = new MythMenu(label, this, "actions");

    MythUIButtonListItem *item = GetItemCurrent();
    MythGenericTree *node = GetNodePtrFromButton(item);
    if (metadata)
    {
        if (!metadata->GetTrailer().isEmpty() ||
                gCoreContext->GetNumSetting("mythvideo.TrailersRandomEnabled", 0) ||
                m_d->m_altPlayerEnabled)
            menu->AddItem(tr("Play..."), NULL, CreatePlayMenu());
        else
            menu->AddItem(tr("Play"), SLOT(playVideo()));
        if (metadata->GetWatched())
            menu->AddItem(tr("Mark as Unwatched"), SLOT(ToggleWatched()));
        else
            menu->AddItem(tr("Mark as Watched"), SLOT(ToggleWatched()));
        menu->AddItem(tr("Video Info"), NULL, CreateInfoMenu());
        if (!m_d->m_notifications.contains(metadata->GetHash()))
        {
            menu->AddItem(tr("Change Video Details"), NULL, CreateManageMenu());
        }
        menu->AddItem(tr("Delete"), SLOT(RemoveVideo()));
    }
    else if (node && node->getInt() != kUpFolder)
    {
        menu->AddItem(tr("Play Folder"), SLOT(playFolder()));
    }


    m_menuPopup = new MythDialogBox(menu, m_popupStack, "videomenupopup");

    if (m_menuPopup->Create())
        m_popupStack->AddScreen(m_menuPopup);
    else
        delete m_menuPopup;
}

/** \fn VideoDialog::CreatePlayMenu()
 *  \brief Create a "Play Menu" for MythVideo.  Appears if multiple play
 *         options exist.
 *  \return MythMenu*.
 */
MythMenu* VideoDialog::CreatePlayMenu()
{
    VideoMetadata *metadata = GetMetadata(GetItemCurrent());
    QString label;

    if (metadata)
        label = tr("Playback Options\n%1").arg(metadata->GetTitle());
    else
        return NULL;

    MythMenu *menu = new MythMenu(label, this, "actions");

    menu->AddItem(tr("Play"), SLOT(playVideo()));

    if (m_d->m_altPlayerEnabled)
    {
        menu->AddItem(tr("Play in Alternate Player"), SLOT(playVideoAlt()));
    }

    if (gCoreContext->GetNumSetting("mythvideo.TrailersRandomEnabled", 0))
    {
         menu->AddItem(tr("Play With Trailers"), SLOT(playVideoWithTrailers()));
    }

    QString trailerFile = metadata->GetTrailer();
    if (QFile::exists(trailerFile) ||
        (!metadata->GetHost().isEmpty() && !trailerFile.isEmpty()))
    {
        menu->AddItem(tr("Play Trailer"), SLOT(playTrailer()));
    }

    return menu;
}

/** \fn VideoDialog::DisplayMenu()
 *  \brief Pop up a MythUI Menu for MythVideo Global Functions.  Bound to MENU.
 *  \return void.
 */
void VideoDialog::DisplayMenu()
{
    QString label = tr("Video Display Menu");

    MythMenu *menu = new MythMenu(label, this, "display");

    menu->AddItem(tr("Scan For Changes"), SLOT(doVideoScan()));
    menu->AddItem(tr("Retrieve All Details"), SLOT(VideoAutoSearch()));
    menu->AddItem(tr("Filter Display"), SLOT(ChangeFilter()));
    menu->AddItem(tr("Browse By..."), NULL, CreateMetadataBrowseMenu());
    menu->AddItem(tr("Change View"), NULL, CreateViewMenu());
    menu->AddItem(tr("Settings"), NULL, CreateSettingsMenu());

    m_menuPopup = new MythDialogBox(menu, m_popupStack, "videomenupopup");

    if (m_menuPopup->Create())
        m_popupStack->AddScreen(m_menuPopup);
    else
        delete m_menuPopup;
}

/** \fn VideoDialog::CreateViewMenu()
 *  \brief Create a  MythMenu for MythVideo Views.
 *  \return MythMenu.
 */
MythMenu* VideoDialog::CreateViewMenu()
{
    QString label = tr("Change View");

    MythMenu *menu = new MythMenu(label, this, "view");

    if (!(m_d->m_type & DLG_BROWSER))
        menu->AddItem(tr("Switch to Browse View"), SLOT(SwitchBrowse()));

    if (!(m_d->m_type & DLG_GALLERY))
        menu->AddItem(tr("Switch to Gallery View"), SLOT(SwitchGallery()));

    if (!(m_d->m_type & DLG_TREE))
        menu->AddItem(tr("Switch to List View"), SLOT(SwitchTree()));

    if (!(m_d->m_type & DLG_MANAGER))
        menu->AddItem(tr("Switch to Manage View"), SLOT(SwitchManager()));

    if (m_d->m_isFlatList)
        menu->AddItem(tr("Show Directory Structure"), SLOT(ToggleFlatView()));
    else
        menu->AddItem(tr("Hide Directory Structure"), SLOT(ToggleFlatView()));

    if (m_d->m_isFileBrowser)
        menu->AddItem(tr("Browse Library (recommended)"), SLOT(ToggleBrowseMode()));
    else
        menu->AddItem(tr("Browse Filesystem (slow)"), SLOT(ToggleBrowseMode()));


    return menu;
}

/** \fn VideoDialog::CreateSettingsMenu()
 *  \brief Create a MythMenu for MythVideo Settings.
 *  \return void.
 */
MythMenu* VideoDialog::CreateSettingsMenu()
{
    QString label = tr("Video Settings");

    MythMenu *menu = new MythMenu(label, this, "settings");

    menu->AddItem(tr("Player Settings"), SLOT(ShowPlayerSettings()));
    menu->AddItem(tr("Metadata Settings"), SLOT(ShowMetadataSettings()));
    menu->AddItem(tr("File Type Settings"), SLOT(ShowExtensionSettings()));

    return menu;
}

/** \fn VideoDialog::ShowPlayerSettings()
 *  \brief Pop up a MythUI Menu for MythVideo Player Settings.
 *  \return void.
 */
void VideoDialog::ShowPlayerSettings()
{
    PlayerSettings *ps = new PlayerSettings(m_mainStack, "player settings");

    if (ps->Create())
        m_mainStack->AddScreen(ps);
    else
        delete ps;
}

/** \fn VideoDialog::ShowMetadataSettings()
 *  \brief Pop up a MythUI Menu for MythVideo Metadata Settings.
 *  \return void.
 */
void VideoDialog::ShowMetadataSettings()
{
    MetadataSettings *ms = new MetadataSettings(m_mainStack, "metadata settings");

    if (ms->Create())
        m_mainStack->AddScreen(ms);
    else
        delete ms;
}

/** \fn VideoDialog::ShowExtensionSettings()
 *  \brief Pop up a MythUI Menu for MythVideo filte Type Settings.
 *  \return void.
 */
void VideoDialog::ShowExtensionSettings()
{
    FileAssocDialog *fa = new FileAssocDialog(m_mainStack, "fa dialog");

    if (fa->Create())
        m_mainStack->AddScreen(fa);
    else
        delete fa;
}

/** \fn VideoDialog::CreateMetadataBrowseMenu()
 *  \brief Create a MythMenu for MythVideo Metadata Browse modes.
 *  \return void.
 */
MythMenu* VideoDialog::CreateMetadataBrowseMenu()
{
    QString label = tr("Browse By");

    MythMenu *menu = new MythMenu(label, this, "metadata");

    if (m_d->m_groupType != BRS_CAST)
        menu->AddItem(tr("Cast"), SLOT(SwitchVideoCastGroup()));

    if (m_d->m_groupType != BRS_CATEGORY)
        menu->AddItem(tr("Category"), SLOT(SwitchVideoCategoryGroup()));

    if (m_d->m_groupType != BRS_INSERTDATE)
        menu->AddItem(tr("Date Added"), SLOT(SwitchVideoInsertDateGroup()));

    if (m_d->m_groupType != BRS_DIRECTOR)
        menu->AddItem(tr("Director"), SLOT(SwitchVideoDirectorGroup()));

    if (m_d->m_groupType != BRS_STUDIO)
        menu->AddItem(tr("Studio"), SLOT(SwitchVideoStudioGroup()));

    if (m_d->m_groupType != BRS_FOLDER)
        menu->AddItem(tr("Folder"), SLOT(SwitchVideoFolderGroup()));

    if (m_d->m_groupType != BRS_GENRE)
        menu->AddItem(tr("Genre"), SLOT(SwitchVideoGenreGroup()));

    if (m_d->m_groupType != BRS_TVMOVIE)
        menu->AddItem(tr("TV/Movies"),SLOT(SwitchVideoTVMovieGroup()));

    if (m_d->m_groupType != BRS_USERRATING)
        menu->AddItem(tr("User Rating"), SLOT(SwitchVideoUserRatingGroup()));

    if (m_d->m_groupType != BRS_YEAR)
        menu->AddItem(tr("Year"), SLOT(SwitchVideoYearGroup()));

    return menu;
}

/** \fn VideoDialog::CreateInfoMenu()
 *  \brief Create a MythMenu for Info pertaining to the selected item.
 *  \return MythMenu*.
 */
MythMenu *VideoDialog::CreateInfoMenu()
{
    QString label = tr("Video Info");

    MythMenu *menu = new MythMenu(label, this, "info");

    if (ItemDetailPopup::Exists())
        menu->AddItem(tr("View Details"), SLOT(DoItemDetailShow()));

    menu->AddItem(tr("View Full Plot"), SLOT(ViewPlot()));

    VideoMetadata *metadata = GetMetadata(GetItemCurrent());
    if (metadata)
    {
        if (!metadata->GetCast().empty())
            menu->AddItem(tr("View Cast"), SLOT(ShowCastDialog()));
        if (!metadata->GetHomepage().isEmpty())
            menu->AddItem(tr("View Homepage"), SLOT(ShowHomepage()));
    }

    return menu;
}

/** \fn VideoDialog::CreateManageMenu()
 *  \brief Create a MythMenu for metadata management.
 *  \return MythMenu*.
 */
MythMenu *VideoDialog::CreateManageMenu()
{
    QString label = tr("Manage Video Details");

    MythMenu *menu = new MythMenu(label, this, "manage");

    VideoMetadata *metadata = GetMetadata(GetItemCurrent());

    menu->AddItem(tr("Edit Details"), SLOT(EditMetadata()));
    menu->AddItem(tr("Retrieve Details"), SLOT(VideoSearch()));
    if (metadata->GetProcessed())
        menu->AddItem(tr("Allow Updates"), SLOT(ToggleProcess()));
    else
        menu->AddItem(tr("Disable Updates"), SLOT(ToggleProcess()));
    menu->AddItem(tr("Reset Details"), SLOT(ResetMetadata()));

    return menu;
}

void VideoDialog::ToggleProcess()
{
    VideoMetadata *metadata = GetMetadata(GetItemCurrent());
    if (metadata)
    {
        metadata->SetProcessed(!metadata->GetProcessed());
        metadata->UpdateDatabase();

        refreshData();
    }
}

/** \fn VideoDialog::ToggleBrowseMode()
 *  \brief Toggle the browseable status for the selected item.
 *  \return void.
 */
void VideoDialog::ToggleBrowseMode()
{
    m_d->m_isFileBrowser = !m_d->m_isFileBrowser;
    gCoreContext->SaveSetting("VideoDialogNoDB",
            QString("%1").arg((int)m_d->m_isFileBrowser));
    reloadData();
}

/** \fn VideoDialog::ToggleFlatView()
 *  \brief Toggle Flat View.
 *  \return void.
 */
void VideoDialog::ToggleFlatView()
{
    m_d->m_isFlatList = !m_d->m_isFlatList;
    gCoreContext->SaveSetting(QString("mythvideo.folder_view_%1").arg(m_d->m_type),
                         QString("%1").arg((int)m_d->m_isFlatList));
    // TODO: This forces a complete tree rebuild, this is SLOW and shouldn't
    // be necessary since MythGenericTree can do a flat view without a rebuild,
    // I just don't want to re-write VideoList just now
    reloadData();
}

/** \fn VideoDialog::handleDirSelect(MythGenericTree *node)
 *  \brief Descend into a selected folder.
 *  \return void.
 */
void VideoDialog::handleDirSelect(MythGenericTree *node)
{
    SetCurrentNode(node);
    loadData();
}

/** \fn VideoDialog::handleDynamicDirSelect(MythGenericTree *node)
 *  \brief Request the latest metadata for a folder.
 *  \return void.
 */
void VideoDialog::handleDynamicDirSelect(MythGenericTree *node)
{
    QStringList route = node->getRouteByString();
    if (m_d->m_videoList && m_d->m_videoList->refreshNode(node))
        reloadData();
    m_videoButtonTree->SetNodeByString(route);
}

/** \fn VideoDialog::handleSelect(MythUIButtonListItem *item)
 *  \brief Handle SELECT action for a given MythUIButtonListItem.
 *  \return void.
 */
void VideoDialog::handleSelect(MythUIButtonListItem *item)
{
    MythGenericTree *node = GetNodePtrFromButton(item);
    int nodeInt = node->getInt();

    switch (nodeInt)
    {
        case kDynamicSubFolder:
            handleDynamicDirSelect(node);
            break;
        case kSubFolder:
            handleDirSelect(node);
            break;
        case kUpFolder:
            goBack();
            break;
        default:
        {
            bool doPlay = true;
            if (m_d->m_type == DLG_GALLERY)
            {
                doPlay = !DoItemDetailShow();
            }

            if (doPlay)
                playVideo();
        }
    };
}

/** \fn VideoDialog::SwitchTree()
 *  \brief Switch to Tree (List) View.
 *  \return void.
 */
void VideoDialog::SwitchTree()
{
    SwitchLayout(DLG_TREE, m_d->m_browse);
}

/** \fn VideoDialog::SwitchGallery()
 *  \brief Switch to Gallery View.
 *  \return void.
 */
void VideoDialog::SwitchGallery()
{
    SwitchLayout(DLG_GALLERY, m_d->m_browse);
}

/** \fn VideoDialog::SwitchBrowse()
 *  \brief Switch to Browser View.
 *  \return void.
 */
void VideoDialog::SwitchBrowse()
{
    SwitchLayout(DLG_BROWSER, m_d->m_browse);
}

/** \fn VideoDialog::SwitchManager()
 *  \brief Switch to Video Manager View.
 *  \return void.
 */
void VideoDialog::SwitchManager()
{
    SwitchLayout(DLG_MANAGER, m_d->m_browse);
}

/** \fn VideoDialog::SwitchVideoFolderGroup()
 *  \brief Switch to Folder (filesystem) browse mode.
 *  \return void.
 */
void VideoDialog::SwitchVideoFolderGroup()
{
    SwitchLayout(m_d->m_type, BRS_FOLDER);
}

/** \fn VideoDialog::SwitchVideoGenreGroup()
 *  \brief Switch to Genre browse mode.
 *  \return void.
 */
void VideoDialog::SwitchVideoGenreGroup()
{
    SwitchLayout(m_d->m_type, BRS_GENRE);
}

/** \fn VideoDialog::SwitchVideoCategoryGroup()
 *  \brief Switch to Category browse mode.
 *  \return void.
 */
void VideoDialog::SwitchVideoCategoryGroup()
{
   SwitchLayout(m_d->m_type, BRS_CATEGORY);
}

/** \fn VideoDialog::SwitchVideoYearGroup()
 *  \brief Switch to Year browse mode.
 *  \return void.
 */
void VideoDialog::SwitchVideoYearGroup()
{
   SwitchLayout(m_d->m_type, BRS_YEAR);
}

/** \fn VideoDialog::SwitchVideoDirectoryGroup()
 *  \brief Switch to Director browse mode.
 *  \return void.
 */
void VideoDialog::SwitchVideoDirectorGroup()
{
   SwitchLayout(m_d->m_type, BRS_DIRECTOR);
}

/** \fn VideoDialog::SwitchVideoStudioGroup()
 *  \brief Switch to Studio browse mode.
 *  \return void.
 */
void VideoDialog::SwitchVideoStudioGroup()
{
   SwitchLayout(m_d->m_type, BRS_STUDIO);
}

/** \fn VideoDialog::SwitchVideoCastGroup()
 *  \brief Switch to Cast browse mode.
 *  \return void.
 */
void VideoDialog::SwitchVideoCastGroup()
{
   SwitchLayout(m_d->m_type, BRS_CAST);
}

/** \fn VideoDialog::SwitchVideoUserRatingGroup()
 *  \brief Switch to User Rating browse mode.
 *  \return void.
 */
void VideoDialog::SwitchVideoUserRatingGroup()
{
   SwitchLayout(m_d->m_type, BRS_USERRATING);
}

/** \fn VideoDialog::SwitchVideoInsertDateGroup()
 *  \brief Switch to Insert Date browse mode.
 *  \return void.
 */
void VideoDialog::SwitchVideoInsertDateGroup()
{
   SwitchLayout(m_d->m_type, BRS_INSERTDATE);
}

/** \fn VideoDialog::SwitchVideoTVMovieGroup()
 *  \brief Switch to Television/Movie browse mode.
 *  \return void.
 */
void VideoDialog::SwitchVideoTVMovieGroup()
{
   SwitchLayout(m_d->m_type, BRS_TVMOVIE);
}

/** \fn VideoDialog::SwitchLayout(DialogType type, BrowseType browse)
 *  \brief Handle a layout or browse mode switch.
 *  \return void.
 */
void VideoDialog::SwitchLayout(DialogType type, BrowseType browse)
{
    m_d->m_switchingLayout = true;

    // save current position so it can be restored after the switch
    SavePosition();

    VideoDialog *mythvideo =
            new VideoDialog(GetMythMainWindow()->GetMainStack(), "mythvideo",
                    m_d->m_videoList, type, browse);

    if (mythvideo->Create())
    {
        gCoreContext->SaveSetting("Default MythVideo View", type);
        gCoreContext->SaveSetting("mythvideo.db_group_type", browse);
        MythScreenStack *screenStack = GetScreenStack();
        screenStack->AddScreen(mythvideo);
        screenStack->PopScreen(this, false, false);
        deleteLater();
    }
    else
    {
        ShowOkPopup(tr("An error occurred when switching views."));
    }
}

/** \fn VideoDialog::ViewPlot()
 *  \brief Display a MythUI Popup with the selected item's plot.
 *  \return void.
 */
void VideoDialog::ViewPlot()
{
    VideoMetadata *metadata = GetMetadata(GetItemCurrent());

    PlotDialog *plotdialog = new PlotDialog(m_popupStack, metadata);

    if (plotdialog->Create())
        m_popupStack->AddScreen(plotdialog);
}

/** \fn VideoDialog::DoItemDetailShow()
 *  \brief Display the Item Detail Popup.
 *  \return true if popup was created
 */
bool VideoDialog::DoItemDetailShow()
{
    VideoMetadata *metadata = GetMetadata(GetItemCurrent());

    if (metadata)
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        ItemDetailPopup *idp = new ItemDetailPopup(mainStack, metadata,
                m_d->m_videoList->getListCache());

        if (idp->Create())
        {
            mainStack->AddScreen(idp);
            return true;
        }
    }

    return false;
}

/** \fn VideoDialog::ShowCastDialog()
 *  \brief Display the Cast if the selected item.
 *  \return void.
 */
void VideoDialog::ShowCastDialog()
{
    VideoMetadata *metadata = GetMetadata(GetItemCurrent());

    CastDialog *castdialog = new CastDialog(m_popupStack, metadata);

    if (castdialog->Create())
        m_popupStack->AddScreen(castdialog);
}

void VideoDialog::ShowHomepage()
{
    VideoMetadata *metadata = GetMetadata(GetItemCurrent());

    if (!metadata)
        return;

    QString url = metadata->GetHomepage();

    if (url.isEmpty())
        return;

    QString browser = gCoreContext->GetSetting("WebBrowserCommand", "");
    QString zoom = gCoreContext->GetSetting("WebBrowserZoomLevel", "1.0");

    if (browser.isEmpty())
    {
        ShowOkPopup(tr("No browser command set! MythVideo needs MythBrowser "
                       "installed to display the homepage."));
        return;
    }

    if (browser.toLower() == "internal")
    {
        GetMythMainWindow()->HandleMedia("WebBrowser", url);
        return;
    }
    else
    {
        QString cmd = browser;
        cmd.replace("%ZOOM%", zoom);
        cmd.replace("%URL%", url);
        cmd.replace('\'', "%27");
        cmd.replace("&","\\&");
        cmd.replace(";","\\;");

        GetMythMainWindow()->AllowInput(false);
        myth_system(cmd, kMSDontDisableDrawing);
        GetMythMainWindow()->AllowInput(true);
        return;
    }
}

/** \fn VideoDialog::playVideo()
 *  \brief Play the selected item.
 *  \return void.
 */
void VideoDialog::playVideo()
{
    VideoMetadata *metadata = GetMetadata(GetItemCurrent());
    if (metadata)
        PlayVideo(metadata->GetFilename(), m_d->m_videoList->getListCache());
}

/** \fn VideoDialog::playVideoAlt()
 *  \brief Play the selected item in an alternate player.
 *  \return void.
 */
void VideoDialog::playVideoAlt()
{
    VideoMetadata *metadata = GetMetadata(GetItemCurrent());
    if (metadata)
        PlayVideo(metadata->GetFilename(),
                  m_d->m_videoList->getListCache(), true);
}

/** \fn VideoDialog::playFolder()
 *  \brief Play all the items in the selected folder.
 *  \return void.
 */
void VideoDialog::playFolder()
{
    const int WATCHED_WATERMARK = 10000; // Play less then this milisec and the chain of
                                         // videos will not be followed when
                                         // playing.
    QTime playing_time;

    MythUIButtonListItem *item = GetItemCurrent();
    MythGenericTree *node = GetNodePtrFromButton(item);
    int list_count;

    if (node && !(node->getInt() >= 0))
        list_count = node->childCount();
    else
        return;

    if (list_count > 0)
    {
        bool video_started = false;
        int i = 0;
        while (i < list_count &&
               (!video_started || playing_time.elapsed() > WATCHED_WATERMARK))
        {
            MythGenericTree *subnode = node->getChildAt(i);
            if (subnode)
            {
                VideoMetadata *metadata = GetMetadataPtrFromNode(subnode);
                if (metadata)
                {
                    playing_time.start();
                    video_started = true;
                    PlayVideo(metadata->GetFilename(),
                              m_d->m_videoList->getListCache());
                }
            }
            i++;
        }
    }
}

namespace
{
    struct SimpleCollect : public DirectoryHandler
    {
        SimpleCollect(QStringList &fileList) : m_fileList(fileList) {}

        DirectoryHandler *newDir(const QString &dirName,
                const QString &fqDirName)
        {
            (void) dirName;
            (void) fqDirName;
            return this;
        }

        void handleFile(const QString &fileName, const QString &fqFileName,
                const QString &extension, const QString &host)
        {
            (void) fileName;
            (void) extension;
            (void) host;
            m_fileList.push_back(fqFileName);
        }

      private:
        QStringList &m_fileList;
    };

    QStringList GetTrailersInDirectory(const QString &startDir)
    {
        FileAssociations::ext_ignore_list extensions;
        FileAssociations::getFileAssociation()
                .getExtensionIgnoreList(extensions);
        QStringList ret;
        SimpleCollect sc(ret);

        (void) ScanVideoDirectory(startDir, &sc, extensions, false);
        return ret;
    }
}

/** \fn VideoDialog::playVideoWithTrailers()
 *  \brief Play the selected item w/ a User selectable # of trailers.
 *  \return void.
 */
void VideoDialog::playVideoWithTrailers()
{
    VideoMetadata *metadata = GetMetadata(GetItemCurrent());
    if (!metadata) return;

    QStringList trailers = GetTrailersInDirectory(gCoreContext->
            GetSetting("mythvideo.TrailersDir"));

    if (trailers.isEmpty())
        return;

    const int trailersToPlay =
            gCoreContext->GetNumSetting("mythvideo.TrailersRandomCount");

    int i = 0;
    while (!trailers.isEmpty() && i < trailersToPlay)
    {
        ++i;
        QString trailer = trailers.takeAt(random() % trailers.size());

        LOG(VB_GENERAL, LOG_DEBUG,
            QString("Random trailer to play will be: %1").arg(trailer));

        VideoPlayerCommand::PlayerFor(trailer).Play();
    }

    PlayVideo(metadata->GetFilename(), m_d->m_videoList->getListCache());
}

/** \fn VideoDialog::playTrailer()
 *  \brief Play the trailer associated with the selected item.
 *  \return void.
 */
void VideoDialog::playTrailer()
{
    VideoMetadata *metadata = GetMetadata(GetItemCurrent());
    if (!metadata) return;
    QString url;

    if (metadata->IsHostSet() && !metadata->GetTrailer().startsWith("/"))
        url = generate_file_url("Trailers", metadata->GetHost(),
                        metadata->GetTrailer());
    else
        url = metadata->GetTrailer();

    VideoPlayerCommand::PlayerFor(url).Play();
}

/** \fn VideoDialog::setParentalLevel(const ParentalLevel::Level &level)
 *  \brief Set the parental level for the library.
 *  \return void.
 */
void VideoDialog::setParentalLevel(const ParentalLevel::Level &level)
{
    m_d->m_parentalLevel.SetLevel(level);
}

/** \fn VideoDialog::shiftParental(int amount)
 *  \brief Shift the parental level for the library by an integer amount.
 *  \return void.
 */
void VideoDialog::shiftParental(int amount)
{
    setParentalLevel(ParentalLevel(m_d->m_parentalLevel.GetLevel()
                    .GetLevel() + amount).GetLevel());
}

/** \fn VideoDialog::ChangeFilter()
 *  \brief Change the filtering of the library.
 *  \return void.
 */
void VideoDialog::ChangeFilter()
{
    MythScreenStack *mainStack = GetScreenStack();

    VideoFilterDialog *filterdialog = new VideoFilterDialog(mainStack,
            "videodialogfilters", m_d->m_videoList.get());

    if (filterdialog->Create())
        mainStack->AddScreen(filterdialog);

    connect(filterdialog, SIGNAL(filterChanged()), SLOT(reloadData()));
}

/** \fn VideoDialog::GetMetadata(MythUIButtonListItem *item)
 *  \brief Retrieve the Database Metadata for a given MythUIButtonListItem.
 *  \return a Metadata object containing the item's videometadata record.
 */
VideoMetadata *VideoDialog::GetMetadata(MythUIButtonListItem *item)
{
    VideoMetadata *metadata = NULL;

    if (item)
    {
        MythGenericTree *node = GetNodePtrFromButton(item);
        if (node)
        {
            int nodeInt = node->getInt();

            if (nodeInt >= 0)
                metadata = GetMetadataPtrFromNode(node);
        }
    }

    return metadata;
}

void VideoDialog::customEvent(QEvent *levent)
{
    if (levent->type() == MetadataFactoryMultiResult::kEventType)
    {
        MetadataFactoryMultiResult *mfmr = dynamic_cast<MetadataFactoryMultiResult*>(levent);

        if (!mfmr)
            return;

        MetadataLookupList list = mfmr->results;

        if (list.count() > 1)
        {
            VideoMetadata *metadata =
                list[0]->GetData().value<VideoMetadata *>();
            dismissFetchDialog(metadata, true);
            MetadataResultsDialog *resultsdialog =
                  new MetadataResultsDialog(m_popupStack, list);

            connect(resultsdialog, SIGNAL(haveResult(RefCountHandler<MetadataLookup>)),
                    SLOT(OnVideoSearchListSelection(RefCountHandler<MetadataLookup>)),
                    Qt::QueuedConnection);

            if (resultsdialog->Create())
                m_popupStack->AddScreen(resultsdialog);
        }
    }
    else if (levent->type() == MetadataFactorySingleResult::kEventType)
    {
        MetadataFactorySingleResult *mfsr = dynamic_cast<MetadataFactorySingleResult*>(levent);

        if (!mfsr)
            return;

        MetadataLookup *lookup = mfsr->result;

        if (!lookup)
            return;

        OnVideoSearchDone(lookup);
    }
    else if (levent->type() == MetadataFactoryNoResult::kEventType)
    {
        MetadataFactoryNoResult *mfnr = dynamic_cast<MetadataFactoryNoResult*>(levent);

        if (!mfnr)
            return;

        MetadataLookup *lookup = mfnr->result;

        if (!lookup)
            return;

        VideoMetadata *metadata =
            lookup->GetData().value<VideoMetadata *>();
        if (metadata)
        {
            dismissFetchDialog(metadata, false);
            metadata->SetProcessed(true);
            metadata->UpdateDatabase();
        }
        LOG(VB_GENERAL, LOG_INFO,
            QString("No results found for %1 %2 %3").arg(lookup->GetTitle())
                .arg(lookup->GetSeason()).arg(lookup->GetEpisode()));
    }
    else if (levent->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = static_cast<DialogCompletionEvent *>(levent);
        QString id = dce->GetId();

        if (id == "scanprompt")
        {
            int result = dce->GetResult();
            if (result == 1)
                doVideoScan();
        }
        else
            m_menuPopup = NULL;
    }
    else if (levent->type() == ImageDLFailureEvent::kEventType)
    {
        MythErrorNotification n(tr("Failed to retrieve image(s)"),
                                _Location,
                                tr("Check logs"));
        GetNotificationCenter()->Queue(n);
    }
}

void VideoDialog::OnVideoImageSetDone(VideoMetadata *metadata)
{
    // The metadata has some cover file set
    dismissFetchDialog(metadata, true);

    metadata->SetProcessed(true);
    metadata->UpdateDatabase();

    MythUIButtonListItem *item = GetItemByMetadata(metadata);
    if (item != NULL)
        UpdateItem(item);
}

MythUIButtonListItem *VideoDialog::GetItemCurrent()
{
    if (m_videoButtonTree)
    {
        return m_videoButtonTree->GetItemCurrent();
    }

    return m_videoButtonList->GetItemCurrent();
}

MythUIButtonListItem *VideoDialog::GetItemByMetadata(VideoMetadata *metadata)
{
    if (m_videoButtonTree)
    {
        return m_videoButtonTree->GetItemCurrent();
    }

    QList<MythGenericTree*>::iterator it;
    QList<MythGenericTree*> *children;
    QMap<int, int> idPosition;

    children = m_d->m_currentNode->getAllChildren();

    for (it = children->begin(); it != children->end(); ++it)
    {
        MythGenericTree *child = *it;
        int nodeInt = child->getInt();
        if (nodeInt != kSubFolder && nodeInt != kUpFolder)
        {
            VideoMetadata *listmeta =
                        GetMetadataPtrFromNode(child);
            if (listmeta)
            {
                int position = child->getPosition();
                int id = listmeta->GetID();
                idPosition.insert(id, position);
            }
        }
    }

    return m_videoButtonList->GetItemAt(idPosition.value(metadata->GetID()));
}

void VideoDialog::VideoSearch(MythGenericTree *node,
                              bool automode)
{
    if (!node)
        node = GetNodePtrFromButton(GetItemCurrent());

    if (!node)
        return;

    VideoMetadata *metadata = GetMetadataPtrFromNode(node);

    if (!metadata)
        return;

    m_metadataFactory->Lookup(metadata, automode, true);

    if (!automode)
    {
        createFetchDialog(metadata);
    }
}

void VideoDialog::VideoAutoSearch(MythGenericTree *node)
{
    if (!node)
        node = m_d->m_rootNode;
    typedef QList<MythGenericTree *> MGTreeChildList;
    MGTreeChildList *lchildren = node->getAllChildren();

    LOG(VB_GENERAL, LOG_DEBUG,
        QString("Fetching details in %1").arg(node->GetText()));

    for (MGTreeChildList::const_iterator p = lchildren->begin();
            p != lchildren->end(); ++p)
    {
        if (((*p)->getInt() == kSubFolder) ||
            ((*p)->getInt() == kUpFolder))
            VideoAutoSearch((*p));
        else
        {
            VideoMetadata *metadata = GetMetadataPtrFromNode((*p));

            if (!metadata)
                continue;

            if (!metadata->GetProcessed())
                VideoSearch((*p), true);
        }
    }
}

void VideoDialog::ToggleWatched()
{
    MythUIButtonListItem *item = GetItemCurrent();
    if (!item)
        return;

    VideoMetadata *metadata = GetMetadata(item);
    if (metadata)
    {
        metadata->SetWatched(!metadata->GetWatched());
        metadata->UpdateDatabase();
        item->DisplayState(WatchedToState(metadata->GetWatched()),
                                       "watchedstate");
    }
}

void VideoDialog::OnVideoSearchListSelection(RefCountHandler<MetadataLookup> lookup)
{
    if (!lookup)
        return;

    lookup->SetStep(kLookupData);
    lookup->IncrRef();
    m_metadataFactory->Lookup(lookup);
}

void VideoDialog::OnParentalChange(int amount)
{
    VideoMetadata *metadata = GetMetadata(GetItemCurrent());
    if (metadata)
    {
        ParentalLevel curshowlevel = metadata->GetShowLevel();

        curshowlevel += amount;

        if (curshowlevel.GetLevel() != metadata->GetShowLevel())
        {
            metadata->SetShowLevel(curshowlevel.GetLevel());
            metadata->UpdateDatabase();
            refreshData();
        }
    }
}

void VideoDialog::EditMetadata()
{
    VideoMetadata *metadata = GetMetadata(GetItemCurrent());
    if (!metadata)
        return;

    MythScreenStack *screenStack = GetScreenStack();

    EditMetadataDialog *md_editor = new EditMetadataDialog(screenStack,
            "mythvideoeditmetadata", metadata,
            m_d->m_videoList->getListCache());

    connect(md_editor, SIGNAL(Finished()), SLOT(refreshData()));

    if (md_editor->Create())
        screenStack->AddScreen(md_editor);
}

void VideoDialog::RemoveVideo()
{
    VideoMetadata *metadata = GetMetadata(GetItemCurrent());

    if (!metadata)
        return;

    QString message = tr("Are you sure you want to permanently delete:\n%1")
                          .arg(metadata->GetTitle());

    MythConfirmationDialog *confirmdialog =
            new MythConfirmationDialog(m_popupStack,message);

    if (confirmdialog->Create())
        m_popupStack->AddScreen(confirmdialog);

    connect(confirmdialog, SIGNAL(haveResult(bool)),
            SLOT(OnRemoveVideo(bool)));
}

void VideoDialog::OnRemoveVideo(bool dodelete)
{
    if (!dodelete)
        return;

    MythUIButtonListItem *item = GetItemCurrent();
    MythGenericTree *gtItem = GetNodePtrFromButton(item);

    VideoMetadata *metadata = GetMetadata(item);

    if (!metadata)
        return;

    if (m_d->m_videoList->Delete(metadata->GetID()))
    {
        if (m_videoButtonTree)
            m_videoButtonTree->RemoveItem(item, false); // FIXME Segfault when true
        else
            m_videoButtonList->RemoveItem(item);

        MythGenericTree *parent = gtItem->getParent();
        parent->deleteNode(gtItem);
    }
    else
    {
        QString message = tr("Failed to delete file");

        MythConfirmationDialog *confirmdialog =
                        new MythConfirmationDialog(m_popupStack,message,false);

        if (confirmdialog->Create())
            m_popupStack->AddScreen(confirmdialog);
    }
}

void VideoDialog::ResetMetadata()
{
    MythUIButtonListItem *item = GetItemCurrent();
    VideoMetadata *metadata = GetMetadata(item);

    if (metadata)
    {
        metadata->Reset();
        metadata->UpdateDatabase();
        UpdateItem(item);
    }
}

void VideoDialog::StartVideoImageSet(VideoMetadata *metadata)
{
    if (!metadata)
        return;

    QStringList cover_dirs;
    cover_dirs += m_d->m_artDir;

    QString cover_file;
    QString inetref = metadata->GetInetRef();
    QString filename = metadata->GetFilename();
    QString title = metadata->GetTitle();
    int season = metadata->GetSeason();
    QString host = metadata->GetHost();
    int episode = metadata->GetEpisode();

    if (metadata->GetCoverFile().isEmpty() ||
        IsDefaultCoverFile(metadata->GetCoverFile()))
    {
        if (GetLocalVideoImage(inetref, filename,
                                cover_dirs, cover_file, title,
                                season, host, "Coverart", episode))
        {
            metadata->SetCoverFile(cover_file);
            OnVideoImageSetDone(metadata);
        }
    }

    QStringList fanart_dirs;
    fanart_dirs += m_d->m_fanDir;

    QString fanart_file;

    if (metadata->GetFanart().isEmpty())
    {
        if (GetLocalVideoImage(inetref, filename,
                                fanart_dirs, fanart_file, title,
                                season, host, "Fanart", episode))
        {
            metadata->SetFanart(fanart_file);
            OnVideoImageSetDone(metadata);
        }
    }

    QStringList banner_dirs;
    banner_dirs += m_d->m_banDir;

    QString banner_file;

    if (metadata->GetBanner().isEmpty())
    {
        if (GetLocalVideoImage(inetref, filename,
                                banner_dirs, banner_file, title,
                                season, host, "Banners", episode))
        {
            metadata->SetBanner(banner_file);
            OnVideoImageSetDone(metadata);
        }
    }

    QStringList screenshot_dirs;
    screenshot_dirs += m_d->m_sshotDir;

    QString screenshot_file;

    if (metadata->GetScreenshot().isEmpty())
    {
        if (GetLocalVideoImage(inetref, filename,
                                screenshot_dirs, screenshot_file, title,
                                season, host, "Screenshots", episode,
                                true))
        {
            metadata->SetScreenshot(screenshot_file);
            OnVideoImageSetDone(metadata);
        }
    }
}

void VideoDialog::OnVideoSearchDone(MetadataLookup *lookup)
{
    if (!lookup)
       return;

    VideoMetadata *metadata = lookup->GetData().value<VideoMetadata *>();

    if (!metadata)
        return;

    dismissFetchDialog(metadata, true);
    metadata->SetTitle(lookup->GetTitle());
    metadata->SetSubtitle(lookup->GetSubtitle());

    if (metadata->GetTagline().isEmpty())
        metadata->SetTagline(lookup->GetTagline());
    if (metadata->GetYear() == 1895 || metadata->GetYear() == 0)
        metadata->SetYear(lookup->GetYear());
    if (metadata->GetReleaseDate() == QDate())
        metadata->SetReleaseDate(lookup->GetReleaseDate());
    if (metadata->GetDirector() == VIDEO_DIRECTOR_UNKNOWN ||
        metadata->GetDirector().isEmpty())
    {
        QList<PersonInfo> director = lookup->GetPeople(kPersonDirector);
        if (director.count() > 0)
            metadata->SetDirector(director.takeFirst().name);
    }
    if (metadata->GetStudio().isEmpty())
    {
        QStringList studios = lookup->GetStudios();
        if (studios.count() > 0)
            metadata->SetStudio(studios.takeFirst());
    }
    if (metadata->GetPlot() == VIDEO_PLOT_DEFAULT ||
        metadata->GetPlot().isEmpty())
        metadata->SetPlot(lookup->GetDescription());
    if (metadata->GetUserRating() == 0)
        metadata->SetUserRating(lookup->GetUserRating());
    if (metadata->GetRating() == VIDEO_RATING_DEFAULT)
        metadata->SetRating(lookup->GetCertification());
    if (metadata->GetLength() == 0)
        metadata->SetLength(lookup->GetRuntime());
    if (metadata->GetSeason() == 0)
        metadata->SetSeason(lookup->GetSeason());
    if (metadata->GetEpisode() == 0)
        metadata->SetEpisode(lookup->GetEpisode());
    if (metadata->GetHomepage().isEmpty())
        metadata->SetHomepage(lookup->GetHomepage());

    metadata->SetInetRef(lookup->GetInetref());

    m_d->AutomaticParentalAdjustment(metadata);

    // Cast
    QList<PersonInfo> actors = lookup->GetPeople(kPersonActor);
    QList<PersonInfo> gueststars = lookup->GetPeople(kPersonGuestStar);

    for (QList<PersonInfo>::const_iterator p = gueststars.begin();
        p != gueststars.end(); ++p)
    {
        actors.append(*p);
    }

    VideoMetadata::cast_list cast;
    QStringList cl;

    for (QList<PersonInfo>::const_iterator p = actors.begin();
        p != actors.end(); ++p)
    {
        cl.append((*p).name);
    }

    for (QStringList::const_iterator p = cl.begin();
        p != cl.end(); ++p)
    {
        QString cn = (*p).trimmed();
        if (cn.length())
        {
            cast.push_back(VideoMetadata::cast_list::
                        value_type(-1, cn));
        }
    }

    metadata->SetCast(cast);

    // Genres
    VideoMetadata::genre_list video_genres;
    QStringList genres = lookup->GetCategories();

    for (QStringList::const_iterator p = genres.begin();
        p != genres.end(); ++p)
    {
        QString genre_name = (*p).trimmed();
        if (genre_name.length())
        {
            video_genres.push_back(
                    VideoMetadata::genre_list::value_type(-1, genre_name));
        }
    }

    metadata->SetGenres(video_genres);

    // Countries
    VideoMetadata::country_list video_countries;
    QStringList countries = lookup->GetCountries();

    for (QStringList::const_iterator p = countries.begin();
        p != countries.end(); ++p)
    {
        QString country_name = (*p).trimmed();
        if (country_name.length())
        {
            video_countries.push_back(
                    VideoMetadata::country_list::value_type(-1,
                            country_name));
        }
    }

    metadata->SetCountries(video_countries);
    metadata->SetProcessed(true);

    metadata->UpdateDatabase();

    MythUIButtonListItem *item = GetItemByMetadata(metadata);
    if (item != NULL)
        UpdateItem(item);

    StartVideoImageSet(metadata);
}

void VideoDialog::doVideoScan()
{
    if (!m_d->m_scanner)
        m_d->m_scanner = new VideoScanner();
    connect(m_d->m_scanner, SIGNAL(finished(bool)), SLOT(scanFinished(bool)));
    m_d->m_scanner->doScan(GetVideoDirs());
}

void VideoDialog::PromptToScan()
{
    QString message = tr("There are no videos in the database, would you like "
                         "to scan your video directories now?");
    MythConfirmationDialog *dialog = new MythConfirmationDialog(m_popupStack,
                                                                message,
                                                                true);
    dialog->SetReturnEvent(this, "scanprompt");
    if (dialog->Create())
        m_popupStack->AddScreen(dialog);
    else
        delete dialog;
}

#include "videodlg.moc"
