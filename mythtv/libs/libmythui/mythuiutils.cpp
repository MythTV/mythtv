
// Own header
#include "mythuiutils.h"

// QT headers
#include <QString>

// libmythbase headers
#include "mythlogging.h"
#include "mythuitype.h"

bool ETPrintWarning::Child(const MythUIType *container,
                           const QString &child_name)
{
    LOG(VB_GUI, LOG_NOTICE,
        QString("Container '%1' is missing child '%2' at %3")
        .arg(container->GetXMLName()).arg(child_name).arg(container->GetXMLLocation()));
    return false;
}

bool ETPrintWarning::Container(const QString &child_name)
{
    LOG(VB_GUI, LOG_NOTICE,
        QString("No valid container to search for child '%1'")
        .arg(child_name));
    return false;
}

bool ETPrintError::Child(const MythUIType *container,
                         const QString &child_name)
{
    LOG(VB_GENERAL, LOG_ERR,
        QString("Container '%1' is missing child '%2' at %3")
        .arg(container->GetXMLName()).arg(child_name).arg(container->GetXMLLocation()));
    return true;
}

bool ETPrintError::Container(const QString &child_name)
{
    LOG(VB_GENERAL, LOG_ERR,
        QString("No valid container to search for child '%1'")
        .arg(child_name));
    return true;
}
