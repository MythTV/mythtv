#ifndef _UTIL_OSX_COCOA_H__
#define _UTIL_OSX_COCOA_H__

#import "ApplicationServices/ApplicationServices.h"

#ifdef USING_DVDV
void *CreateOSXCocoaPool(void);
void DeleteOSXCocoaPool(void*&);
#else
inline void *CreateOSXCocoaPool(void) { return NULL; }
inline void DeleteOSXCocoaPool(void*&) {}
#endif

CGDirectDisplayID GetOSXCocoaDisplay(void* view);

#endif // _UTIL_OSX_COCOA_H__
