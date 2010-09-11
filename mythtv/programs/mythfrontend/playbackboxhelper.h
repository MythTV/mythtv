#ifndef _FREE_SPACE_H_
#define _FREE_SPACE_H_

#include <stdint.h>

#include <QStringList>
#include <QDateTime>
#include <QThread>
#include <QMutex>
#include <QHash>
#include <QMap>

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

typedef enum ArtworkTypes
{
    kArtworkFan    = 0,
    kArtworkBanner = 1,
    kArtworkCover  = 2,
} ArtworkType;
QString toString(ArtworkType t);
QString toLocalDir(ArtworkType t);
QString toSG(ArtworkType t);

class PlaybackBoxHelper : public QThread
{
    friend class PBHEventHandler;

    Q_OBJECT

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

    QString LocateArtwork(const QString &seriesid, const QString &title,
                          ArtworkType, const QString &host,
                          const ProgramInfo *pginfo);

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
    uint64_t            m_freeSpaceTotalMB;
    uint64_t            m_freeSpaceUsedMB;

    // Artwork Variables //////////////////////////////////////////////////////
    QHash<QString, QString>    m_artworkFilenameCache;
};

#endif // _FREE_SPACE_H_
