// MythTV
#include "platforms/mythcocoautils.h"

// OSX
#import <Cocoa/Cocoa.h>

// Dummy NSThread for Cocoa multithread initialization
@implementation NSThread (Dummy)

    - (void) run
{
}

@end

void *CreateOSXCocoaPool(void)
{
    // Cocoa requires a message to be sent informing the Cocoa event
    // thread that the application is multi-threaded. Apple recommends
    // creating a dummy NSThread to get this message sent.
    if (![NSThread isMultiThreaded])
    {
        NSThread *thr = [[NSThread alloc] init];
        SEL threadSelector = @selector(run);
        [NSThread detachNewThreadSelector:threadSelector
         toTarget:thr
         withObject:nil];
    }

    NSAutoreleasePool *pool = nullptr;
    pool = [[NSAutoreleasePool alloc] init];
    return pool;
}

void DeleteOSXCocoaPool(void* &pool)
{
    if (pool)
    {
        NSAutoreleasePool *a_pool = (NSAutoreleasePool*) pool;
        pool = nullptr;
        [a_pool release];
    }
}
