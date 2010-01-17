#ifndef SEARCH_H
#define SEARCH_H

#include <QString>
#include <QList>
#include <QObject>
#include <QProcess>
#include <QDomDocument>
#include <QDateTime>
#include <QPair>
#include <QMap>
#include <QTimer>

#include "parse.h"

class Search;
class QDomDocument;

class Search : public QObject
{
    friend class MRSSParser;
    Q_OBJECT

  public:

    Search();
    ~Search();

    void reset(void);
    void executeSearch(const QString &script, const QString &query,
                       uint pagenum = 1);
    void process(void);

    uint numResults();
    uint numReturned();
    uint numIndex();

    ResultVideo::resultList GetVideoList();

  private:

    QProcess       *m_searchProcess;

    QByteArray     m_data;
    QDomDocument   m_document;
    QTimer         *m_timer;
    ResultVideo::resultList m_videoList;

    uint parseNumResults(QDomDocument domDoc);
    uint parseNumReturned(QDomDocument domDoc);
    uint parseNumIndex(QDomDocument domDoc);

  signals:

    void finishedSearch(Search *item);
    void searchTimedOut(Search *item);

  private slots:

    void slotProcessSearchExit(int exitcode,
                               QProcess::ExitStatus exitstatus);
    void slotSearchTimeout(void);
};


#endif
