#ifndef NETGRABBERMANAGER_H
#define NETGRABBERMANAGER_H

#include <QObject>
#include <QEvent>
#include <QDomElement>
#include "mthread.h"
#include <QMetaType>
#include <QMutex>
#include <QTimer>

#include "rssparse.h"
#include "mythexp.h"
#include "mythsystemlegacy.h"

class MPUBLIC GrabberScript : public QObject, public MThread
{

    Q_OBJECT

  public:

    GrabberScript(const QString& title,
                  const QString& image,
                  const ArticleType& type,
                  const QString& author,
                  const bool& search,
                  const bool& tree,
                  const QString& description,
                  const QString& commandline,
                  const double& version);
    ~GrabberScript();

    const QString& GetTitle() const { return m_title; }
    const QString& GetImage() const { return m_image; }
    const ArticleType& GetType() const { return m_type; }
    const QString& GetAuthor() const { return m_author; }
    const bool& GetSearch() const { return m_search; }
    const bool& GetTree() const { return m_tree; }
    const QString& GetDescription() const { return m_description; }
    const QString& GetCommandline() const { return m_commandline; }
    const double& GetVersion() const { return m_version; }

    virtual void run(void);

    typedef QList<GrabberScript *> scriptList;

  signals:

    void finished(void);

  private:

    void parseDBTree(const QString &feedtitle, const QString &path,
                     const QString &pathThumb, QDomElement& domElem,
                     const ArticleType &type);
    mutable QMutex m_lock;

    QString     m_title;
    QString     m_image;
    ArticleType m_type;
    QString     m_author;
    bool        m_search;
    bool        m_tree;
    QString     m_description;
    QString     m_commandline;
    double      m_version;
};
Q_DECLARE_METATYPE(GrabberScript *);

class MPUBLIC GrabberManager : public QObject
{
    Q_OBJECT

  public:
    GrabberManager();
    ~GrabberManager();
    void startTimer();
    void stopTimer();
    void doUpdate();
    void refreshAll();

  signals:
    void finished(void);

  private slots:
    void timeout(void);

  private:

    mutable QMutex                 m_lock;
    QTimer                        *m_timer;
    GrabberScript::scriptList      m_scripts;
    uint                           m_updateFreq;
    uint                           m_runningCount;
    bool                           m_refreshAll;
};

const int kGrabberUpdateEventType = QEvent::User + 5000;

class MPUBLIC GrabberUpdateEvent : public QEvent
{
  public:
    GrabberUpdateEvent(void)
         : QEvent((QEvent::Type)kGrabberUpdateEventType) {}
    ~GrabberUpdateEvent() {}
};

class MPUBLIC GrabberDownloadThread : public QObject, public MThread
{
    Q_OBJECT

  public:

    GrabberDownloadThread(QObject *parent);
    ~GrabberDownloadThread();
    
    void refreshAll();
    void cancel();

  signals:
    void finished();

  protected:

    void run();

  private:

    QObject               *m_parent;
    QList<GrabberScript*>  m_scripts;
    QMutex                 m_mutex;
    bool                   m_refreshAll;

};

class MPUBLIC Search : public QObject
{
    friend class MRSSParser;
    Q_OBJECT

  public:

    Search();
    ~Search();

    void resetSearch(void);
    void executeSearch(const QString &script, const QString &query,
                       uint pagenum = 1);
    void process(void);

    QByteArray GetData() { return m_data; };
    void SetData(QByteArray data);

    uint numResults() { return m_numResults; };
    uint numReturned() { return m_numReturned; };
    uint numIndex() { return m_numIndex; };

    ResultItem::resultList GetVideoList() { return m_videoList; };

  private:

    MythSystemLegacy             *m_searchProcess;

    QByteArray              m_data;
    QDomDocument            m_document;
    ResultItem::resultList  m_videoList;

    uint                    m_numResults;
    uint                    m_numReturned;
    uint                    m_numIndex;

  signals:

    void finishedSearch(Search *item);
    void searchTimedOut(Search *item);

  private slots:

    void slotProcessSearchExit(uint exitcode = 0);
};

#endif
