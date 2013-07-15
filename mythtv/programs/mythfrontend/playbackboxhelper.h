#ifndef _FREE_SPACE_H_
#define _FREE_SPACE_H_

#include <stdint.h>

#include <QStringList>
#include <QDateTime>
#include <QMutex>

#include "mythcorecontext.h"
#include "metadatacommon.h"
#include "mthread.h"
#include "mythtypes.h"

class PreviewGenerator;
class PBHEventHandler;
class ProgramInfo;
class QStringList;
class QObject;
class QTimer;

typedef enum CheckAvailabilityType {
    kCheckForCache,
    kCheckForMenuAction,
    kCheckForPlayAction,
    kCheckForPlaylistAction,
} CheckAvailabilityType;

class PlaybackBoxHelper : public MThread
{
    friend class PBHEventHandler;

  public:
    PlaybackBoxHelper(QObject *listener);
    ~PlaybackBoxHelper(void);

    void ForceFreeSpaceUpdate(void);
    void StopRecording(const ProgramInfo&);
    void DeleteRecording(
        uint chanid, const QDateTime &recstartts, 
        bool forceDelete, bool forgetHistory);
    void DeleteRecordings(const QStringList&);
    void UndeleteRecording(uint chanid, const QDateTime &recstartts);
    void CheckAvailability(const ProgramInfo&,
                           CheckAvailabilityType cat = kCheckForCache);
    QString GetPreviewImage(const ProgramInfo&, bool check_availibility = true);

    QString LocateArtwork(const QString &inetref, uint season,
                          VideoArtworkType type, const ProgramInfo *pginfo,
                          const QString &groupname = NULL);

    uint64_t GetFreeSpaceTotalMB(void) const;
    uint64_t GetFreeSpaceUsedMB(void) const;

  private:
    void UpdateFreeSpace(void);

  private:
    QObject            *m_listener;
    PBHEventHandler    *m_eventHandler;
    mutable QMutex      m_lock;

    // Free disk space tracking
    uint64_t            m_freeSpaceTotalMB;
    uint64_t            m_freeSpaceUsedMB;

    // Artwork Variables //////////////////////////////////////////////////////
    InfoMap             m_artworkCache;
};

#endif // _FREE_SPACE_H_
