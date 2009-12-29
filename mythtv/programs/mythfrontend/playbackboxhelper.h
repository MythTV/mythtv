#ifndef _FREE_SPACE_H_
#define _FREE_SPACE_H_

#include <stdint.h>

#include <QThread>
#include <QMutex>

class PBHEventHandler;
class ProgramInfo;
class QStringList;
class QDateTime;
class QObject;
class QTimer;

class PlaybackBoxHelper : public QThread
{
    friend class PBHEventHandler;

  public:
    PlaybackBoxHelper(QObject *listener);
    ~PlaybackBoxHelper(void);

    void ForceFreeSpaceUpdate(void);
    void StopRecording(const ProgramInfo&);
    void DeleteRecording(
        uint chanid, const QDateTime &recstartts, bool forceDelete);
    void DeleteRecordings(const QStringList&);
    void UndeleteRecording(uint chanid, const QDateTime &recstartts);

    virtual void run(void);      // QThread

    uint64_t GetFreeSpaceTotalMB(void) const;
    uint64_t GetFreeSpaceUsedMB(void) const;

  private:
    void UpdateFreeSpace(void);

  private:
    QObject            *m_listener;
    PBHEventHandler    *m_eventHandler;
    mutable QMutex      m_lock;

    // Free disk space tracking
    int                 m_freeSpaceTimerId;
    uint64_t            m_freeSpaceTotalMB;
    uint64_t            m_freeSpaceUsedMB;

    static uint kUpdateFreeSpaceInterval;
};

#endif // _FREE_SPACE_H_
