#ifndef NETGRABBERMANAGER_H
#define NETGRABBERMANAGER_H

#include <QObject>
#include <QEvent>
#include <QDomElement>
#include <QMetaType>
#include <QMutex>
#include <QRecursiveMutex>
#include <QTimer>

#include "libmythbase/mthread.h"
#include "libmythbase/mythbaseexp.h"
#include "libmythbase/mythsystemlegacy.h"
#include "libmythbase/rssparse.h"

class MBASE_PUBLIC GrabberScript : public QObject, public MThread
{

    Q_OBJECT

  public:

    GrabberScript(QString title,
                  QString image,
                  ArticleType type,
                  QString author,
                  bool search,
                  bool tree,
                  QString description,
                  QString commandline,
                  double version)
        : MThread("GrabberScript"),
          m_title(std::move(title)), m_image(std::move(image)), m_type(type),
          m_author(std::move(author)), m_search(search), m_tree(tree),
          m_description(std::move(description)),
          m_commandline(std::move(commandline)), m_version(version) {};
    ~GrabberScript() override;

    const QString& GetTitle() const { return m_title; }
    const QString& GetImage() const { return m_image; }
    const ArticleType& GetType() const { return m_type; }
    const QString& GetAuthor() const { return m_author; }
    const bool& GetSearch() const { return m_search; }
    const bool& GetTree() const { return m_tree; }
    const QString& GetDescription() const { return m_description; }
    const QString& GetCommandline() const { return m_commandline; }
    const double& GetVersion() const { return m_version; }

    void run(void) override; // MThread

    using scriptList = QList<GrabberScript *>;

  signals:

    void finished(void);

  private:

    void parseDBTree(const QString &feedtitle, const QString &path,
                     const QString &pathThumb, QDomElement& domElem,
                     ArticleType type);
    mutable QRecursiveMutex m_lock;

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

class MBASE_PUBLIC GrabberManager : public QObject
{
    Q_OBJECT

  public:
    GrabberManager();
    ~GrabberManager() override;
    void startTimer();
    void stopTimer();
    void doUpdate();
    void refreshAll();

  signals:
    void finished(void);

  private slots:
    void timeout(void);

  private:

    mutable QRecursiveMutex        m_lock;
    QTimer                        *m_timer        {nullptr};
    GrabberScript::scriptList      m_scripts;
    std::chrono::hours             m_updateFreq   {24h};
    uint                           m_runningCount {0};
    bool                           m_refreshAll   {false};
};

const int kGrabberUpdateEventType = QEvent::User + 5000;

class MBASE_PUBLIC GrabberUpdateEvent : public QEvent
{
  public:
    GrabberUpdateEvent(void)
         : QEvent((QEvent::Type)kGrabberUpdateEventType) {}
    ~GrabberUpdateEvent() override = default;
};

class MBASE_PUBLIC GrabberDownloadThread : public QObject, public MThread
{
    Q_OBJECT

  public:

    explicit GrabberDownloadThread(QObject *parent);
    ~GrabberDownloadThread() override;
    
    void refreshAll();
    void cancel();

  signals:
    void finished();

  protected:

    void run() override; // MThread

  private:

    QObject               *m_parent     {nullptr};
    QList<GrabberScript*>  m_scripts;
    QMutex                 m_mutex;
    bool                   m_refreshAll {false};

};

class MBASE_PUBLIC Search : public QObject
{
    friend class MRSSParser;
    Q_OBJECT

  public:

    Search();
    ~Search() override;

    void resetSearch(void);
    void executeSearch(const QString &script, const QString &query,
                       const QString &pagenum = "");
    void process(void);

    QByteArray GetData() { return m_data; };
    void SetData(QByteArray data);

    uint numResults() const { return m_numResults; };
    uint numReturned() const { return m_numReturned; };
    uint numIndex() const { return m_numIndex; };
    QString nextPageToken() { return m_nextPageToken; }
    QString prevPageToken() { return m_prevPageToken; }

    ResultItem::resultList GetVideoList() { return m_videoList; };

  private:

    MythSystemLegacy       *m_searchProcess {nullptr};

    QByteArray              m_data;
    QDomDocument            m_document;
    ResultItem::resultList  m_videoList;

    uint                    m_numResults    {0};
    uint                    m_numReturned   {0};
    uint                    m_numIndex      {0};

    QString m_nextPageToken;
    QString m_prevPageToken;

  signals:

    void finishedSearch(Search *item);
    void searchTimedOut(Search *item);

  private slots:
    void slotProcessSearchExit(uint exitcode);
    void slotProcessSearchExit(void);
};

#endif // NETGRABBERMANAGER_H
