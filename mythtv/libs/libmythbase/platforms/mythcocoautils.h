#ifndef MYTHCOCOAUTILS_H
#define MYTHCOCOAUTILS_H

// MythTV
#include "mythbaseexp.h"

// OSX
#import "ApplicationServices/ApplicationServices.h"

MBASE_PUBLIC void* CreateOSXCocoaPool(void);
MBASE_PUBLIC void  DeleteOSXCocoaPool(void*&);

class CocoaAutoReleasePool
{
  public:
    CocoaAutoReleasePool() { m_pool = CreateOSXCocoaPool(); }
   ~CocoaAutoReleasePool() { DeleteOSXCocoaPool(m_pool);    }

  private:
    void *m_pool;
};

#endif // MYTHCOCOAUTILS_H
