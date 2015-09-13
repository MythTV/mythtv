//
//  util-osx.mm
//  MythTV
//
//  Created by Jean-Yves Avenard on 2/08/2015.
//  Copyright (c) 2015 Jean-Yves Avenard. All rights reserved.
//

#include "util-osx.h"

#import <Foundation/NSDictionary.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>

extern "C" {

static int intAtStringIndex(NSArray *array, int index)
{
    return [(NSString *)[array objectAtIndex:index] integerValue];
}


void GetOSXVersion(int32_t* aMajorVersion, int32_t* aMinorVersion)
{
    *aMajorVersion = *aMinorVersion = 0;
    NSString* versionString = [[NSDictionary dictionaryWithContentsOfFile:
                                @"/System/Library/CoreServices/SystemVersion.plist"] objectForKey:@"ProductVersion"];
    NSArray* versions = [versionString componentsSeparatedByString:@"."];
    NSUInteger count = [versions count];
    if (count > 0)
    {
        *aMajorVersion = intAtStringIndex(versions, 0);
        if (count > 1)
        {
            *aMinorVersion = intAtStringIndex(versions, 1);
        }
    }
}

}
