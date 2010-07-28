#ifndef UTIL_OSX_H
#define UTIL_OSX_H
#import <CoreFoundation/CFDictionary.h> 
#import "ApplicationServices/ApplicationServices.h"
#include <QWindowsStyle>

extern int   get_int_CF(CFDictionaryRef dict, CFStringRef key);
extern float get_float_CF(CFDictionaryRef dict, CFStringRef key);
CGDirectDisplayID GetOSXDisplay(WId win);

#endif
