#ifndef _FREE_SPACE_H_
#define _FREE_SPACE_H_

#include <stdint.h>

#include <QStringList>
#include <QDateTime>
#include <QThread>
#include <QMutex>
#include <QMap>

class PreviewGenerator;
class PBHEventHandler;
class ProgramInfo;
class QStringList;
class QObject;
class QTimer;

class PreviewGenState
{
  public:
    PreviewGenState() :
        gen(NULL), genStarted(false), ready(false),
        attempts(0), lastBlockTime(0) {}
    PreviewGenerator *gen;
    bool              genStarted;
    bool              ready;
    uint              attempts;
    uint              lastBlockTime;
    QDateTime         blockRetryUntil;

    static const uint maxAttempts;
    static const uint minBlockSeconds;
};
typedef QMap<QString,PreviewGenState> PreviewMap;
typedef QMap<QString,QDateTime>       FileTimeStampMap;

typedef enum CheckAvailabilityType {
    kCheckForCache,
    kCheckForMenuAction,
    kCheckForPlayAction,
    kCheckForPlaylistAction,
} CheckAvailabilityType;

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
    void GetPreviewImage(const ProgramInfo&);

    virtual void run(void);      // QThread

    uint64_t GetFreeSpaceTotalMB(void) const;
    uint64_t GetFreeSpaceUsedMB(void) const;

  private slots:
    void previewThreadDone(const QString &fn, bool &success);
    void previewReady(const ProgramInfo *pginfo);

  private:
    void UpdateFreeSpace(void);

    QString GeneratePreviewImage(ProgramInfo &pginfo);
    bool SetPreviewGenerator(const QString &fn, PreviewGenerator *g);
    void IncPreviewGeneratorPriority(const QString &fn);
    void UpdatePreviewGeneratorThreads(void);
    bool IsGeneratingPreview(const QString &fn, bool really = false) const;
    uint IncPreviewGeneratorAttempts(const QString &fn);
    void ClearPreviewGeneratorAttempts(const QString &fn);

  private:
    QObject            *m_listener;
    PBHEventHandler    *m_eventHandler;
    mutable QMutex      m_lock;

    // Free disk space tracking
    uint64_t            m_freeSpaceTotalMB;
    uint64_t            m_freeSpaceUsedMB;

    // Preview Pixmap Variables ///////////////////////////////////////////////
    mutable QMutex      m_previewGeneratorLock;
    uint                m_previewGeneratorMode;
    FileTimeStampMap    m_previewFileTS;
    bool                m_previewSuspend;
    PreviewMap          m_previewGenerator;
    QStringList         m_previewGeneratorQueue;
    uint                m_previewGeneratorRunning;
    uint                m_previewGeneratorMaxThreads;
};

#endif // _FREE_SPACE_H_
