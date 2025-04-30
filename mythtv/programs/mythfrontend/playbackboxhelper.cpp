// C++
#include <algorithm>

// Qt
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QStringList>

// MythTV
#include "libmythbase/filesysteminfo.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythevent.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythrandom.h"
#include "libmythbase/programinfo.h"
#include "libmythbase/remoteutil.h"
#include "libmythbase/storagegroup.h"
#include "libmythtv/metadataimagehelper.h"
#include "libmythtv/previewgeneratorqueue.h"
#include "libmythtv/tvremoteutil.h"

//  MythFrontend
#include "playbackboxhelper.h"

#define LOC      QString("PlaybackBoxHelper: ")
#define LOC_WARN QString("PlaybackBoxHelper Warning: ")
#define LOC_ERR  QString("PlaybackBoxHelper Error: ")

class PBHEventHandler : public QObject
{
  public:
    explicit PBHEventHandler(PlaybackBoxHelper &pbh) :
        m_pbh(pbh)
    {
        StorageGroup::ClearGroupToUseCache();
    }
    ~PBHEventHandler() override
    {
        if (m_freeSpaceTimerId)
            killTimer(m_freeSpaceTimerId);
        if (m_checkAvailabilityTimerId)
            killTimer(m_checkAvailabilityTimerId);
    }

    bool event(QEvent* /*e*/) override; // QObject
    void UpdateFreeSpaceEvent(void);
    AvailableStatusType CheckAvailability(const QStringList &slist);
    PlaybackBoxHelper &m_pbh;
    int m_freeSpaceTimerId         { 0 };
    int m_checkAvailabilityTimerId { 0 };
    static constexpr std::chrono::milliseconds kUpdateFreeSpaceInterval { 15s };
    QMap<QString, QStringList> m_fileListCache;
    QHash<uint, QStringList> m_checkAvailability;
};

AvailableStatusType PBHEventHandler::CheckAvailability(const QStringList &slist)
{
    QTime tm = QTime::currentTime();

    QStringList::const_iterator it2 = slist.begin();
    ProgramInfo evinfo(it2, slist.end());
    bool first {true};
    CheckAvailabilityType firstType {};
    QSet<CheckAvailabilityType> cats;
    for (; it2 != slist.end(); ++it2)
    {
        auto type = (CheckAvailabilityType)(*it2).toUInt();
        if (first)
        {
            firstType = type;
            first = false;
        }
        cats.insert(type);
    }

    {
        QMutexLocker locker(&m_pbh.m_lock);
        QHash<uint, QStringList>::iterator cit =
            m_checkAvailability.find(evinfo.GetRecordingID());
        if (cit != m_checkAvailability.end())
            m_checkAvailability.erase(cit);
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
               // This query should be unnecessary if the ProgramInfo Updater is working
//             evinfo.SetFilesize(evinfo.QueryFilesize());
//             if (!evinfo.GetFilesize())
//             {
                availableStatus =
                    (evinfo.GetRecordingStatus() == RecStatus::Recording) ?
                    asNotYetAvailable : asZeroByte;
//             }
        }
    }

    QStringList list;
    list.push_back(QString::number(evinfo.GetRecordingID()));
    list.push_back(evinfo.GetPathname());
    auto *e0 = new MythEvent("SET_PLAYBACK_URL", list);
    QCoreApplication::postEvent(m_pbh.m_listener, e0);

    list.clear();
    list.push_back(QString::number(evinfo.GetRecordingID()));
    list.push_back(QString::number((int)firstType));
    list.push_back(QString::number((int)availableStatus));
    list.push_back(QString::number(evinfo.GetFilesize()));
    list.push_back(QString::number(tm.hour()));
    list.push_back(QString::number(tm.minute()));
    list.push_back(QString::number(tm.second()));
    list.push_back(QString::number(tm.msec()));

    for (auto type : std::as_const(cats))
    {
        if (type == kCheckForCache && cats.size() > 1)
            continue;
        list[1] = QString::number((int)type);
        auto *e = new MythEvent("AVAILABILITY", list);
        QCoreApplication::postEvent(m_pbh.m_listener, e);
    }

    return availableStatus;
}

bool PBHEventHandler::event(QEvent *e)
{
    if (e->type() == QEvent::Timer)
    {
        auto *te = (QTimerEvent*)e;
        const int timer_id = te->timerId();
        if (timer_id == m_freeSpaceTimerId)
            UpdateFreeSpaceEvent();
        if (timer_id == m_checkAvailabilityTimerId)
        {
            QStringList slist;
            {
                QMutexLocker locker(&m_pbh.m_lock);
                QHash<uint, QStringList>::iterator it =
                    m_checkAvailability.begin();
                if (it != m_checkAvailability.end())
                    slist = *it;
            }

            if (slist.size() >= 1 + NUMPROGRAMLINES)
                CheckAvailability(slist);
        }
        return true;
    }
    if (e->type() == MythEvent::kMythEventMessage)
    {
        auto *me = dynamic_cast<MythEvent*>(e);
        if (me == nullptr)
            return QObject::event(e);

        if (me->Message() == "UPDATE_FREE_SPACE")
        {
            UpdateFreeSpaceEvent();
            return true;
        }
        if (me->Message() == "STOP_RECORDING")
        {
            ProgramInfo pginfo(me->ExtraDataList());
            if (pginfo.GetChanID())
                RemoteStopRecording(&pginfo);
            return true;
        }
        if (me->Message() == "DELETE_RECORDINGS")
        {
            QStringList successes;
            QStringList failures;
            QStringList list = me->ExtraDataList();
            while (list.size() >= 3)
            {
                uint      recordingID   = list[0].toUInt();
                bool      forceDelete   = list[1].toUInt() != 0U;
                bool      forgetHistory = list[2].toUInt() != 0U;

                bool ok = RemoteDeleteRecording( recordingID, forceDelete,
                                                 forgetHistory);

                QStringList &res = (ok) ? successes : failures;
                for (uint i = 0; i < 3; i++)
                {
                    res.push_back(list.front());
                    list.pop_front();
                }
            }
            if (!successes.empty())
            {
                auto *oe = new MythEvent("DELETE_SUCCESSES", successes);
                QCoreApplication::postEvent(m_pbh.m_listener, oe);
            }
            if (!failures.empty())
            {
                auto *oe = new MythEvent("DELETE_FAILURES", failures);
                QCoreApplication::postEvent(m_pbh.m_listener, oe);
            }

            return true;
        }
        if (me->Message() == "UNDELETE_RECORDINGS")
        {
            QStringList successes;
            QStringList failures;
            QStringList list = me->ExtraDataList();
            while (!list.empty())
            {
                uint recordingID = list[0].toUInt();

                bool ok = RemoteUndeleteRecording(recordingID);

                QStringList &res = (ok) ? successes : failures;

                res.push_back(QString::number(recordingID));
                list.pop_front();
            }
            if (!successes.empty())
            {
                auto *oe = new MythEvent("UNDELETE_SUCCESSES", successes);
                QCoreApplication::postEvent(m_pbh.m_listener, oe);
            }
            if (!failures.empty())
            {
                auto *oe = new MythEvent("UNDELETE_FAILURES", failures);
                QCoreApplication::postEvent(m_pbh.m_listener, oe);
            }

            return true;
        }
        if (me->Message() == "GET_PREVIEW")
        {
            const QString& token = me->ExtraData(0);
            bool check_avail = (bool) me->ExtraData(1).toInt();
            QStringList list = me->ExtraDataList();
            QStringList::const_iterator it = list.cbegin()+2;
            ProgramInfo evinfo(it, list.cend());
            if (!evinfo.HasPathname())
                return true;

            list.clear();
            evinfo.ToStringList(list);
            list += QString::number(kCheckForCache);
            if (check_avail && (asAvailable != CheckAvailability(list)))
                return true;
            if (asAvailable != evinfo.GetAvailableStatus())
                return true;

            // Now we can actually request the preview...
            PreviewGeneratorQueue::GetPreviewImage(evinfo, token);

            return true;
        }
        if (me->Message() == "CHECK_AVAILABILITY")
        {
            if (me->ExtraData(0) != QString::number(kCheckForCache))
            {
                if (m_checkAvailabilityTimerId)
                    killTimer(m_checkAvailabilityTimerId);
                m_checkAvailabilityTimerId = startTimer(0ms);
            }
            else if (!m_checkAvailabilityTimerId)
            {
                m_checkAvailabilityTimerId = startTimer(50ms);
            }
        }
        else if (me->Message() == "LOCATE_ARTWORK")
        {
            const QString&         inetref    = me->ExtraData(0);
            uint                   season     = me->ExtraData(1).toUInt();
            const auto             type       = (VideoArtworkType)me->ExtraData(2).toInt();
#if 0 /* const ref for an unused variable doesn't make much sense either */
            uint                   recordingID = me->ExtraData(3).toUInt();
            const QString          &group     = me->ExtraData(4);
#endif
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
                auto *oe = new MythEvent("FOUND_ARTWORK", list);
                QCoreApplication::postEvent(m_pbh.m_listener, oe);
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
    m_listener(listener), m_eventHandler(new PBHEventHandler(*this))
{
    start();
    m_eventHandler->moveToThread(qthread());
    // Prime the pump so the disk free display starts updating
    ForceFreeSpaceUpdate();
}

PlaybackBoxHelper::~PlaybackBoxHelper()
{
    // delete the event handler
    m_eventHandler->deleteLater();
    m_eventHandler = nullptr;

    MThread::exit();
    wait();
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
    auto *e = new MythEvent("STOP_RECORDING", list);
    QCoreApplication::postEvent(m_eventHandler, e);
}

void PlaybackBoxHelper::DeleteRecording( uint recordingID, bool forceDelete,
                                         bool forgetHistory)
{
    QStringList list;
    list.push_back(QString::number(recordingID));
    list.push_back((forceDelete)    ? "1" : "0");
    list.push_back((forgetHistory)  ? "1" : "0");
    DeleteRecordings(list);
}

void PlaybackBoxHelper::DeleteRecordings(const QStringList &list)
{
    auto *e = new MythEvent("DELETE_RECORDINGS", list);
    QCoreApplication::postEvent(m_eventHandler, e);
}

void PlaybackBoxHelper::UndeleteRecording(uint recordingID)
{
    QStringList list;
    list.push_back(QString::number(recordingID));
    auto *e = new MythEvent("UNDELETE_RECORDINGS", list);
    QCoreApplication::postEvent(m_eventHandler, e);
}

void PlaybackBoxHelper::UpdateFreeSpace(void)
{
    FileSystemInfoList fsInfos = FileSystemInfoManager::GetInfoList();

    QMutexLocker locker(&m_lock);
    for (const auto& fsInfo : std::as_const(fsInfos))
    {
        if (fsInfo.getPath() == "TotalDiskSpace")
        {
            m_freeSpaceTotalMB = (uint64_t) (fsInfo.getTotalSpace() >> 10);
            m_freeSpaceUsedMB  = (uint64_t) (fsInfo.getUsedSpace()  >> 10);
        }
    }
    auto *e = new MythEvent("UPDATE_USAGE_UI");
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
    QHash<uint, QStringList>::iterator it =
        m_eventHandler->m_checkAvailability.find(pginfo.GetRecordingID());
    if (it == m_eventHandler->m_checkAvailability.end())
    {
        QStringList list;
        pginfo.ToStringList(list);
        list += catstr;
        m_eventHandler->m_checkAvailability[pginfo.GetRecordingID()] = list;
    }
    else
    {
        (*it).push_back(catstr);
    }
    auto *e = new MythEvent("CHECK_AVAILABILITY", QStringList(catstr));
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
        m_artworkCache.constFind(cacheKey);

    if (it != m_artworkCache.constEnd())
        return *it;

    QStringList list(inetref);
    list.push_back(QString::number(season));
    list.push_back(QString::number(type));
    list.push_back((pginfo)?QString::number(pginfo->GetRecordingID()):"");
    list.push_back(groupname);
    auto *e = new MythEvent("LOCATE_ARTWORK", list);
    QCoreApplication::postEvent(m_eventHandler, e);

    return {};
}

QString PlaybackBoxHelper::GetPreviewImage(
    const ProgramInfo &pginfo, bool check_availability)
{
    if (!check_availability && pginfo.GetAvailableStatus() != asAvailable)
        return {};

    if (pginfo.GetAvailableStatus() == asPendingDelete)
        return {};

    QString token = QString("%1:%2")
        .arg(pginfo.MakeUniqueKey()).arg(MythRandom());

    QStringList extra(token);
    extra.push_back(check_availability?"1":"0");
    pginfo.ToStringList(extra);
    auto *e = new MythEvent("GET_PREVIEW", extra);
    QCoreApplication::postEvent(m_eventHandler, e);

    return token;
}
