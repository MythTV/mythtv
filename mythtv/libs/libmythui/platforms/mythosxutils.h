#ifndef MYTHOSXUTILS_H_
#define MYTHOSXUTILS_H_

// Qt
#include <QWidget> // for WId

// MythTV
#include "mythuiexp.h"

// OSX
#import <CoreFoundation/CFDictionary.h> 
#import <ApplicationServices/ApplicationServices.h>

MUI_PUBLIC int    get_int_CF(CFDictionaryRef dict, CFStringRef key);
MUI_PUBLIC float  get_float_CF(CFDictionaryRef dict, CFStringRef key);
MUI_PUBLIC double get_double_CF(CFDictionaryRef dict, CFStringRef key);
MUI_PUBLIC bool   get_bool_CF(CFDictionaryRef dict, CFStringRef key);
CGDirectDisplayID GetOSXDisplay(WId win);

#endif
