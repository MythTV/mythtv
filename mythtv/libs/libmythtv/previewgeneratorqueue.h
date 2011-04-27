// -*- Mode: c++ -*-
#ifndef _PREVIEW_GENERATOR_QUEUE_H_
#define _PREVIEW_GENERATOR_QUEUE_H_

#include <QStringList>
#include <QDateTime>
#include <QThread>
#include <QMutex>
#include <QMap>
#include <QSet>

#include "mythtvexp.h"
#include "previewgenerator.h"
#include "mythtvexp.h"

class ProgramInfo;
class QSize;

class PreviewGenState
{
  public:
    PreviewGenState() :
        gen(NULL), genStarted(false),
        attempts(0), lastBlockTime(0) {}
    PreviewGenerator *gen;
    bool              genStarted;
    uint              attempts;
    uint              lastBlockTime;
    QDateTime         blockRetryUntil;
    QSet<QString>     tokens;
};
typedef QMap<QString,PreviewGenState> PreviewMap;

class MTV_PUBLIC PreviewGeneratorQueue : public QThread
{
    Q_OBJECT

  public:
    static void CreatePreviewGeneratorQueue(
        PreviewGenerator::Mode mode,
        uint maxAttempts, uint minBlockSeconds);
    static void TeardownPreviewGeneratorQueue();

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
    void run(void);

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
    static PreviewGeneratorQueue *s_pgq;
    QSet<QObject*> m_listeners;

    mutable QMutex         m_lock;
    PreviewGenerator::Mode m_mode;
    PreviewMap             m_previewMap;
    QMap<QString,QString>  m_tokenToKeyMap;
    QStringList            m_queue;
    uint                   m_running;
    uint                   m_maxThreads;
    uint                   m_maxAttempts;
    uint                   m_minBlockSeconds;
};

#endif // _PREVIEW_GENERATOR_QUEUE_H_
