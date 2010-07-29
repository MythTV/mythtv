#ifndef _UTIL_OSX_COCOA_H__
#define _UTIL_OSX_COCOA_H__

#import "ApplicationServices/ApplicationServices.h"

void *CreateOSXCocoaPool(void);
void DeleteOSXCocoaPool(void*&);

CGDirectDisplayID GetOSXCocoaDisplay(void* view);

#endif // _UTIL_OSX_COCOA_H__
