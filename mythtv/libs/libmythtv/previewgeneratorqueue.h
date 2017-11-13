// -*- Mode: c++ -*-
#ifndef _PREVIEW_GENERATOR_QUEUE_H_
#define _PREVIEW_GENERATOR_QUEUE_H_

#include <QStringList>
#include <QDateTime>
#include <QMutex>
#include <QMap>
#include <QSet>

#include "previewgenerator.h"
#include "mythtvexp.h"
#include "mthread.h"

class ProgramInfo;
class QSize;

/**
 * This class holds all the state information related to a specific
 * preview generator.
 */
class PreviewGenState
{
  public:
    PreviewGenState() :
        gen(NULL), genStarted(false),
        attempts(0), lastBlockTime(0) {}

    /// A pointer to the generator that this state object describes.
    PreviewGenerator *gen;

    /// The preview generator for this file is currently running.
    bool              genStarted;

    /// How many attempts have been made to generate a preview for
    /// this file.
    uint              attempts;

    /// The amount of time (in seconds) that this generator was
    /// blocked before it could start. Initialized to zero.
    uint              lastBlockTime;

    /// Any request to generate an image will be ignored until after
    /// this time.
    QDateTime         blockRetryUntil;

    /// The full set of tokens for all callers that have requested
    /// this preview.
    QSet<QString>     tokens;
};
typedef QMap<QString,PreviewGenState> PreviewMap;

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
        uint maxAttempts, uint minBlockSeconds);
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
    static void GetPreviewImage(const ProgramInfo &pginfo, QString token)
    {
        GetPreviewImage(pginfo, QSize(0,0), "", -1, true, token);
    }
    static void GetPreviewImage(const ProgramInfo&, const QSize&,
                                const QString &outputfile,
                                long long time, bool in_seconds,
                                QString token);
    static void AddListener(QObject*);
    static void RemoveListener(QObject*);

  private:
    PreviewGeneratorQueue(PreviewGenerator::Mode mode,
                          uint maxAttempts, uint minBlockSeconds);
    ~PreviewGeneratorQueue();

    QString GeneratePreviewImage(ProgramInfo &pginfo, const QSize&,
                                 const QString &outputfile,
                                 long long time, bool in_seconds,
                                 QString token);

    void GetInfo(const QString &key, uint &queue_depth, uint &preview_tokens);
    void SetPreviewGenerator(const QString &key, PreviewGenerator *g);
    void IncPreviewGeneratorPriority(const QString &key, QString token);
    void UpdatePreviewGeneratorThreads(void);
    bool IsGeneratingPreview(const QString &key) const;
    uint IncPreviewGeneratorAttempts(const QString &key);
    void ClearPreviewGeneratorAttempts(const QString &key);

    virtual bool event(QEvent *e); // QObject

    void SendEvent(const ProgramInfo &pginfo,
                   const QString     &eventname,
                   const QString     &fn,
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
    uint                   m_running;
    /// The maximum number of threads that may concurrently generate
    /// previews.
    uint                   m_maxThreads;
    /// How many times total will the code attempt to generate a
    /// preview for a specific file, before giving up and ignoring all
    /// future requests.
    uint                   m_maxAttempts;
    /// How long after a failed preview generation attempt will the
    /// code ignore subsequent requests.
    uint                   m_minBlockSeconds;
};

#endif // _PREVIEW_GENERATOR_QUEUE_H_
