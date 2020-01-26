#ifndef CLEANUPGUARD_H
#define CLEANUPGUARD_H

#include "mythbaseexp.h"

class MBASE_PUBLIC CleanupGuard
{
  public:
    using CleanupFunc = void (*)();

  public:
    explicit CleanupGuard(CleanupFunc cleanFunction);

    ~CleanupGuard();

  private:
    CleanupFunc m_cleanFunction;
};

#endif // CLEANUPGUARD_H
