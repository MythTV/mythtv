// MythTV
#include "mythutilscocoa.h"

// OSX
#import <Cocoa/Cocoa.h>

CGDirectDisplayID GetOSXCocoaDisplay(void* view)
{
    NSView *thisview = static_cast<NSView *>(view);
    if (!thisview)
        return 0;
    NSScreen *screen = [[thisview window] screen];
    if (!screen)
        return 0;
    NSDictionary* desc = [screen deviceDescription];
    return (CGDirectDisplayID)[[desc objectForKey:@"NSScreenNumber"] intValue];
}
