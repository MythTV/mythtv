#ifndef NETUTILS_H_
#define NETUTILS_H_

#include <QString>
#include <QDateTime>

#include "mythbaseexp.h"
#include "rssparse.h"
#include "netgrabbermanager.h"
#include "rssmanager.h"

// Generic Data Formatting
MBASE_PUBLIC QString GetDownloadFilename(const QString& title, const QString& url);

// Tree DB Utils

MBASE_PUBLIC bool findTreeGrabberInDB(const QString &commandline, ArticleType type);
MBASE_PUBLIC GrabberScript* findTreeGrabberByCommand(const QString &url, ArticleType type);
MBASE_PUBLIC GrabberScript::scriptList findAllDBTreeGrabbersByHost(ArticleType type);
MBASE_PUBLIC GrabberScript::scriptList findAllDBTreeGrabbers(void);
MBASE_PUBLIC bool findSearchGrabberInDB(const QString &commandline, ArticleType type);
MBASE_PUBLIC GrabberScript* findSearchGrabberByCommand(const QString &url, ArticleType type);
MBASE_PUBLIC GrabberScript::scriptList findAllDBSearchGrabbers(ArticleType type);
MBASE_PUBLIC bool markTreeUpdated(GrabberScript *script, const QDateTime& curTime);
MBASE_PUBLIC bool needsUpdate(GrabberScript *script, std::chrono::hours updateFreq);
MBASE_PUBLIC QDateTime lastUpdate(GrabberScript* script);

MBASE_PUBLIC bool clearTreeItems(const QString &feedcommand);
MBASE_PUBLIC bool isTreeInUse(const QString &feedcommand);
MBASE_PUBLIC bool insertTreeArticleInDB(const QString &feedtitle, const QString &path,
                       const QString &paththumb, ResultItem *item, ArticleType type);
MBASE_PUBLIC QMultiMap<QPair<QString,QString>, ResultItem*> getTreeArticles(const QString &feedtitle,
                                                                       ArticleType type);

MBASE_PUBLIC bool insertTreeInDB(GrabberScript *script, ArticleType type);
MBASE_PUBLIC bool insertSearchInDB(GrabberScript *script, ArticleType type);
MBASE_PUBLIC bool insertGrabberInDB(const QString &name, const QString &thumbnail,
                ArticleType type, const QString &author,
                const QString &description, const QString &commandline,
                double version, bool search, bool tree,
                bool podcast);

MBASE_PUBLIC bool removeTreeFromDB(GrabberScript *script);
MBASE_PUBLIC bool removeSearchFromDB(GrabberScript *script);
MBASE_PUBLIC bool removeGrabberFromDB(const QString &commandline, bool search);

// RSS DB Utils

MBASE_PUBLIC bool findInDB(const QString &url, ArticleType type);
MBASE_PUBLIC RSSSite* findByURL(const QString &url, ArticleType type);
MBASE_PUBLIC RSSSite::rssList findAllDBRSS(void);
MBASE_PUBLIC RSSSite::rssList findAllDBRSSByType(ArticleType type);
MBASE_PUBLIC bool insertInDB(RSSSite *site);
MBASE_PUBLIC bool insertInDB(const QString &name, const QString &sortname,
                const QString &thumbnail,
                const QString &description, const QString &url,
                const QString &author, bool download,
                const QDateTime &updated, ArticleType type);

MBASE_PUBLIC bool removeFromDB(RSSSite *site);
MBASE_PUBLIC bool removeFromDB(const QString &url, ArticleType type);

MBASE_PUBLIC void markUpdated(RSSSite *site);
MBASE_PUBLIC bool clearRSSArticles(const QString &feedtitle, ArticleType type);
MBASE_PUBLIC bool insertRSSArticleInDB(const QString &feedtitle, ResultItem *item,
                                  ArticleType type);
MBASE_PUBLIC ResultItem::resultList getRSSArticles(const QString &feedtitle,
                                              ArticleType type);

#endif // NETUTILS_H_
