#ifndef UTIL_OSX_H
#define UTIL_OSX_H

#import <CoreFoundation/CFDictionary.h> 
#import <ApplicationServices/ApplicationServices.h>
#include "mythuiexp.h"
#include <QWidget> // for WId

MUI_PUBLIC int    get_int_CF(CFDictionaryRef dict, CFStringRef key);
MUI_PUBLIC float  get_float_CF(CFDictionaryRef dict, CFStringRef key);
MUI_PUBLIC double get_double_CF(CFDictionaryRef dict, CFStringRef key);
MUI_PUBLIC bool   get_bool_CF(CFDictionaryRef dict, CFStringRef key);
CGDirectDisplayID GetOSXDisplay(WId win);

#endif
