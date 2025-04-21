
#include "devices/AppleRemote.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sys/errno.h>
#include <sys/sysctl.h>  // for sysctlbyname
#include <sysexits.h>
#include <unistd.h>

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>     // for Gestalt
#include <AvailabilityMacros.h>

#include <sstream>

#include "libmythbase/mythconfig.h"
#include "libmythbase/mythlogging.h"

#if !HAVE_IOMAINPORT
#define kIOMainPortDefault kIOMasterPortDefault
#endif

AppleRemote*    AppleRemote::_instance = nullptr;


#define REMOTE_SWITCH_COOKIE 19
#define REMOTE_COOKIE_STR    "19_"
#define LONG_PRESS_COUNT     10

#define LOC QString("AppleRemote::")

static io_object_t _findAppleRemoteDevice(const char *devName);

AppleRemote * AppleRemote::Get()
{
    if (_instance == nullptr)
        _instance = new AppleRemote();

    return _instance;
}

AppleRemote::~AppleRemote()
{
    stopListening();

    if (isRunning())
    {
        exit(0);
    }
    if (this == _instance)
    {
        _instance = nullptr;
    }
}

bool AppleRemote::isListeningToRemote()
{
    return (hidDeviceInterface != nullptr && !cookies.empty() && queue != nullptr);
}

void AppleRemote::setListener(AppleRemote::Listener* listener)
{
    _listener = listener;
}

void AppleRemote::startListening()
{
    if (queue != nullptr)   // already listening
        return;

    io_object_t hidDevice = _findAppleRemoteDevice("AppleIRController");

    if (!hidDevice)
        hidDevice = _findAppleRemoteDevice("AppleTVIRReceiver");

    if (!hidDevice ||
        !_createDeviceInterface(hidDevice) ||
        !_initCookies() || !_openDevice())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "startListening() failed");
        stopListening();
        return;
    }

    IOObjectRelease(hidDevice);
}

void AppleRemote::stopListening()
{
    if (queue != nullptr)
    {
        (*queue)->stop(queue);
        (*queue)->dispose(queue);
        (*queue)->Release(queue);
        queue = nullptr;
    }

    if (!cookies.empty())
        cookies.clear();

    if (hidDeviceInterface != nullptr)
    {
        (*hidDeviceInterface)->close(hidDeviceInterface);
        (*hidDeviceInterface)->Release(hidDeviceInterface);
        hidDeviceInterface = nullptr;
    }
}

void AppleRemote::run()
{
    RunProlog();
    CFRunLoopRun();
    exec();  // prevent QThread exiting, by entering its run loop
    CFRunLoopStop(CFRunLoopGetCurrent());
    RunEpilog();
}

AppleRemote::AppleRemote() : MThread("AppleRemote")
{
    _initCookieMap();
}

/// Apple keeps changing the "interface" between the remote and the OS.
/// This initialises a table that stores those mappings.
///
/// The white plastic remote has a Play+Pause button in the middle of the
/// navigation ring, but for menu navigation, we send an "enter" event instead.
/// Ideally we would remap that for TV/videos, but MythTV can't easily do that.
///
/// The new Aluminium remote has separate Select and Play+Pause buttons.
///
void AppleRemote::_initCookieMap()
{
    // 10.4 sequences:
    cookieToButtonMapping["14_12_11_6_5_"]        = Up;
    cookieToButtonMapping["14_13_11_6_5_"]        = Down;
    cookieToButtonMapping["14_7_6_5_14_7_6_5_"]   = Menu;
    cookieToButtonMapping["14_8_6_5_14_8_6_5_"]   = Select;
    cookieToButtonMapping["14_9_6_5_14_9_6_5_"]   = Right;
    cookieToButtonMapping["14_10_6_5_14_10_6_5_"] = Left;
    cookieToButtonMapping["14_6_5_4_2_"]          = RightHold;
    cookieToButtonMapping["14_6_5_3_2_"]          = LeftHold;
    cookieToButtonMapping["14_6_5_14_6_5_"]       = MenuHold;
    cookieToButtonMapping["18_14_6_5_18_14_6_5_"] = PlayHold;
    cookieToButtonMapping["19_"]                  = ControlSwitched;

    // 10.5 sequences:
    cookieToButtonMapping["31_29_28_18_"]         = Up;
    cookieToButtonMapping["31_30_28_18_"]         = Down;
    cookieToButtonMapping["31_20_18_31_20_18_"]   = Menu;
    cookieToButtonMapping["31_21_18_31_21_18_"]   = Select;
    cookieToButtonMapping["31_22_18_31_22_18_"]   = Right;
    cookieToButtonMapping["31_23_18_31_23_18_"]   = Left;
    cookieToButtonMapping["31_18_4_2_"]           = RightHold;
    cookieToButtonMapping["31_18_3_2_"]           = LeftHold;
    cookieToButtonMapping["31_18_31_18_"]         = MenuHold;
    cookieToButtonMapping["35_31_18_35_31_18_"]   = PlayHold;
    cookieToButtonMapping["39_"]                  = ControlSwitched;

    // 10.6 sequences:
    cookieToButtonMapping["33_31_30_21_20_2_"]            = Up;
    cookieToButtonMapping["33_32_30_21_20_2_"]            = Down;
    cookieToButtonMapping["33_22_21_20_2_33_22_21_20_2_"] = Menu;
    cookieToButtonMapping["33_23_21_20_2_33_23_21_20_2_"] = Select;
    cookieToButtonMapping["33_24_21_20_2_33_24_21_20_2_"] = Right;
    cookieToButtonMapping["33_25_21_20_2_33_25_21_20_2_"] = Left;
    cookieToButtonMapping["33_21_20_14_12_2_"]            = RightHold;
    cookieToButtonMapping["33_21_20_13_12_2_"]            = LeftHold;
    cookieToButtonMapping["33_21_20_2_33_21_20_2_"]       = MenuHold;
    cookieToButtonMapping["37_33_21_20_2_37_33_21_20_2_"] = PlayHold;

    // Aluminium remote which has an extra button:
    cookieToButtonMapping["33_21_20_8_2_33_21_20_8_2_"]   = PlayPause;
    cookieToButtonMapping["33_21_20_3_2_33_21_20_3_2_"]   = Select;
    cookieToButtonMapping["33_21_20_11_2_33_21_20_11_2_"] = PlayHold;
}

static io_object_t _findAppleRemoteDevice(const char *devName)
{
    CFMutableDictionaryRef hidMatchDictionary = nullptr;
    io_iterator_t          hidObjectIterator = 0;
    io_object_t            hidDevice = 0;
    IOReturn               ioReturnValue;


    hidMatchDictionary = IOServiceMatching(devName);

    // check for matching devices
    ioReturnValue = IOServiceGetMatchingServices(kIOMainPortDefault,
                                                 hidMatchDictionary,
                                                 &hidObjectIterator);

    if ((ioReturnValue == kIOReturnSuccess) && (hidObjectIterator != 0))
        hidDevice = IOIteratorNext(hidObjectIterator);
    else
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("_findAppleRemoteDevice(%1) failed").arg(devName));

    // IOServiceGetMatchingServices consumes a reference to the dictionary,
    // so we don't need to release the dictionary ref.
    hidMatchDictionary = nullptr;
    return hidDevice;
}

bool AppleRemote::_initCookies()
{
    IOHIDDeviceInterface122** handle;
    CFArrayRef                elements;
    IOReturn                  success;

    handle  = (IOHIDDeviceInterface122**)hidDeviceInterface;
    success = (*handle)->copyMatchingElements(handle,
                                              nullptr,
                                              (CFArrayRef*)&elements);

    if (success == kIOReturnSuccess)
    {
        for (CFIndex i = 0; i < CFArrayGetCount(elements); ++i)
        {
            CFDictionaryRef    element;
            CFTypeRef          object;
            long               number;
            IOHIDElementCookie cookie;

            element = (CFDictionaryRef)CFArrayGetValueAtIndex(elements, i);
            object  = CFDictionaryGetValue(element,
                                           CFSTR(kIOHIDElementCookieKey));

            if (object == nullptr || CFGetTypeID(object) != CFNumberGetTypeID())
                continue;

            if (!CFNumberGetValue((CFNumberRef)object,
                                  kCFNumberLongType, &number))
                continue;

            cookie = (IOHIDElementCookie)number;

            cookies.push_back((int)cookie);
        }
        return true;
    }
    return false;
}

bool AppleRemote::_createDeviceInterface(io_object_t hidDevice)
{
    IOReturn              ioReturnValue;
    IOCFPlugInInterface** plugInInterface = nullptr;
    SInt32                score = 0;


    ioReturnValue
        = IOCreatePlugInInterfaceForService(hidDevice,
                                            kIOHIDDeviceUserClientTypeID,
                                            kIOCFPlugInInterfaceID,
                                            &plugInInterface, &score);

    if ((kIOReturnSuccess == ioReturnValue) &&
        plugInInterface && *plugInInterface)
    {
        HRESULT plugInResult = (*plugInInterface)->QueryInterface
                                (plugInInterface,
                                 CFUUIDGetUUIDBytes(kIOHIDDeviceInterfaceID),
                                 (LPVOID*) (&hidDeviceInterface));

        if (plugInResult != S_OK)
            LOG(VB_GENERAL, LOG_ERR, LOC + "_createDeviceInterface() failed");

        (*plugInInterface)->Release(plugInInterface);
    }
    return hidDeviceInterface != nullptr;
}

bool AppleRemote::_openDevice()
{
    CFRunLoopSourceRef eventSource;
    IOReturn           ioReturnValue;
    IOHIDOptionsType   openMode;


    if (openInExclusiveMode)
        openMode = kIOHIDOptionsTypeSeizeDevice;
    else
        openMode = kIOHIDOptionsTypeNone;

    ioReturnValue = (*hidDeviceInterface)->open(hidDeviceInterface, openMode);

    if (ioReturnValue != KERN_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "_openDevice() failed");
        return false;
    }
    queue = (*hidDeviceInterface)->allocQueue(hidDeviceInterface);
    if (!queue)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "_openDevice() - error allocating queue");
        return false;
    }

    HRESULT result = (*queue)->create(queue, 0, 12);
    if (result != S_OK || !queue)
        LOG(VB_GENERAL, LOG_ERR, LOC + "_openDevice() - error creating queue");

    for (int & iter : cookies)
    {
        auto cookie = (IOHIDElementCookie)iter;
        (*queue)->addElement(queue, cookie, 0);
    }

    ioReturnValue = (*queue)->createAsyncEventSource(queue, &eventSource);
    if (ioReturnValue != KERN_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
                "_openDevice() - failed to create async event source");
        return false;
    }

    ioReturnValue = (*queue)->setEventCallout(queue, QueueCallbackFunction,
                                              this, nullptr);
    if (ioReturnValue != KERN_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "_openDevice() - error registering callback");
        return false;
    }

    CFRunLoopAddSource(CFRunLoopGetCurrent(),
                       eventSource, kCFRunLoopDefaultMode);
    (*queue)->start(queue);
    return true;
}

void AppleRemote::QueueCallbackFunction(void* target, IOReturn result,
                                        void* refcon, void* sender)
{
    auto* remote = static_cast<AppleRemote*>(target);

    remote->_queueCallbackFunction(result, refcon, sender);
}

void AppleRemote::_queueCallbackFunction(IOReturn result,
                                         void* /*refcon*/, void* /*sender*/)
{
    AbsoluteTime      zeroTime = {0,0};
    SInt32            sumOfValues = 0;
    std::stringstream cookieString;

    while (result == kIOReturnSuccess)
    {
        IOHIDEventStruct event;

        result = (*queue)->getNextEvent(queue, &event, zeroTime, 0);
        if (result != kIOReturnSuccess)
            break;

        if (REMOTE_SWITCH_COOKIE == (int)event.elementCookie)
        {
            remoteId=event.value;
            _handleEventWithCookieString(REMOTE_COOKIE_STR, 0);
        }
        else
        {
            sumOfValues+=event.value;
            cookieString << std::dec << (int)event.elementCookie << "_";
        }
    }

    _handleEventWithCookieString(cookieString.str(), sumOfValues);
}

void AppleRemote::_handleEventWithCookieString(std::string cookieString,
                                               SInt32 sumOfValues)
{
    std::map<std::string,AppleRemote::Event>::iterator ii;

    ii = cookieToButtonMapping.find(cookieString);
    if (ii != cookieToButtonMapping.end() && _listener)
    {
        AppleRemote::Event buttonid = ii->second;
        if (_listener)
            _listener->appleRemoteButton(buttonid, sumOfValues>0);
    }
}
