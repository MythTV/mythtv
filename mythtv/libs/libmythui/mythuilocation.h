#ifndef MYTHUILOCATION_H
#define MYTHUILOCATION_H

// Qt
#include <QReadWriteLock>
#include <QStringList>

// MythTV
#include "mythuiexp.h"

class MUI_PUBLIC MythUILocation
{
  public:
    void    AddCurrentLocation(const QString& Location);
    QString RemoveCurrentLocation();
    QString GetCurrentLocation(bool FullPath = false, bool MainStackOnly = true);

  private:
    QReadWriteLock m_locationLock;
    QStringList    m_currentLocation;
};

#endif
