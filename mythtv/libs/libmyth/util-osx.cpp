/******************************************************************************
 * = NAME
 * util-osx.cpp
 *
 * = DESCRIPTION
 * A few routines for extracting data (int/float)
 * from OS X Core Foundation data structures
 * 
 * = REVISION
 * $Id$
 *
 * = AUTHORS
 * Nigel Pearson
 *****************************************************************************/

#import "util-osx.h"

#import <CoreFoundation/CFNumber.h>

#include <stdio.h>


int get_int_CF(CFDictionaryRef dict, CFStringRef key)
{
    CFNumberRef ref = (CFNumberRef) CFDictionaryGetValue(dict, key);
    int         val = 0;
 
    if ( ! ref )
        puts("get_int_CF() - Failed to get number reference");
    else
        if ( ! CFNumberGetValue(ref, kCFNumberSInt32Type, &val) )
            puts("get_int_CF() - Failed to get 32bit int from number");
 
    return val;
}

float get_float_CF(CFDictionaryRef dict, CFStringRef key)
{
    CFNumberRef ref = (CFNumberRef) CFDictionaryGetValue(dict, key);
    float       val = 0.0;

    if ( ! ref )
        puts("get_float_CF() - Failed to get number reference");
    else
        if ( ! CFNumberGetValue(ref, kCFNumberFloatType, &val) )
            puts("get_float_CF() - Failed to get float from number");

    return val;
}
