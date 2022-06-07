// MythTV
#include "mythmainwindow.h"
#include "mythuilocation.h"

void MythUILocation::AddCurrentLocation(const QString& Location)
{
    QWriteLocker locker(&m_locationLock);
    if (m_currentLocation.isEmpty() || m_currentLocation.last() != Location)
        m_currentLocation.push_back(Location);
}

QString MythUILocation::RemoveCurrentLocation()
{
    QWriteLocker locker(&m_locationLock);
    if (m_currentLocation.isEmpty())
        return {"UNKNOWN"};
    return m_currentLocation.takeLast();
}

QString MythUILocation::GetCurrentLocation(bool FullPath, bool MainStackOnly)
{
    QString result;
    QReadLocker locker(&m_locationLock);

    MythMainWindow* window = GetMythMainWindow();
    if (!window)
        return result;

    if (FullPath)
    {
        // get main stack top screen
        MythScreenStack *stack = window->GetMainStack();
        result = stack->GetLocation(true);

        if (!MainStackOnly)
        {
            // get popup stack main screen
            stack = window->GetStack("popup stack");

            if (!stack->GetLocation(true).isEmpty())
                result += '/' + stack->GetLocation(false);
        }

        // if there's a location in the stringlist add that (non mythui screen or external app running)
        if (!m_currentLocation.isEmpty())
        {
            for (int x = 0; x < m_currentLocation.count(); x++)
                result += '/' + m_currentLocation[x];
        }
    }
    else
    {
        // get main stack top screen
        MythScreenStack *stack = window->GetMainStack();
        result = stack->GetLocation(false);

        if (!MainStackOnly)
        {
            // get popup stack top screen
            stack = window->GetStack("popup stack");

            if (!stack->GetLocation(false).isEmpty())
                result = stack->GetLocation(false);
        }

        // if there's a location in the stringlist use that (non mythui screen or external app running)
        if (!m_currentLocation.isEmpty())
            result = m_currentLocation.last();
    }

    if (result.isEmpty())
        result = "UNKNOWN";

    return result;
}
