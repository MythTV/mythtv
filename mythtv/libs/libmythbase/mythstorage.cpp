// -*- Mode: c++ -*-

// Myth headers
#include "mythstorage.h"
#include "mythdb.h"

void SimpleDBStorage::Load(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    MSqlBindings bindings;
    query.prepare(
        "SELECT " + GetColumnName() +
        "  FROM " + GetTableName() +
        " WHERE " + GetWhereClause(bindings));
    query.bindValues(bindings);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("SimpleDBStorage::Load()", query);
    }
    else if (query.next())
    {
        QString result = query.value(0).toString();
        if (!result.isNull())
        {
            initval = result;
            user->SetDBValue(result);
        }
    }
}

void SimpleDBStorage::Save(QString _table)
{
    if (!IsSaveRequired())
        return;

    MSqlBindings bindings;
    QString querystr = QString("SELECT * FROM " + _table + " WHERE "
                               + GetWhereClause(bindings) + ';');

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
        MSqlBindings bindings;

        querystr = QString("UPDATE " + _table + " SET " + GetSetClause(bindings) +
                           " WHERE " + GetWhereClause(bindings) + ';');

        query.prepare(querystr);
        query.bindValues(bindings);

        if (!query.exec())
            MythDB::DBError("SimpleDBStorage::Save() update", query);
    }
    else
    {
        // Row does not exist yet
        MSqlBindings bindings;

        querystr = QString("INSERT INTO " + _table + " SET "
                           + GetSetClause(bindings) + ';');

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

    bindings.insert(tagname, user->GetDBValue());

    return clause;
}

bool SimpleDBStorage::IsSaveRequired(void) const
{
    return user->GetDBValue() != initval;
}

void SimpleDBStorage::SetSaveRequired(void)
{
    initval.clear();
}

//////////////////////////////////////////////////////////////////////

QString GenericDBStorage::GetWhereClause(MSqlBindings &bindings) const
{
    QString keycolumnTag = ":WHERE" + keycolumn.toUpper();

    bindings.insert(keycolumnTag, keyvalue);

    return keycolumn + " = " + keycolumnTag;
}

QString GenericDBStorage::GetSetClause(MSqlBindings &bindings) const
{
    QString keycolumnTag = ":SETKEY" + keycolumn.toUpper();
    QString columnTag    = ":SETCOL" + GetColumnName().toUpper();

    bindings.insert(keycolumnTag, keyvalue);
    bindings.insert(columnTag,    user->GetDBValue());

    return keycolumn + " = " + keycolumnTag + ", " +
        GetColumnName() + " = " + columnTag;
}

//////////////////////////////////////////////////////////////////////

HostDBStorage::HostDBStorage(StorageUser *_user, const QString &name) :
    SimpleDBStorage(_user, "settings", "data"), settingname(name)
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

    bindings.insert(valueTag, settingname);
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

    bindings.insert(valueTag, settingname);
    bindings.insert(dataTag, user->GetDBValue());
    bindings.insert(hostnameTag, MythDB::getMythDB()->GetHostName());

    return clause;
}

//////////////////////////////////////////////////////////////////////

GlobalDBStorage::GlobalDBStorage(
    StorageUser *_user, const QString &name) :
    SimpleDBStorage(_user, "settings", "data"), settingname(name)
{
}

QString GlobalDBStorage::GetWhereClause(MSqlBindings &bindings) const
{
    QString valueTag(":WHEREVALUE");
    QString clause("value = " + valueTag);

    bindings.insert(valueTag, settingname);

    return clause;
}

QString GlobalDBStorage::GetSetClause(MSqlBindings &bindings) const
{
    QString valueTag(":SETVALUE");
    QString dataTag(":SETDATA");

    QString clause("value = " + valueTag + ", data = " + dataTag);

    bindings.insert(valueTag, settingname);
    bindings.insert(dataTag, user->GetDBValue());

    return clause;
}
