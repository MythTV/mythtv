#ifndef UTIL_OSX_H
#define UTIL_OSX_H
    #import <CoreFoundation/CFDictionary.h> 

    extern int   get_int_CF(CFDictionaryRef dict, CFStringRef key);
    extern float get_float_CF(CFDictionaryRef dict, CFStringRef key);
#endif
