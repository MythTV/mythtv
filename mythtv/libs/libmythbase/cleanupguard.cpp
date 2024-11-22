#include "cleanupguard.h"

CleanupGuard::CleanupGuard(CleanupFunc cleanFunction) :
    m_cleanFunction(cleanFunction)
{
}

CleanupGuard::~CleanupGuard()
{
    m_cleanFunction();
}
