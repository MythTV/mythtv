
// C++ headers
#include <cstdlib>
#include <set>

// QT headers
#include <QDir>
#include <QImageReader>

// Myth headers
#include <mythtv/mythcontext.h>
#include <mythtv/mythdirs.h>

// MythUI headers
#include <mythtv/libmythui/mythmainwindow.h>
#include <mythtv/libmythui/mythsystem.h>
#include <mythtv/libmythui/mythdialogbox.h>

// MythVideo headers
#include "globals.h"
#include "videoutils.h"
#include "metadata.h"
#include "metadatalistmanager.h"

namespace
{
    const int WATCHED_WATERMARK = 10000; // Less than this and the chain of
                                         // videos will not be followed when
                                         // playing.
    const QString VIDEO_COVERFILE_DEFAULT_OLD = QObject::tr("None");
}

void PlayVideo(const QString &filename, const MetadataListManager &video_list)
{
    MetadataListManager::MetadataPtr item = video_list.byFilename(filename);

    if (!item) return;

    QTime playing_time;

    do
    {
        playing_time.start();

        QString internal_mrl;
        QString handler = Metadata::getPlayer(item.get(), internal_mrl);
        // See if this is being handled by a plugin..
        if (!gContext->GetMainWindow()->
                HandleMedia(handler, internal_mrl, item->Plot(), item->Title(),
                            item->Director(), item->Length(),
                            QString::number(item->Year())))
        {
            // No internal handler for this, play external
            QString command = Metadata::getPlayCommand(item.get());
            if (command.length())
            {
                gContext->sendPlaybackStart();
                myth_system(command);
                gContext->sendPlaybackEnd();
            }
        }

        if (item->ChildID() > 0)
        {
            item = video_list.byID(item->ChildID());
        }
        else
        {
            break;
        }
    }
    while (item && playing_time.elapsed() > WATCHED_WATERMARK);
}

QStringList GetVideoDirs()
{
    QStringList tmp = gContext->GetSetting("VideoStartupDir",
                DEFAULT_VIDEOSTARTUP_DIR).split(":", QString::SkipEmptyParts);
    for (QStringList::iterator p = tmp.begin(); p != tmp.end(); ++p)
    {
        *p = QDir::cleanDirPath(*p);
    }
    return tmp;
}

QString getDisplayYear(int year)
{
    return year == VIDEO_YEAR_DEFAULT ? "?" : QString::number(year);
}

QString getDisplayRating(const QString &rating)
{
    if (rating == "<NULL>")
        return QObject::tr("No rating available.");
    return rating;
}

QString getDisplayUserRating(float userrating)
{
    return QString::number(userrating, 'f', 1);
}

QString getDisplayLength(int length)
{
    return QString::number(length) + " " + QObject::tr("minutes");
}

QString getDisplayBrowse(bool browse)
{
    return browse ? QObject::tr("Yes") : QObject::tr("No");
}

bool isDefaultCoverFile(const QString &coverfile)
{
    return coverfile == VIDEO_COVERFILE_DEFAULT ||
            coverfile == VIDEO_COVERFILE_DEFAULT_OLD;
}

QStringList GetCastList(const Metadata &item)
{
    QStringList al;

    const Metadata::cast_list &cast = item.getCast();
    for (Metadata::cast_list::const_iterator p = cast.begin();
         p != cast.end(); ++p)
    {
        al.push_back(p->second);
    }

    if (!al.count())
        al << QObject::tr("None defined");

    return al;
}

QString GetCast(const Metadata &item, const QString &sep /*= ", "*/)
{
    return GetCastList(item).join(sep);
}

bool GetLocalVideoPoster(const QString &video_uid, const QString &filename,
                         const QStringList &in_dirs, QString &poster)
{
    QStringList search_dirs(in_dirs);

    QFileInfo qfi(filename);
    search_dirs += qfi.dirPath(true);

    const QString base_name = qfi.baseName(true);
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

//////////////////////////////////////////

TimeoutSignalProxy::TimeoutSignalProxy()
{
    m_item = 0;
    connect(&m_timer, SIGNAL(timeout()), SLOT(OnTimeout()));
}

void TimeoutSignalProxy::start(int timeout, Metadata *item, const QString &url)
{
    m_item = item;
    m_url = url;
    m_timer.start(timeout, true);
}

void TimeoutSignalProxy::stop()
{
    if (m_timer.isActive())
        m_timer.stop();
}

void TimeoutSignalProxy::OnTimeout()
{
    emit SigTimeout(m_url, m_item);
}

//////////////////////////////////////////

URLOperationProxy::URLOperationProxy()
{
    m_item = 0;
    connect(&m_url_op, SIGNAL(finished(Q3NetworkOperation *)),
            SLOT(OnFinished(Q3NetworkOperation *)));
}

void URLOperationProxy::copy(const QString &uri, const QString &dest, Metadata *item)
{
    m_item = item;
    m_url_op.copy(uri, dest, false, false);
}

void URLOperationProxy::stop()
{
    m_url_op.stop();
}

void URLOperationProxy::OnFinished(Q3NetworkOperation *op)
{
    emit SigFinished(op, m_item);
}

//////////////////////////////////////////

ExecuteExternalCommand::ExecuteExternalCommand(QObject *oparent)
                        : QObject(oparent)
{
    m_purpose = QObject::tr("Command");

    connect(&m_process, SIGNAL(readyReadStdout()),
            SLOT(OnReadReadyStdout()));
    connect(&m_process, SIGNAL(readyReadStderr()),
            SLOT(OnReadReadyStderr()));
    connect(&m_process, SIGNAL(processExited()),
            SLOT(OnProcessExit()));
}

ExecuteExternalCommand::~ExecuteExternalCommand()
{
}

void ExecuteExternalCommand::StartRun(const QString &command, const QStringList &args,
                const QString &purpose)
{
    m_purpose = purpose;

    // TODO: punting on spaces in path to command
    QStringList split_args = command.split(' ', QString::SkipEmptyParts);
    split_args += args;

    m_process.clearArguments();
    m_process.setArguments(split_args);

    VERBOSE(VB_GENERAL, QString("%1: Executing '%2'").arg(purpose)
            .arg(split_args.join(" ")));

    m_raw_cmd = split_args[0];
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
    else if (!m_process.start())
    {
        err_msg = QString("\"%1\" failed: Could not start process")
                .arg(m_raw_cmd);
    }

    if (err_msg.length())
    {
        ShowError(err_msg);
    }
}

void ExecuteExternalCommand::OnReadReadyStdout()
{
    QByteArray buf = m_process.readStdout();
    m_std_out += QString::fromUtf8(buf.data(), buf.size());
}

void ExecuteExternalCommand::OnReadReadyStderr()
{
    QByteArray buf = m_process.readStderr();
    m_std_error += QString::fromUtf8(buf.data(), buf.size());
}

void ExecuteExternalCommand::OnProcessExit()
{
    if (!m_process.normalExit())
    {
        ShowError(QString("\"%1\" failed: Process exited abnormally")
                    .arg(m_raw_cmd));
    }

    if (m_std_error.length())
    {
        ShowError(m_std_error);
    }

    QStringList std_out = m_std_out.split("\n", QString::SkipEmptyParts);
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

    VERBOSE(VB_IMPORTANT, m_std_out);

    OnExecDone(m_process.normalExit(), std_out, m_std_error.split("\n"));
}

void ExecuteExternalCommand::OnExecDone(bool normal_exit, const QStringList &out,
                                        const QStringList &err)
{
}

void ExecuteExternalCommand::ShowError(const QString &error_msg)
{
    VERBOSE(VB_IMPORTANT, error_msg);

    QString message = QString(QObject::tr("%1 failed\n\n%2\n\nCheck VideoManager Settings"))
                                .arg(m_purpose).arg(error_msg);

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythConfirmationDialog *okPopup = new MythConfirmationDialog(popupStack, message, false);

    if (okPopup->Create())
        popupStack->AddScreen(okPopup, false);
}

//////////////////////////////////////////

VideoTitleSearch::VideoTitleSearch(QObject *oparent)
                 : ExecuteExternalCommand(oparent)
{
    m_item = 0;
}

void VideoTitleSearch::Run(const QString &title, Metadata *item)
{
    m_item = item;

    QString def_cmd = QDir::cleanDirPath(QString("%1/%2")
            .arg(GetShareDir())
            .arg("mythvideo/scripts/imdb.pl -M tv=no;video=no"));

    QString cmd = gContext->GetSetting("MovieListCommandLine", def_cmd);

    QStringList args;
    args += title;
    StartRun(cmd, args, "Video Search");
}

void VideoTitleSearch::OnExecDone(bool normal_exit, const QStringList &out,
                                  const QStringList &err)
{
    (void) err;

    SearchListResults results;
    if (normal_exit)
    {
        for (QStringList::const_iterator p = out.begin();
                p != out.end(); ++p)
        {
            results.insert((*p).section(':', 0, 0), (*p).section(':', 1));
        }
    }

    emit SigSearchResults(normal_exit, results, m_item);
    deleteLater();
}

//////////////////////////////////////////

VideoUIDSearch::VideoUIDSearch(QObject *oparent)
               : ExecuteExternalCommand(oparent)
{
}

void VideoUIDSearch::Run(const QString &video_uid, Metadata *item)
{
    m_item = item;
    m_video_uid = video_uid;

    const QString def_cmd = QDir::cleanDirPath(QString("%1/%2")
            .arg(GetShareDir())
            .arg("mythvideo/scripts/imdb.pl -D"));
    const QString cmd = gContext->GetSetting("MovieDataCommandLine",
                                                def_cmd);

    StartRun(cmd, QStringList(video_uid), "Video Data Query");
}

void VideoUIDSearch::OnExecDone(bool normal_exit, const QStringList &out,
                                const QStringList &err)
{
    (void) err;
    emit SigSearchResults(normal_exit, out, m_item, m_video_uid);
    deleteLater();
}

//////////////////////////////////////////

VideoPosterSearch::VideoPosterSearch(QObject *oparent)
                  : ExecuteExternalCommand(oparent)
{
}

void VideoPosterSearch::Run(const QString &video_uid, Metadata *item)
{
    m_item = item;

    const QString default_cmd =
            QDir::cleanPath(QString("%1/%2")
                                .arg(GetShareDir())
                                .arg("mythvideo/scripts/imdb.pl -P"));
    const QString cmd = gContext->GetSetting("MoviePosterCommandLine",
                                                default_cmd);
    StartRun(cmd, QStringList(video_uid), "Poster Query");
}

void VideoPosterSearch::OnExecDone(bool normal_exit, const QStringList &out,
                                   const QStringList &err)
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
