#include <set>
#include <map>

#include <QApplication>
#include <QTimer>
#include <QList>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QHttp>
#include <QBuffer>
#include <QUrl>
#include <QImageReader>

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
#include <mythtv/libmythui/mythsystem.h>

#include "videoscan.h"
#include "globals.h"
#include "videofilter.h"
#include "editmetadata.h"
#include "metadatalistmanager.h"
#include "videopopups.h"
#include "videolist.h"
#include "videoutils.h"
#include "dbaccess.h"
#include "dirscan.h"
#include "playercommand.h"
#include "videodlg.h"

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

    MythGenericTree *GetNodePtrFromButton(MythUIButtonListItem *item)
    {
        if (item)
            return item->GetData().value<MythGenericTree *>();

        return 0;
    }

    Metadata *GetMetadataPtrFromNode(MythGenericTree *node)
    {
        if (node)
            return node->GetData().value<TreeNodeData>().GetMetadata();

        return 0;
    }

    class SearchResultsDialog : public MythScreenType
    {
        Q_OBJECT

      public:
        SearchResultsDialog(MythScreenStack *lparent,
                const SearchListResults &results) :
            MythScreenType(lparent, "videosearchresultspopup"),
            m_results(results), m_resultsList(0)
        {
        }

        bool Create()
        {
            if (!LoadWindowFromXML("video-ui.xml", "moviesel", this))
                return false;

            bool err = false;
            UIUtilE::Assign(this, m_resultsList, "results", &err);
            if (err)
            {
                VERBOSE(VB_IMPORTANT, "Cannot load screen 'moviesel'");
                return false;
            }

            QMapIterator<QString, QString> resultsIterator(m_results);
            while (resultsIterator.hasNext())
            {
                resultsIterator.next();
                MythUIButtonListItem *button =
                    new MythUIButtonListItem(m_resultsList,
                            resultsIterator.value());
                QString key = resultsIterator.key();
                button->SetData(key);
            }

            connect(m_resultsList, SIGNAL(itemClicked(MythUIButtonListItem *)),
                    SLOT(sendResult(MythUIButtonListItem *)));

            if (!BuildFocusList())
                VERBOSE(VB_IMPORTANT, "Failed to build a focuslist.");

            return true;
        }

     signals:
        void haveResult(QString);

      private:
        SearchListResults m_results;
        MythUIButtonList *m_resultsList;

      private slots:
        void sendResult(MythUIButtonListItem* item)
        {
            emit haveResult(item->GetData().toString());
            Close();
        }
    };

    bool GetLocalVideoPoster(const QString &video_uid, const QString &filename,
                             const QStringList &in_dirs, QString &poster)
    {
        QStringList search_dirs(in_dirs);

        QFileInfo qfi(filename);
        search_dirs += qfi.absolutePath();

        const QString base_name = qfi.completeBaseName();
        QList<QByteArray> image_types = QImageReader::supportedImageFormats();

        typedef std::set<QString> image_type_list;
        image_type_list image_exts;

        for (QList<QByteArray>::const_iterator it = image_types.begin();
                it != image_types.end(); ++it)
        {
            image_exts.insert(QString(*it).toLower());
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
                sfn += fntm.arg(*dir).arg(base_name).arg(*ext);
                sfn += fntm.arg(*dir).arg(video_uid).arg(*ext);

                for (QStringList::const_iterator i = sfn.begin();
                        i != sfn.end(); ++i)
                {
                    if (QFile::exists(*i))
                    {
                        poster = *i;
                        return true;
                    }
                }
            }
        }

        return false;
    }

    void PlayVideo(const QString &filename,
            const MetadataListManager &video_list)
    {
        const int WATCHED_WATERMARK = 10000; // Less than this and the chain of
                                             // videos will not be followed when
                                             // playing.

        MetadataListManager::MetadataPtr item = video_list.byFilename(filename);

        if (!item) return;

        QTime playing_time;

        do
        {
            playing_time.start();

            VideoPlayerCommand::PlayerFor(item.get()).Play();

            if (item->ChildID() > 0)
                item = video_list.byID(item->ChildID());
            else
                break;
        }
        while (item && playing_time.elapsed() > WATCHED_WATERMARK);
    }

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
            MythUIImage *image;
            UIUtilW::Assign(m_screen, image, name);
            if (image)
            {
                if (filename.size())
                {
                    image->SetFilename(filename);
                    image->Load();
                }
                else
                    image->Reset();
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
            m_item->setText(value, name);
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

    void CopyMetadataToUI(const Metadata *metadata,
            CopyMetadataDestination &dest)
    {
        typedef std::map<QString, QString> valuelist;
        valuelist tmp;

        if (metadata)
        {
            const QString coverfile = metadata->CoverFile();
            if (!IsDefaultCoverFile(coverfile))
                tmp["coverimage"] = coverfile;

            tmp["coverfile"] = coverfile;

            tmp["video_player"] = VideoPlayerCommand::PlayerFor(metadata)
                    .GetCommandDisplayName();
            tmp["player"] = metadata->PlayCommand();

            tmp["filename"] = metadata->Filename();
            tmp["title"] = metadata->Title();
            tmp["director"] = metadata->Director();
            tmp["plot"] = metadata->Plot();
            tmp["genres"] = GetDisplayGenres(*metadata);
            tmp["countries"] = GetDisplayCountries(*metadata);
            tmp["cast"] = GetDisplayCast(*metadata).join(", ");
            tmp["rating"] = GetDisplayRating(metadata->Rating());
            tmp["length"] = GetDisplayLength(metadata->Length());
            tmp["year"] = GetDisplayYear(metadata->Year());
            tmp["userrating"] = GetDisplayUserRating(metadata->UserRating());

            tmp["userratingstate"] =
                    QString::number((int)(metadata->UserRating() / 2));
            tmp["videolevel"] = ParentalLevelToState(metadata->ShowLevel());

            tmp["inetref"] = metadata->InetRef();
            tmp["child_id"] = QString::number(metadata->ChildID());
            tmp["browseable"] = GetDisplayBrowse(metadata->Browse());
            tmp["category"] = metadata->Category();
        }

        struct helper
        {
            helper(valuelist &values, CopyMetadataDestination &d) :
                m_vallist(values), m_dest(d) {}

            void handleImage(const QString &name)
            {
                m_dest.handleImage(name, m_vallist[name]);
            }

            void handleText(const QString &name)
            {
                m_dest.handleText(name, m_vallist[name]);
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

        h.handleImage("coverimage");

        h.handleText("coverfile");
        h.handleText("video_player");
        h.handleText("player");
        h.handleText("filename");
        h.handleText("title");
        h.handleText("director");
        h.handleText("plot");
        h.handleText("genres");
        h.handleText("countries");
        h.handleText("cast");
        h.handleText("rating");
        h.handleText("length");
        h.handleText("year");
        h.handleText("userrating");

        h.handleText("inetref");
        h.handleText("child_id");
        h.handleText("browseable");
        h.handleText("category");

        h.handleState("userratingstate");
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
    ItemDetailPopup(MythScreenStack *lparent, Metadata *metadata,
            const MetadataListManager &listManager) :
        MythScreenType(lparent, WINDOW_NAME), m_metadata(metadata),
        m_listManager(listManager)
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

        ScreenCopyDest dest(this);
        CopyMetadataToUI(m_metadata, dest);

        return true;
    }

  private slots:
    void OnPlay()
    {
        PlayVideo(m_metadata->Filename(), m_listManager);
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
            gContext->GetMainWindow()->TranslateKeyPress("Video", levent,
                    actions);

            if (!OnKeyAction(actions))
            {
                gContext->GetMainWindow()->TranslateKeyPress("TV Frontend",
                        levent, actions);
                OnKeyAction(actions);
            }
        }

        return true;
    }

  private:
    static const char * const WINDOW_NAME;
    Metadata *m_metadata;
    const MetadataListManager &m_listManager;

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
    VideoDialogPrivate(VideoListPtr videoList, VideoDialog::DialogType type) :
        m_switchingLayout(false), m_firstLoadPass(true),
        m_rememberPosition(false), m_videoList(videoList), m_rootNode(0),
        m_currentNode(0), m_treeLoaded(false), m_isFlatList(false),
        m_type(type), m_scanner(0)
    {
        if (gContext->GetNumSetting("mythvideo.ParentalLevelFromRating", 0))
        {
            for (ParentalLevel sl(ParentalLevel::plLowest);
                sl.GetLevel() <= ParentalLevel::plHigh && sl.good(); ++sl)
            {
                QString ratingstring =
                        gContext->GetSetting(QString("mythvideo.AutoR2PL%1")
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
                gContext->GetNumSetting("mythvideo.VideoTreeRemember", 0);

        m_isFileBrowser = gContext->GetNumSetting("VideoDialogNoDB", 0);

        m_artDir = gContext->GetSetting("VideoArtworkDir");
    }

    ~VideoDialogPrivate()
    {
        delete m_scanner;
        StopAllRunningCoverDownloads();

        if (m_rememberPosition && m_lastTreeNodePath.length())
        {
            gContext->SaveSetting("mythvideo.VideoTreeLastActive",
                    m_lastTreeNodePath);
        }
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

    void DelayVideoListDestruction(VideoListPtr videoList)
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

    bool m_rememberPosition;

    VideoListPtr m_videoList;

    MythGenericTree *m_rootNode;
    MythGenericTree *m_currentNode;

    bool m_treeLoaded;

    bool m_isFileBrowser;
    bool m_isFlatList;
    VideoDialog::DialogType m_type;

    QString m_artDir;
    VideoScanner *m_scanner;

    QString m_lastTreeNodePath;

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
        VideoListPtr video_list, DialogType type) :
    MythScreenType(lparent, lname), m_menuPopup(0), m_busyPopup(0),
    m_videoButtonList(0), m_videoButtonTree(0), m_titleText(0),
    m_novideoText(0), m_positionText(0), m_crumbText(0), m_coverImage(0),
    m_parentalLevelState(0)
{
    m_d = new VideoDialogPrivate(video_list, type);

    m_popupStack = GetMythMainWindow()->GetStack("popup stack");

    m_d->m_videoList->setCurrentVideoFilter(VideoFilterSettings(true,
                    lname));

    srand(time(NULL));
}

VideoDialog::~VideoDialog()
{
    if (!m_d->m_switchingLayout)
        m_d->DelayVideoListDestruction(m_d->m_videoList);

    delete m_d;
}

bool VideoDialog::Create()
{
    if (m_d->m_type == DLG_DEFAULT)
    {
        m_d->m_type = static_cast<DialogType>(
                gContext->GetNumSetting("Default MythVideo View", DLG_GALLERY));
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
                    gContext->GetNumSetting("mythvideo.db_folder_view", 1);
            windowName = "manager";
            flatlistDefault = 1;
            break;
        case DLG_DEFAULT:
        default:
            break;
    }

    m_d->m_isFlatList =
            gContext->GetNumSetting(QString("mythvideo.folder_view_%1")
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

    UIUtilW::Assign(this, m_coverImage, "coverimage");

    UIUtilW::Assign(this, m_parentalLevelState, "parentallevel");

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen '" + windowName + "'");
        return false;
    }

    CheckedSet(m_novideoText, tr("No Videos Available"));

    CheckedSet(m_parentalLevelState, "None");

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist.");

    if (m_d->m_type == DLG_TREE)
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

    connect(&m_d->m_parentalLevel, SIGNAL(SigLevelChanged()),
            SLOT(reloadData()));

    m_d->m_parentalLevel.SetLevel(ParentalLevel(gContext->
                    GetNumSetting("VideoDefaultParentalLevel",
                            ParentalLevel::plLowest)));

    return true;
}

void VideoDialog::refreshData()
{
    fetchVideos();
    loadData();

    CheckedSet(m_parentalLevelState,
            ParentalLevelToState(m_d->m_parentalLevel.GetLevel()));

    if (m_novideoText)
        m_novideoText->SetVisible(!m_d->m_treeLoaded);
}

void VideoDialog::reloadData()
{
    m_d->m_treeLoaded = false;
    refreshData();
}

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
                        gContext->GetSetting("mythvideo.VideoTreeLastActive",
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

        MythGenericTree *selectedNode = m_d->m_currentNode->getSelectedChild();

        typedef QList<MythGenericTree *> MGTreeChildList;
        MGTreeChildList *lchildren = m_d->m_currentNode->getAllChildren();

        for (MGTreeChildList::const_iterator p = lchildren->begin();
                p != lchildren->end(); ++p)
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

    UpdatePosition();
}

void VideoDialog::UpdateItem(MythUIButtonListItem *item)
{
    if (!item)
        return;

    MythGenericTree *node = GetNodePtrFromButton(item);

    Metadata *metadata = GetMetadata(item);

    MythUIButtonListItemCopyDest dest(item);
    CopyMetadataToUI(metadata, dest);

    item->setText(metadata ? metadata->Title() : node->getString());

    QString imgFilename = GetCoverImage(node);

    if (!imgFilename.isEmpty() && QFileInfo(imgFilename).exists())
        item->SetImage(imgFilename);

    int nodeInt = node->getInt();
    if (nodeInt == kSubFolder)
    {
        item->setText(QString("%1").arg(node->childCount() - 1), "childcount");
        item->DisplayState("subfolder", "nodetype");
    }
    else if (nodeInt == kUpFolder)
        item->DisplayState("upfolder", "nodetype");

    if (item == GetItemCurrent())
        UpdateText(item);
}

void VideoDialog::fetchVideos()
{
    MythGenericTree *oldroot = m_d->m_rootNode;
    if (!m_d->m_treeLoaded)
    {
        m_d->m_rootNode = m_d->m_videoList->buildVideoList(m_d->m_isFileBrowser,
                m_d->m_isFlatList, m_d->m_parentalLevel.GetLevel(), true);
    }
    else
    {
        m_d->m_videoList->refreshList(m_d->m_isFileBrowser,
                m_d->m_parentalLevel.GetLevel(), m_d->m_isFlatList);
        m_d->m_rootNode = m_d->m_videoList->GetTreeRoot();
    }

    m_d->m_treeLoaded = true;

    m_d->m_rootNode->setOrderingIndex(kNodeSort);

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

QString VideoDialog::GetCoverImage(MythGenericTree *node)
{
    int nodeInt = node->getInt();

    QString icon_file;

    if (nodeInt  == kSubFolder || nodeInt == kUpFolder)  // subdirectory
    {
        // load folder icon
        QString folder_path = node->GetData().value<TreeNodeData>().GetPath();

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
        const Metadata *metadata = GetMetadataPtrFromNode(node);

        if (metadata)
            icon_file = metadata->CoverFile();
    }

    if (IsDefaultCoverFile(icon_file))
        icon_file.clear();

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
            if (!m_menuPopup && GetMetadata(GetItemCurrent()))
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
            if (m_d->m_type != DLG_TREE
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

void VideoDialog::SetCurrentNode(MythGenericTree *node)
{
    if (!node)
        return;

    m_d->m_currentNode = node;
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

    ScreenCopyDest dest(this);
    CopyMetadataToUI(metadata, dest);

    if (!metadata)
        CheckedSet(m_titleText, item->text());
    UpdatePosition();

    CheckedSet(m_crumbText, m_d->m_currentNode->getRouteByString().join(" > "));

    MythGenericTree *node = GetNodePtrFromButton(item);

    if (node && node->getInt() == kSubFolder)
        CheckedSet(this, "childcount",
                QString("%1").arg(node->childCount() - 1));

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
    MythGenericTree *node = GetNodePtrFromButton(item);
    if (node && node->getInt() >= 0)
    {
        m_menuPopup->AddButton(tr("Watch This Video"), SLOT(playVideo()));

        if (gContext->GetNumSetting("mythvideo.TrailersRandomEnabled", 0))
        {
             m_menuPopup->AddButton(tr("Watch With Trailers"),
                  SLOT(playVideoWithTrailers()));
        }

        Metadata *metadata = GetMetadata(item);
        if (metadata)
        {
            QString trailerFile = metadata->GetTrailer();
            if (QFile::exists(trailerFile))
            {
                 m_menuPopup->AddButton(tr("Watch Trailer"),
                         SLOT(playTrailer()));
            }
        }

        m_menuPopup->AddButton(tr("Video Info"), SLOT(InfoMenu()));
        m_menuPopup->AddButton(tr("Manage Video"), SLOT(ManageMenu()));
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

    if (!(m_d->m_type & DLG_BROWSER))
        m_menuPopup->AddButton(tr("Switch to Browse View"),
                SLOT(SwitchBrowse()));

    if (!(m_d->m_type & DLG_GALLERY))
        m_menuPopup->AddButton(tr("Switch to Gallery View"),
                SLOT(SwitchGallery()));

    if (!(m_d->m_type & DLG_TREE))
        m_menuPopup->AddButton(tr("Switch to List View"), SLOT(SwitchTree()));

    if (!(m_d->m_type & DLG_MANAGER))
        m_menuPopup->AddButton(tr("Switch to Manage View"),
                SLOT(SwitchManager()));

    if (m_d->m_isFileBrowser)
        m_menuPopup->AddButton(tr("Disable File Browse Mode"),
                                                    SLOT(ToggleBrowseMode()));
    else
        m_menuPopup->AddButton(tr("Enable File Browse Mode"),
                                                    SLOT(ToggleBrowseMode()));

    if (m_d->m_isFlatList)
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

    if (ItemDetailPopup::Exists())
        m_menuPopup->AddButton(tr("View Details"), SLOT(DoItemDetailShow()));

    m_menuPopup->AddButton(tr("View Full Plot"), SLOT(ViewPlot()));

    Metadata *metadata = GetMetadata(GetItemCurrent());
    if (metadata && metadata->getCast().size())
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
    m_d->m_isFileBrowser = !m_d->m_isFileBrowser;
    gContext->SetSetting("VideoDialogNoDB",
            QString("%1").arg((int)m_d->m_isFileBrowser));
    reloadData();
}

void VideoDialog::ToggleFlatView()
{
    m_d->m_isFlatList = !m_d->m_isFlatList;
    gContext->SetSetting(QString("mythvideo.folder_view_%1").arg(m_d->m_type),
                         QString("%1").arg((int)m_d->m_isFlatList));
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
    MythGenericTree *node = GetNodePtrFromButton(item);
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
    m_d->m_switchingLayout = true;

    if (m_d->m_rememberPosition && m_videoButtonTree)
    {
        MythGenericTree *node = m_videoButtonTree->GetCurrentNode();
        if (node)
        {
            m_d->m_lastTreeNodePath = node->getRouteByString().join("\n");
        }
    }

    VideoDialog *mythvideo =
            new VideoDialog(GetMythMainWindow()->GetMainStack(), "mythvideo",
                    m_d->m_videoList, type);

    if (mythvideo->Create())
    {
        MythScreenStack *screenStack = GetScreenStack();
        screenStack->AddScreen(mythvideo, false);
        screenStack->PopScreen(this, false, false);
        deleteLater();
    }
    else
    {
        ShowOkPopup(tr("An error occurred when switching views."));
    }
}

void VideoDialog::ViewPlot()
{
    Metadata *metadata = GetMetadata(GetItemCurrent());

    PlotDialog *plotdialog = new PlotDialog(m_popupStack, metadata);

    if (plotdialog->Create())
        m_popupStack->AddScreen(plotdialog);
}

bool VideoDialog::DoItemDetailShow()
{
    Metadata *metadata = GetMetadata(GetItemCurrent());

    if (metadata)
    {
        ItemDetailPopup *idp = new ItemDetailPopup(m_popupStack, metadata,
                m_d->m_videoList->getListCache());

        if (idp->Create())
        {
            m_popupStack->AddScreen(idp, false);
            return true;
        }
    }

    return false;
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
    if (metadata)
        PlayVideo(metadata->Filename(), m_d->m_videoList->getListCache());
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
                const QString &extension)
        {
            (void) fileName;
            (void) extension;
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
        ScanVideoDirectory(startDir, &sc, extensions, false);
        return ret;
    }
}

void VideoDialog::playVideoWithTrailers()
{
    Metadata *metadata = GetMetadata(GetItemCurrent());
    if (!metadata) return;

    QStringList trailers = GetTrailersInDirectory(gContext->
            GetSetting("mythvideo.TrailersDir"));

    if (trailers.isEmpty())
        return;

    const int trailersToPlay =
            gContext->GetNumSetting("mythvideo.TrailersRandomCount");

    int i = 0;
    while (trailers.size() && i < trailersToPlay)
    {
        ++i;
        QString trailer = trailers.takeAt(rand() % trailers.size());

        VERBOSE(VB_GENERAL | VB_EXTRA,
                QString("Random trailer to play will be: %1").arg(trailer));

        VideoPlayerCommand::PlayerFor(trailer).Play();
    }

    PlayVideo(metadata->Filename(), m_d->m_videoList->getListCache());
}

void VideoDialog::playTrailer()
{
    Metadata *metadata = GetMetadata(GetItemCurrent());
    if (!metadata) return;

    VideoPlayerCommand::PlayerFor(metadata->GetTrailer()).Play();
}

void VideoDialog::setParentalLevel(const ParentalLevel::Level &level)
{
    m_d->m_parentalLevel.SetLevel(level);
}

void VideoDialog::shiftParental(int amount)
{
    setParentalLevel(ParentalLevel(m_d->m_parentalLevel.GetLevel()
                    .GetLevel() + amount).GetLevel());
}

void VideoDialog::ChangeFilter()
{
    MythScreenStack *mainStack = GetScreenStack();

    VideoFilterDialog *filterdialog = new VideoFilterDialog(mainStack,
            "videodialogfilters", m_d->m_videoList.get());

    if (filterdialog->Create())
        mainStack->AddScreen(filterdialog);

    connect(filterdialog, SIGNAL(filterChanged()), SLOT(reloadData()));
}

Metadata *VideoDialog::GetMetadata(MythUIButtonListItem *item)
{
    Metadata *metadata = NULL;

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
    if (levent->type() == kMythDialogBoxCompletionEventType)
    {
        m_menuPopup = NULL;
    }
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
            "mythvideoeditmetadata", metadata,
            m_d->m_videoList->getListCache());

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

    if (m_d->m_videoList->Delete(metadata->ID()))
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
        metadata->Reset();

        QString cover_file;
        if (GetLocalVideoPoster(metadata->InetRef(), metadata->Filename(),
                        QStringList(m_d->m_artDir), cover_file))
        {
            metadata->setCoverFile(cover_file);
        }

        metadata->updateDatabase();

        UpdateItem(item);
    }
}

// Copy video poster to appropriate directory and set the item's cover file.
// This is the start of an async operation that needs to always complete
// to OnVideoPosterSetDone.
void VideoDialog::StartVideoPosterSet(Metadata *metadata)
{
    //createBusyDialog(QObject::tr("Fetching poster for %1 (%2)")
    //                    .arg(metadata->InetRef())
    //                    .arg(metadata->Title()));
    QStringList search_dirs;
    search_dirs += m_d->m_artDir;

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
            QString fileprefix = m_d->m_artDir;

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
            m_d->AddCoverDownload(d);
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
        m_d->RemoveCoverDownload(dynamic_cast<CoverDownloadProxy *>
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

        m_d->AutomaticParentalAdjustment(metadata);

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
        metadata->Reset();
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
            metadata->Reset();
            metadata->updateDatabase();
            UpdateItem(GetItemCurrent());
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
    if (!m_d->m_scanner)
        m_d->m_scanner = new VideoScanner();
    connect(m_d->m_scanner, SIGNAL(finished()), SLOT(reloadData()));
    m_d->m_scanner->doScan(GetVideoDirs());
}

#include "videodlg.moc"
