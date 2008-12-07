#include <set>

#include <QApplication>
#include <QTimer>
#include <QList>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QHttp>
#include <QBuffer>
#include <QUrl>

#include <mythtv/mythcontext.h>
#include <mythtv/compat.h>
#include <mythtv/mythdirs.h>

#include <mythtv/libmythui/mythuihelper.h>
#include <mythtv/libmythui/mythprogressdialog.h>
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythuibuttonlist.h>
#include <mythtv/libmythui/mythuibuttontree.h>
#include <mythtv/libmythui/mythuiimage.h>
#include <mythtv/libmythui/mythuistatetype.h>
#include <mythtv/libmythui/mythdialogbox.h>
#include <mythtv/libmythui/mythgenerictree.h>

#include "videodlg.h"
#include "videoscan.h"
#include "globals.h"
#include "videofilter.h"
#include "editmetadata.h"
#include "metadatalistmanager.h"
#include "videopopups.h"

namespace
{
    bool IsValidDialogType(int num)
    {
        for (int i = 1; i <= VideoDialog::dtLast - 1; i <<= 1)
            if (num == i) return true;
        return false;
    }

    QString ParentalLevelToState(const ParentalLevel &level)
    {
        QString ret;
        switch (level.GetLevel())
        {
            case ParentalLevel::plLowest :
                ret = "Lowest";
                break;
            case ParentalLevel::plLow :
                ret = "Low";
                break;
            case ParentalLevel::plMedium :
                ret = "Medium";
                break;
            case ParentalLevel::plHigh :
                ret = "High";
                break;
            default:
                ret = "None";
        }

        return ret;
    }

    class CoverDownloadProxy : public QObject
    {
        Q_OBJECT

      signals:
        void SigFinished(CoverDownloadErrorState reason, QString errorMsg,
                         Metadata *item);
      public:
        static CoverDownloadProxy *Create(const QUrl &url, const QString &dest,
                                          Metadata *item)
        {
            return new CoverDownloadProxy(url, dest, item);
        }

      public:
        void StartCopy()
        {
            m_id = m_http.get(m_url.toString(), &m_data_buffer);

            m_timer.start(gContext->GetNumSetting("PosterDownloadTimeout", 30)
                          * 1000);
        }

        void Stop()
        {
            if (m_timer.isActive())
                m_timer.stop();

            VERBOSE(VB_GENERAL, tr("Cover download stopped."));
            m_http.abort();
        }

      private:
        CoverDownloadProxy(const QUrl &url, const QString &dest,
                           Metadata *item) : m_item(item), m_dest_file(dest),
            m_id(0), m_url(url), m_error_state(esOK)
        {
            connect(&m_http, SIGNAL(requestFinished(int, bool)),
                    SLOT(OnFinished(int, bool)));

            connect(&m_timer, SIGNAL(timeout()), SLOT(OnDownloadTimeout()));
            m_timer.setSingleShot(true);
            m_http.setHost(m_url.host());
        }

        ~CoverDownloadProxy() {}

      private slots:
        void OnDownloadTimeout()
        {
            VERBOSE(VB_IMPORTANT, QString("Copying of '%1' timed out")
                    .arg(m_url.toString()));
            m_error_state = esTimeout;
            Stop();
        }

        void OnFinished(int id, bool error)
        {
            QString errorMsg;
            if (error)
                errorMsg = m_http.errorString();

            if (id == m_id)
            {
                if (m_timer.isActive())
                    m_timer.stop();

                if (!error)
                {
                    QFile dest_file(m_dest_file);
                    if (dest_file.exists())
                        dest_file.remove();

                    if (dest_file.open(QIODevice::WriteOnly))
                    {
                        const QByteArray &data = m_data_buffer.data();
                        qint64 size = dest_file.write(data);
                        if (size != data.size())
                        {
                            errorMsg = tr("Error writing data to file %1.")
                                    .arg(m_dest_file);
                            m_error_state = esError;
                        }
                    }
                    else
                    {
                        errorMsg = tr("Error: file error '%1' for file %2").
                                arg(dest_file.errorString()).arg(m_dest_file);
                        m_error_state = esError;
                    }
                }

                emit SigFinished(m_error_state, errorMsg, m_item);
            }
        }

      private:
        Metadata *m_item;
        QHttp m_http;
        QBuffer m_data_buffer;
        QString m_dest_file;
        int m_id;
        QTimer m_timer;
        QUrl m_url;
        CoverDownloadErrorState m_error_state;
    };

    /** \class ExecuteExternalCommand
     *
     * \brief Base class for executing an external script or other process, must
     *        be subclassed.
     *
     */
    class ExecuteExternalCommand : public QObject
    {
        Q_OBJECT

      protected:
        ExecuteExternalCommand(QObject *oparent) :
            QObject(oparent), m_purpose(QObject::tr("Command"))
        {

            connect(&m_process, SIGNAL(readyReadStandardOutput()),
                    SLOT(OnReadReadyStandardOutput()));
            connect(&m_process, SIGNAL(readyReadStandardError()),
                    SLOT(OnReadReadyStandardError()));
            connect(&m_process, SIGNAL(finished(int, QProcess::ExitStatus)),
                    SLOT(OnProcessFinished(int, QProcess::ExitStatus)));
            connect(&m_process, SIGNAL(error(QProcess::ProcessError)),
                    SLOT(OnProcessError(QProcess::ProcessError)));
        }

        void StartRun(QString command, QStringList extra_args, QString purpose)
        {
            m_purpose = purpose;

            // TODO: punting on spaces in path to command
            QStringList args = command.split(' ', QString::SkipEmptyParts);
            args += extra_args;

            if (args.size())
            {
                m_raw_cmd = args[0];
                args.pop_front();

                VERBOSE(VB_GENERAL, QString("%1: Executing \"'%2' %3\"")
                        .arg(purpose).arg(m_raw_cmd).arg(args.join(" ")));

                QFileInfo fi(m_raw_cmd);

                QString err_msg;

                if (!fi.exists())
                {
                    err_msg = QString("\"%1\" failed: does not exist")
                            .arg(m_raw_cmd);
                }
                else if (!fi.isExecutable())
                {
                    err_msg = QString("\"%1\" failed: not executable")
                            .arg(m_raw_cmd);
                }

                m_process.start(m_raw_cmd, args);
                if (!m_process.waitForStarted())
                {
                    err_msg = QString("\"%1\" failed: Could not start process")
                            .arg(m_raw_cmd);
                }

                if (err_msg.length())
                {
                    ShowError(err_msg);
                }
            }
            else
            {
                ShowError(tr("No command to run."));
            }
        }

        virtual void OnExecDone(bool normal_exit, QStringList out,
                QStringList err) = 0;

      private slots:
        void OnReadReadyStandardOutput()
        {
            QByteArray buf = m_process.readAllStandardOutput();
            m_std_out += QString::fromUtf8(buf.data(), buf.size());
        }

        void OnReadReadyStandardError()
        {
            QByteArray buf = m_process.readAllStandardError();
            m_std_error += QString::fromUtf8(buf.data(), buf.size());
        }

        void OnProcessFinished(int exitCode, QProcess::ExitStatus status)
        {
            (void) exitCode;

            if (status != QProcess::NormalExit)
            {
                ShowError(QString("\"%1\" failed: Process exited abnormally")
                            .arg(m_raw_cmd));
            }

            if (m_std_error.length())
            {
                ShowError(m_std_error);
            }

            QStringList std_out =
                    m_std_out.split("\n", QString::SkipEmptyParts);
            for (QStringList::iterator p = std_out.begin();
                    p != std_out.end(); )
            {
                QString check = (*p).trimmed();
                if (check.at(0) == '#' || !check.length())
                {
                    p = std_out.erase(p);
                }
                else
                    ++p;
            }

            VERBOSE(VB_GENERAL|VB_EXTRA, m_std_out);

            OnExecDone(status == QProcess::NormalExit, std_out,
                    m_std_error.split("\n"));
        }

        void OnProcessError(QProcess::ProcessError error)
        {
            ShowError(QString("\"%1\" failed: Process error %2")
                    .arg(m_raw_cmd).arg(error));
            OnExecDone(false, m_std_out.split("\n"), m_std_error.split("\n"));
        }

      private:
        void ShowError(QString error_msg)
        {
            VERBOSE(VB_IMPORTANT, error_msg);

            QString message =
                    QString(QObject::tr("%1 failed\n\n%2\n\nCheck VideoManager "
                                    "Settings")).arg(m_purpose).arg(error_msg);

            MythScreenStack *popupStack =
                    GetMythMainWindow()->GetStack("popup stack");

            MythConfirmationDialog *okPopup =
                    new MythConfirmationDialog(popupStack, message, false);

            if (okPopup->Create())
                popupStack->AddScreen(okPopup, false);
        }

      private:
        QString m_std_error;
        QString m_std_out;
        QProcess m_process;
        QString m_purpose;
        QString m_raw_cmd;
    };

    /** \class VideoTitleSearch
     *
     * \brief Executes the external command to do video title searches.
     *
     */
    class VideoTitleSearch : public ExecuteExternalCommand
    {
        Q_OBJECT

      signals:
        void SigSearchResults(bool normal_exit, const SearchListResults &items,
                Metadata *item);

      public:
        VideoTitleSearch(QObject *oparent) :
            ExecuteExternalCommand(oparent), m_item(0) {}

        void Run(QString title, Metadata *item)
        {
            m_item = item;

            QString def_cmd = QDir::cleanPath(QString("%1/%2")
                    .arg(GetShareDir())
                    .arg("mythvideo/scripts/tmdb.pl -M"));

            QString cmd = gContext->GetSetting("MovieListCommandLine", def_cmd);

            QStringList args;
            args += title;
            StartRun(cmd, args, "Video Search");
        }

      private:
        ~VideoTitleSearch() {}

        void OnExecDone(bool normal_exit, QStringList out, QStringList err)
        {
            (void) err;

            SearchListResults results;
            if (normal_exit)
            {
                for (QStringList::const_iterator p = out.begin();
                        p != out.end(); ++p)
                {
                    results.insert((*p).section(':', 0, 0),
                            (*p).section(':', 1));
                }
            }

            emit SigSearchResults(normal_exit, results, m_item);
            deleteLater();
        }

      private:
        Metadata *m_item;
    };

    /** \class VideoUIDSearch
     *
     * \brief Execute the command to do video searches based on their ID.
     *
     */
    class VideoUIDSearch : public ExecuteExternalCommand
    {
        Q_OBJECT

      signals:
        void SigSearchResults(bool normal_exit, QStringList results,
                Metadata *item, QString video_uid);

      public:
        VideoUIDSearch(QObject *oparent) :
            ExecuteExternalCommand(oparent), m_item(0) {}

        void Run(QString video_uid, Metadata *item)
        {
            m_item = item;
            m_video_uid = video_uid;

            const QString def_cmd = QDir::cleanPath(QString("%1/%2")
                    .arg(GetShareDir())
                    .arg("mythvideo/scripts/tmdb.pl -D"));
            const QString cmd = gContext->GetSetting("MovieDataCommandLine",
                                                        def_cmd);

            StartRun(cmd, QStringList(video_uid), "Video Data Query");
        }

      private:
        ~VideoUIDSearch() {}

        void OnExecDone(bool normal_exit, QStringList out, QStringList err)
        {
            (void) err;
            emit SigSearchResults(normal_exit, out, m_item, m_video_uid);
            deleteLater();
        }

      private:
        Metadata *m_item;
        QString m_video_uid;
    };

    /** \class VideoPosterSearch
     *
     * \brief Execute external video poster command.
     *
     */
    class VideoPosterSearch : public ExecuteExternalCommand
    {
        Q_OBJECT

      signals:
        void SigPosterURL(QString url, Metadata *item);

      public:
        VideoPosterSearch(QObject *oparent) :
            ExecuteExternalCommand(oparent), m_item(0) {}

        void Run(QString video_uid, Metadata *item)
        {
            m_item = item;

            const QString default_cmd =
                    QDir::cleanPath(QString("%1/%2")
                                        .arg(GetShareDir())
                                        .arg("mythvideo/scripts/tmdb.pl -P"));
            const QString cmd = gContext->GetSetting("MoviePosterCommandLine",
                                                        default_cmd);
            StartRun(cmd, QStringList(video_uid), "Poster Query");
        }

      private:
        ~VideoPosterSearch() {}

        void OnExecDone(bool normal_exit, QStringList out, QStringList err)
        {
            (void) err;
            QString url;
            if (normal_exit && out.size())
            {
                for (QStringList::const_iterator p = out.begin();
                        p != out.end(); ++p)
                {
                    if ((*p).length())
                    {
                        url = *p;
                        break;
                    }
                }
            }

            emit SigPosterURL(url, m_item);
            deleteLater();
        }

      private:
        Metadata *m_item;
    };

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


    // Note: no data here because there isn't another good QObject to listen
    // for this event.
    struct SwitchLayoutEvent : public QEvent
    {
        enum EventID { eID = QEvent::User + 10308 };
        SwitchLayoutEvent() : QEvent(static_cast<QEvent::Type>(eID)) {}
    };

    // TODO: Make a way to remove a screen from the screen stack without
    // deleting it (but allow it to be deleted later). This is all to stop
    // VideoDialog from deleting itself when creating a new view.
    class LayoutSwitchHelper : public QObject
    {
        Q_OBJECT

      public:
        static void Create(VideoDialog::DialogType type,
                VideoDialog::VideoListPtr videoList,
                MythScreenStack *screenStack)
        {
            new LayoutSwitchHelper(type, videoList, screenStack);
        }

      private:
        LayoutSwitchHelper(VideoDialog::DialogType type,
                VideoDialog::VideoListPtr videoList,
                MythScreenStack *screenStack) : m_type(type),
            m_videoList(videoList), m_screenStack(screenStack)
        {
            qApp->postEvent(this, new SwitchLayoutEvent);
        }

        ~LayoutSwitchHelper() {}

      protected:
        void customEvent(QEvent *levent)
        {
            if (levent->type() ==
                    static_cast<QEvent::Type>(SwitchLayoutEvent::eID))
            {
                m_screenStack->PopScreen(false);

                VideoDialog *mythvideo =
                        new VideoDialog(GetMythMainWindow()->GetMainStack(),
                                "mythvideo", m_videoList, m_type);

                if (mythvideo->Create())
                {
                    m_screenStack->AddScreen(mythvideo, false);
                }
                else
                {
                    ShowOkPopup(tr("An error occurred when switching views."));
                }

                deleteLater();
            }
        }

      private:
        VideoDialog::DialogType m_type;
        VideoDialog::VideoListPtr m_videoList;
        MythScreenStack *m_screenStack;
    };
}

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

  public:
    VideoDialogPrivate() : m_switchingLayout(false), m_firstLoadPass(true)
    {
        if (gContext->GetNumSetting("mythvideo.ParentalLevelFromRating", 0))
        {
            for (ParentalLevel sl(ParentalLevel::plLowest);
                sl.GetLevel() <= ParentalLevel::plHigh && sl.good(); ++sl)
            {
                QString ratingstring = gContext->GetSetting(
                                QString("mythvideo.AutoR2PL%1").arg(sl.GetLevel()));
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
    }

    ~VideoDialogPrivate()
    {
        StopAllRunningCoverDownloads();
    }

    void AutomaticParentalAdjustment(Metadata *metadata)
    {
        if (metadata && m_rating_to_pl.size())
        {
            QString rating = metadata->Rating();
            for (parental_level_map::const_iterator p = m_rating_to_pl.begin();
                    rating.length() && p != m_rating_to_pl.end(); ++p)
            {
                if (rating.indexOf(p->first) != -1)
                {
                    metadata->setShowLevel(p->second);
                    break;
                }
            }
        }
    }

    void DelayVideoListDestruction(VideoDialog::VideoListPtr videoList)
    {
        m_savedPtr = new VideoListDeathDelay(videoList);
    }

    void AddCoverDownload(CoverDownloadProxy *download)
    {
        m_running_downloads.insert(download);
    }

    void RemoveCoverDownload(CoverDownloadProxy *download)
    {
        if (download)
        {
            cover_download_list::iterator p =
                    m_running_downloads.find(download);
            if (p != m_running_downloads.end())
                m_running_downloads.erase(p);
        }
    }

    void StopAllRunningCoverDownloads()
    {
        cover_download_list tmp(m_running_downloads);
        for (cover_download_list::iterator p = tmp.begin(); p != tmp.end(); ++p)
            (*p)->Stop();
    }

  public:
    typedef std::set<CoverDownloadProxy *> cover_download_list;
    cover_download_list m_running_downloads;
    ParentalLevelNotifyContainer m_parentalLevel;
    bool m_switchingLayout;

    static VideoDialog::VideoListDeathDelayPtr m_savedPtr;

    bool m_firstLoadPass;

  private:
    parental_level_map m_rating_to_pl;
};

VideoDialog::VideoListDeathDelayPtr VideoDialogPrivate::m_savedPtr;

VideoListDeathDelay::VideoListDeathDelay(VideoDialog::VideoListPtr toSave) :
    QObject(qApp), m_savedList(toSave)
{
    QTimer::singleShot(3000, this, SLOT(OnTimeUp()));
}

VideoDialog::VideoListPtr VideoListDeathDelay::GetSaved()
{
    return m_savedList;
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
                         VideoListPtr video_list, DialogType type)
            : MythScreenType(lparent, lname), m_rootNode(0), m_currentNode(0),
            m_treeLoaded(false), m_isFlatList(false), m_type(type),
            m_scanner(0), m_menuPopup(0), m_busyPopup(0), m_videoButtonList(0),
            m_videoButtonTree(0), m_titleText(0), m_novideoText(0),
            m_positionText(0), m_crumbText(0), m_coverImage(0),
            m_parentalLevelState(0), m_videoLevelState(0), m_userRatingState(0),
            m_videoList(video_list), m_rememberPosition(false)
{
    m_private = new VideoDialogPrivate;

    m_popupStack = GetMythMainWindow()->GetStack("popup stack");

    m_videoList->setCurrentVideoFilter(VideoFilterSettings(true, lname));

    m_isFileBrowser = gContext->GetNumSetting("VideoDialogNoDB", 0);

    m_artDir = gContext->GetSetting("VideoArtworkDir");

    m_rememberPosition =
            gContext->GetNumSetting("mythvideo.VideoTreeRemember", 0);
}

VideoDialog::~VideoDialog()
{
    if (m_rememberPosition && m_videoButtonTree)
    {
        MythGenericTree *node = m_videoButtonTree->GetCurrentNode();
        if (node)
        {
            gContext->SaveSetting("mythvideo.VideoTreeLastActive",
                    node->getRouteByString().join("\n"));
        }
    }

    if (m_scanner)
        delete m_scanner;

    if (!m_private->m_switchingLayout)
        m_private->DelayVideoListDestruction(m_videoList);

    delete m_private;
}

bool VideoDialog::Create()
{
    if (m_type == DLG_DEFAULT)
    {
        m_type = static_cast<DialogType>(
                gContext->GetNumSetting("Default MythVideo View", DLG_GALLERY));
    }

    if (!IsValidDialogType(m_type))
    {
        m_type = DLG_GALLERY;
    }

    QString windowName = "videogallery";
    int flatlistDefault = 0;

    switch (m_type)
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
            m_isFlatList =
                    gContext->GetNumSetting("mythvideo.db_folder_view", 1);
            windowName = "manager";
            flatlistDefault = 1;
            break;
        case DLG_DEFAULT:
        default:
            break;
    }

    m_isFlatList = gContext->GetNumSetting(
            QString("mythvideo.folder_view_%1").arg(m_type), flatlistDefault);

    if (!LoadWindowFromXML("video-ui.xml", windowName, this))
        return false;

    try
    {
        if (m_type == DLG_TREE)
            UIUtilE::Assign(this, m_videoButtonTree, "videos");
        else
            UIUtilE::Assign(this, m_videoButtonList, "videos");
    }
    catch (UIUtilException &e)
    {
        VERBOSE(VB_IMPORTANT, e.What());
        return false;
    }

    UIUtil::Assign(this, m_titleText, "title");
    UIUtil::Assign(this, m_novideoText, "novideos");
    UIUtil::Assign(this, m_positionText, "position");
    UIUtil::Assign(this, m_crumbText, "breadcrumbs");

    UIUtil::Assign(this, m_coverImage, "coverimage");

    UIUtil::Assign(this, m_parentalLevelState, "parentallevel");
    UIUtil::Assign(this, m_videoLevelState, "videolevel");
    UIUtil::Assign(this, m_userRatingState, "userratingstate");
    CheckedSet(m_novideoText, tr("No Videos Available"));

    CheckedSet(m_parentalLevelState, "None");

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist.");

    if (m_type == DLG_TREE)
    {
        SetFocusWidget(m_videoButtonTree);
        m_videoButtonTree->SetActive(true);

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
        m_videoButtonList->SetActive(true);

        connect(m_videoButtonList, SIGNAL(itemClicked(MythUIButtonListItem *)),
                SLOT(handleSelect(MythUIButtonListItem *)));
        connect(m_videoButtonList, SIGNAL(itemSelected(MythUIButtonListItem *)),
                SLOT(UpdateText(MythUIButtonListItem *)));
    }

    connect(&m_private->m_parentalLevel, SIGNAL(SigLevelChanged()),
            SLOT(reloadData()));

    m_private->m_parentalLevel.SetLevel(ParentalLevel(gContext->
                    GetNumSetting("VideoDefaultParentalLevel",
                            ParentalLevel::plLowest)));

    return true;
}

void VideoDialog::refreshData()
{
    fetchVideos();
    loadData();

    CheckedSet(m_parentalLevelState,
            ParentalLevelToState(m_private->m_parentalLevel.GetLevel()));

    if (m_novideoText)
        m_novideoText->SetVisible(!m_treeLoaded);
}

void VideoDialog::reloadData()
{
    m_treeLoaded = false;
    refreshData();
}

void VideoDialog::loadData()
{
    if (m_type == DLG_TREE)
    {
        m_videoButtonTree->AssignTree(m_rootNode);
        if (m_private->m_firstLoadPass)
        {
            m_private->m_firstLoadPass = false;

            if (m_rememberPosition)
            {
                QStringList route =
                        gContext->GetSetting("mythvideo.VideoTreeLastActive",
                                "").split("\n");
                m_videoButtonTree->SetNodeByString(route);
            }
        }
    }
    else
    {
        m_videoButtonList->Reset();

        if (!m_treeLoaded)
            return;

        QList<MythGenericTree *> *lchildren = m_currentNode->getAllChildren();
        MythGenericTree *node;
        MythGenericTree *selectedNode = m_currentNode->getSelectedChild();
        QListIterator<MythGenericTree *> it(*lchildren);
        while (it.hasNext())
        {
            node = it.next();
            if (!node)
                return;

            int nodeInt = node->getInt();

            Metadata *metadata = NULL;

            if (nodeInt >= 0)
                metadata = m_videoList->getVideoListMetadata(nodeInt);

            // Masking videos from a different parental level saves us have to
            // rebuild the tree
    //         if (metadata &&
    //                 metadata->ShowLevel() > m_currentParentalLevel->GetLevel())
    //             continue;

            QString text = "";

            MythUIButtonListItem *item =
                new MythUIButtonListItem(m_videoButtonList, text, 0, true,
                                        MythUIButtonListItem::NotChecked);
            item->SetData(qVariantFromValue(node));

            UpdateItem(item);

            if (node == selectedNode)
                m_videoButtonList->SetItemCurrent(item);
        }
    }

    UpdatePosition();
}

void VideoDialog::UpdateItem(MythUIButtonListItem *item)
{
    if (!item)
        item = GetItemCurrent();

    if (!item)
        return;

    MythGenericTree *node = qVariantValue<MythGenericTree *>(item->GetData());

    Metadata *metadata = GetMetadata(item);

    item->setText(metadata ? metadata->Title() : node->getString());

    QString imgFilename = GetCoverImage(node);

    if (!imgFilename.isEmpty() && QFileInfo(imgFilename).exists())
        item->SetImage(imgFilename);

    int nodeInt = node->getInt();
    if (nodeInt == kSubFolder)
    {
        item->setText(QString("%1").arg(node->childCount()-1), "childcount");
        item->DisplayState("subfolder", "nodetype");
    }
    else if (nodeInt == kUpFolder)
        item->DisplayState("upfolder", "nodetype");

    if (metadata)
    {
        item->setText(metadata->Director(), "director");
        item->setText(metadata->Plot(), "plot");
        item->setText(GetCast(*metadata), "cast");
        item->setText(getDisplayRating(metadata->Rating()), "rating");
        item->setText(metadata->InetRef(), "inetref");
        item->setText(getDisplayYear(metadata->Year()), "year");
        item->setText(getDisplayUserRating(metadata->UserRating()),
                "userrating");
        item->setText(getDisplayLength(metadata->Length()), "length");
        item->setText(metadata->CoverFile(), "coverfile");
        item->setText(metadata->Filename(), "filename");
        item->setText(QString::number(metadata->ChildID()), "child_id");
        item->setText(getDisplayBrowse(metadata->Browse()), "browseable");
        item->setText(metadata->Category(), "category");

        item->DisplayState(QString::number(metadata->ShowLevel()), "videolevel");
        item->DisplayState(QString::number((int)(metadata->UserRating() / 2)),
                                "userratingstate");
    }

    if (item == GetItemCurrent())
        UpdateText(item);
}

void VideoDialog::fetchVideos()
{
    MythGenericTree *oldroot = m_rootNode;
    if (!m_treeLoaded)
    {
        m_rootNode = m_videoList->buildVideoList(m_isFileBrowser, m_isFlatList,
                m_private->m_parentalLevel.GetLevel(), true);
    }
    else
    {
        m_videoList->refreshList(m_isFileBrowser,
                m_private->m_parentalLevel.GetLevel(), m_isFlatList);
        m_rootNode = m_videoList->GetTreeRoot();
    }

    m_treeLoaded = true;

    m_rootNode->setOrderingIndex(kNodeSort);

    // Move a node down if there is a single directory item here...
    if (m_rootNode->childCount() == 1)
    {
        MythGenericTree *node = m_rootNode->getChildAt(0);
        if (node->getInt() == kSubFolder && node->childCount() > 1)
            m_rootNode = node;
        else if (node->getInt() == kUpFolder)
            m_treeLoaded = false;
    }
    else if (m_rootNode->childCount() == 0)
        m_treeLoaded = false;

    if (!m_currentNode || m_rootNode != oldroot)
        SetCurrentNode(m_rootNode);
}

QString VideoDialog::GetCoverImage(MythGenericTree *node)
{
    int nodeInt = node->getInt();

    QString icon_file = "";

    if (nodeInt  == kSubFolder || nodeInt == kUpFolder)  // subdirectory
    {
        // load folder icon
        int folder_id = node->getAttribute(kFolderPath);
        QString folder_path = m_videoList->getFolderPath(folder_id);

        QString filename = QString("%1/folder").arg(folder_path);

        QStringList test_files;
        test_files.append(filename + ".png");
        test_files.append(filename + ".jpg");
        test_files.append(filename + ".gif");
        for (QStringList::const_iterator tfp = test_files.begin();
                tfp != test_files.end(); ++tfp)
        {
            if (QFile::exists(*tfp))
            {
                icon_file = *tfp;
                break;
            }
        }

        // If we found nothing, load the first image we find
        if (icon_file.isEmpty())
        {
            QDir vidDir(folder_path);
            QStringList imageTypes;

            imageTypes.append("*.jpg");
            imageTypes.append("*.png");
            imageTypes.append("*.gif");
            vidDir.setNameFilters(imageTypes);

            QStringList fList = vidDir.entryList();
            if (!fList.isEmpty())
            {
                icon_file = QString("%1/%2")
                                    .arg(folder_path)
                                    .arg(fList.at(0));
            }
        }
    }
    else
    {
        Metadata *metadata = NULL;
        metadata = m_videoList->getVideoListMetadata(nodeInt);

        if (metadata)
            icon_file = metadata->CoverFile();
    }

    if (icon_file == "No Cover")
        icon_file = "";

    return icon_file;
}

bool VideoDialog::keyPressEvent(QKeyEvent *levent)
{
    if (GetFocusWidget()->keyPressEvent(levent))
        return true;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Video", levent, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "INFO")
        {
            if (!m_menuPopup && m_currentNode->getInt() > kSubFolder)
                InfoMenu();
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
                VideoMenu();
        }
        else if (action == "ESCAPE")
        {
            if ((m_type != DLG_TREE) && m_currentNode != m_rootNode)
                handled = goBack();
            else
                handled = false;
        }
        else
            handled = false;
    }

    if (!handled)
    {
        gContext->GetMainWindow()->TranslateKeyPress("TV Frontend", levent,
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

void VideoDialog::createBusyDialog(QString title)
{
    if (m_busyPopup)
        return;

    QString message = title;

    m_busyPopup = new MythUIBusyDialog(message, m_popupStack,
            "mythvideobusydialog");

    if (m_busyPopup->Create())
        m_popupStack->AddScreen(m_busyPopup, false);
}

void VideoDialog::createOkDialog(QString title)
{
    QString message = title;

    MythConfirmationDialog *okPopup =
            new MythConfirmationDialog(m_popupStack, message, false);

    if (okPopup->Create())
        m_popupStack->AddScreen(okPopup, false);
}

bool VideoDialog::goBack()
{
    bool handled = false;

//     if (m_parent->IsExitingToMain())
//         return handled;

    if (m_currentNode != m_rootNode)
    {
        // one dir up
        MythGenericTree *lparent = m_currentNode->getParent();
        if (lparent)
        {
            // move one node up in the video tree
            SetCurrentNode(lparent);

            handled = true;
        }
    }

    loadData();

    return handled;
}

void VideoDialog::SetCurrentNode(MythGenericTree *node)
{
    if (!node)
        return;

    m_currentNode = node;
}

void VideoDialog::UpdatePosition()
{
    MythUIButtonList *currentList = GetItemCurrent()->parent();

    if (!currentList)
        return;

    CheckedSet(m_positionText, QString(tr("%1 of %2"))
            .arg(currentList->GetCurrentPos() + 1)
            .arg(currentList->GetCount()));
}

void VideoDialog::UpdateText(MythUIButtonListItem *item)
{
    if (!item)
        return;

    MythUIButtonList *currentList = item->parent();

    if (!currentList)
        return;

    Metadata *metadata = GetMetadata(item);

    CheckedSet(m_titleText, metadata ? metadata->Title() : item->text());
    UpdatePosition();

    CheckedSet(m_crumbText, m_currentNode->getRouteByString().join(" > "));

    MythGenericTree *node = qVariantValue<MythGenericTree *>(item->GetData());

    if (metadata)
    {
        item->setText(metadata->Filename(), "filename");
        item->setText(Metadata::getPlayer(metadata), "video_player");

        QString coverfile = metadata->CoverFile();

        if (m_coverImage)
        {
            if (coverfile != "No Cover")
            {
                m_coverImage->SetFilename(coverfile);
                m_coverImage->Load();
            }
            else
                m_coverImage->Reset();
        }

        CheckedSet(this, "director", metadata->Director());
        CheckedSet(this, "plot", metadata->Plot());
        CheckedSet(this, "cast", GetCast(*metadata));
        CheckedSet(this, "rating", getDisplayRating(metadata->Rating()));
        CheckedSet(this, "inetref", metadata->InetRef());
        CheckedSet(this, "year", getDisplayYear(metadata->Year()));
        CheckedSet(this, "userrating",
                getDisplayUserRating(metadata->UserRating()));
        CheckedSet(this, "length", getDisplayLength(metadata->Length()));
        CheckedSet(this, "filename", metadata->Filename());
        CheckedSet(this, "player", metadata->PlayCommand());
        CheckedSet(this, "coverfile", metadata->CoverFile());
        CheckedSet(this, "child_id", QString::number(metadata->ChildID()));
        CheckedSet(this, "browseable",
                getDisplayBrowse(metadata->Browse()));
        CheckedSet(this, "category", metadata->Category());
        CheckedSet(m_videoLevelState,
                ParentalLevelToState(metadata->ShowLevel()));
        CheckedSet(this, "childcount", "");
        CheckedSet(m_userRatingState,
                    QString::number((int)(metadata->UserRating() / 2)));
    }
    else
    {
        if (m_coverImage)
            m_coverImage->Reset();

        if (node && node->getInt() == kSubFolder)
            CheckedSet(this, "childcount",
                    QString("%1").arg(node->childCount()-1));

        CheckedSet(this, "director", "");
        CheckedSet(this, "plot", "");
        CheckedSet(this, "cast", "");
        CheckedSet(this, "rating", "");
        CheckedSet(this, "inetref", "");
        CheckedSet(this, "year", "");
        CheckedSet(this, "userrating", "");
        CheckedSet(this, "length", "");
        CheckedSet(this, "filename", "");
        CheckedSet(this, "player", "");
        CheckedSet(this, "coverfile", "");
        CheckedSet(this, "child_id", "");
        CheckedSet(this, "browseable", "");
        CheckedSet(this, "category", "");
    }

    if (node)
        node->becomeSelectedChild();
}

void VideoDialog::VideoMenu()
{
    QString label = tr("Select action");

    m_menuPopup = new MythDialogBox(label, m_popupStack, "videomenupopup");

    if (m_menuPopup->Create())
        m_popupStack->AddScreen(m_menuPopup, false);

    m_menuPopup->SetReturnEvent(this, "actions");

    MythUIButtonListItem *item = GetItemCurrent();
    if (item)
    {
        MythGenericTree *node =
                qVariantValue<MythGenericTree *>(item->GetData());
        if (node && node->getInt() >= 0)
        {
            m_menuPopup->AddButton(tr("Watch This Video"), SLOT(playVideo()));
            m_menuPopup->AddButton(tr("Video Info"), SLOT(InfoMenu()));
            m_menuPopup->AddButton(tr("Manage Video"), SLOT(ManageMenu()));
        }
    }
    m_menuPopup->AddButton(tr("Scan For Changes"), SLOT(doVideoScan()));
    m_menuPopup->AddButton(tr("Change View"), SLOT(ViewMenu()));
    m_menuPopup->AddButton(tr("Filter Display"), SLOT(ChangeFilter()));

    m_menuPopup->AddButton(tr("Cancel"));
}

void VideoDialog::ViewMenu()
{
    QString label = tr("Change View");

    m_menuPopup = new MythDialogBox(label, m_popupStack, "videomenupopup");

    if (m_menuPopup->Create())
        m_popupStack->AddScreen(m_menuPopup, false);

    m_menuPopup->SetReturnEvent(this, "view");

    if (!(m_type & DLG_BROWSER))
        m_menuPopup->AddButton(tr("Switch to Browse View"),
                SLOT(SwitchBrowse()));

    if (!(m_type & DLG_GALLERY))
        m_menuPopup->AddButton(tr("Switch to Gallery View"),
                SLOT(SwitchGallery()));

    if (!(m_type & DLG_TREE))
        m_menuPopup->AddButton(tr("Switch to List View"), SLOT(SwitchTree()));

    if (!(m_type & DLG_MANAGER))
        m_menuPopup->AddButton(tr("Switch to Manage View"),
                SLOT(SwitchManager()));


    if (m_isFileBrowser)
        m_menuPopup->AddButton(tr("Disable File Browse Mode"),
                                                    SLOT(ToggleBrowseMode()));
    else
        m_menuPopup->AddButton(tr("Enable File Browse Mode"),
                                                    SLOT(ToggleBrowseMode()));

    if (m_isFlatList)
        m_menuPopup->AddButton(tr("Disable Flat View"),
                                                    SLOT(ToggleFlatView()));
    else
        m_menuPopup->AddButton(tr("Enable Flat View"),
                                                    SLOT(ToggleFlatView()));

    m_menuPopup->AddButton(tr("Cancel"));
}

void VideoDialog::InfoMenu()
{
    QString label = tr("Select action");

    m_menuPopup = new MythDialogBox(label, m_popupStack, "videomenupopup");

    if (m_menuPopup->Create())
        m_popupStack->AddScreen(m_menuPopup, false);

    m_menuPopup->SetReturnEvent(this, "info");

    m_menuPopup->AddButton(tr("View Full Plot"), SLOT(ViewPlot()));
    m_menuPopup->AddButton(tr("View Cast"), SLOT(ShowCastDialog()));

    m_menuPopup->AddButton(tr("Cancel"));
}

void VideoDialog::ManageMenu()
{
    QString label = tr("Select action");

    m_menuPopup = new MythDialogBox(label, m_popupStack, "videomenupopup");

    if (m_menuPopup->Create())
        m_popupStack->AddScreen(m_menuPopup, false);

    m_menuPopup->SetReturnEvent(this, "manage");

    m_menuPopup->AddButton(tr("Edit Metadata"), SLOT(EditMetadata()));
    m_menuPopup->AddButton(tr("Download Metadata"), SLOT(VideoSearch()));
    m_menuPopup->AddButton(tr("Manually Enter Video #"),
            SLOT(ManualVideoUID()));
    m_menuPopup->AddButton(tr("Manually Enter Video Title"),
            SLOT(ManualVideoTitle()));
    m_menuPopup->AddButton(tr("Reset Metadata"), SLOT(ResetMetadata()));
    m_menuPopup->AddButton(tr("Toggle Browseable"), SLOT(ToggleBrowseable()));
    m_menuPopup->AddButton(tr("Remove Video"), SLOT(RemoveVideo()));

    m_menuPopup->AddButton(tr("Cancel"));
}

void VideoDialog::ToggleBrowseMode()
{
    m_isFileBrowser = !m_isFileBrowser;
    gContext->SetSetting("VideoDialogNoDB",
            QString("%1").arg((int)m_isFileBrowser));
    reloadData();
}

void VideoDialog::ToggleFlatView()
{
    m_isFlatList = !m_isFlatList;
    gContext->SetSetting(QString("mythvideo.folder_view_%1").arg(m_type),
                         QString("%1").arg((int)m_isFlatList));
    // TODO: This forces a complete tree rebuild, this is SLOW and shouldn't
    // be necessary since MythGenericTree can do a flat view without a rebuild,
    // I just don't want to re-write VideoList just now
    reloadData();
}

void VideoDialog::handleDirSelect(MythGenericTree *node)
{
    SetCurrentNode(node);
    loadData();
}

void VideoDialog::handleSelect(MythUIButtonListItem *item)
{
    MythGenericTree *node = qVariantValue<MythGenericTree *>(item->GetData());
    int nodeInt = node->getInt();

    switch (nodeInt)
    {
        case kSubFolder:
            handleDirSelect(node);
            break;
        case kUpFolder:
            goBack();
            break;
        default:
            playVideo();
    };
}

void VideoDialog::SwitchTree()
{
    SwitchLayout(DLG_TREE);
}

void VideoDialog::SwitchGallery()
{
    SwitchLayout(DLG_GALLERY);
}

void VideoDialog::SwitchBrowse()
{
    SwitchLayout(DLG_BROWSER);
}

void VideoDialog::SwitchManager()
{
    SwitchLayout(DLG_MANAGER);
}

void VideoDialog::SwitchLayout(DialogType type)
{
    m_private->m_switchingLayout = true;
    LayoutSwitchHelper::Create(type, m_videoList, GetScreenStack());
}

void VideoDialog::ViewPlot()
{
    Metadata *metadata = GetMetadata(GetItemCurrent());

    PlotDialog *plotdialog = new PlotDialog(m_popupStack, metadata);

    if (plotdialog->Create())
        m_popupStack->AddScreen(plotdialog);
}

void VideoDialog::ShowCastDialog()
{
    Metadata *metadata = GetMetadata(GetItemCurrent());

    CastDialog *castdialog = new CastDialog(m_popupStack, metadata);

    if (castdialog->Create())
        m_popupStack->AddScreen(castdialog);
}

void VideoDialog::playVideo()
{
    Metadata *metadata = GetMetadata(GetItemCurrent());

    PlayVideo(metadata->Filename(), m_videoList->getListCache());

    gContext->GetMainWindow()->raise();
    gContext->GetMainWindow()->activateWindow();
    if (gContext->GetMainWindow()->currentWidget())
        gContext->GetMainWindow()->currentWidget()->setFocus();
}

void VideoDialog::setParentalLevel(const ParentalLevel::Level &level)
{
    m_private->m_parentalLevel.SetLevel(level);
}

void VideoDialog::shiftParental(int amount)
{
    setParentalLevel(ParentalLevel(m_private->m_parentalLevel.GetLevel()
                    .GetLevel() + amount).GetLevel());
}

void VideoDialog::ChangeFilter()
{
    MythScreenStack *mainStack = GetScreenStack();

    VideoFilterDialog *filterdialog = new VideoFilterDialog(mainStack,
            "videodialogfilters", m_videoList.get());

    if (filterdialog->Create())
        mainStack->AddScreen(filterdialog);

    connect(filterdialog, SIGNAL(filterChanged()), SLOT(reloadData()));
}

void VideoDialog::customEvent(QEvent *levent)
{
    if (levent->type() == kMythDialogBoxCompletionEventType)
    {
        m_menuPopup = NULL;
    }
}

Metadata *VideoDialog::GetMetadata(MythUIButtonListItem *item)
{
    Metadata *metadata = NULL;

    if (item)
    {
        MythGenericTree *node =
                qVariantValue<MythGenericTree *>(item->GetData());
        if (node)
        {
            int nodeInt = node->getInt();

            if (nodeInt >= 0)
                metadata = m_videoList->getVideoListMetadata(nodeInt);
        }
    }

    return metadata;
}

MythUIButtonListItem *VideoDialog::GetItemCurrent()
{
    if (m_videoButtonTree)
    {
        return m_videoButtonTree->GetItemCurrent();
    }

    return m_videoButtonList->GetItemCurrent();
}

void VideoDialog::VideoSearch()
{
    Metadata *metadata = GetMetadata(GetItemCurrent());

    if (metadata)
        StartVideoSearchByTitle(metadata->InetRef(), metadata->Title(),
                                metadata);
}

void VideoDialog::ToggleBrowseable()
{
    Metadata *metadata = GetMetadata(GetItemCurrent());
    if (metadata)
    {
        metadata->setBrowse(!metadata->Browse());
        metadata->updateDatabase();

        refreshData();
    }
}

// void VideoDialog::OnVideoSearchListCancel()
// {
//     // I'm keeping this behavior for now, though item
//     // modification on Cancel is seems anathema to me.
//     Metadata *item = GetItemCurrent();
//
//     if (item && isDefaultCoverFile(item->CoverFile()))
//     {
//         QStringList search_dirs;
//         search_dirs += m_artDir;
//         QString cover_file;
//
//         if (GetLocalVideoPoster(item->InetRef(), item->Filename(),
//                                 search_dirs, cover_file))
//         {
//             item->setCoverFile(cover_file);
//             item->updateDatabase();
//             loadData();
//         }
//     }
// }

void VideoDialog::OnVideoSearchListSelection(QString video_uid)
{
    Metadata *metadata = GetMetadata(GetItemCurrent());
    if (metadata && !video_uid.isEmpty())
    {
        StartVideoSearchByUID(video_uid, metadata);
    }
}

// void VideoDialog::RefreshVideoList(bool resort_only)
// {
//     static bool updateML = false;
//     if (updateML == true)
//         return;
//     updateML = true;
//
//     unsigned int selected_id = 0;
//     const Metadata *metadata = GetMetadata(GetItemCurrent());
//     if (metadata)
//         selected_id = metadata->ID();
//
//     if (resort_only)
//     {
//         m_videoList->resortList(true);
//     }
//     else
//     {
//         m_videoList->refreshList(false,
//                 ParentalLevel(ParentalLevel::plNone), true);
//     }
//
//     m_videoButtonList->OnListChanged();
//
//     // TODO: This isn't perfect, if you delete the last item your selection
//     // reverts to the first item.
//     if (selected_id)
//     {
//         MetadataListManager::MetadataPtr sel_item =
//                 m_videoList->getListCache().byID(selected_id);
//         if (sel_item)
//         {
//             m_videoButtonList->SetSelectedItem(sel_item->getFlatIndex());
//         }
//     }
//
//     updateML = false;
// }

void VideoDialog::AutomaticParentalAdjustment(Metadata *metadata)
{
    m_private->AutomaticParentalAdjustment(metadata);
}

void VideoDialog::OnParentalChange(int amount)
{
    Metadata *metadata = GetMetadata(GetItemCurrent());
    if (metadata)
    {
        ParentalLevel curshowlevel = metadata->ShowLevel();

        curshowlevel += amount;

        if (curshowlevel.GetLevel() != metadata->ShowLevel())
        {
            metadata->setShowLevel(curshowlevel.GetLevel());
            metadata->updateDatabase();
            refreshData();
        }
    }
}

void VideoDialog::ManualVideoUID()
{
    QString message = tr("Enter Video Unique ID:");

    MythTextInputDialog *searchdialog =
                                new MythTextInputDialog(m_popupStack,message);

    if (searchdialog->Create())
        m_popupStack->AddScreen(searchdialog);

    connect(searchdialog, SIGNAL(haveResult(QString)),
            SLOT(OnManualVideoUID(QString)), Qt::QueuedConnection);
}

void VideoDialog::OnManualVideoUID(QString video_uid)
{
    Metadata *metadata = GetMetadata(GetItemCurrent());
    if (video_uid.length())
        StartVideoSearchByUID(video_uid, metadata);
}

void VideoDialog::ManualVideoTitle()
{
    QString message = tr("Enter Video Title:");

    MythTextInputDialog *searchdialog =
                                new MythTextInputDialog(m_popupStack,message);

    if (searchdialog->Create())
        m_popupStack->AddScreen(searchdialog);

    connect(searchdialog, SIGNAL(haveResult(QString)),
            SLOT(OnManualVideoTitle(QString)), Qt::QueuedConnection);
}

void VideoDialog::OnManualVideoTitle(QString title)
{
    Metadata *metadata = GetMetadata(GetItemCurrent());
    if (title.length() && metadata)
    {
        StartVideoSearchByTitle(VIDEO_INETREF_DEFAULT, title, metadata);
    }
}

void VideoDialog::EditMetadata()
{
    Metadata *metadata = GetMetadata(GetItemCurrent());
    if (!metadata)
        return;

    MythScreenStack *screenStack = GetScreenStack();

    EditMetadataDialog *md_editor = new EditMetadataDialog(screenStack,
            "mythvideoeditmetadata",metadata, m_videoList->getListCache());

    connect(md_editor, SIGNAL(Finished()), SLOT(refreshData()));

    if (md_editor->Create())
        screenStack->AddScreen(md_editor);
}

void VideoDialog::RemoveVideo()
{
    Metadata *metadata = GetMetadata(GetItemCurrent());

    if (!metadata)
        return;

    QString message = tr("Delete this file?");

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

    Metadata *metadata = GetMetadata(GetItemCurrent());

    if (!metadata)
        return;

    if (m_videoList->Delete(metadata->ID()))
        refreshData();
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
    Metadata *metadata = GetMetadata(item);

    if (metadata)
    {
        ResetItem(metadata);

        QString cover_file;
        QStringList search_dirs;
        search_dirs += m_artDir;
        if (GetLocalVideoPoster(metadata->InetRef(), metadata->Filename(),
                        search_dirs, cover_file))
        {
            metadata->setCoverFile(cover_file);
            metadata->updateDatabase();
            UpdateItem(item);
        }
    }
}

void VideoDialog::ResetItem(Metadata *metadata)
{
    if (metadata)
    {
        metadata->Reset();
        metadata->updateDatabase();

        UpdateItem(GetItemCurrent());
    }
}

// Possibly move the following elsewhere, e.g. video utils

// Copy video poster to appropriate directory and set the item's cover file.
// This is the start of an async operation that needs to always complete
// to OnVideoPosterSetDone.
void VideoDialog::StartVideoPosterSet(Metadata *metadata)
{
    //createBusyDialog(QObject::tr("Fetching poster for %1 (%2)")
    //                    .arg(metadata->InetRef())
    //                    .arg(metadata->Title()));
    QStringList search_dirs;
    search_dirs += m_artDir;

    QString cover_file;

    if (GetLocalVideoPoster(metadata->InetRef(), metadata->Filename(),
                            search_dirs, cover_file))
    {
        metadata->setCoverFile(cover_file);
        OnVideoPosterSetDone(metadata);
        return;
    }

    // Obtain video poster
    VideoPosterSearch *vps = new VideoPosterSearch(this);
    connect(vps, SIGNAL(SigPosterURL(QString, Metadata *)),
            SLOT(OnPosterURL(QString, Metadata *)));
    vps->Run(metadata->InetRef(), metadata);
}

void VideoDialog::OnPosterURL(QString uri, Metadata *metadata)
{
    if (metadata)
    {
        if (uri.length())
        {
            QString fileprefix = m_artDir;

            QDir dir;

            // If the video artwork setting hasn't been set default to
            // using ~/.mythtv/MythVideo
            if (fileprefix.length() == 0)
            {
                fileprefix = GetConfDir();

                dir.setPath(fileprefix);
                if (!dir.exists())
                    dir.mkdir(fileprefix);

                fileprefix += "/MythVideo";
            }

            dir.setPath(fileprefix);
            if (!dir.exists())
                dir.mkdir(fileprefix);

            QUrl url(uri);

            QString ext = QFileInfo(url.path()).suffix();
            QString dest_file = QString("%1/%2.%3").arg(fileprefix)
                    .arg(metadata->InetRef()).arg(ext);
            VERBOSE(VB_IMPORTANT, QString("Copying '%1' -> '%2'...")
                    .arg(url.toString()).arg(dest_file));

            CoverDownloadProxy *d =
                    CoverDownloadProxy::Create(url, dest_file, metadata);
            metadata->setCoverFile(dest_file);

            connect(d, SIGNAL(SigFinished(CoverDownloadErrorState,
                                          QString, Metadata *)),
                    SLOT(OnPosterCopyFinished(CoverDownloadErrorState,
                                              QString, Metadata *)));

            d->StartCopy();
            m_private->AddCoverDownload(d);
        }
        else
        {
            metadata->setCoverFile("");
            OnVideoPosterSetDone(metadata);
        }
    }
    else
        OnVideoPosterSetDone(metadata);
}

void VideoDialog::OnPosterCopyFinished(CoverDownloadErrorState error,
                                       QString errorMsg, Metadata *item)
{
    QObject *src = sender();
    if (src)
        m_private->RemoveCoverDownload(dynamic_cast<CoverDownloadProxy *>
                                       (src));

    if (error != esOK && item)
        item->setCoverFile("");

    VERBOSE(VB_IMPORTANT, tr("Poster download finished: %1 %2")
            .arg(errorMsg).arg(error));

    if (error == esTimeout)
    {
        createOkDialog(tr("A poster exists for this item but could not be "
                            "retrieved within the timeout period.\n"));
    }

    OnVideoPosterSetDone(item);
}

// This is the final call as part of a StartVideoPosterSet
void VideoDialog::OnVideoPosterSetDone(Metadata *metadata)
{
    // The metadata has some cover file set
    if (m_busyPopup)
    {
        m_busyPopup->Close();
        m_busyPopup = NULL;
    }

    metadata->updateDatabase();
    UpdateItem(GetItemCurrent());
}

void VideoDialog::StartVideoSearchByUID(QString video_uid, Metadata *metadata)
{
    // Starting the busy dialog here triggers a bizarre segfault
    //createBusyDialog(video_uid);
    VideoUIDSearch *vns = new VideoUIDSearch(this);
    connect(vns, SIGNAL(SigSearchResults(bool, QStringList, Metadata *,
                            QString)),
            SLOT(OnVideoSearchByUIDDone(bool, QStringList, Metadata *,
                            QString)));
    vns->Run(video_uid, metadata);
}

void VideoDialog::OnVideoSearchByUIDDone(bool normal_exit, QStringList output,
        Metadata *metadata, QString video_uid)
{
    if (m_busyPopup)
    {
        m_busyPopup->Close();
        m_busyPopup = NULL;
    }

    std::map<QString, QString> data;

    if (normal_exit && output.size())
    {
        for (QStringList::const_iterator p = output.begin();
            p != output.end(); ++p)
        {
            data[(*p).section(':', 0, 0)] = (*p).section(':', 1);
        }
        // set known values
        metadata->setTitle(data["Title"]);
        metadata->setYear(data["Year"].toInt());
        metadata->setDirector(data["Director"]);
        metadata->setPlot(data["Plot"]);
        metadata->setUserRating(data["UserRating"].toFloat());
        metadata->setRating(data["MovieRating"]);
        metadata->setLength(data["Runtime"].toInt());

        AutomaticParentalAdjustment(metadata);

        // Cast
        Metadata::cast_list cast;
        QStringList cl = data["Cast"].split(",", QString::SkipEmptyParts);

        for (QStringList::const_iterator p = cl.begin();
            p != cl.end(); ++p)
        {
            QString cn = (*p).trimmed();
            if (cn.length())
            {
                cast.push_back(Metadata::cast_list::
                            value_type(-1, cn));
            }
        }

        metadata->setCast(cast);

        // Genres
        Metadata::genre_list video_genres;
        QStringList genres = data["Genres"].split(",", QString::SkipEmptyParts);

        for (QStringList::const_iterator p = genres.begin();
            p != genres.end(); ++p)
        {
            QString genre_name = (*p).trimmed();
            if (genre_name.length())
            {
                video_genres.push_back(
                        Metadata::genre_list::value_type(-1, genre_name));
            }
        }

        metadata->setGenres(video_genres);

        // Countries
        Metadata::country_list video_countries;
        QStringList countries =
                data["Countries"].split(",", QString::SkipEmptyParts);
        for (QStringList::const_iterator p = countries.begin();
            p != countries.end(); ++p)
        {
            QString country_name = (*p).trimmed();
            if (country_name.length())
            {
                video_countries.push_back(
                        Metadata::country_list::value_type(-1,
                                country_name));
            }
        }

        metadata->setCountries(video_countries);

        metadata->setInetRef(video_uid);
        StartVideoPosterSet(metadata);
    }
    else
    {
        ResetItem(metadata);
        metadata->updateDatabase();
        UpdateItem(GetItemCurrent());
    }
}

void VideoDialog::StartVideoSearchByTitle(QString video_uid, QString title,
                                            Metadata *metadata)
{
    if (video_uid == VIDEO_INETREF_DEFAULT)
    {
        createBusyDialog(title);

        VideoTitleSearch *vts = new VideoTitleSearch(this);
        connect(vts, SIGNAL(SigSearchResults(bool, const SearchListResults &,
                                Metadata *)),
                SLOT(OnVideoSearchByTitleDone(bool, const SearchListResults &,
                                Metadata *)));
        vts->Run(title, metadata);
    }
    else
    {
        SearchListResults videos;
        videos.insertMulti(video_uid, title);
        OnVideoSearchByTitleDone(true, videos, metadata);
    }
}

void VideoDialog::OnVideoSearchByTitleDone(bool normal_exit,
        const SearchListResults &results, Metadata *metadata)
{
    if (m_busyPopup)
    {
        m_busyPopup->Close();
        m_busyPopup = NULL;
    }

    (void) normal_exit;
    VERBOSE(VB_IMPORTANT,
            QString("GetVideoList returned %1 possible matches")
            .arg(results.size()));

    if (results.size() == 1)
    {
        // Only one search result, fetch data.
        if (results.begin().value().isEmpty())
        {
            ResetItem(metadata);
            return;
        }
        StartVideoSearchByUID(results.begin().key(), metadata);
    }
    else if (results.size() < 1)
    {
        createOkDialog(tr("No matches were found."));
    }
    else
    {
        SearchResultsDialog *resultsdialog =
                new SearchResultsDialog(m_popupStack, results);

        if (resultsdialog->Create())
            m_popupStack->AddScreen(resultsdialog);

        connect(resultsdialog, SIGNAL(haveResult(QString)),
                SLOT(OnVideoSearchListSelection(QString)),
                Qt::QueuedConnection);
    }
}

void VideoDialog::doVideoScan()
{
    if (!m_scanner)
        m_scanner = new VideoScanner();
    connect(m_scanner, SIGNAL(finished()), SLOT(reloadData()));
    m_scanner->doScan(GetVideoDirs());
}

#include "videodlg.moc"
