// Myth headers
#include <libmythbase/mythdb.h>

// MythNews headers
#include "newsdbutil.h"
#include "newssite.h"

bool findInDB(const QString& name)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT name FROM newssites WHERE name = :NAME ;");
    query.bindValue(":NAME", name);
    if (!query.exec() || !query.isActive()) {
        MythDB::DBError("new find in db", query);
        return false;
    }

    return query.size() > 0;
}

bool insertInDB(NewsSiteItem* site)
{
    if (!site) return false;

    return insertInDB(site->m_name, site->m_url, site->m_ico, site->m_category,
                      site->m_podcast);
}

bool insertInDB(const QString &name, const QString &url,
                          const QString &icon, const QString &category,
                          const bool podcast)
{
    if (findInDB(name))
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO newssites (name,category,url,ico,podcast,updated) "
            " VALUES( :NAME, :CATEGORY, :URL, :ICON, :PODCAST, 0);");
    query.bindValue(":NAME", name);
    query.bindValue(":CATEGORY", category);
    query.bindValue(":URL", url);
    query.bindValue(":ICON", icon);
    query.bindValue(":PODCAST", podcast);
    if (!query.exec() || !query.isActive()) {
        MythDB::DBError("news: inserting in DB", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}

bool removeFromDB(NewsSiteItem* site)
{
    if (!site) return false;

    return removeFromDB(site->m_name);
}

bool removeFromDB(const QString &name)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM newssites WHERE name = :NAME ;");
    query.bindValue(":NAME", name);
    if (!query.exec() || !query.isActive()) {
        MythDB::DBError("news: delete from db", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}
