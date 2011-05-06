#include <algorithm>
using namespace std;

#include <QCoreApplication>
#include <QStringList>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>

#include "previewgeneratorqueue.h"
#include "playbackboxhelper.h"
#include "mythcorecontext.h"
#include "tvremoteutil.h"
#include "storagegroup.h"
#include "programinfo.h"
#include "remoteutil.h"
#include "mythevent.h"
#include "mythdirs.h"

#define LOC      QString("PlaybackBoxHelper: ")
#define LOC_WARN QString("PlaybackBoxHelper Warning: ")
#define LOC_ERR  QString("PlaybackBoxHelper Error: ")

QString toString(ArtworkType t)
{
    switch (t)
    {
        case kArtworkFan:    return "fanart";
        case kArtworkBanner: return "banner";
        case kArtworkCover:  return "coverart";
    }
    return QString();
}

QString toLocalDir(ArtworkType t)
{
    static QMutex m;
    static bool initrun = false;
    static QString kFanDir, kBannerDir, kCoverDir;

    QMutexLocker locker(&m);
    if (!initrun)
    {
        kFanDir    = gCoreContext->GetSetting("mythvideo.fanartDir");
        kBannerDir = gCoreContext->GetSetting("mythvideo.bannerDir");
        kCoverDir  = gCoreContext->GetSetting("VideoArtworkDir");
        initrun = true;
    }

    switch (t)
    {
        case kArtworkFan:    return kFanDir;
        case kArtworkBanner: return kBannerDir;
        case kArtworkCover:  return kCoverDir;
    }
    return QString();
}

QString toSG(ArtworkType t)
{
    switch (t)
    {
        case kArtworkFan:    return "Fanart";
        case kArtworkBanner: return "Banners";
        case kArtworkCover:  return "Coverart";
    }
    return QString();
}

class PBHEventHandler : public QObject
{
  public:
    PBHEventHandler(PlaybackBoxHelper &pbh) :
        m_pbh(pbh), m_freeSpaceTimerId(0), m_checkAvailabilityTimerId(0)
    {
        StorageGroup::ClearGroupToUseCache();
    }
    virtual bool event(QEvent*); // QObject
    void UpdateFreeSpaceEvent(void);
    AvailableStatusType CheckAvailability(const QStringList &slist);
    PlaybackBoxHelper &m_pbh;
    int m_freeSpaceTimerId;
    int m_checkAvailabilityTimerId;
    static const uint kUpdateFreeSpaceInterval;
    QMap<QString, QStringList> m_fileListCache;
    QHash<QString, QStringList> m_checkAvailability;
};

const uint PBHEventHandler::kUpdateFreeSpaceInterval = 15000; // 15 seconds

AvailableStatusType PBHEventHandler::CheckAvailability(const QStringList &slist)
{
    QTime tm = QTime::currentTime();

    QStringList::const_iterator it = slist.begin();
    ProgramInfo evinfo(it, slist.end());
    QSet<CheckAvailabilityType> cats;
    for (; it != slist.end(); ++it)
        cats.insert((CheckAvailabilityType)(*it).toUInt());

    {
        QMutexLocker locker(&m_pbh.m_lock);
        QHash<QString, QStringList>::iterator it =
            m_checkAvailability.find(evinfo.MakeUniqueKey());
        if (it != m_checkAvailability.end())
            m_checkAvailability.erase(it);
        if (m_checkAvailability.empty() && m_checkAvailabilityTimerId)
            killTimer(m_checkAvailabilityTimerId);
    }

    if (cats.empty())
        return asFileNotFound;

    AvailableStatusType availableStatus = asAvailable;
    if (!evinfo.HasPathname() && !evinfo.GetChanID())
        availableStatus = asFileNotFound;
    else
    {
        // Note IsFileReadable() implicitly calls GetPlaybackURL
        // when necessary, we rely on this.
        if (!evinfo.IsFileReadable())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("CHECK_AVAILABILITY '%1' "
                            "file not found")
                    .arg(evinfo.GetPathname()));
            availableStatus = asFileNotFound;
        }
        else if (!evinfo.GetFilesize())
        {
            evinfo.SetFilesize(evinfo.QueryFilesize());
            if (!evinfo.GetFilesize())
            {
                availableStatus =
                    (evinfo.GetRecordingStatus() == rsRecording) ?
                    asNotYetAvailable : asZeroByte;
            }
        }
    }

    QStringList list;
    list.push_back(evinfo.MakeUniqueKey());
    list.push_back(evinfo.GetPathname());
    MythEvent *e0 = new MythEvent("SET_PLAYBACK_URL", list);
    QCoreApplication::postEvent(m_pbh.m_listener, e0);

    list.clear();
    list.push_back(evinfo.MakeUniqueKey());
    list.push_back(QString::number((int)*cats.begin()));
    list.push_back(QString::number((int)availableStatus));
    list.push_back(QString::number(evinfo.GetFilesize()));
    list.push_back(QString::number(tm.hour()));
    list.push_back(QString::number(tm.minute()));
    list.push_back(QString::number(tm.second()));
    list.push_back(QString::number(tm.msec()));

    QSet<CheckAvailabilityType>::iterator cit = cats.begin();
    for (; cit != cats.end(); ++cit)
    {
        if (*cit == kCheckForCache && cats.size() > 1)
            continue;
        list[1] = QString::number((int)*cit);
        MythEvent *e = new MythEvent("AVAILABILITY", list);
        QCoreApplication::postEvent(m_pbh.m_listener, e);
    }

    return availableStatus;
}

bool PBHEventHandler::event(QEvent *e)
{
    if (e->type() == QEvent::Timer)
    {
        QTimerEvent *te = (QTimerEvent*)e;
        const int timer_id = te->timerId();
        if (timer_id == m_freeSpaceTimerId)
            UpdateFreeSpaceEvent();
        if (timer_id == m_checkAvailabilityTimerId)
        {
            QStringList slist;
            {
                QMutexLocker locker(&m_pbh.m_lock);
                QHash<QString, QStringList>::iterator it =
                    m_checkAvailability.begin();
                if (it != m_checkAvailability.end())
                    slist = *it;
            }

            if (slist.size() >= 1 + NUMPROGRAMLINES)
                CheckAvailability(slist);
        }
        return true;
    }
    else if (e->type() == (QEvent::Type) MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent*)e;
        if (me->Message() == "UPDATE_FREE_SPACE")
        {
            UpdateFreeSpaceEvent();
            return true;
        }
        else if (me->Message() == "STOP_RECORDING")
        {
            ProgramInfo pginfo(me->ExtraDataList());
            if (pginfo.GetChanID())
                RemoteStopRecording(&pginfo);
            return true;
        }
        else if (me->Message() == "DELETE_RECORDINGS")
        {
            QStringList successes;
            QStringList failures;
            QStringList list = me->ExtraDataList();
            while (list.size() >= 4)
            {
                uint      chanid        = list[0].toUInt();
                QDateTime recstartts    = QDateTime::fromString(
                    list[1], Qt::ISODate);
                bool      forceDelete   = list[2].toUInt();
                bool      forgetHistory = list[3].toUInt();

                bool ok = RemoteDeleteRecording(
                    chanid, recstartts, forceDelete, forgetHistory);

                QStringList &res = (ok) ? successes : failures;
                for (uint i = 0; i < 4; i++)
                {
                    res.push_back(list.front());
                    list.pop_front();
                }
            }
            if (!successes.empty())
            {
                MythEvent *e = new MythEvent("DELETE_SUCCESSES", successes);
                QCoreApplication::postEvent(m_pbh.m_listener, e);
            }
            if (!failures.empty())
            {
                MythEvent *e = new MythEvent("DELETE_FAILURES", failures);
                QCoreApplication::postEvent(m_pbh.m_listener, e);
            }

            return true;
        }
        else if (me->Message() == "UNDELETE_RECORDINGS")
        {
            QStringList successes;
            QStringList failures;
            QStringList list = me->ExtraDataList();
            while (list.size() >= 2)
            {
                uint      chanid        = list[0].toUInt();
                QDateTime recstartts    = QDateTime::fromString(
                    list[1], Qt::ISODate);

                bool ok = RemoteUndeleteRecording(chanid, recstartts);

                QStringList &res = (ok) ? successes : failures;
                for (uint i = 0; i < 2; i++)
                {
                    res.push_back(list.front());
                    list.pop_front();
                }
            }
            if (!successes.empty())
            {
                MythEvent *e = new MythEvent("UNDELETE_SUCCESSES", successes);
                QCoreApplication::postEvent(m_pbh.m_listener, e);
            }
            if (!failures.empty())
            {
                MythEvent *e = new MythEvent("UNDELETE_FAILURES", failures);
                QCoreApplication::postEvent(m_pbh.m_listener, e);
            }

            return true;
        }
        else if (me->Message() == "GET_PREVIEW")
        {
            QString token = me->ExtraData(0);
            bool check_avail = (bool) me->ExtraData(1).toInt();
            QStringList list = me->ExtraDataList();
            QStringList::const_iterator it = list.begin()+2;
            ProgramInfo evinfo(it, list.end());
            if (!evinfo.HasPathname())
                return true;

            list.clear();
            evinfo.ToStringList(list);
            list += QString::number(kCheckForCache);
            if (check_avail && (asAvailable != CheckAvailability(list)))
                return true;
            else if (asAvailable != evinfo.GetAvailableStatus())
                return true;

            // Now we can actually request the preview...
            PreviewGeneratorQueue::GetPreviewImage(evinfo, token);

            return true;
        }
        else if (me->Message() == "CHECK_AVAILABILITY")
        {
            if (me->ExtraData(0) != QString::number(kCheckForCache))
            {
                if (m_checkAvailabilityTimerId)
                    killTimer(m_checkAvailabilityTimerId);
                m_checkAvailabilityTimerId = startTimer(0);
            }
            else if (!m_checkAvailabilityTimerId)
                m_checkAvailabilityTimerId = startTimer(50);
        }
        else if (me->Message() == "LOCATE_ARTWORK")
        {
            QString           seriesid  = me->ExtraData(0);
            QString           title     = me->ExtraData(1);
            const ArtworkType type      = (ArtworkType)me->ExtraData(2).toInt();
            const QString     host      = me->ExtraData(3);
            const QString     sgroup    = toSG(type);
            const QString     localDir  = toLocalDir(type);
            const QString     cacheKey = QString("%1:%2:%3")
                .arg((int)type).arg(seriesid).arg(title);

            seriesid = (seriesid == "_GROUP_") ? QString() : seriesid;

            // Attempts to match image file in specified directory.
            // Falls back like this:
            //
            //     Pushing Daisies 5.png
            //     PushingDaisies5.png
            //     PushingDaisiesSeason5.png
            //     Pushing Daisies Season 5 Episode 1.png
            //     PuShinG DaisIES s05e01.png
            //     etc. (you get it)
            //
            // Or any permutation thereof including -,_, or . instead of space
            // Then, match by seriesid (for future PBB grabber):
            //
            //     EP0012345.png
            //
            // Then, as a final fallback, match just title
            //
            //     Pushing Daisies.png (or Pushing_Daisies.png, etc.)
            //
            // All this allows for grabber to grab an image with format:
            //
            //     Title SeasonNumber.ext or Title SeasonNum # Epnum #.ext
            //     or SeriesID.ext or Title.ext (without caring about cases,
            //     spaces, dashes, periods, or underscores)

            QDir dir(localDir);
            dir.setSorting(QDir::Name | QDir::Reversed | QDir::IgnoreCase);

            QStringList entries = dir.entryList();
            QString grpHost = sgroup + ":" + host;

            if (!m_fileListCache.contains(grpHost))
            {
                QStringList sgEntries;
                RemoteGetFileList(host, "", &sgEntries, sgroup, true);
                m_fileListCache[grpHost] = sgEntries;
            }

            title.replace(' ', "(?:\\s|-|_|\\.)?");
            QString regs[] = {
                QString("%1" // title
                        "(?:\\s|-|_|\\.)?" // optional separator
                        "(?:" // begin optional Season portion
                        "S(?:eason)?" // optional "S" or "Season"
                        "(?:\\s|-|_|\\.)?" // optional separator
                        "[0-9]{1,3}" // number
                        ")?" // end optional Season portion
                        "(?:" // begin optional Episode portion
                        "(?:\\s|-|_|\\.)?" // optional separator
                        "(?:x?" // optional "x"
                        "(?:\\s|-|_|\\.)?" // optional separator
                        "(?:E(?:pisode)?)?" // optional "E" or "Episode"
                        "(?:\\s|-|_|\\.)?)?" // optional separator
                        "[0-9]{1,3}" // number portion of
                                     // optional Episode portion
                        ")?" // end optional Episode portion
                        "(?:_%2)?" // optional Suffix portion
                        "\\.(?:png|gif|jpg)" // file extension
                    ).arg(title).arg(toString(type)),
                QString("%1_%2\\.(?:png|jpg|gif)")
                .arg(seriesid).arg(toString(type)),
                QString("%1\\.(?:png|jpg|gif)").arg(seriesid),
            };

            QString foundFile;
            for (uint i = 0; i < sizeof(regs) / sizeof(QString); i++)
            {
                QRegExp re(regs[i], Qt::CaseInsensitive);

                for (QStringList::const_iterator it = entries.begin();
                     it != entries.end(); ++it)
                {
                    if (re.exactMatch(*it))
                    {
                        foundFile = *it;
                        break;
                    }
                }

                for (QStringList::const_iterator it =
                         m_fileListCache[grpHost].begin();
                     it != m_fileListCache[grpHost].end(); ++it)
                {
                    if (!re.exactMatch(*it))
                        continue;

                    foundFile = QString("myth://%1@%2:%3/%4")
                        .arg(StorageGroup::GetGroupToUse(host, sgroup))
                        .arg(gCoreContext->GetSettingOnHost(
                                 "BackendServerIP", host))
                        .arg(gCoreContext->GetSettingOnHost(
                                 "BackendServerPort", host))
                        .arg(*it);
                    break;
                }
            }

            if (!foundFile.isEmpty())
            {
                if (localDir.endsWith("/"))
                    localDir.left(localDir.length()-1);
                if (!foundFile.startsWith("myth://"))
                    foundFile = QString("%1/%2").arg(localDir).arg(foundFile);
            }

            {
                QMutexLocker locker(&m_pbh.m_lock);
                m_pbh.m_artworkFilenameCache[cacheKey] = foundFile;
            }

            if (!foundFile.isEmpty())
            {
                QStringList list = me->ExtraDataList();
                list.push_back(foundFile);
                MythEvent *e = new MythEvent("FOUND_ARTWORK", list);
                QCoreApplication::postEvent(m_pbh.m_listener, e);
            }

            return true;
        }
    }

    return QObject::event(e);
}

void PBHEventHandler::UpdateFreeSpaceEvent(void)
{
    if (m_freeSpaceTimerId)
        killTimer(m_freeSpaceTimerId);
    m_pbh.UpdateFreeSpace();
    m_freeSpaceTimerId = startTimer(kUpdateFreeSpaceInterval);
}

//////////////////////////////////////////////////////////////////////

PlaybackBoxHelper::PlaybackBoxHelper(QObject *listener) :
    m_listener(listener), m_eventHandler(NULL),
    // Free Space Tracking Variables
    m_freeSpaceTotalMB(0ULL), m_freeSpaceUsedMB(0ULL)
{
    start();
}

PlaybackBoxHelper::~PlaybackBoxHelper()
{
    exit();
    wait();

    // delete the event handler
    delete m_eventHandler;
    m_eventHandler = NULL;
}

void PlaybackBoxHelper::ForceFreeSpaceUpdate(void)
{
    QCoreApplication::postEvent(
        m_eventHandler, new MythEvent("UPDATE_FREE_SPACE"));
}

void PlaybackBoxHelper::StopRecording(const ProgramInfo &pginfo)
{
    QStringList list;
    pginfo.ToStringList(list);
    MythEvent *e = new MythEvent("STOP_RECORDING", list);
    QCoreApplication::postEvent(m_eventHandler, e);
}

void PlaybackBoxHelper::DeleteRecording(
    uint chanid, const QDateTime &recstartts, bool forceDelete,
    bool forgetHistory)
{
    QStringList list;
    list.push_back(QString::number(chanid));
    list.push_back(recstartts.toString(Qt::ISODate));
    list.push_back((forceDelete)    ? "1" : "0");
    list.push_back((forgetHistory)  ? "1" : "0");
    DeleteRecordings(list);
}

void PlaybackBoxHelper::DeleteRecordings(const QStringList &list)
{
    MythEvent *e = new MythEvent("DELETE_RECORDINGS", list);
    QCoreApplication::postEvent(m_eventHandler, e);
}

void PlaybackBoxHelper::UndeleteRecording(
    uint chanid, const QDateTime &recstartts)
{
    QStringList list;
    list.push_back(QString::number(chanid));
    list.push_back(recstartts.toString(Qt::ISODate));
    MythEvent *e = new MythEvent("UNDELETE_RECORDINGS", list);
    QCoreApplication::postEvent(m_eventHandler, e);
}

void PlaybackBoxHelper::run(void)
{
    m_eventHandler = new PBHEventHandler(*this);
    exec();
}

void PlaybackBoxHelper::UpdateFreeSpace(void)
{
    vector<FileSystemInfo> fsInfos = RemoteGetFreeSpace();

    QMutexLocker locker(&m_lock);
    for (uint i = 0; i < fsInfos.size(); i++)
    {
        if (fsInfos[i].directory == "TotalDiskSpace")
        {
            m_freeSpaceTotalMB = (uint64_t) (fsInfos[i].totalSpaceKB >> 10);
            m_freeSpaceUsedMB  = (uint64_t) (fsInfos[i].usedSpaceKB  >> 10);
        }
    }
}

uint64_t PlaybackBoxHelper::GetFreeSpaceTotalMB(void) const
{
    QMutexLocker locker(&m_lock);
    return m_freeSpaceTotalMB;
}

uint64_t PlaybackBoxHelper::GetFreeSpaceUsedMB(void) const
{
    QMutexLocker locker(&m_lock);
    return m_freeSpaceUsedMB;
}

void PlaybackBoxHelper::CheckAvailability(
    const ProgramInfo &pginfo, CheckAvailabilityType cat)
{
    QString catstr = QString::number((int)cat);
    QMutexLocker locker(&m_lock);
    QHash<QString, QStringList>::iterator it =
        m_eventHandler->m_checkAvailability.find(pginfo.MakeUniqueKey());
    if (it == m_eventHandler->m_checkAvailability.end())
    {
        QStringList list;
        pginfo.ToStringList(list);
        list += catstr;
        m_eventHandler->m_checkAvailability[pginfo.MakeUniqueKey()] = list;
    }
    else
    {
        (*it).push_back(catstr);
    }
    MythEvent *e = new MythEvent("CHECK_AVAILABILITY", QStringList(catstr));
    QCoreApplication::postEvent(m_eventHandler, e);
}

QString PlaybackBoxHelper::LocateArtwork(
    const QString &seriesid, const QString &title,
    ArtworkType type, const QString &host,
    const ProgramInfo *pginfo)
{
    QString cacheKey = QString("%1:%2:%3")
        .arg((int)type).arg(seriesid).arg(title);

    QMutexLocker locker(&m_lock);

    QHash<QString,QString>::const_iterator it =
        m_artworkFilenameCache.find(cacheKey);

    if (it != m_artworkFilenameCache.end())
        return *it;

    QStringList list(seriesid);
    list.push_back(title);
    list.push_back(QString::number((int)type));
    list.push_back(host);
    list.push_back((pginfo)?pginfo->MakeUniqueKey():"");
    MythEvent *e = new MythEvent("LOCATE_ARTWORK", list);
    QCoreApplication::postEvent(m_eventHandler, e);

    return QString();
}

QString PlaybackBoxHelper::GetPreviewImage(
    const ProgramInfo &pginfo, bool check_availability)
{
    if (!check_availability && pginfo.GetAvailableStatus() != asAvailable)
        return QString();

    if (pginfo.GetAvailableStatus() == asPendingDelete)
        return QString();

    QString token = QString("%1:%2")
        .arg(pginfo.MakeUniqueKey()).arg(rand());

    QStringList extra(token);
    extra.push_back(check_availability?"1":"0");
    pginfo.ToStringList(extra);
    MythEvent *e = new MythEvent("GET_PREVIEW", extra);
    QCoreApplication::postEvent(m_eventHandler, e);

    return token;
}
