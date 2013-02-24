#ifndef NETUTILS_H_
#define NETUTILS_H_

#include <QString>
#include <QDateTime>

#include "rssparse.h"
#include "netgrabbermanager.h"
#include "mythrssmanager.h"

// Generic Data Formatting
MPUBLIC QString GetDownloadFilename(QString title, QString url);

// Tree DB Utils

MPUBLIC bool findTreeGrabberInDB(const QString &commandline, ArticleType type);
MPUBLIC GrabberScript* findTreeGrabberByCommand(const QString &url, ArticleType type);
MPUBLIC GrabberScript::scriptList findAllDBTreeGrabbersByHost(ArticleType type);
MPUBLIC GrabberScript::scriptList findAllDBTreeGrabbers(void);
MPUBLIC bool findSearchGrabberInDB(const QString &commandline, ArticleType type);
MPUBLIC GrabberScript* findSearchGrabberByCommand(const QString &url, ArticleType type);
MPUBLIC GrabberScript::scriptList findAllDBSearchGrabbers(ArticleType type);
MPUBLIC bool markTreeUpdated(GrabberScript *script, QDateTime curTime);
MPUBLIC bool needsUpdate(GrabberScript *script, uint updateFreq);
MPUBLIC QDateTime lastUpdate(GrabberScript* script);

MPUBLIC bool clearTreeItems(const QString &feedcommand);
MPUBLIC bool isTreeInUse(const QString &feedcommand);
MPUBLIC bool insertTreeArticleInDB(const QString &feedtitle, const QString &path,
                       const QString &paththumb, ResultItem *item, ArticleType type);
MPUBLIC QMultiMap<QPair<QString,QString>, ResultItem*> getTreeArticles(const QString &feedtitle,
                                                                       ArticleType type);

MPUBLIC bool insertTreeInDB(GrabberScript *script, ArticleType type);
MPUBLIC bool insertSearchInDB(GrabberScript *script, ArticleType type);
MPUBLIC bool insertGrabberInDB(const QString &name, const QString &thumbnail,
                ArticleType type, const QString &author,
                const QString &description, const QString &commandline,
                const double &version, bool search, bool tree,
                bool podcast);

MPUBLIC bool removeTreeFromDB(GrabberScript *script);
MPUBLIC bool removeSearchFromDB(GrabberScript *script);
MPUBLIC bool removeGrabberFromDB(const QString &commandline, const bool &search);

// RSS DB Utils

MPUBLIC bool findInDB(const QString &url, ArticleType type);
MPUBLIC RSSSite* findByURL(const QString &url, ArticleType type);
MPUBLIC RSSSite::rssList findAllDBRSS(void);
MPUBLIC RSSSite::rssList findAllDBRSSByType(ArticleType type);
MPUBLIC bool insertInDB(RSSSite *site);
MPUBLIC bool insertInDB(const QString &name, const QString &thumbnail,
                const QString &description, const QString &url,
                const QString &author, const bool &download,
                const QDateTime &updated, ArticleType type);

MPUBLIC bool removeFromDB(RSSSite *site);
MPUBLIC bool removeFromDB(const QString &url, ArticleType type);

MPUBLIC void markUpdated(RSSSite *site);
MPUBLIC bool clearRSSArticles(const QString &feedtitle, ArticleType type);
MPUBLIC bool insertRSSArticleInDB(const QString &feedtitle, ResultItem *item,
                                  ArticleType type);
MPUBLIC ResultItem::resultList getRSSArticles(const QString &feedtitle,
                                              ArticleType type);

#endif // NETUTILS_H_
