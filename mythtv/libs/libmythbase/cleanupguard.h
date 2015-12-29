#ifndef _CLEANUPGUARD_H_
#define _CLEANUPGUARD_H_

#include "mythbaseexp.h"

class MBASE_PUBLIC CleanupGuard
{
  public:
    typedef void (*CleanupFunc)();

  public:
    explicit CleanupGuard(CleanupFunc cleanFunction);

    ~CleanupGuard();

  private:
    CleanupFunc m_cleanFunction;
};

#endif // _CLEANUPGUARD_H_
