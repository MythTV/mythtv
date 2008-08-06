// Myth headers
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

// MythNews headers
#include "newsdbutil.h"

bool findInDB(const QString& name)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT name FROM newssites WHERE name = :NAME ;");
    query.bindValue(":NAME", name);
    if (!query.exec() || !query.isActive()) {
        MythContext::DBError("new find in db", query);
        return false;
    }

    return query.size() > 0;
}

bool insertInDB(NewsSiteItem* site)
{
    if (!site) return false;

    return insertInDB(site->name, site->url, site->ico, site->category,
                      site->podcast);
}

bool insertInDB(const QString &name, const QString &url,
                          const QString &icon, const QString &category,
                          const bool &podcast)
{
    if (findInDB(name))
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO newssites (name,category,url,ico,podcast) "
            " VALUES( :NAME, :CATEGORY, :URL, :ICON, :PODCAST );");
    query.bindValue(":NAME", name);
    query.bindValue(":CATEGORY", category);
    query.bindValue(":URL", url);
    query.bindValue(":ICON", icon);
    query.bindValue(":PODCAST", podcast);
    if (!query.exec() || !query.isActive()) {
        MythContext::DBError("news: inserting in DB", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}

bool removeFromDB(NewsSiteItem* site)
{
    if (!site) return false;

    return removeFromDB(site->name);
}

bool removeFromDB(const QString &name)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM newssites WHERE name = :NAME ;");
    query.bindValue(":NAME", name);
    if (!query.exec() || !query.isActive()) {
        MythContext::DBError("news: delete from db", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}
