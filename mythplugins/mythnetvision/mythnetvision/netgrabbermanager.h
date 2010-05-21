#ifndef NETGRABBERMANAGER_H
#define NETGRABBERMANAGER_H

#include <QObject>
#include <QEvent>
#include <QDomElement>
#include <QThread>
#include <QMetaType>
#include <QMutex>
#include <QTimer>
#include <QProcess>

#include "rssparse.h"

class GrabberScript : public QThread
{

    Q_OBJECT

  public:

    GrabberScript(const QString& title,
                  const QString& image,
                  const bool& search,
                  const bool& tree,
                  const QString& commandline);
    ~GrabberScript();

    const QString& GetTitle() const { return m_title; }
    const QString& GetImage() const { return m_image; }
    const bool& GetSearch() const { return m_search; }
    const bool& GetTree() const { return m_tree; }
    const QString& GetCommandline() const { return m_commandline; }

    virtual void run(void);

    typedef QList<GrabberScript *> scriptList;

  signals:

    void finished(void);

  private:

    void parseDBTree(const QString &feedtitle, const QString &path,
                     const QString &pathThumb, QDomElement& domElem);
    mutable QMutex m_lock;

    QString   m_title;
    QString   m_image;
    bool      m_rss;
    bool      m_search;
    bool      m_tree;
    QString   m_commandline;
    QProcess  m_getTree;
};
Q_DECLARE_METATYPE(GrabberScript *);

class GrabberManager : public QObject
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

class GrabberUpdateEvent : public QEvent
{
  public:
    GrabberUpdateEvent(void)
         : QEvent((QEvent::Type)kGrabberUpdateEventType) {}
    ~GrabberUpdateEvent() {}
};

class GrabberDownloadThread : public QThread
{
  public:

    GrabberDownloadThread(QObject *parent);
    ~GrabberDownloadThread();
    
    void refreshAll();
    void cancel();

  protected:

    void run();

  private:

    QObject               *m_parent;
    QList<GrabberScript*>  m_scripts;
    QMutex                 m_mutex;
    bool                   m_refreshAll;

};

class Search : public QObject
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

    uint numResults() { return m_numResults; };
    uint numReturned() { return m_numReturned; };
    uint numIndex() { return m_numIndex; };

    ResultVideo::resultList GetVideoList() { return m_videoList; };

  private:

    QProcess               *m_searchProcess;

    QByteArray              m_data;
    QDomDocument            m_document;
    QTimer                 *m_searchtimer;
    ResultVideo::resultList m_videoList;

    uint                    m_numResults;
    uint                    m_numReturned;
    uint                    m_numIndex;

  signals:

    void finishedSearch(Search *item);
    void searchTimedOut(Search *item);

  private slots:

    void slotProcessSearchExit(int exitcode,
                               QProcess::ExitStatus exitstatus);
    void slotSearchTimeout(void);
};

#endif
