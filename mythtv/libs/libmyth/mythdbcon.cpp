#include "mythdbcon.h"

bool MSqlQuery::prepare(const QString& query)
{
    static QMutex prepareLock;
    QMutexLocker lock(&prepareLock);
    return QSqlQuery::prepare(query); 
}