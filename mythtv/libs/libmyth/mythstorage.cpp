// -*- Mode: c++ -*-

// Myth headers
#include "mythstorage.h"
#include "settings.h"

void SimpleDBStorage::load(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    MSqlBindings bindings;
    query.prepare(
        "SELECT " + column + " "
        "FROM "   + table + " " +
        "WHERE "  + whereClause(bindings));
    query.bindValues(bindings);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("SimpleDBStorage::load()", query);
    }
    else if (query.next())
    {
        QString result = query.value(0).toString();
        if (!result.isNull())
        {
            result = QString::fromUtf8(query.value(0).toString());
            setting->setValue(result);
            setting->setUnchanged();
        }
    }
}

void SimpleDBStorage::save(QString table)
{
    if (!setting->isChanged())
        return;

    MSqlBindings bindings;
    QString querystr = QString("SELECT * FROM " + table + " WHERE "
                               + whereClause(bindings) + ";");

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(querystr);
    query.bindValues(bindings);

    if (!query.exec())
    {
        MythContext::DBError("SimpleDBStorage::save() query", query);
        return;
    }

    if (query.isActive() && query.next())
    {
        // Row already exists
        // Don"t change this QString. See the CVS logs rev 1.91.
        MSqlBindings bindings;

        querystr = QString("UPDATE " + table + " SET " + setClause(bindings) +
                           " WHERE " + whereClause(bindings) + ";");

        query.prepare(querystr);
        query.bindValues(bindings);

        if (!query.exec())
            MythContext::DBError("SimpleDBStorage::save() update", query);
    }
    else
    {
        // Row does not exist yet
        MSqlBindings bindings;

        querystr = QString("INSERT INTO " + table + " SET "
                           + setClause(bindings) + ";");

        query.prepare(querystr);
        query.bindValues(bindings);

        if (!query.exec())
            MythContext::DBError("SimpleDBStorage::save() insert", query);
    }
}

void SimpleDBStorage::save(void)
{
    save(table);
}

QString SimpleDBStorage::setClause(MSqlBindings &bindings)
{
    QString tagname(":SET" + column.upper());
    QString clause(column + " = " + tagname);

    bindings.insert(tagname, setting->getValue().utf8());

    return clause;
}

//////////////////////////////////////////////////////////////////////

HostDBStorage::HostDBStorage(Setting *_setting, QString name) :
    SimpleDBStorage(_setting, "settings", "data")
{
    _setting->setName(name);
}

QString HostDBStorage::whereClause(MSqlBindings &bindings)
{
    /* Returns a where clause of the form:
     * "value = :VALUE AND hostname = :HOSTNAME"
     * The necessary bindings are added to the MSQLBindings&
     */
    QString valueTag(":WHEREVALUE");
    QString hostnameTag(":WHEREHOSTNAME");

    QString clause("value = " + valueTag + " AND hostname = " + hostnameTag);

    bindings.insert(valueTag, setting->getName());
    bindings.insert(hostnameTag, gContext->GetHostName());

    return clause;
}

QString HostDBStorage::setClause(MSqlBindings &bindings)
{
    QString valueTag(":SETVALUE");
    QString dataTag(":SETDATA");
    QString hostnameTag(":SETHOSTNAME");
    QString clause("value = " + valueTag + ", data = " + dataTag
                   + ", hostname = " + hostnameTag);

    bindings.insert(valueTag, setting->getName());
    bindings.insert(dataTag, setting->getValue().utf8());
    bindings.insert(hostnameTag, gContext->GetHostName());

    return clause;
}

//////////////////////////////////////////////////////////////////////

GlobalDBStorage::GlobalDBStorage(Setting *_setting, QString name) :
    SimpleDBStorage(_setting, "settings", "data")
{
    _setting->setName(name);
}

QString GlobalDBStorage::whereClause(MSqlBindings &bindings)
{
    QString valueTag(":WHEREVALUE");
    QString clause("value = " + valueTag);

    bindings.insert(valueTag, setting->getName());

    return clause;
}

QString GlobalDBStorage::setClause(MSqlBindings &bindings)
{
    QString valueTag(":SETVALUE");
    QString dataTag(":SETDATA");

    QString clause("value = " + valueTag + ", data = " + dataTag);

    bindings.insert(valueTag, setting->getName());
    bindings.insert(dataTag, setting->getValue().utf8());

    return clause;
}
