
// Own header
#include "mythuiutils.h"

// QT headers
#include <QString>

// libmythbase headers
#include "mythlogging.h"

bool ETPrintWarning::Child(const QString &container_name,
                           const QString &child_name)
{
    LOG(VB_GENERAL, LOG_WARNING, 
        QString("Warning: container '%1' is missing child '%2'")
            .arg(container_name).arg(child_name));
    return false;
}

bool ETPrintWarning::Container(const QString &child_name)
{
    LOG(VB_GENERAL, LOG_WARNING, 
        QString("Warning: no valid container to search for child '%1'")
            .arg(child_name));
    return false;
}

bool ETPrintError::Child(const QString &container_name,
                           const QString &child_name)
{
    LOG(VB_GENERAL, LOG_ERR,
        QString("Error: container '%1' is missing child '%2'")
            .arg(container_name).arg(child_name));
    return true;
}

bool ETPrintError::Container(const QString &child_name)
{
    LOG(VB_GENERAL, LOG_ERR,
        QString("Error: no valid container to search for child '%1'")
            .arg(child_name));
    return true;
}
