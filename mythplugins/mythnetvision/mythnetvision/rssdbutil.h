#ifndef RSSDBUTIL_H_
#define RSSDBUTIL_H_

#include <QString>
#include <QDateTime>

#include "rssdbutil.h"
#include "rssmanager.h"

class QString;
class QDateTime;
class NewsSiteItem;

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

#endif
