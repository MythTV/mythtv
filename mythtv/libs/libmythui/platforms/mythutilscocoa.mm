// MythTV
#include "libmythbase/mythconfig.h"
#include "mythutilscocoa.h"

// OSX
#import <Cocoa/Cocoa.h>
#import <IOKit/graphics/IOGraphicsLib.h>
#include <AvailabilityMacros.h>

#if !HAVE_IOMAINPORT
#define kIOMainPortDefault kIOMasterPortDefault
#endif

CGDirectDisplayID GetOSXCocoaDisplay(void* View)
{
    NSView *thisview = static_cast<NSView *>(View);
    if (!thisview)
        return 0;
    NSScreen *screen = [[thisview window] screen];
    if (!screen)
        return 0;
    NSDictionary* desc = [screen deviceDescription];
    return (CGDirectDisplayID)[[desc objectForKey:@"NSScreenNumber"] intValue];
}

static Boolean CFNumberEqualsUInt32(CFNumberRef Number, uint32_t Uint32)
{
    if (Number == nullptr)
        return (Uint32 == 0);
    int64_t Int64;
    if (!CFNumberGetValue(Number, kCFNumberSInt64Type, &Int64))
        return false;
    return Int64 == Uint32;
}

QByteArray GetOSXEDID(CGDirectDisplayID Display)
{
    QByteArray result;
    if (!Display)
        return result;

    uint32_t vendor = CGDisplayVendorNumber(Display);
    uint32_t model  = CGDisplayModelNumber(Display);
    uint32_t serial = CGDisplaySerialNumber(Display);
    CFMutableDictionaryRef matching = IOServiceMatching("IODisplayConnect");

    io_iterator_t iter;
    if (IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iter))
      return result;

    io_service_t service = 0;
    while ((service = IOIteratorNext(iter)) != 0)
    {
        CFDictionaryRef info     = IODisplayCreateInfoDictionary(service, kIODisplayOnlyPreferredName);
        CFNumberRef vendorID     = static_cast<CFNumberRef>(CFDictionaryGetValue(info, CFSTR(kDisplayVendorID)));
        CFNumberRef productID    = static_cast<CFNumberRef>(CFDictionaryGetValue(info, CFSTR(kDisplayProductID)));
        CFNumberRef serialNumber = static_cast<CFNumberRef>(CFDictionaryGetValue(info, CFSTR(kDisplaySerialNumber)));

        if (CFNumberEqualsUInt32(vendorID, vendor) &&
            CFNumberEqualsUInt32(productID, model) &&
            CFNumberEqualsUInt32(serialNumber, serial))
        {
            CFDataRef edid = static_cast<CFDataRef>(CFDictionaryGetValue(info, CFSTR(kIODisplayEDIDKey)));
            if (edid)
            {
                const char* data = reinterpret_cast<const char*>(CFDataGetBytePtr(edid));
                int length = CFDataGetLength(edid);
                result = QByteArray(data, length);
            }
            CFRelease(info);
            break;
        }
        CFRelease(info);
    }
    IOObjectRelease(iter);
    return result;
}
