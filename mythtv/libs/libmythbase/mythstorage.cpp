// -*- Mode: c++ -*-

// Myth headers
#include "mythstorage.h"

#include "mythdb.h"
#include "mythcorecontext.h"

void SimpleDBStorage::Load(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    MSqlBindings bindings;
    query.prepare(
        "SELECT CAST(" + GetColumnName() + " AS CHAR)"
        " FROM " + GetTableName() +
        " WHERE " + GetWhereClause(bindings));
    query.bindValues(bindings);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("SimpleDBStorage::Load()", query);
    }
    else if (query.next())
    {
        QString result = query.value(0).toString();
        // a 'NULL' QVariant does not get converted to a 'NULL' QString
        if (!query.value(0).isNull())
        {
            m_initval = result;
            m_user->SetDBValue(result);
        }
    }
}

void SimpleDBStorage::Save(const QString &table)
{
    if (!IsSaveRequired())
        return;

    MSqlBindings bindings;
    QString querystr = "SELECT * FROM " + table + " WHERE "
                       + GetWhereClause(bindings) + ';';

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(querystr);
    query.bindValues(bindings);

    if (!query.exec())
    {
        MythDB::DBError("SimpleDBStorage::Save() query", query);
        return;
    }

    if (query.isActive() && query.next())
    {
        // Row already exists
        // Don"t change this QString. See the CVS logs rev 1.91.
        querystr = "UPDATE " + table + " SET " + GetSetClause(bindings) +
                   " WHERE " + GetWhereClause(bindings) + ';';

        query.prepare(querystr);
        query.bindValues(bindings);

        if (!query.exec())
            MythDB::DBError("SimpleDBStorage::Save() update", query);
    }
    else
    {
        // Row does not exist yet
        querystr = "INSERT INTO " + table + " SET "
                   + GetSetClause(bindings) + ';';

        query.prepare(querystr);
        query.bindValues(bindings);

        if (!query.exec())
            MythDB::DBError("SimpleDBStorage::Save() insert", query);
    }
}

void SimpleDBStorage::Save(void)
{
    Save(GetTableName());
}

QString SimpleDBStorage::GetSetClause(MSqlBindings &bindings) const
{
    QString tagname(":SET" + GetColumnName().toUpper());
    QString clause(GetColumnName() + " = " + tagname);

    bindings.insert(tagname, m_user->GetDBValue());

    return clause;
}

bool SimpleDBStorage::IsSaveRequired(void) const
{
    return m_user->GetDBValue() != m_initval;
}

void SimpleDBStorage::SetSaveRequired(void)
{
    m_initval.clear();
}

//////////////////////////////////////////////////////////////////////

QString GenericDBStorage::GetWhereClause(MSqlBindings &bindings) const
{
    QString keycolumnTag = ":WHERE" + m_keycolumn.toUpper();

    bindings.insert(keycolumnTag, m_keyvalue);

    return m_keycolumn + " = " + keycolumnTag;
}

QString GenericDBStorage::GetSetClause(MSqlBindings &bindings) const
{
    QString keycolumnTag = ":SETKEY" + m_keycolumn.toUpper();
    QString columnTag    = ":SETCOL" + GetColumnName().toUpper();

    bindings.insert(keycolumnTag, m_keyvalue);
    bindings.insert(columnTag,    m_user->GetDBValue());

    return m_keycolumn + " = " + keycolumnTag + ", " +
        GetColumnName() + " = " + columnTag;
}

//////////////////////////////////////////////////////////////////////

HostDBStorage::HostDBStorage(StorageUser *_user, QString name) :
    SimpleDBStorage(_user, "settings", "data"), m_settingname(std::move(name))
{
}

QString HostDBStorage::GetWhereClause(MSqlBindings &bindings) const
{
    /* Returns a where clause of the form:
     * "value = :VALUE AND hostname = :HOSTNAME"
     * The necessary bindings are added to the MSQLBindings&
     */
    QString valueTag(":WHEREVALUE");
    QString hostnameTag(":WHEREHOSTNAME");

    QString clause("value = " + valueTag + " AND hostname = " + hostnameTag);

    bindings.insert(valueTag, m_settingname);
    bindings.insert(hostnameTag, MythDB::getMythDB()->GetHostName());

    return clause;
}

QString HostDBStorage::GetSetClause(MSqlBindings &bindings) const
{
    QString valueTag(":SETVALUE");
    QString dataTag(":SETDATA");
    QString hostnameTag(":SETHOSTNAME");
    QString clause("value = " + valueTag + ", data = " + dataTag
                   + ", hostname = " + hostnameTag);

    bindings.insert(valueTag, m_settingname);
    bindings.insert(dataTag, m_user->GetDBValue());
    bindings.insert(hostnameTag, MythDB::getMythDB()->GetHostName());

    return clause;
}

void HostDBStorage::Save(void)
{
    SimpleDBStorage::Save();
    gCoreContext->ClearSettingsCache(
        MythDB::getMythDB()->GetHostName() + ' ' + m_settingname);
}

//////////////////////////////////////////////////////////////////////

GlobalDBStorage::GlobalDBStorage(
    StorageUser *_user, QString name) :
    SimpleDBStorage(_user, "settings", "data"), m_settingname(std::move(name))
{
}

QString GlobalDBStorage::GetWhereClause(MSqlBindings &bindings) const
{
    QString valueTag(":WHEREVALUE");
    QString clause("value = " + valueTag);

    bindings.insert(valueTag, m_settingname);

    return clause;
}

QString GlobalDBStorage::GetSetClause(MSqlBindings &bindings) const
{
    QString valueTag(":SETVALUE");
    QString dataTag(":SETDATA");

    QString clause("value = " + valueTag + ", data = " + dataTag);

    bindings.insert(valueTag, m_settingname);
    bindings.insert(dataTag, m_user->GetDBValue());

    return clause;
}

void GlobalDBStorage::Save(void)
{
    SimpleDBStorage::Save();
    gCoreContext->ClearSettingsCache(m_settingname);
}
