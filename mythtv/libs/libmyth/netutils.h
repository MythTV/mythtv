#ifndef NETUTILS_H_
#define NETUTILS_H_

#include <QString>
#include <QDateTime>

#include "rssparse.h"
#include "netgrabbermanager.h"
#include "mythrssmanager.h"

// Generic Data Formatting
MPUBLIC QString GetDisplaySeasonEpisode(int seasEp, int digits);
MPUBLIC QString GetThumbnailFilename(QString url, QString title);

// Tree DB Utils

MPUBLIC bool findTreeGrabberInDB(const QString &commandline);
MPUBLIC GrabberScript* findTreeGrabberByCommand(const QString &url);
MPUBLIC GrabberScript::scriptList findAllDBTreeGrabbers(void);
MPUBLIC bool findSearchGrabberInDB(const QString &commandline);
MPUBLIC GrabberScript* findSearchGrabberByCommand(const QString &url);
MPUBLIC GrabberScript::scriptList findAllDBSearchGrabbers(void);
MPUBLIC bool markTreeUpdated(GrabberScript *script, QDateTime curTime);
MPUBLIC bool needsUpdate(GrabberScript *script, uint updateFreq);
MPUBLIC QDateTime lastUpdate(GrabberScript* script);

MPUBLIC bool clearTreeItems(const QString &feedtitle);
MPUBLIC bool isTreeInUse(const QString &feedtitle);
MPUBLIC bool insertTreeArticleInDB(const QString &feedtitle, const QString &path,
                       const QString &paththumb, ResultItem *item);
MPUBLIC QMultiMap<QPair<QString,QString>, ResultItem*> getTreeArticles(const QString &feedtitle);

MPUBLIC bool insertTreeInDB(GrabberScript *script);
MPUBLIC bool insertTreeInDB(const QString &name, const QString &thumbnail,
                const QString &commandline);
MPUBLIC bool insertSearchInDB(GrabberScript *script);
MPUBLIC bool insertSearchInDB(const QString &name, const QString &thumbnail,
                const QString &commandline);

MPUBLIC bool removeTreeFromDB(GrabberScript *script);
MPUBLIC bool removeTreeFromDB(const QString &commandline);
MPUBLIC bool removeSearchFromDB(GrabberScript *script);
MPUBLIC bool removeSearchFromDB(const QString &commandline);

// RSS DB Utils

MPUBLIC bool findInDB(const QString &url);
MPUBLIC RSSSite* findByURL(const QString &url);
MPUBLIC RSSSite::rssList findAllDBRSS();
MPUBLIC bool insertInDB(RSSSite *site);
MPUBLIC bool insertInDB(const QString &name, const QString &thumbnail,
                const QString &description, const QString &url,
                const QString &author, const bool &download,
                const QDateTime &updated);

MPUBLIC bool removeFromDB(RSSSite *site);
MPUBLIC bool removeFromDB(const QString &url);

MPUBLIC void markUpdated(RSSSite *site);
MPUBLIC bool clearRSSArticles(const QString &feedtitle);
MPUBLIC bool insertArticleInDB(const QString &feedtitle, ResultItem *item);
MPUBLIC ResultItem::resultList getRSSArticles(const QString &feedtitle);

#endif // NETUTILS_H_
