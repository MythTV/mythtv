#ifndef MYTHUTILSCOCOA_H_
#define MYTHUTILSCOCOA_H_

// Qt
#include <QByteArray>

// MythTV
#include "mythuiexp.h"

// OSX
#import "ApplicationServices/ApplicationServices.h"

CGDirectDisplayID GetOSXCocoaDisplay(void* View);
QByteArray        GetOSXEDID        (CGDirectDisplayID Display);

#endif // MYTHUTILSCOCOA_H_
