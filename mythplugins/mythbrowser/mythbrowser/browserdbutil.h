#ifndef BROWSERDBUTIL_H_
#define BROWSERDBUTIL_H_

#include <QStringList>

class Bookmark;

bool UpgradeBrowserDatabaseSchema(void);

bool FindInDB(const QString &category, const QString& name);
bool InsertInDB(Bookmark *site);
bool InsertInDB(const QString &category, const QString &name, const QString &url, const bool &isHomepage);

bool ResetHomepageFromDB();
bool UpdateHomepageInDB(Bookmark* site);

bool RemoveFromDB(Bookmark *site);
bool RemoveFromDB(const QString &category, const QString &name);

int  GetCategoryList(QStringList &list);
int  GetSiteList(QList<Bookmark*>  &siteList);

#endif
