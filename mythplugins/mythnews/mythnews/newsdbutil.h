#ifndef NEWSDBUTIL_H_
#define NEWSDBUTIL_H_

class QString;
class NewsSiteItem;

bool findInDB(const QString &name);
bool insertInDB(NewsSiteItem *site);
bool insertInDB(const QString &name, const QString &url,
                const QString &icon, const QString &category,
                bool podcast);

bool removeFromDB(NewsSiteItem *site);
bool removeFromDB(const QString &name);

#endif
