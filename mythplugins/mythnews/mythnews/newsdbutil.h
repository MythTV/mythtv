#ifndef NEWSDBUTIL_H_
#define NEWSDBUTIL_H_

#include "newsengine.h"

bool findInDB(const QString& name);
bool insertInDB(NewsSiteItem* site);
bool insertInDB(const QString &name, const QString &url,
                          const QString &icon, const QString &category);

bool removeFromDB(NewsSiteItem* site);
bool removeFromDB(const QString &name);

#endif
