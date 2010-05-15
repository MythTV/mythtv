#include <set>
#include <map>

#include <QApplication>
#include <QTimer>
#include <QList>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QHttp>
#include <QBuffer>
#include <QUrl>
#include <QImage>
#include <QImageReader>

#include <mythcontext.h>
#include <compat.h>
#include <mythdirs.h>

#include <mythuihelper.h>
#include <mythprogressdialog.h>
#include <mythuitext.h>
#include <mythuibutton.h>
#include <mythuibuttonlist.h>
#include <mythuibuttontree.h>
#include <mythuiimage.h>
#include <mythuistatetype.h>
#include <mythdialogbox.h>
#include <mythgenerictree.h>
#include <mythsystem.h>
#include <remotefile.h>
#include <remoteutil.h>
#include <storagegroup.h>

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
#include "fileassoc.h"
#include "playersettings.h"
#include "metadatasettings.h"

namespace
{
    bool IsValidDialogType(int num)
    {
        for (int i = 1; i <= VideoDialog::dtLast - 1; i <<= 1)
            if (num == i) return true;
        return false;
    }

    class ImageDownloadProxy : public QObject
    {
        Q_OBJECT

      signals:
        void SigFinished(ImageDownloadErrorState reason, QString errorMsg,
                         Metadata *item, const QString &);
      public:
        static ImageDownloadProxy *Create(const QUrl &url, const QString &dest,
                                          Metadata *item,
                                          const QString &db_value)
        {
            return new ImageDownloadProxy(url, dest, item, db_value);
        }

      public:
        void StartCopy()
        {
            m_id = m_http.get(m_url.toEncoded(), &m_data_buffer);

            m_timer.start(gCoreContext->GetNumSetting("PosterDownloadTimeout", 60)
                          * 1000);
        }

        void Stop()
        {
            if (m_timer.isActive())
                m_timer.stop();

            m_http.abort();
        }

      public slots:
        void InspectHeader(const QHttpResponseHeader &header)
        {
            if (header.statusCode() == 302)
            {
                QString m_redirectUrl = header.value("Location");
                m_redirectCount++;
            }
            else if (header.statusCode() == 404)
            {
                VERBOSE(VB_IMPORTANT, QString("404 error received when "
                                              "retrieving '%1'")
                                                    .arg(m_url.toString()));
            }
            else
                m_redirectUrl.clear();
        }

      private:
        ImageDownloadProxy(const QUrl &url, const QString &dest,
                           Metadata *item, const QString &db_value)
          : m_item(item), m_dest_file(dest), m_db_value(db_value),
            m_id(0), m_url(url), m_error_state(esOK), m_redirectCount(0)
        {
            connect(&m_http,
                    SIGNAL(responseHeaderReceived(const QHttpResponseHeader &)),
                    SLOT(InspectHeader(const QHttpResponseHeader &)));
            connect(&m_http, SIGNAL(requestFinished(int, bool)),
                    SLOT(OnFinished(int, bool)));

            connect(&m_timer, SIGNAL(timeout()), SLOT(OnDownloadTimeout()));
            
            m_timer.setSingleShot(true);
            m_http.setHost(m_url.host());
        }

        ~ImageDownloadProxy() {}

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
            if (!m_redirectUrl.isEmpty() && m_redirectCount <= 8)
            {
                m_url.setUrl(m_redirectUrl);
                m_data_buffer.reset();
                StartCopy();
                return;
            }

            QString errorMsg;
            if (error)
                errorMsg = m_http.errorString();

            if (id == m_id)
            {
                if (m_timer.isActive())
                    m_timer.stop();

                if (!error)
                {
                    if (m_dest_file.startsWith("myth://"))
                    {
                        QImage testImage;
                        const QByteArray &testArray = m_data_buffer.data();
                        bool didLoad = testImage.loadFromData(testArray);
                        if (!didLoad)
                        {
                            errorMsg = tr("Tried to write %1, but it appears to "
                                          "be an HTML redirect (filesize %2).")
                                    .arg(m_dest_file).arg(m_data_buffer.size());
                            m_error_state = esError;
                        }
                        else
                        {
                            RemoteFile *outFile = new RemoteFile(m_dest_file, true);
                            if (!outFile->isOpen())
                            {
                                VERBOSE(VB_IMPORTANT,
                                    QString("VideoDialog: Failed to open "
                                            "remote file (%1) for write.  Does Coverart "
                                            "Storage Group Exist?").arg(m_dest_file));
                                delete outFile;
                                outFile = NULL;
                                m_error_state = esError;
                            }
                            else
                            {
                                off_t written = outFile->Write(m_data_buffer.data(), m_data_buffer.size());
                                if (written != m_data_buffer.size())
                                {
                                    errorMsg = tr("Error writing image to file %1.")
                                            .arg(m_dest_file);
                                    m_error_state = esError;
                                }

                                delete outFile;
                            }
                        }
                    }
                    else
                    {
                        QFile dest_file(m_dest_file);
                        if (dest_file.exists())
                            dest_file.remove();

                        if (dest_file.open(QIODevice::WriteOnly))
                        {
                            QImage testImage;
                            const QByteArray &testArray = m_data_buffer.data();
                            bool didLoad = testImage.loadFromData(testArray);
                            if (!didLoad)
                            {
                                errorMsg = tr("Tried to write %1, but it appears to "
                                              "be an HTML redirect (filesize %2).")
                                        .arg(m_dest_file).arg(m_data_buffer.size());
                                dest_file.remove();
                                m_error_state = esError;
                            }
                            else
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
                        }
                        else
                        {
                            errorMsg = tr("Error: file error '%1' for file %2").
                                    arg(dest_file.errorString()).arg(m_dest_file);
                            m_error_state = esError;
                        }
                    }
                }

                emit SigFinished(m_error_state, errorMsg, m_item, m_db_value);
            }
        }

      private:
        Metadata *m_item;
        QHttp m_http;
        QBuffer m_data_buffer;
        QString m_dest_file;
        QString m_db_value;
        int m_id;
        QTimer m_timer;
        QUrl m_url;
        ImageDownloadErrorState m_error_state;
        QString m_redirectUrl;
        int m_redirectCount;
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
                popupStack->AddScreen(okPopup);
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
        void SigSearchResults(bool normal_exit, const QStringList &items,
                Metadata *item);

      public:
        VideoTitleSearch(QObject *oparent) :
            ExecuteExternalCommand(oparent), m_item(0) {}

        void Run(QString title, Metadata *item)
        {
            m_item = item;
            int m_season, m_episode; 
            QString cmd;
            m_season = m_item->GetSeason();
            m_episode = m_item->GetEpisode();

            if (m_season > 0 || m_episode > 0)
            {
                const QString def_cmd = QDir::cleanPath(QString("%1/%2")
                    .arg(GetShareDir())
                    .arg("mythvideo/scripts/Television/ttvdb.py"));

                cmd = gCoreContext->GetSetting("mythvideo.TVGrabber",
                                                        def_cmd);
                cmd.append(QString(" -l %1 -M").arg(GetMythUI()->GetLanguage()));
            }
            else
            {
                QString def_cmd = QDir::cleanPath(QString("%1/%2")
                    .arg(GetShareDir())
                    .arg("mythvideo/scripts/Movie/tmdb.py"));

                cmd = gCoreContext->GetSetting("mythvideo.MovieGrabber", def_cmd);
                cmd.append(QString(" -l %1 -M").arg(GetMythUI()->GetLanguage()));
            }
                QStringList args;
                args += title;
                StartRun(cmd, args, "Video Search");
        }

      private:
        ~VideoTitleSearch() {}

        void OnExecDone(bool normal_exit, QStringList out, QStringList err)
        {
            (void) err;

            emit SigSearchResults(normal_exit, out, m_item);
            deleteLater();
        }

      private:
        Metadata *m_item;
    };

    /** \class VideoTitleSubtitleSearch
     *
     * \brief Executes the external command to do video title/subtitle searches.
     *
     */
    class VideoTitleSubtitleSearch : public ExecuteExternalCommand
    {
        Q_OBJECT

      signals:
        void SigSearchResults(bool normal_exit, QStringList result,
                Metadata *item);

      public:
        VideoTitleSubtitleSearch(QObject *oparent) :
            ExecuteExternalCommand(oparent), m_item(0) {}

        void Run(QString title, QString subtitle, Metadata *item)
        {
            m_item = item;
            QString cmd;

                const QString def_cmd = QDir::cleanPath(QString("%1/%2")
                    .arg(GetShareDir())
                    .arg("mythvideo/scripts/Television/ttvdb.py"));
                cmd = gCoreContext->GetSetting("mythvideo.TVGrabber",
                                                        def_cmd);
                cmd.append(" -N");
                QStringList args;
                args += title;
                args += subtitle;
                StartRun(cmd, args, "Video Search");
        }

      private:
        ~VideoTitleSubtitleSearch() {}

        void OnExecDone(bool normal_exit, QStringList out, QStringList err)
        {
            (void) err;

            emit SigSearchResults(normal_exit, out, m_item);
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
            int m_season, m_episode;
            m_season = m_item->GetSeason();
            m_episode = m_item->GetEpisode();

            if (m_season > 0 || m_episode > 0)
            {
                const QString def_cmd = QDir::cleanPath(QString("%1/%2")
                    .arg(GetShareDir())
                    .arg("mythvideo/scripts/Television/ttvdb.py"));
                QString cmd = gCoreContext->GetSetting("mythvideo.TVGrabber",
                                                        def_cmd);
                cmd.append(QString(" -l %1 -D").arg(GetMythUI()->GetLanguage()));
                QStringList args;
                args << video_uid << QString::number(m_season) 
                                  << QString::number(m_episode);
                StartRun(cmd, args, "Video Data Query");
            }
            else
            {
                const QString def_cmd = QDir::cleanPath(QString("%1/%2")
                    .arg(GetShareDir())
                    .arg("mythvideo/scripts/Movie/tmdb.py"));
                QString cmd = gCoreContext->GetSetting("mythvideo.MovieGrabber",
                                                        def_cmd);
                cmd.append(QString(" -l %1 -D").arg(GetMythUI()->GetLanguage()));
                StartRun(cmd, QStringList(video_uid), "Video Data Query");
            }
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
                const QStringList &results) :
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

            for (QStringList::const_iterator i = m_results.begin();
                    i != m_results.end(); ++i)
            {
                QString key = ((*i).left((*i).indexOf(':')));
                QString value = ((*i).right((*i).length() - (*i).indexOf(":") - 1));
                MythUIButtonListItem *button = new MythUIButtonListItem(m_resultsList, value);
                VERBOSE(VB_GENERAL|VB_EXTRA, QString("Inserting into ButtonList: %1:%2").arg(key).arg(value));
                button->SetData(key);
            }

            connect(m_resultsList, SIGNAL(itemClicked(MythUIButtonListItem *)),
                    SLOT(sendResult(MythUIButtonListItem *)));

            BuildFocusList();

            return true;
        }

     signals:
        void haveResult(QString);

      private:
        QStringList m_results;
        MythUIButtonList *m_resultsList;

      private slots:
        void sendResult(MythUIButtonListItem* item)
        {
            emit haveResult(item->GetData().toString());
            Close();
        }
    };

    bool GetLocalVideoImage(const QString &video_uid, const QString &filename,
                             const QStringList &in_dirs, QString &image,
                             const QString &title, int season, const QString host, 
                             QString sgroup, int episode = 0,
                             bool isScreenshot = false)
    {
        QStringList search_dirs(in_dirs);
        QFileInfo qfi(filename);
        search_dirs += qfi.absolutePath();

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
                sfn += fntm.arg(*dir).arg(QString(base_name + "_%1").arg(suffix)).arg(*ext);
                sfn += fntm.arg(*dir).arg(QString(video_uid + "_%1").arg(suffix)).arg(*ext);
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
            const MetadataListManager &video_list, bool useAltPlayer = false)
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

            if (useAltPlayer)
                VideoPlayerCommand::AltPlayerFor(item.get()).Play();
            else
                VideoPlayerCommand::PlayerFor(item.get()).Play();

            if (item->GetChildID() > 0)
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
        FanartLoader() : itemsPast(0)
        {
            connect(&m_fanartTimer, SIGNAL(timeout()), SLOT(fanartLoad()));
        }
        
       ~FanartLoader()
        {
            m_fanartTimer.disconnect(this);
        }

        void LoadImage(const QString &filename, MythUIImage *image)
        {
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
                    if (filename.size())
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

    void CopyMetadataToUI(const Metadata *metadata,
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

        MetadataMap metadataMap;
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
            bool handled = GetMythMainWindow()->TranslateKeyPress("Video", levent, 
                           actions);

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

        m_artDir = gCoreContext->GetSetting("VideoArtworkDir");
        m_sshotDir = gCoreContext->GetSetting("mythvideo.screenshotDir");
        m_fanDir = gCoreContext->GetSetting("mythvideo.fanartDir");
        m_banDir = gCoreContext->GetSetting("mythvideo.bannerDir");
    }

    ~VideoDialogPrivate()
    {
        delete m_scanner;
        StopAllRunningImageDownloads();

        if (m_rememberPosition && m_lastTreeNodePath.length())
        {
            gCoreContext->SaveSetting("mythvideo.VideoTreeLastActive",
                    m_lastTreeNodePath);
        }
    }

    void AutomaticParentalAdjustment(Metadata *metadata)
    {
        if (metadata && m_rating_to_pl.size())
        {
            QString rating = metadata->GetRating();
            for (parental_level_map::const_iterator p = m_rating_to_pl.begin();
                    rating.length() && p != m_rating_to_pl.end(); ++p)
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

    void AddImageDownload(ImageDownloadProxy *download)
    {
        m_running_downloads.insert(download);
    }

    void RemoveImageDownload(ImageDownloadProxy *download)
    {
        if (download)
        {
            image_download_list::iterator p =
                    m_running_downloads.find(download);
            if (p != m_running_downloads.end())
                m_running_downloads.erase(p);
        }
    }

    void StopAllRunningImageDownloads()
    {
        image_download_list tmp(m_running_downloads);
        for (image_download_list::iterator p = tmp.begin(); p != tmp.end(); ++p)
            (*p)->Stop();
    }

  public:
    typedef std::set<ImageDownloadProxy *> image_download_list;
    image_download_list m_running_downloads;
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
    bool m_isGroupList;
    int  m_groupType;
    bool m_isFlatList;
    bool m_altPlayerEnabled;
    VideoDialog::DialogType m_type;
    VideoDialog::BrowseType m_browse;

    QString m_artDir;
    QString m_sshotDir;
    QString m_fanDir;
    QString m_banDir;
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
        VideoListPtr video_list, DialogType type, BrowseType browse) :
    MythScreenType(lparent, lname), m_menuPopup(0), m_busyPopup(0),
    m_videoButtonList(0), m_videoButtonTree(0), m_titleText(0),
    m_novideoText(0), m_positionText(0), m_crumbText(0), m_coverImage(0),
    m_screenshot(0), m_banner(0), m_fanart(0), m_trailerState(0), 
    m_parentalLevelState(0), m_watchedState(0)
{
    m_d = new VideoDialogPrivate(video_list, type, browse);

    m_popupStack = GetMythMainWindow()->GetStack("popup stack");

    m_d->m_videoList->setCurrentVideoFilter(VideoFilterSettings(true,
                    lname));

    srand(time(NULL));

    StorageGroup::ClearGroupToUseCache();
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

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen '" + windowName + "'");
        return false;
    }

    CheckedSet(m_trailerState, "None");
    CheckedSet(m_parentalLevelState, "None");
    CheckedSet(m_watchedState, "None");

    BuildFocusList();

    CheckedSet(m_novideoText, tr("Video dialog loading, or no videos available..."));

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

    connect(&m_d->m_parentalLevel, SIGNAL(SigLevelChanged()),
            SLOT(reloadData()));

    return true;
}

void VideoDialog::Init()
{

    m_d->m_parentalLevel.SetLevel(ParentalLevel(gCoreContext->
                    GetNumSetting("VideoDefaultParentalLevel",
                            ParentalLevel::plLowest)));
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

    if (m_novideoText)
        m_novideoText->SetVisible(!m_d->m_treeLoaded);
}

void VideoDialog::reloadAllData(bool dbChanged)
{
    delete m_d->m_scanner;
    m_d->m_scanner = 0;

    if (dbChanged) {
        m_d->m_videoList->InvalidateCache();
    }
    reloadData();
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

    Metadata *metadata = GetMetadata(item);

    if (metadata)
    {
        MetadataMap metadataMap;
        metadata->toMap(metadataMap);
        item->SetTextFromMap(metadataMap);
    }

    MythUIButtonListItemCopyDest dest(item);
    CopyMetadataToUI(metadata, dest);

    MythGenericTree *parent = node->getParent();

    if (parent && metadata && ((QString::compare(parent->getString(),
                            metadata->GetTitle(), Qt::CaseInsensitive) == 0) ||
                            parent->getString().startsWith(tr("Season"), Qt::CaseInsensitive)))
        item->SetText(metadata->GetSubtitle());
    else
        item->SetText(metadata ? metadata->GetTitle() : node->getString());

    QString coverimage = GetCoverImage(node);
    QString screenshot = GetScreenshot(node);
    QString banner     = GetBanner(node);
    QString fanart     = GetFanart(node);

    if (!screenshot.isEmpty() && parent && metadata &&
        ((QString::compare(parent->getString(),
                            metadata->GetTitle(), Qt::CaseInsensitive) == 0) ||
            parent->getString().startsWith(tr("Season"), Qt::CaseInsensitive)))
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
        item->SetText(node->getString(), "title");
        item->SetText(node->getString());
    }
    else if (nodeInt == kUpFolder)
    {
        item->DisplayState("upfolder", "nodetype");
        item->SetText(node->getString(), "title");
        item->SetText(node->getString());
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

/** \fn VideoDialog::RemoteImageCheck(QString host, QString filename)
 *  \brief Search for a given (image) filename in the Video SG. 
 *  \return A QString of the full myth:// URL to a matching image.
 */
QString VideoDialog::RemoteImageCheck(QString host, QString filename)
{
    QString result = "";
    //VERBOSE(VB_GENERAL, QString("RemoteImageCheck(%1)").arg(filename));

    QStringList dirs = GetVideoDirsByHost(host);

    if (dirs.size() > 0)
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
                VERBOSE(VB_GENERAL, QString("Backend : %1 currently Unreachable. Skipping this one.")
                                    .arg(host));
                break;
            }

            if ((list.size() > 0) && (list.at(0) == fname))
                result = generate_file_url("Videos", host, filename);

            if (!result.isEmpty())
            {
            //    VERBOSE(VB_GENERAL, QString("RemoteImageCheck(%1) res :%2: :%3:")
            //                        .arg(fname).arg(result).arg(*iter));
                break;
            }

        }
    }

    return result;
}

/** \fn VideoDialog::GetImageFromFolder(Metadata *metadata)
 *  \brief Attempt to find/fallback a cover image for a given metadata item.
 *  \return QString local or myth:// for the first found cover file.
 */
QString VideoDialog::GetImageFromFolder(Metadata *metadata)
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

            if (dirs.size() > 0)
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
                            if (matches.size() > 0)
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
        VERBOSE(VB_GENERAL|VB_EXTRA, QString("Found Image : %1 :")
                                    .arg(icon_file));
    else
        VERBOSE(VB_GENERAL|VB_EXTRA,
                QString("Could not find cover Image : %1 ")
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

        // VERBOSE(VB_GENERAL, QString("GetCoverImage host : %1  prefix : %2 file : %3")
        //                            .arg(host).arg(prefix).arg(filename));

        QStringList test_files;
        test_files.append(filename + ".png");
        test_files.append(filename + ".jpg");
        test_files.append(filename + ".gif");
        bool foundCover;

        for (QStringList::const_iterator tfp = test_files.begin();
                tfp != test_files.end(); ++tfp)
        {
            QString imagePath = *tfp;
            //VERBOSE(VB_GENERAL, QString("Cover check :%1 : ").arg(*tfp));

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

                if (dirs.size() > 0)
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
                                if (matches.size() > 0)
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
                            Metadata *metadata = GetMetadataPtrFromNode(subnode);
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
            VERBOSE(VB_GENERAL|VB_EXTRA, QString("Found Image : %1 :")
                                        .arg(icon_file));
        else
            VERBOSE(VB_GENERAL|VB_EXTRA,
                    QString("Could not find folder cover Image : %1 ")
                    .arg(folder_path));
    }
    else
    {
        const Metadata *metadata = GetMetadataPtrFromNode(node);

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
                
                Metadata *metadata = GetMetadataPtrFromNode(subnode);
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
                                     node->getString(), levels + 1);
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
        const Metadata *metadata = GetMetadataPtrFromNode(node);

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
    const Metadata *metadata = GetMetadataPtrFromNode(node);

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
    const Metadata *metadata = GetMetadataPtrFromNode(node);

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
void VideoDialog::createBusyDialog(QString title)
{
    if (m_busyPopup)
        return;

    QString message = title;

    m_busyPopup = new MythUIBusyDialog(message, m_popupStack,
            "mythvideobusydialog");

    if (m_busyPopup->Create())
        m_popupStack->AddScreen(m_busyPopup);
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

/** \fn VideoDialog::searchComplet(QString string)
 *  \brief After using incremental search, move to the selected item.
 *  \return void.
 */
void VideoDialog::searchComplete(QString string)
{
    VERBOSE(VB_GENERAL | VB_EXTRA,
            QString("Jumping to: %1").arg(string));

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
        QString title = child->getString();
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
        childList << child->getString();
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

    CheckedSet(m_positionText, QString(tr("%1 of %2"))
            .arg(currentList->GetCurrentPos() + 1)
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

    Metadata *metadata = GetMetadata(item);

    MythGenericTree *node = GetNodePtrFromButton(item);

    if (metadata)
    {
        MetadataMap metadataMap;
        metadata->toMap(metadataMap);
        SetTextFromMap(metadataMap);
    }
    else
    {
        MetadataMap metadataMap;
        ClearMap(metadataMap);
        SetTextFromMap(metadataMap);
    }

    ScreenCopyDest dest(this);
    CopyMetadataToUI(metadata, dest);

    if (node && node->getInt() == kSubFolder && !metadata)
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
        CheckedSet(m_crumbText, m_d->m_currentNode->getRouteByString().join(" > "));

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
    Metadata *metadata = GetMetadata(GetItemCurrent());
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

    m_menuPopup = new MythDialogBox(label, m_popupStack, "videomenupopup");

    if (m_menuPopup->Create())
        m_popupStack->AddScreen(m_menuPopup);

    m_menuPopup->SetReturnEvent(this, "actions");

    MythUIButtonListItem *item = GetItemCurrent();
    MythGenericTree *node = GetNodePtrFromButton(item);
    if (node && node->getInt() >= 0)
    {
        if (!metadata->GetTrailer().isEmpty() ||
                gCoreContext->GetNumSetting("mythvideo.TrailersRandomEnabled", 0) ||
                m_d->m_altPlayerEnabled)
            m_menuPopup->AddButton(tr("Play..."), SLOT(PlayMenu()), true);
        else
            m_menuPopup->AddButton(tr("Play"), SLOT(playVideo()));
        if (metadata->GetWatched())
            m_menuPopup->AddButton(tr("Mark as Unwatched"), SLOT(ToggleWatched()));
        else
            m_menuPopup->AddButton(tr("Mark as Watched"), SLOT(ToggleWatched()));
        m_menuPopup->AddButton(tr("Video Info"), SLOT(InfoMenu()), true);
        m_menuPopup->AddButton(tr("Metadata Options"), SLOT(ManageMenu()), true);
        m_menuPopup->AddButton(tr("Video Options"), SLOT(VideoOptionMenu()), true);
        m_menuPopup->AddButton(tr("Delete"), SLOT(RemoveVideo()));
    }
    if (node && !(node->getInt() >= 0) && node->getInt() != kUpFolder)
        m_menuPopup->AddButton(tr("Play Folder"), SLOT(playFolder()));
}

/** \fn VideoDialog::PlayMenu()
 *  \brief Pop up a MythUI "Play Menu" for MythVideo.  Appears if multiple play 
 *         options exist.
 *  \return void.
 */
void VideoDialog::PlayMenu()
{
    Metadata *metadata = GetMetadata(GetItemCurrent());
    QString label;

    if (metadata)
        label = tr("Playback Options\n%1").arg(metadata->GetTitle());
    else
        return;

    m_menuPopup = new MythDialogBox(label, m_popupStack, "play");

    if (m_menuPopup->Create())
        m_popupStack->AddScreen(m_menuPopup);

    m_menuPopup->SetReturnEvent(this, "actions");

    m_menuPopup->AddButton(tr("Play"), SLOT(playVideo()));

    if (m_d->m_altPlayerEnabled)
    {
        m_menuPopup->AddButton(tr("Play in Alternate Player"), SLOT(playVideoAlt()));
    } 

    if (gCoreContext->GetNumSetting("mythvideo.TrailersRandomEnabled", 0))
    {
         m_menuPopup->AddButton(tr("Play With Trailers"),
              SLOT(playVideoWithTrailers()));
    }

    QString trailerFile = metadata->GetTrailer();
    if (QFile::exists(trailerFile) ||
        (!metadata->GetHost().isEmpty() && !trailerFile.isEmpty()))
    {
        m_menuPopup->AddButton(tr("Play Trailer"),
                        SLOT(playTrailer()));
    }
}

/** \fn VideoDialog::DisplayMenu()
 *  \brief Pop up a MythUI Menu for MythVideo Global Functions.  Bound to MENU.
 *  \return void.
 */
void VideoDialog::DisplayMenu()
{
    QString label = tr("Video Display Menu");

    m_menuPopup = new MythDialogBox(label, m_popupStack, "videomenupopup");

    if (m_menuPopup->Create())
        m_popupStack->AddScreen(m_menuPopup);

    m_menuPopup->SetReturnEvent(this, "display");

    m_menuPopup->AddButton(tr("Scan For Changes"), SLOT(doVideoScan()));
    m_menuPopup->AddButton(tr("Filter Display"), SLOT(ChangeFilter()));

    m_menuPopup->AddButton(tr("Browse By..."), SLOT(MetadataBrowseMenu()), true);

    m_menuPopup->AddButton(tr("Change View"), SLOT(ViewMenu()), true);

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

    m_menuPopup->AddButton(tr("Settings"), SLOT(SettingsMenu()), true);
}

/** \fn VideoDialog::ViewMenu()
 *  \brief Pop up a MythUI Menu for MythVideo Views.
 *  \return void.
 */
void VideoDialog::ViewMenu()
{
    QString label = tr("Change View");

    m_menuPopup = new MythDialogBox(label, m_popupStack, "videomenupopup");

    if (m_menuPopup->Create())
        m_popupStack->AddScreen(m_menuPopup);

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
}

/** \fn VideoDialog::SettingsMenu()
 *  \brief Pop up a MythUI Menu for MythVideo Settings.
 *  \return void.
 */
void VideoDialog::SettingsMenu()
{
    QString label = tr("Video Settings");

    m_menuPopup = new MythDialogBox(label, m_popupStack, "videosettingspopup");

    if (m_menuPopup->Create())
        m_popupStack->AddScreen(m_menuPopup);

    m_menuPopup->SetReturnEvent(this, "view");

    m_menuPopup->AddButton(tr("Player Settings"), SLOT(ShowPlayerSettings()));
    m_menuPopup->AddButton(tr("Metadata Settings"), SLOT(ShowMetadataSettings()));
    m_menuPopup->AddButton(tr("File Type Settings"), SLOT(ShowExtensionSettings()));
}

/** \fn VideoDialog::ShowPlayerSettings()
 *  \brief Pop up a MythUI Menu for MythVideo Player Settings.
 *  \return void.
 */
void VideoDialog::ShowPlayerSettings()
{
    PlayerSettings *ps = new PlayerSettings(m_popupStack, "player settings");

    if (ps->Create())
        m_popupStack->AddScreen(ps);
    else
        delete ps;
}

/** \fn VideoDialog::ShowMetadataSettings()
 *  \brief Pop up a MythUI Menu for MythVideo Metadata Settings.
 *  \return void.
 */
void VideoDialog::ShowMetadataSettings()
{
    MetadataSettings *ms = new MetadataSettings(m_popupStack, "metadata settings");
    
    if (ms->Create())
        m_popupStack->AddScreen(ms);
    else
        delete ms;
}

/** \fn VideoDialog::ShowExtensionSettings()
 *  \brief Pop up a MythUI Menu for MythVideo filte Type Settings.
 *  \return void.
 */
void VideoDialog::ShowExtensionSettings()
{
    FileAssocDialog *fa = new FileAssocDialog(m_popupStack, "fa dialog");

    if (fa->Create())
        m_popupStack->AddScreen(fa);
    else
        delete fa;
}

/** \fn VideoDialog::MetadataBrowseMenu()
 *  \brief Pop up a MythUI Menu for MythVideo Metadata Browse modes.
 *  \return void.
 */
void VideoDialog::MetadataBrowseMenu()
{
    QString label = tr("Browse By");

    m_menuPopup = new MythDialogBox(label, m_popupStack, "videomenupopup");

    if (m_menuPopup->Create())
        m_popupStack->AddScreen(m_menuPopup);

    m_menuPopup->SetReturnEvent(this, "metadata");

    if (m_d->m_groupType != BRS_CAST)
        m_menuPopup->AddButton(tr("Cast"),
                  SLOT(SwitchVideoCastGroup()));

    if (m_d->m_groupType != BRS_CATEGORY)
        m_menuPopup->AddButton(tr("Category"),
                  SLOT(SwitchVideoCategoryGroup()));

    if (m_d->m_groupType != BRS_INSERTDATE)
        m_menuPopup->AddButton(tr("Date Added"),
                  SLOT(SwitchVideoInsertDateGroup()));

    if (m_d->m_groupType != BRS_DIRECTOR)  
        m_menuPopup->AddButton(tr("Director"),
                  SLOT(SwitchVideoDirectorGroup()));

    if (m_d->m_groupType != BRS_FOLDER)
        m_menuPopup->AddButton(tr("Folder"),
                 SLOT(SwitchVideoFolderGroup()));

    if (m_d->m_groupType != BRS_GENRE)
        m_menuPopup->AddButton(tr("Genre"),
                  SLOT(SwitchVideoGenreGroup()));

    if (m_d->m_groupType != BRS_TVMOVIE)
        m_menuPopup->AddButton(tr("TV/Movies"),
                  SLOT(SwitchVideoTVMovieGroup()));

    if (m_d->m_groupType != BRS_USERRATING)
        m_menuPopup->AddButton(tr("User Rating"),
                  SLOT(SwitchVideoUserRatingGroup()));

    if (m_d->m_groupType != BRS_YEAR)
        m_menuPopup->AddButton(tr("Year"),
                  SLOT(SwitchVideoYearGroup()));
}

/** \fn VideoDialog::InfoMenu()
 *  \brief Pop up a MythUI Menu for Info pertaining to the selected item.
 *  \return void.
 */
void VideoDialog::InfoMenu()
{
    QString label = tr("Video Info");

    m_menuPopup = new MythDialogBox(label, m_popupStack, "videomenupopup");

    if (m_menuPopup->Create())
        m_popupStack->AddScreen(m_menuPopup);

    m_menuPopup->SetReturnEvent(this, "info");

    if (ItemDetailPopup::Exists())
        m_menuPopup->AddButton(tr("View Details"), SLOT(DoItemDetailShow()));

    m_menuPopup->AddButton(tr("View Full Plot"), SLOT(ViewPlot()));

    Metadata *metadata = GetMetadata(GetItemCurrent());
    if (metadata)
    {
        if (metadata->GetCast().size())
            m_menuPopup->AddButton(tr("View Cast"), SLOT(ShowCastDialog()));
        if (!metadata->GetHomepage().isEmpty())
            m_menuPopup->AddButton(tr("View Homepage"), SLOT(ShowHomepage()));
    }
}

/** \fn VideoDialog::VideoOptionMenu()
 *  \brief Pop up a MythUI Menu to toggle watched/browseable.
 *  \return void.
 */
void VideoDialog::VideoOptionMenu()
{
    QString label = tr("Video Options");

    m_menuPopup = new MythDialogBox(label, m_popupStack, "videomenupopup");

    Metadata *metadata = GetMetadata(GetItemCurrent());

    if (m_menuPopup->Create())
        m_popupStack->AddScreen(m_menuPopup);

    m_menuPopup->SetReturnEvent(this, "option");

    if (metadata->GetWatched())
        m_menuPopup->AddButton(tr("Mark as Unwatched"), SLOT(ToggleWatched()));
    else
        m_menuPopup->AddButton(tr("Mark as Watched"), SLOT(ToggleWatched()));

    if (metadata->GetBrowse())
        m_menuPopup->AddButton(tr("Mark as Non-Browseable"), SLOT(ToggleBrowseable()));
    else
        m_menuPopup->AddButton(tr("Mark as Browseable"), SLOT(ToggleBrowseable()));
}

/** \fn VideoDialog::ManageMenu()
 *  \brief Pop up a MythUI Menu for metadata management.
 *  \return void.
 */
void VideoDialog::ManageMenu()
{
    QString label = tr("Manage Metadata");

    m_menuPopup = new MythDialogBox(label, m_popupStack, "videomenupopup");

    if (m_menuPopup->Create())
        m_popupStack->AddScreen(m_menuPopup);

    m_menuPopup->SetReturnEvent(this, "manage");

    m_menuPopup->AddButton(tr("Edit Metadata"), SLOT(EditMetadata()));
    m_menuPopup->AddButton(tr("Download Metadata"), SLOT(VideoSearch()));
    m_menuPopup->AddButton(tr("Search TV by Title/Subtitle"),
                          SLOT(TitleSubtitleSearch()));
    m_menuPopup->AddButton(tr("Manually Enter Video #"),
            SLOT(ManualVideoUID()));
    m_menuPopup->AddButton(tr("Manually Enter Video Title"),
            SLOT(ManualVideoTitle()));
    m_menuPopup->AddButton(tr("Reset Metadata"), SLOT(ResetMetadata()));
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
    Metadata *metadata = GetMetadata(GetItemCurrent());

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
    Metadata *metadata = GetMetadata(GetItemCurrent());

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
    Metadata *metadata = GetMetadata(GetItemCurrent());

    CastDialog *castdialog = new CastDialog(m_popupStack, metadata);

    if (castdialog->Create())
        m_popupStack->AddScreen(castdialog);
}

void VideoDialog::ShowHomepage()
{
    Metadata *metadata = GetMetadata(GetItemCurrent());

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
        myth_system(cmd, MYTH_SYSTEM_DONT_BLOCK_PARENT);
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
    Metadata *metadata = GetMetadata(GetItemCurrent());
    if (metadata)
        PlayVideo(metadata->GetFilename(), m_d->m_videoList->getListCache());
}

/** \fn VideoDialog::playVideoAlt()
 *  \brief Play the selected item in an alternate player.
 *  \return void.
 */
void VideoDialog::playVideoAlt()
{
    Metadata *metadata = GetMetadata(GetItemCurrent());
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
                Metadata *metadata = GetMetadataPtrFromNode(subnode);
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
    Metadata *metadata = GetMetadata(GetItemCurrent());
    if (!metadata) return;

    QStringList trailers = GetTrailersInDirectory(gCoreContext->
            GetSetting("mythvideo.TrailersDir"));

    if (trailers.isEmpty())
        return;

    const int trailersToPlay =
            gCoreContext->GetNumSetting("mythvideo.TrailersRandomCount");

    int i = 0;
    while (trailers.size() && i < trailersToPlay)
    {
        ++i;
        QString trailer = trailers.takeAt(rand() % trailers.size());

        VERBOSE(VB_GENERAL | VB_EXTRA,
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
    Metadata *metadata = GetMetadata(GetItemCurrent());
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
    if (levent->type() == DialogCompletionEvent::kEventType)
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

MythUIButtonListItem *VideoDialog::GetItemByMetadata(Metadata *metadata)
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
            Metadata *listmeta = 
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

void VideoDialog::VideoSearch() {
    Metadata *metadata = GetMetadata(GetItemCurrent());

    if (metadata)
        StartVideoSearchByTitle(metadata->GetInetRef(), metadata->GetTitle(),
                                metadata);
}

void VideoDialog::TitleSubtitleSearch()
{
    Metadata *metadata = GetMetadata(GetItemCurrent());

    if (metadata)
        StartVideoSearchByTitleSubtitle(metadata->GetTitle(),
                                metadata->GetSubtitle(), metadata);
}

void VideoDialog::ToggleBrowseable()
{
    Metadata *metadata = GetMetadata(GetItemCurrent());
    if (metadata)
    {
        metadata->SetBrowse(!metadata->GetBrowse());
        metadata->UpdateDatabase();

        refreshData();
    }
}

void VideoDialog::ToggleWatched()
{
    Metadata *metadata = GetMetadata(GetItemCurrent());
    if (metadata)
    {
        metadata->SetWatched(!metadata->GetWatched());
        metadata->UpdateDatabase();

        refreshData();
    }
}

// void VideoDialog::OnVideoSearchListCancel()
// {
//     // I'm keeping this behavior for now, though item
//     // modification on Cancel is seems anathema to me.
//     Metadata *item = GetItemCurrent();
//
//     if (item && isDefaultCoverFile(item->GetCoverFile()))
//     {
//         QStringList search_dirs;
//         search_dirs += m_artDir;
//         QString cover_file;
//
//         if (GetLocalVideoPoster(item->InetRef(), item->GetFilename(),
//                                 search_dirs, cover_file))
//         {
//             item->SetCoverFile(cover_file);
//             item->UpdateDatabase();
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

void VideoDialog::ManualVideoUID()
{
    QString message = tr("Enter Video Unique ID:");

    MythTextInputDialog *searchdialog =
                                new MythTextInputDialog(m_popupStack, message);

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
                                new MythTextInputDialog(m_popupStack, message);

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

    QString message = tr("Are you sure you want to delete:\n%1")
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

    Metadata *metadata = GetMetadata(item);

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
    Metadata *metadata = GetMetadata(item);

    if (metadata)
    {
        metadata->Reset();
        QString inetref = metadata->GetInetRef();
        QString filename = metadata->GetFilename();
        QString title = metadata->GetTitle();
        int season = metadata->GetSeason();
        QString host = metadata->GetHost();
        int episode = metadata->GetEpisode();

        QString cover_file;
        if (GetLocalVideoImage(inetref, filename,
                        QStringList(m_d->m_artDir), cover_file,
                        title, season, host, "Coverart", episode))
        {
            metadata->SetCoverFile(cover_file);
        }

        QString screenshot_file;
        if (GetLocalVideoImage(inetref, filename,
                        QStringList(m_d->m_sshotDir), screenshot_file,
                        title, season, host, "Screenshots", episode,
                        true))
        {   
            metadata->SetScreenshot(screenshot_file);
        }


        QString fanart_file;
        if (GetLocalVideoImage(inetref, filename,
                        QStringList(m_d->m_fanDir), fanart_file,
                        title, season, host, "Fanart", episode))
        {
            metadata->SetFanart(fanart_file);
        }

        QString banner_file;
        if (GetLocalVideoImage(inetref, filename,
                        QStringList(m_d->m_banDir), banner_file,
                        title, season, host, "Banners", episode))
        {
            metadata->SetBanner(banner_file);
        }

        metadata->UpdateDatabase();

        UpdateItem(item);
    }
}

// Copy video images to appropriate directory and set the item's image files.
// This is the start of an async operation that needs to always complete
// to OnVideo*SetDone.
void VideoDialog::StartVideoImageSet(Metadata *metadata, QStringList coverart,
                                     QStringList fanart, QStringList banner,
                                     QStringList screenshot)
{
    //createBusyDialog(QObject::tr("Fetching poster for %1 (%2)")
    //                    .arg(metadata->InetRef())
    //                    .arg(metadata->Title()));
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

        if (!coverart.isEmpty() && (cover_file.isEmpty() ||
            IsDefaultCoverFile(cover_file)))
        {
            OnImageURL(coverart.takeAt(0).trimmed(), metadata, "Coverart");
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

        if (!fanart.isEmpty() && metadata->GetFanart().isEmpty())
        {
            if (metadata->GetSeason() >= 1 && fanart.count() >= metadata->GetSeason())
                OnImageURL(fanart.takeAt(metadata->GetSeason() - 1), metadata,
                           "Fanart");
            else
                OnImageURL(fanart.takeAt(0).trimmed(), metadata, "Fanart");
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

        if (!banner.isEmpty() && metadata->GetBanner().isEmpty())
        {
            OnImageURL(banner.takeAt(0).trimmed(), metadata, "Banners");
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

        if (!screenshot.isEmpty() && metadata->GetScreenshot().isEmpty())
        {
            OnImageURL(screenshot.takeAt(0).trimmed(), metadata, "Screenshots");
        }
    }
}

void VideoDialog::OnImageURL(QString uri, Metadata *metadata, QString type)
{
    if (metadata)
    {
        if (uri.length())
        {
            QString fileprefix;
            QString suffix;
            QString host = metadata->GetHost();
            int season = metadata->GetSeason();
            int episode = metadata->GetEpisode();

            if (type == "Coverart")
            {
                fileprefix = m_d->m_artDir;
                suffix = QString("coverart");
            }
            if (type == "Fanart")
            {
                fileprefix = m_d->m_fanDir;
                suffix = QString("fanart");
            }
            if (type == "Banners")
            {
                fileprefix = m_d->m_banDir;
                suffix = QString("banner");
            }
            if (type == "Screenshots")
            {
                fileprefix = m_d->m_sshotDir;
                suffix = QString("screenshot");
            }

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
            QString dest_file;
            QString db_value;

            if (season > 0 || episode > 0)
            {
                QString title;

                // Name TV downloads so that they already work with the PBB

                if (type == "Screenshots")
                    title = QString("%1 Season %2x%3_%4").arg(metadata->GetTitle())
                            .arg(season).arg(episode).arg(suffix);
                else
                    title = QString("%1 Season %2_%3").arg(metadata->GetTitle())
                            .arg(season).arg(suffix);

                title.remove('?');
                title.remove('<');
                title.remove('>');
                title.remove('/');
                title.remove('\\');
                title.remove('|');
                title.remove('*');
                title.remove('[');
                title.remove(']');
                title.remove(':');
                title.remove('"');
                title.remove('^');

                if (!host.isEmpty())
                {
                    QString combFileName = QString("%1.%2").arg(title)
                                                    .arg(ext);
                    dest_file = generate_file_url(type, host,
                        combFileName);
                    db_value = combFileName;
                }
                else
                {
                    dest_file = QString("%1/%2.%3").arg(fileprefix)
                            .arg(title).arg(ext);
                    db_value = dest_file;
                }
            }
            else
            {
                if (!host.isEmpty())
                {
                    QString combFileName = QString("%1_%2.%3")
                                           .arg(metadata->GetInetRef())
                                           .arg(suffix).arg(ext);
                    dest_file = generate_file_url(type, host,
                        combFileName);
                    db_value = combFileName;
                }
                else
                {
                    dest_file = QString("%1/%2_%3.%4").arg(fileprefix)
                            .arg(metadata->GetInetRef()).arg(suffix).arg(ext);
                    db_value = dest_file;
                }
            }

            VERBOSE(VB_IMPORTANT, QString("Copying '%1' -> '%2'...")
                    .arg(url.toString()).arg(dest_file));

            ImageDownloadProxy *id =
                    ImageDownloadProxy::Create(url, dest_file, metadata,
                                               db_value);

            connect(id, SIGNAL(SigFinished(ImageDownloadErrorState,
                                           QString, Metadata *,
                                           const QString &)),
                    SLOT(OnImageCopyFinished(ImageDownloadErrorState,
                                              QString, Metadata *,
                                              const QString &)));

            id->StartCopy();
            m_d->AddImageDownload(id);
        }
        else
        {
            if (type == "Coverart")
                metadata->SetCoverFile("");
            if (type == "Fanart")
                metadata->SetFanart("");
            if (type == "Banners")
                metadata->SetBanner("");
            if (type == "Screenshot")
                metadata->SetScreenshot("");

            OnVideoImageSetDone(metadata);
        }
    }
    else
        OnVideoImageSetDone(metadata);
}

void VideoDialog::OnImageCopyFinished(ImageDownloadErrorState error,
                                       QString errorMsg, Metadata *item,
                                       const QString &imagePath)
{
    QObject *src = sender();
    if (src)
        m_d->RemoveImageDownload(dynamic_cast<ImageDownloadProxy *>
                                       (src));

    QString type;

    if (imagePath.contains("_coverart."))
        type = QString("Coverart");
    else if (imagePath.contains("_fanart."))
        type = QString("Fanart");
    else if (imagePath.contains("_banner."))
        type = QString("Banner");
    else if (imagePath.contains("_screenshot."))
        type = QString("Screenshot");

    if (item)
    {
        if (error != esOK)
        {
            if (type == "Coverart")
                item->SetCoverFile("");
            else if (type == "Fanart")
                item->SetFanart("");
            else if (type == "Banner")
                item->SetBanner("");
            else if (type == "Screenshot")
                item->SetScreenshot("");
        }
        else
        {
            if (type == "Coverart")
                item->SetCoverFile(imagePath);
            else if (type == "Fanart")
                item->SetFanart(imagePath);
            else if (type == "Banner")
                item->SetBanner(imagePath);
            else if (type == "Screenshot")
                item->SetScreenshot(imagePath);
        }
    }

    VERBOSE(VB_IMPORTANT, tr("%1 download finished: %2 %3")
            .arg(type).arg(errorMsg).arg(error));

    if (error == esTimeout)
    {
        createOkDialog(tr("%1 exists for this item but could not be "
                            "retrieved within the timeout period.\n")
                            .arg(type));
    }

    OnVideoImageSetDone(item);
}

// This is the final call as part of a StartVideoImageSet
void VideoDialog::OnVideoImageSetDone(Metadata *metadata)
{
    // The metadata has some cover file set
    if (m_busyPopup)
    {
        m_busyPopup->Close();
        m_busyPopup = NULL;
    }

    metadata->UpdateDatabase();
    MythUIButtonListItem *item = GetItemByMetadata(metadata);
    if (item != NULL)
        UpdateItem(item);
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
    QStringList coverart, fanart, banner, screenshot;

    if (normal_exit && output.size())
    {
        for (QStringList::const_iterator p = output.begin();
            p != output.end(); ++p)
        {
            data[(*p).section(':', 0, 0)] = (*p).section(':', 1);
        }
        // Set known values, but always set Title.
        // Allows for partial fill.  Reset Metadata for full fill.

        metadata->SetTitle(data["Title"]);
        metadata->SetSubtitle(data["Subtitle"]);

        if (metadata->GetTagline().isEmpty())
            metadata->SetTagline(data["Tagline"]);
        if (metadata->GetYear() == 1895 || metadata->GetYear() == 0)
            metadata->SetYear(data["Year"].toInt());
        if (metadata->GetReleaseDate() == QDate())
            metadata->SetReleaseDate(QDate::fromString(data["ReleaseDate"], "yyyy-MM-dd"));
        if (metadata->GetDirector() == VIDEO_DIRECTOR_UNKNOWN ||
            metadata->GetDirector().isEmpty())
            metadata->SetDirector(data["Director"]);
        if (metadata->GetPlot() == VIDEO_PLOT_DEFAULT ||
            metadata->GetPlot().isEmpty())
            metadata->SetPlot(data["Plot"]);
        if (metadata->GetUserRating() == 0)
            metadata->SetUserRating(data["UserRating"].toFloat());
        if (metadata->GetRating() == VIDEO_RATING_DEFAULT)
            metadata->SetRating(data["MovieRating"]);
        if (metadata->GetLength() == 0)
            metadata->SetLength(data["Runtime"].toInt());
        if (metadata->GetSeason() == 0)
            metadata->SetSeason(data["Season"].toInt());
        if (metadata->GetEpisode() == 0)
            metadata->SetEpisode(data["Episode"].toInt());

        m_d->AutomaticParentalAdjustment(metadata);

        // Imagery
        coverart = data["Coverart"].split(",", QString::SkipEmptyParts);
        fanart = data["Fanart"].split(",", QString::SkipEmptyParts);
        banner = data["Banner"].split(",", QString::SkipEmptyParts);
        screenshot = data["Screenshot"].split(",", QString::SkipEmptyParts);

        // Inetref
        // Always update this if it exists-- This allows us to transition
        // seamlessly to TMDB numbers from IMDB numbers.
        if (data["InetRef"].length())
            metadata->SetInetRef(data["InetRef"]);
        else
            metadata->SetInetRef(video_uid);

        if (metadata->GetHomepage().isEmpty())
            metadata->SetHomepage(data["URL"].trimmed());

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

        metadata->SetCast(cast);

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

        metadata->SetGenres(video_genres);

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

        metadata->SetCountries(video_countries);

        metadata->UpdateDatabase();
        MythUIButtonListItem *item = GetItemByMetadata(metadata);
        if (item != NULL)
            UpdateItem(item);

        StartVideoImageSet(metadata, coverart, fanart, banner, screenshot);

    }
    else
    {
        metadata->UpdateDatabase();
        MythUIButtonListItem *item = GetItemByMetadata(metadata);
        if (item != NULL)
            UpdateItem(item);
    }
}

void VideoDialog::StartVideoSearchByTitle(QString video_uid, QString title,
                                            Metadata *metadata)
{
    if (video_uid.isEmpty())
    {
        createBusyDialog(title);

        metadata->SetTitle(Metadata::FilenameToMeta(metadata->GetFilename(), 1));
        QString seas = Metadata::FilenameToMeta(metadata->GetFilename(), 2);
        metadata->SetSeason(seas.toInt());
        QString ep = Metadata::FilenameToMeta(metadata->GetFilename(), 3);
        metadata->SetEpisode(ep.toInt());

        VideoTitleSearch *vts = new VideoTitleSearch(this);
        connect(vts, SIGNAL(SigSearchResults(bool, const QStringList &,
                                Metadata *)),
                SLOT(OnVideoSearchByTitleDone(bool, const QStringList &,
                                Metadata *)));
        vts->Run(title, metadata);
    }
    else if (video_uid == VIDEO_INETREF_DEFAULT)
    {
        createBusyDialog(title);

        VideoTitleSearch *vts = new VideoTitleSearch(this);
        connect(vts, SIGNAL(SigSearchResults(bool, const QStringList &,
                                Metadata *)),
                SLOT(OnVideoSearchByTitleDone(bool, const QStringList &,
                                Metadata *)));
        vts->Run(title, metadata);
    }
    else
    {
        QStringList videos;
        videos.append(QString("%1:%2").arg(video_uid).arg(title));
        OnVideoSearchByTitleDone(true, videos, metadata);
    }
}

void VideoDialog::OnVideoSearchByTitleDone(bool normal_exit,
        const QStringList &results, Metadata *metadata)
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
        QString keyValue = results.first();
        QString key = (keyValue.left(keyValue.indexOf(':')));
        QString value = (keyValue.right(keyValue.length() - keyValue.indexOf(":") - 1));

        // Only one search result, fetch data.
        if (value.isEmpty())
        {
            metadata->Reset();
            metadata->UpdateDatabase();
            MythUIButtonListItem *item = GetItemByMetadata(metadata);
            if (item != NULL)
                UpdateItem(item);
            return;
        }
        StartVideoSearchByUID(key, metadata);
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

void VideoDialog::StartVideoSearchByTitleSubtitle(QString title,
                                            QString subtitle, Metadata *metadata)
{
        createBusyDialog(title);

        VideoTitleSubtitleSearch *vtss = new VideoTitleSubtitleSearch(this);

        connect(vtss, SIGNAL(SigSearchResults(bool, QStringList,
                                Metadata *)),
                SLOT(OnVideoSearchByTitleSubtitleDone(bool, QStringList,
                                Metadata *)));
        vtss->Run(title, subtitle, metadata);
}

void VideoDialog::OnVideoSearchByTitleSubtitleDone(bool normal_exit,
        QStringList result, Metadata *metadata)
{
    if (m_busyPopup)
    {
        m_busyPopup->Close();
        m_busyPopup = NULL;
    }

    (void) normal_exit;
    QString SeasEp;

    if (!result.isEmpty())
        SeasEp = result.takeAt(0);

    if (!SeasEp.isEmpty())
    {

        // Stuff to parse Season and Episode here
        QString season, episode = NULL;

        QRegExp group("(?:[s])?(\\d{1,3})(?:\\s|-)?(?:[ex])" //Season
                      "(?:\\s|-)?(\\d{1,3})", // Episode
                      Qt::CaseInsensitive);

        int pos = group.indexIn(SeasEp);
        if (pos > -1)
        {
            QString groupResult = group.cap(0); // AEW indent
            season = group.cap(1);
            episode = group.cap(2);
        }

        VERBOSE(VB_IMPORTANT,
            QString("Season and Episode found!  It was: %1")
            .arg(SeasEp));

        if (!season.isNull() && !episode.isNull())
        {
            metadata->SetSeason(season.toInt());
            metadata->SetEpisode(episode.toInt());
            StartVideoSearchByTitle(VIDEO_INETREF_DEFAULT, 
                                metadata->GetTitle(), metadata);
        }
    }
    else
        createOkDialog(tr("No matches were found."));
}

void VideoDialog::doVideoScan()
{
    if (!m_d->m_scanner)
        m_d->m_scanner = new VideoScanner();
    connect(m_d->m_scanner, SIGNAL(finished(bool)), SLOT(reloadAllData(bool)));
    m_d->m_scanner->doScan(GetVideoDirs());
}

#include "videodlg.moc"
