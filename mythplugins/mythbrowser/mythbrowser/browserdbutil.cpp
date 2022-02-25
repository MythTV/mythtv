// Qt
#include <QSqlError>

// MythTV
#include <libmyth/mythcontext.h>
#include <libmythbase/mythdb.h>
#include <libmythbase/mythdbcheck.h>
#include <libmythbase/mythsorthelper.h>

// mythbrowser
#include "bookmarkmanager.h"
#include "browserdbutil.h"

const QString currentDatabaseVersion = "1003";
const QString MythBrowserVersionName = "BrowserDBSchemaVer";

bool UpgradeBrowserDatabaseSchema(void)
{
    QString dbver = gCoreContext->GetSetting("BrowserDBSchemaVer");

    if (dbver == currentDatabaseVersion)
        return true;

    if (dbver == "")
    {
        LOG(VB_GENERAL, LOG_NOTICE,
            "Inserting MythBrowser initial database information.");

        DBUpdates updates
        {
            "DROP TABLE IF EXISTS websites;",
            "CREATE TABLE websites ("
            "id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY, "
            "category VARCHAR(100) NOT NULL, "
            "name VARCHAR(100) NOT NULL, "
            "url VARCHAR(255) NOT NULL);"
        };
        if (!performActualUpdate("MythBrowser", MythBrowserVersionName,
                                 updates, "1000", dbver))
            return false;
    }

    if (dbver == "1000") 
    { 
        DBUpdates updates
        {
            "UPDATE settings SET data = 'Internal' WHERE data LIKE '%mythbrowser' AND value = 'WebBrowserCommand';"
        };
        if (!performActualUpdate("MythBrowser", MythBrowserVersionName,
                                 updates, "1001", dbver))
            return false;
    }

    if (dbver == "1001")
    {
        DBUpdates updates
        {
            "DELETE FROM keybindings "
            " WHERE action = 'DELETETAB' AND context = 'Browser';"
        };
        if (!performActualUpdate("MythBrowser", MythBrowserVersionName,
                                 updates, "1002", dbver))
            return false;
    }

    if (dbver == "1002")
    {
        DBUpdates updates
        {
            "ALTER TABLE `websites` ADD `homepage` BOOL NOT NULL;"
        };
        if (!performActualUpdate("MythBrowser", MythBrowserVersionName,
                                 updates, "1003", dbver))
            return false;
    }

    return true;
}

bool FindInDB(const QString &category, const QString& name)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT name FROM websites "
                  "WHERE category = :CATEGORY AND name = :NAME ;");
    query.bindValue(":CATEGORY", category);
    query.bindValue(":NAME", name);
    if (!query.exec())
    {
        MythDB::DBError("mythbrowser: find in db", query);
        return false;
    }

    return (query.size() > 0);
}

bool ResetHomepageFromDB()
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE `websites` SET `homepage` = '0' WHERE `homepage` = '1';");

    return query.exec();
}

bool UpdateHomepageInDB(Bookmark* site)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE `websites` SET `homepage` = '1' "
                  "WHERE `category` = :CATEGORY "
                  "AND `name` = :NAME;");
    query.bindValue(":CATEGORY", site->m_category);
    query.bindValue(":NAME", site->m_name);

    return query.exec();
}

bool InsertInDB(Bookmark* site)
{
    if (!site)
        return false;

    return InsertInDB(site->m_category, site->m_name, site->m_url, site->m_isHomepage);
}

bool InsertInDB(const QString &category,
                const QString &name,
                const QString &url,
                const bool isHomepage)
{
    if (category.isEmpty() || name.isEmpty() || url.isEmpty())
        return false;

    if (FindInDB(category, name))
        return false;

    QString _url = url.trimmed();
    if (!_url.startsWith("http://") && !_url.startsWith("https://") &&
            !_url.startsWith("file:/"))
        _url.prepend("http://");

    _url.replace("&amp;","&");

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO websites (category, name, url, homepage) "
                  "VALUES(:CATEGORY, :NAME, :URL, :HOMEPAGE);");
    query.bindValue(":CATEGORY", category);
    query.bindValue(":NAME", name);
    query.bindValue(":URL", _url);
    query.bindValue(":HOMEPAGE", isHomepage);
    if (!query.exec())
    {
        MythDB::DBError("mythbrowser: inserting in DB", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}

bool RemoveFromDB(Bookmark *site)
{
    if (!site)
        return false;

    return RemoveFromDB(site->m_category, site->m_name);
}

bool RemoveFromDB(const QString &category, const QString &name)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM websites "
                  "WHERE category = :CATEGORY AND name = :NAME;");
    query.bindValue(":CATEGORY", category);
    query.bindValue(":NAME", name);
    if (!query.exec())
    {
        MythDB::DBError("mythbrowser: delete from db", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}

int GetCategoryList(QStringList &list)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT DISTINCT category FROM websites "
                  "ORDER BY category;");

    if (!query.exec())
    {
        MythDB::DBError("mythbrowser: get category list", query);
        return 0;
    }

    while (query.next())
    {
        list << query.value(0).toString();
    }

    return list.size();
}

int GetSiteList(QList<Bookmark*>  &siteList)
{
    while (!siteList.isEmpty())
        delete siteList.takeFirst();

    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.exec("SELECT category, name, url, homepage FROM websites "
               "ORDER BY category, name"))
    {
        LOG(VB_GENERAL, LOG_ERR, "BookmarkManager: Error in loading from DB");
    }
    else
    {
        std::shared_ptr<MythSortHelper>sh = getMythSortHelper();
        while (query.next())
        {
            auto *site = new Bookmark();
            site->m_category = query.value(0).toString();
            site->m_name = query.value(1).toString();
            site->m_sortName = sh->doTitle(site->m_name);
            site->m_url = query.value(2).toString();
            site->m_isHomepage = query.value(3).toBool();
            site->m_selected = false;
            siteList.append(site);
        }
        std::sort(siteList.begin(), siteList.end(), Bookmark::sortByName);
    }

    return siteList.size();
}
