
// Own header
#include "mythuiutils.h"

// QT headers
#include <QString>

// libmythdb headers
#include "mythverbose.h"

bool ETPrintWarning::Child(const QString &container_name,
                           const QString &child_name)
{
    VERBOSE(VB_GENERAL | VB_EXTRA, QObject::tr("Warning: container '%1' is "
                    "missing child '%2'").arg(container_name).arg(child_name));
    return false;
}

bool ETPrintWarning::Container(const QString &child_name)
{
    VERBOSE(VB_GENERAL | VB_EXTRA, QObject::tr("Warning: no valid container to "
                    "search for child '%1'").arg(child_name));
    return false;
}

bool ETPrintError::Child(const QString &container_name,
                           const QString &child_name)
{
    VERBOSE(VB_IMPORTANT, QObject::tr("Error: container '%1' is "
                    "missing child '%2'").arg(container_name).arg(child_name));
    return true;
}

bool ETPrintError::Container(const QString &child_name)
{
    VERBOSE(VB_IMPORTANT, QObject::tr("Error: no valid container to "
                    "search for child '%1'").arg(child_name));
    return true;
}
