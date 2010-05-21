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
