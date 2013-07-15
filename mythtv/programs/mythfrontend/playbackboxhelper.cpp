#include <algorithm>
using namespace std;

#include <QCoreApplication>
#include <QStringList>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>
#include <QMap>
#include <QHash>

#include "previewgeneratorqueue.h"
#include "metadataimagehelper.h"
#include "playbackboxhelper.h"
#include "mythcorecontext.h"
#include "filesysteminfo.h"
#include "tvremoteutil.h"
#include "storagegroup.h"
#include "mythlogging.h"
#include "programinfo.h"
#include "remoteutil.h"
#include "mythevent.h"
#include "mythdirs.h"
#include "compat.h" // for random()

#define LOC      QString("PlaybackBoxHelper: ")
#define LOC_WARN QString("PlaybackBoxHelper Warning: ")
#define LOC_ERR  QString("PlaybackBoxHelper Error: ")

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
        {
            killTimer(m_checkAvailabilityTimerId);
            m_checkAvailabilityTimerId = 0;
        }
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
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("CHECK_AVAILABILITY '%1' file not found")
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
                QDateTime recstartts    = MythDate::fromString(list[1]);
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
                QDateTime recstartts    = MythDate::fromString(list[1]);

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
            QString                inetref   = me->ExtraData(0);
            uint                   season    = me->ExtraData(1).toUInt();
            const VideoArtworkType type      = (VideoArtworkType)me->ExtraData(2).toInt();
            const QString          pikey     = me->ExtraData(3);
            const QString          group     = me->ExtraData(4);
            const QString          cacheKey = QString("%1:%2:%3")
                .arg((int)type).arg(inetref).arg(season);

            ArtworkMap map = GetArtwork(inetref, season);

            ArtworkInfo info = map.value(type);

            QString foundFile;

            if (!info.url.isEmpty())
            {
                foundFile = info.url;
                QMutexLocker locker(&m_pbh.m_lock);
                m_pbh.m_artworkCache[cacheKey] = foundFile;
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
    MThread("PlaybackBoxHelper"),
    m_listener(listener), m_eventHandler(new PBHEventHandler(*this)),
    // Free Space Tracking Variables
    m_freeSpaceTotalMB(0ULL), m_freeSpaceUsedMB(0ULL)
{
    start();
    m_eventHandler->moveToThread(qthread());
    // Prime the pump so the disk free display starts updating
    ForceFreeSpaceUpdate();
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

void PlaybackBoxHelper::UpdateFreeSpace(void)
{
    QList<FileSystemInfo> fsInfos = FileSystemInfo::RemoteGetInfo();

    QMutexLocker locker(&m_lock);
    for (int i = 0; i < fsInfos.size(); i++)
    {
        if (fsInfos[i].getPath() == "TotalDiskSpace")
        {
            m_freeSpaceTotalMB = (uint64_t) (fsInfos[i].getTotalSpace() >> 10);
            m_freeSpaceUsedMB  = (uint64_t) (fsInfos[i].getUsedSpace()  >> 10);
        }
    }
    MythEvent *e = new MythEvent("UPDATE_USAGE_UI");
    QCoreApplication::postEvent(m_listener, e);
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
    const QString &inetref, uint season,
    const VideoArtworkType type,
    const ProgramInfo *pginfo,
    const QString &groupname)
{
    QString cacheKey = QString("%1:%2:%3")
        .arg((int)type).arg(inetref).arg(season);

    QMutexLocker locker(&m_lock);

    InfoMap::const_iterator it =
        m_artworkCache.find(cacheKey);

    if (it != m_artworkCache.end())
        return *it;

    QStringList list(inetref);
    list.push_back(QString::number(season));
    list.push_back(QString::number(type));
    list.push_back((pginfo)?pginfo->MakeUniqueKey():"");
    list.push_back(groupname);
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
        .arg(pginfo.MakeUniqueKey()).arg(random());

    QStringList extra(token);
    extra.push_back(check_availability?"1":"0");
    pginfo.ToStringList(extra);
    MythEvent *e = new MythEvent("GET_PREVIEW", extra);
    QCoreApplication::postEvent(m_eventHandler, e);

    return token;
}
