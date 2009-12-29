#include <QCoreApplication>
#include <QStringList>
#include <QDateTime>

#include "playbackboxhelper.h"
#include "tvremoteutil.h"
#include "programinfo.h"
#include "remoteutil.h"
#include "mythevent.h"

class PBHEventHandler : public QObject
{
  public:
    PBHEventHandler(PlaybackBoxHelper &pbh) : m_pbh(pbh) { }
    virtual bool event(QEvent*); // QObject
    PlaybackBoxHelper &m_pbh;
};

bool PBHEventHandler::event(QEvent *e)
{
    if (e->type() == QEvent::Timer)
    {
        QTimerEvent *te = (QTimerEvent*)e;
        const int timer_id = te->timerId();
        if (timer_id == m_pbh.m_freeSpaceTimerId)
            m_pbh.UpdateFreeSpace();
    }
    else if (e->type() == (QEvent::Type) MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent*)e;
        if (me->Message() == "UPDATE_FREE_SPACE")
        {
            m_pbh.UpdateFreeSpace();
            return true;
        }
        else if (me->Message() == "STOP_RECORDING")
        {
            ProgramInfo pginfo;
            if (pginfo.FromStringList(me->ExtraDataList(), 0))
                RemoteStopRecording(&pginfo);
            return true;
        }
        else if (me->Message() == "DELETE_RECORDINGS")
        {
            QStringList successes;
            QStringList failures;
            QStringList list = me->ExtraDataList();
            while (list.size() >= 3)
            {
                uint      chanid        = list[0].toUInt();
                QDateTime recstartts    = QDateTime::fromString(
                    list[1], Qt::ISODate);
                bool      forceDelete   = list[2].toUInt();

                bool ok = RemoteDeleteRecording(
                    chanid, recstartts, forceDelete);

                QStringList &res = (ok) ? successes : failures;
                for (uint i = 0; i < 3; i++)
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
        }
    }

    return QObject::event(e);
}

//////////////////////////////////////////////////////////////////////

uint PlaybackBoxHelper::kUpdateFreeSpaceInterval = 15000; // 15 seconds

PlaybackBoxHelper::PlaybackBoxHelper(QObject *listener) :
    m_listener(listener), m_eventHandler(NULL),
    m_freeSpaceTimerId(0), m_freeSpaceTotalMB(0ULL), m_freeSpaceUsedMB(0ULL)
{
    start();
}

PlaybackBoxHelper::~PlaybackBoxHelper()
{
    exit();
    wait();
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
    uint chanid, const QDateTime &recstartts, bool forceDelete)
{
    QStringList list;
    list.push_back(QString::number(chanid));
    list.push_back(recstartts.toString(Qt::ISODate));
    list.push_back((forceDelete)    ? "1" : "0");
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
    if (m_freeSpaceTimerId)
        killTimer(m_freeSpaceTimerId);

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

    m_freeSpaceTimerId = startTimer(kUpdateFreeSpaceInterval);
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
