#ifndef NETUTILS_H_
#define NETUTILS_H_

#include <QString>
#include <QDateTime>

#include "rssparse.h"
#include "netgrabbermanager.h"
#include "mythrssmanager.h"

// Generic Data Formatting
QString GetDisplaySeasonEpisode(int seasEp, int digits);
QString GetThumbnailFilename(QString url, QString title);

// Tree DB Utils

bool findTreeGrabberInDB(const QString &commandline);
GrabberScript* findTreeGrabberByCommand(const QString &url);
GrabberScript::scriptList findAllDBTreeGrabbers(void);
bool findSearchGrabberInDB(const QString &commandline);
GrabberScript* findSearchGrabberByCommand(const QString &url);
GrabberScript::scriptList findAllDBSearchGrabbers(void);
bool markTreeUpdated(GrabberScript *script, QDateTime curTime);
bool needsUpdate(GrabberScript *script, uint updateFreq);
QDateTime lastUpdate(GrabberScript* script);

bool clearTreeItems(const QString &feedtitle);
bool isTreeInUse(const QString &feedtitle);
bool insertTreeArticleInDB(const QString &feedtitle, const QString &path,
                       const QString &paththumb, ResultVideo *item);
QMultiMap<QPair<QString,QString>, ResultVideo*> getTreeArticles(const QString &feedtitle);

bool insertTreeInDB(GrabberScript *script);
bool insertTreeInDB(const QString &name, const QString &thumbnail,
                const QString &commandline);
bool insertSearchInDB(GrabberScript *script);
bool insertSearchInDB(const QString &name, const QString &thumbnail,
                const QString &commandline);

bool removeTreeFromDB(GrabberScript *script);
bool removeTreeFromDB(const QString &commandline);
bool removeSearchFromDB(GrabberScript *script);
bool removeSearchFromDB(const QString &commandline);

// RSS DB Utils

bool findInDB(const QString &url);
RSSSite* findByURL(const QString &url);
RSSSite::rssList findAllDBRSS();
bool insertInDB(RSSSite *site);
bool insertInDB(const QString &name, const QString &thumbnail,
                const QString &description, const QString &url,
                const QString &author, const bool &download,
                const QDateTime &updated);

bool removeFromDB(RSSSite *site);
bool removeFromDB(const QString &url);

void markUpdated(RSSSite *site);
bool clearRSSArticles(const QString &feedtitle);
bool insertArticleInDB(const QString &feedtitle, ResultVideo *item);
ResultVideo::resultList getRSSArticles(const QString &feedtitle);

#endif // NETUTILS_H_
