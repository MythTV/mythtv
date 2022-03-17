// -*- Mode: c++ -*-
#ifndef PREVIEW_GENERATOR_QUEUE_H
#define PREVIEW_GENERATOR_QUEUE_H

#include <QStringList>
#include <QDateTime>
#include <QMutex>
#include <QMap>
#include <QSet>

#include "libmythbase/mthread.h"

#include "previewgenerator.h"
#include "mythtvexp.h"

class ProgramInfo;
class QSize;

/**
 * This class holds all the state information related to a specific
 * preview generator.
 */
class PreviewGenState
{
  public:
    PreviewGenState() = default;

    /// A pointer to the generator that this state object describes.
    PreviewGenerator *m_gen           {nullptr};

    /// The preview generator for this file is currently running.
    bool              m_genStarted    {false};

    /// How many attempts have been made to generate a preview for
    /// this file.
    uint              m_attempts      {0};

    /// The amount of time (in seconds) that this generator was
    /// blocked before it could start. Initialized to zero.
    std::chrono::seconds m_lastBlockTime {0s};

    /// Any request to generate an image will be ignored until after
    /// this time.
    QDateTime         m_blockRetryUntil;

    /// The full set of tokens for all callers that have requested
    /// this preview.
    QSet<QString>     m_tokens;
};
using PreviewMap = QMap<QString,PreviewGenState>;

/**
 * This class implements a queue of preview generation requests.  It
 * has a rendezvous mechanism (messaging) so that the actual
 * generation can run in it own separate thread and not affect the
 * program main thread. It also implements an API for starting and
 * retrieving information about requests.  This queue is effectively a
 * singleton, although the code does not enforce that.  It is expected
 * that the queue will be created at application startup, and torn
 * down at application shutdown.
 *
 * Preview requestors use tokens to refer to their request.  These
 * have no meaning to the preview generator code, and are mapped to
 * keys which are the used internally for indexing.  Multiple caller
 * tokens can map to the same internal key. (I.E. A preview for a
 * program was requested from two different parts of the code.)
 */
class MTV_PUBLIC PreviewGeneratorQueue : public QObject, public MThread
{
    Q_OBJECT

  public:
    static void CreatePreviewGeneratorQueue(
        PreviewGenerator::Mode mode,
        uint maxAttempts, std::chrono::seconds minBlockSeconds);
    static void TeardownPreviewGeneratorQueue();

    /**
     * Submit a request for the generation of a preview image.  This
     * information will be packaged up into an event and sent over to the
     * worker thread that generates previews.
     *
     * \param[in] pginfo Generate the image for this program.
     * \param[in] token A user specified value used to match up this
     *            request with the response from the backend, and as a key for
     *            some indexing.  A token isn't required, but is strongly
     *            suggested.
     */
    static void GetPreviewImage(const ProgramInfo &pginfo, const QString& token)
    {
        GetPreviewImage(pginfo, QSize(0,0), "", -1s, -1, token);
    }
    static void GetPreviewImage(const ProgramInfo &pginfo, QSize outputsize,
                                const QString &outputfile,
                                std::chrono::seconds time, long long frame,
                                const QString& token);
    static void AddListener(QObject *listener);
    static void RemoveListener(QObject *listener);

  private:
    PreviewGeneratorQueue(PreviewGenerator::Mode mode,
                          uint maxAttempts, std::chrono::seconds minBlockSeconds);
    ~PreviewGeneratorQueue() override;

    QString GeneratePreviewImage(ProgramInfo &pginfo, QSize size,
                                 const QString &outputfile,
                                 std::chrono::seconds time, long long frame,
                                 const QString& token);

    void GetInfo(const QString &key, uint &queue_depth, uint &token_cnt);
    void SetPreviewGenerator(const QString &key, PreviewGenerator *g);
    void IncPreviewGeneratorPriority(const QString &key, const QString& token);
    void UpdatePreviewGeneratorThreads(void);
    bool IsGeneratingPreview(const QString &key) const;
    uint IncPreviewGeneratorAttempts(const QString &key);
    void ClearPreviewGeneratorAttempts(const QString &key);

    bool event(QEvent *e) override; // QObject

    void SendEvent(const ProgramInfo &pginfo,
                   const QString     &eventname,
                   const QString     &filename,
                   const QString     &token,
                   const QString     &msg,
                   const QDateTime   &dt);

  private:
    /// The singleton queue. Allows all the static functions to find
    /// the queue.
    static PreviewGeneratorQueue *s_pgq;
    /// The set of all listeners that want messages when a preview
    /// request is queued or finishes.
    QSet<QObject*> m_listeners;
    /// The thread interlock for this data structure.
    mutable QMutex         m_lock;
    ///
    PreviewGenerator::Mode m_mode;
    /// A mapping from the generated preview name to the state
    /// information on the progress of generating the preview.
    PreviewMap             m_previewMap;
    /// A mapping from requestor tokens to internal keys.
    QMap<QString,QString>  m_tokenToKeyMap;
    /// The queue of previews to be generated. The next item to be
    /// processed is the one at the *back* of the queue.
    QStringList            m_queue;
    /// The number of threads currently generating previews.
    uint                   m_running    {0};
    /// The maximum number of threads that may concurrently generate
    /// previews.
    uint                   m_maxThreads {2};
    /// How many times total will the code attempt to generate a
    /// preview for a specific file, before giving up and ignoring all
    /// future requests.
    uint                   m_maxAttempts;
    /// How long after a failed preview generation attempt will the
    /// code ignore subsequent requests.
    std::chrono::seconds   m_minBlockSeconds;
};

#endif // PREVIEW_GENERATOR_QUEUE_H
