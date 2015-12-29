#include "cleanupguard.h"
#include "referencecounter.h"

CleanupGuard::CleanupGuard(CleanupFunc cleanFunction) :
    m_cleanFunction(cleanFunction)
{
}

CleanupGuard::~CleanupGuard()
{
    m_cleanFunction();
    ReferenceCounter::PrintDebug();
}
