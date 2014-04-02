
#include "AppleRemote.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/errno.h>
#include <sys/sysctl.h>  // for sysctlbyname
#include <sysexits.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>     // for Gestalt

#include <sstream>

#include "mythlogging.h"

AppleRemote*    AppleRemote::_instance = 0;


#define REMOTE_SWITCH_COOKIE 19
#define REMOTE_COOKIE_STR    "19_"
#define ATV_COOKIE_STR       "17_9_280_"
#define LONG_PRESS_COUNT     10
#define KEY_RESPONSE_TIME    150  /* msecs before we send a key up event */

#define LOC QString("AppleRemote::")

typedef struct _ATV_IR_EVENT
{
    UInt32  time_ms32;
    UInt32  time_ls32;  // units of microsecond
    UInt32  unknown1;
    UInt32  keycode;
    UInt32  unknown2;
} ATV_IR_EVENT;

static io_object_t _findAppleRemoteDevice(const char *devName);

AppleRemote::Listener::~Listener()
{
}

AppleRemote * AppleRemote::Get()
{
    if (_instance == 0)
        _instance = new AppleRemote();

    return _instance;
}

AppleRemote::~AppleRemote()
{
    stopListening();

    if (mUsingNewAtv)
        delete mCallbackTimer;

    if (isRunning())
    {
        exit(0);
    }
    if (this == _instance)
    {
        _instance = 0;
    }
}

bool AppleRemote::isListeningToRemote()
{
    return (hidDeviceInterface != NULL && !cookies.empty() && queue != NULL);
}

void AppleRemote::setListener(AppleRemote::Listener* listener)
{
    _listener = listener;
}

void AppleRemote::startListening()
{
    if (queue != NULL)   // already listening
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
    if (queue != NULL)
    {
        (*queue)->stop(queue);
        (*queue)->dispose(queue);
        (*queue)->Release(queue);
        queue = NULL;
    }

    if (!cookies.empty())
        cookies.clear();

    if (hidDeviceInterface != NULL)
    {
        (*hidDeviceInterface)->close(hidDeviceInterface);
        (*hidDeviceInterface)->Release(hidDeviceInterface);
        hidDeviceInterface = NULL;
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

// Figure out if we're running on the Apple TV, and what version
static float GetATVversion()
{
    SInt32 macVersion;
    size_t len = 512;
    char   hw_model[512] = "unknown";


    Gestalt(gestaltSystemVersion, &macVersion);

    if ( macVersion > 0x1040 )  // Mac OS 10.5 or greater is
        return 0.0;             // definitely not an Apple TV

    sysctlbyname("hw.model", &hw_model, &len, NULL, 0);

    float  version = 0.0;
    if ( strstr(hw_model,"AppleTV1,1") )
    {
        // Find the build version of the AppleTV OS
        FILE *inpipe = popen("sw_vers -buildVersion", "r");
        char linebuf[1000];
        if (inpipe && fgets(linebuf, sizeof(linebuf) - 1, inpipe) )
        {
            if (     strstr(linebuf,"8N5107") ) version = 1.0;
            else if (strstr(linebuf,"8N5239") ) version = 1.1;
            else if (strstr(linebuf,"8N5400") ) version = 2.0;
            else if (strstr(linebuf,"8N5455") ) version = 2.01;
            else if (strstr(linebuf,"8N5461") ) version = 2.02;
            else if (strstr(linebuf,"8N5519") ) version = 2.1;
            else if (strstr(linebuf,"8N5622") ) version = 2.2;
            else
                version = 2.3;
        }
        if (inpipe)
            pclose(inpipe);
    }
    return version;
}

// protected
AppleRemote::AppleRemote() : MThread("AppleRemote"),
                             openInExclusiveMode(true),
                             hidDeviceInterface(0),
                             queue(0),
                             remoteId(0),
                             _listener(0),
                             mUsingNewAtv(false),
                             mLastEvent(AppleRemote::Undefined),
                             mEventCount(0),
                             mKeyIsDown(false)
{
    if ( GetATVversion() > 2.2 )
    {
        LOG(VB_GENERAL, LOG_INFO,
            LOC + "AppleRemote() detected Apple TV > v2.3");
        mUsingNewAtv = true;
        mCallbackTimer = new QTimer();
        QObject::connect(mCallbackTimer,       SIGNAL(timeout()),
                         (const QObject*)this, SLOT(TimeoutHandler()));
        mCallbackTimer->setSingleShot(true);
        mCallbackTimer->setInterval(KEY_RESPONSE_TIME);
    }

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

    // ATV 1.0, 2.0-2.02
    cookieToButtonMapping["14_12_11_6_"]          = Up;
    cookieToButtonMapping["14_13_11_6_"]          = Down;
    cookieToButtonMapping["14_7_6_14_7_6_"]       = Menu;
    cookieToButtonMapping["14_8_6_14_8_6_"]       = Select;
    cookieToButtonMapping["14_9_6_14_9_6_"]       = Right;
    cookieToButtonMapping["14_10_6_14_10_6_"]     = Left;
    cookieToButtonMapping["14_6_4_2_"]            = RightHold;
    cookieToButtonMapping["14_6_3_2_"]            = LeftHold;
    cookieToButtonMapping["14_6_14_6_"]           = MenuHold;
    cookieToButtonMapping["18_14_6_18_14_6_"]     = PlayHold;

    // ATV 1.0, 2.1-2.2
    cookieToButtonMapping["15_13_12_"]            = Up;
    cookieToButtonMapping["15_14_12_"]            = Down;
    cookieToButtonMapping["15_8_15_8_"]           = Menu;
    cookieToButtonMapping["15_9_15_9_"]           = Select;
    cookieToButtonMapping["15_10_15_10_"]         = Right;
    cookieToButtonMapping["15_11_15_11_"]         = Left;
    cookieToButtonMapping["15_5_3_"]              = RightHold;
    cookieToButtonMapping["15_4_3_"]              = LeftHold;
    cookieToButtonMapping["15_6_15_6_"]           = MenuHold;
    cookieToButtonMapping["19_15_19_15_"]         = PlayHold;

    // ATV 2.30
    cookieToButtonMapping["17_9_280_80"]          = Up;
    cookieToButtonMapping["17_9_280_48"]          = Down;
    cookieToButtonMapping["17_9_280_64"]          = Menu;
    cookieToButtonMapping["17_9_280_32"]          = Select;
    cookieToButtonMapping["17_9_280_96"]          = Right;
    cookieToButtonMapping["17_9_280_16"]          = Left;

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
    CFMutableDictionaryRef hidMatchDictionary = 0;
    io_iterator_t          hidObjectIterator = 0;
    io_object_t            hidDevice = 0;
    IOReturn               ioReturnValue;


    hidMatchDictionary = IOServiceMatching(devName);

    // check for matching devices
    ioReturnValue = IOServiceGetMatchingServices(kIOMasterPortDefault,
                                                 hidMatchDictionary,
                                                 &hidObjectIterator);

    if ((ioReturnValue == kIOReturnSuccess) && (hidObjectIterator != 0))
        hidDevice = IOIteratorNext(hidObjectIterator);
    else
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("_findAppleRemoteDevice(%1) failed").arg(devName));

    // IOServiceGetMatchingServices consumes a reference to the dictionary,
    // so we don't need to release the dictionary ref.
    hidMatchDictionary = 0;
    return hidDevice;
}

bool AppleRemote::_initCookies()
{
    IOHIDDeviceInterface122** handle;
    CFArrayRef                elements;
    IOReturn                  success;

    handle  = (IOHIDDeviceInterface122**)hidDeviceInterface;
    success = (*handle)->copyMatchingElements(handle,
                                              NULL,
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

            if (object == 0 || CFGetTypeID(object) != CFNumberGetTypeID())
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
    IOCFPlugInInterface** plugInInterface = NULL;
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
    return hidDeviceInterface != 0;
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

    for (std::vector<int>::iterator iter = cookies.begin();
         iter != cookies.end();
         ++iter)
    {
        IOHIDElementCookie cookie = (IOHIDElementCookie)(*iter);
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
                                              this, NULL);
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
    AppleRemote* remote = static_cast<AppleRemote*>(target);

    if (remote->mUsingNewAtv)
        remote->_queueCallbackATV23(result);
    else
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

void AppleRemote::_queueCallbackATV23(IOReturn result)
{
    AbsoluteTime      zeroTime = {0,0};
    SInt32            sumOfValues = 0;
    std::stringstream cookieString;
    UInt32            key_code = 0;


    if (mCallbackTimer->isActive())
    {
        mCallbackTimer->stop();
    }

    while (result == kIOReturnSuccess)
    {
        IOHIDEventStruct  event;

        result = (*queue)->getNextEvent(queue, &event, zeroTime, 0);
        if (result != kIOReturnSuccess)
            continue;

        if ( ((int)event.elementCookie == 280) && (event.longValueSize == 20))
        {
            ATV_IR_EVENT* atv_ir_event = (ATV_IR_EVENT*)event.longValue;
            key_code = atv_ir_event->keycode;
        }

        if (((int)event.elementCookie) != 5 )
        {
            sumOfValues += event.value;
            cookieString << std::dec << (int)event.elementCookie << "_";
        }
    }

    if (strcmp(cookieString.str().c_str(), ATV_COOKIE_STR) == 0)
    {
        cookieString << std::dec << (int) ( (key_code & 0x00007F00) >> 8);

        sumOfValues = 1;
        _handleEventATV23(cookieString.str(), sumOfValues);
    }
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

// With the ATV from 2.3 onwards, we just get IR events.
// We need to simulate the key up and hold events

void AppleRemote::_handleEventATV23(std::string cookieString,
                                    SInt32      sumOfValues)
{
    std::map<std::string,AppleRemote::Event>::iterator ii;
    ii = cookieToButtonMapping.find(cookieString);

    if (ii != cookieToButtonMapping.end() )
    {
        AppleRemote::Event event = ii->second;

        if (mLastEvent == Undefined)    // new event
        {
            mEventCount = 1;
            // Need to figure out if this is a long press or a short press,
            // so can't just send a key down event right now. It will be
            // scheduled to run
        }
        else if (event != mLastEvent)    // a new event, faster than timer
        {
            mEventCount = 1;
            mKeyIsDown = true;

            if (_listener)
            {
                // Only send key up events for events that have separateRelease
                // defined as true in AppleRemoteListener.cpp
                if (mLastEvent == Up       || mLastEvent == Down ||
                    mLastEvent == LeftHold || mLastEvent == RightHold)
                {
                    _listener->appleRemoteButton(mLastEvent,
                                                 /*pressedDown*/false);
                }
                _listener->appleRemoteButton(event, mKeyIsDown);
            }
        }
        else // Same event again
        {
            AppleRemote::Event newEvent = Undefined;

            ++mEventCount;

            // Can the event have a hold state?
            switch (event)
            {
                case Right:
                    newEvent = RightHold;
                    break;
                case Left:
                    newEvent = LeftHold;
                    break;
                case Menu:
                    newEvent = MenuHold;
                    break;
                case Select:
                    newEvent = PlayHold;
                    break;
                default:
                    newEvent = event;
            }

            if (newEvent == event) // Doesn't have a long press
            {
                if (mKeyIsDown)
                {
                    if (_listener)
                    {
                        // Only send key up events for events that have separateRelease
                        // defined as true in AppleRemoteListener.cpp
                        if (mLastEvent == Up || mLastEvent == Down ||
                            mLastEvent == LeftHold || mLastEvent == RightHold)
                        {
                            _listener->appleRemoteButton(mLastEvent, /*pressedDown*/false);
                        }
                    }
                }

                mKeyIsDown = true;
                if (_listener)
                {
                    _listener->appleRemoteButton(newEvent, mKeyIsDown);
                }
            }
            else if (mEventCount == LONG_PRESS_COUNT)
            {
                mKeyIsDown = true;
                if (_listener)
                {
                    _listener->appleRemoteButton(newEvent, mKeyIsDown);
                }
            }
        }

        mLastEvent = event;
        mCallbackTimer->start();
    }
}

// Calls key down / up events on the ATV > v2.3
void AppleRemote::TimeoutHandler()
{
    if (_listener)
    {
        _listener->appleRemoteButton(mLastEvent, !mKeyIsDown);
    }

    mKeyIsDown = !mKeyIsDown;

    if (!mKeyIsDown)
    {
        mEventCount = 0;
        mLastEvent = Undefined;
    }
    else
    {
        // Schedule a key up event for events that have separateRelease
        // defined as true in AppleRemoteListener.cpp

        if (mLastEvent == Up       || mLastEvent == Down ||
            mLastEvent == LeftHold || mLastEvent == RightHold)
        {
            mCallbackTimer->start();
        }
        else
        {
            mKeyIsDown = false;
            mEventCount = 0;
            mLastEvent = Undefined;
        }

    }
}
