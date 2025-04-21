// MythTV
#include "mythconfig.h"
#include "platforms/mythcocoautils.h"
#include "mythlogging.h"
#include "mythpowerosx.h"

// OSX
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>
#include <AvailabilityMacros.h>

#if !HAVE_IOMAINPORT
#define kIOMainPortDefault kIOMasterPortDefault
#endif

#define LOC QString("PowerOSX: ")

static OSStatus SendAppleEventToSystemProcess(AEEventID EventToSend);

/*! \brief Power management for OSX
 *
 * \note Qt only creates a CFRunLoop in the main thread so this object must be
 * created and used in the main thread only - otherwise callbacks will not work.
*/
MythPowerOSX::MythPowerOSX()
{
    MythPowerOSX::Init();
}

MythPowerOSX::~MythPowerOSX()
{
    CocoaAutoReleasePool pool;

    // deregister power status change notifications
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(),
                          IONotificationPortGetRunLoopSource(m_powerNotifyPort),
                          kCFRunLoopDefaultMode );
    IODeregisterForSystemPower(&m_powerNotifier);
    IOServiceClose(m_rootPowerDomain);
    IONotificationPortDestroy(m_powerNotifyPort);

    // deregister power source change notifcations
    if (m_powerRef)
    {
        CFRunLoopRemoveSource(CFRunLoopGetCurrent(), m_powerRef, kCFRunLoopDefaultMode);
        CFRelease(m_powerRef);
    }
}

void MythPowerOSX::Init(void)
{
    CocoaAutoReleasePool pool;

    // Register for power status updates
    m_rootPowerDomain = IORegisterForSystemPower(this, &m_powerNotifyPort, PowerCallBack, &m_powerNotifier);
    if (m_rootPowerDomain)
    {
        CFRunLoopAddSource(CFRunLoopGetCurrent(),
                           IONotificationPortGetRunLoopSource(m_powerNotifyPort),
                           kCFRunLoopDefaultMode);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to setup power status callback");
    }

    // Is there a battery?
    CFArrayRef batteryinfo = NULL;
    if (IOPMCopyBatteryInfo(kIOMainPortDefault, &batteryinfo) == kIOReturnSuccess)
    {
        CFRelease(batteryinfo);

        // register for notification of power source changes
        m_powerRef = IOPSNotificationCreateRunLoopSource(PowerSourceCallBack, this);
        if (m_powerRef)
        {
            CFRunLoopAddSource(CFRunLoopGetCurrent(), m_powerRef, kCFRunLoopDefaultMode);
            Refresh();
        }
    }

    if (!m_powerRef)
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to setup power source callback");

    m_features = FeatureShutdown | FeatureRestart;
    if (IOPMSleepEnabled())
        m_features |= FeatureSuspend;

    MythPower::Init();
}

bool MythPowerOSX::DoFeature(bool /*Delayed*/)
{
    if (!((m_features & m_scheduledFeature) && m_scheduledFeature))
        return false;

    FeatureHappening();
    AEEventID event = 0;
    switch (m_scheduledFeature)
    {
        case FeatureSuspend:  event = kAESleep;    break;
        case FeatureShutdown: event = kAEShutDown; break;
        case FeatureRestart:  event = kAERestart;  break;
        default: return false;
    }
    return SendAppleEventToSystemProcess(event) == noErr;
}

void MythPowerOSX::Refresh(void)
{
    if (!m_powerRef)
        return;

    int newlevel = UnknownPower;
    CFTypeRef  info = IOPSCopyPowerSourcesInfo();
    CFArrayRef list = IOPSCopyPowerSourcesList(info);

    for (int i = 0; i < CFArrayGetCount(list); i++)
    {
        CFTypeRef source = CFArrayGetValueAtIndex(list, i);
        CFDictionaryRef description = IOPSGetPowerSourceDescription(info, source);

        if (static_cast<CFBooleanRef>(CFDictionaryGetValue(description, CFSTR(kIOPSIsPresentKey))) == kCFBooleanFalse)
            continue;

        auto type = static_cast<CFStringRef>(CFDictionaryGetValue(description, CFSTR(kIOPSTransportTypeKey)));
        if (type && CFStringCompare(type, CFSTR(kIOPSInternalType), 0) == kCFCompareEqualTo)
        {
            auto state = static_cast<CFStringRef>(CFDictionaryGetValue(description, CFSTR(kIOPSPowerSourceStateKey)));
            if (state && CFStringCompare(state, CFSTR(kIOPSACPowerValue), 0) == kCFCompareEqualTo)
            {
                newlevel = ACPower;
            }
            else if (state && CFStringCompare(state, CFSTR(kIOPSBatteryPowerValue), 0) == kCFCompareEqualTo)
            {
                int32_t current;
                int32_t max;
                auto capacity = static_cast<CFNumberRef>(CFDictionaryGetValue(description, CFSTR(kIOPSCurrentCapacityKey)));
                CFNumberGetValue(capacity, kCFNumberSInt32Type, &current);
                capacity = static_cast<CFNumberRef>(CFDictionaryGetValue(description, CFSTR(kIOPSMaxCapacityKey)));
                CFNumberGetValue(capacity, kCFNumberSInt32Type, &max);
                newlevel = static_cast<int>(((static_cast<float>(current) /
                                             (static_cast<float>(max))) * 100.0F));
            }
            else
            {
                newlevel = UnknownPower;
            }
        }
    }

    CFRelease(list);
    CFRelease(info);
    PowerLevelChanged(newlevel);
}

/*! \brief Receive notification of changes to the power supply.
 *
 *  Changes may be a change in the battery level or switches between mains power and battery.
*/
void MythPowerOSX::PowerSourceCallBack(void *Reference)
{
    CocoaAutoReleasePool pool;
    auto* power = static_cast<MythPowerOSX*>(Reference);
    if (power)
        power->Refresh();
}

/*! \brief Receive notification of power status changes.
 *
 * \note Spontaneous shutdown and sleep events (e.g. user clicks on Shutdown
 * from main menu bar) will cause signals to be sent to MythFrontend - which
 * usually trigger the exit prompt.
*/
void MythPowerOSX::PowerCallBack(void *Reference, io_service_t /*unused*/,
                                 natural_t Type, void *Data)
{
    CocoaAutoReleasePool pool;
    auto* power  = static_cast<MythPowerOSX*>(Reference);
    if (!power)
        return;

    switch (Type)
    {
        case kIOMessageCanSystemPowerOff:
            IOAllowPowerChange(power->m_rootPowerDomain, reinterpret_cast<intptr_t>(Data));
            power->FeatureHappening(FeatureShutdown);
            break;
        case kIOMessageCanSystemSleep:
            IOAllowPowerChange(power->m_rootPowerDomain, reinterpret_cast<intptr_t>(Data));
            power->FeatureHappening(FeatureSuspend);
            break;
        case kIOMessageSystemWillPowerOff:
            IOAllowPowerChange(power->m_rootPowerDomain, reinterpret_cast<intptr_t>(Data));
            power->FeatureHappening(FeatureShutdown);
            break;
        case kIOMessageSystemWillRestart:
            IOAllowPowerChange(power->m_rootPowerDomain, reinterpret_cast<intptr_t>(Data));
            power->FeatureHappening(FeatureRestart);
            break;
        case kIOMessageSystemWillSleep:
            IOAllowPowerChange(power->m_rootPowerDomain, reinterpret_cast<intptr_t>(Data));
            power->FeatureHappening(FeatureSuspend);
            break;
        case kIOMessageSystemHasPoweredOn:
            power->DidWakeUp();
            break;
    }
}

// see Technical Q&A QA1134
OSStatus SendAppleEventToSystemProcess(AEEventID EventToSend)
{
    AEAddressDesc targetDesc;
    static const ProcessSerialNumber kPSNOfSystemProcess = { 0, kSystemProcess };
    AppleEvent eventReply       = { typeNull, nullptr };
    AppleEvent appleEventToSend = { typeNull, nullptr };

    OSStatus error = noErr;
    error = AECreateDesc(typeProcessSerialNumber, &kPSNOfSystemProcess,
                         sizeof(kPSNOfSystemProcess), &targetDesc);
    if (error != noErr)
        return error;

    error = AECreateAppleEvent(kCoreEventClass, EventToSend, &targetDesc,
                               kAutoGenerateReturnID, kAnyTransactionID, &appleEventToSend);
    AEDisposeDesc(&targetDesc);
    if (error != noErr)
        return error;

    error = AESendMessage(&appleEventToSend, &eventReply, kAENormalPriority, kAEDefaultTimeout);
    AEDisposeDesc(&appleEventToSend);
    if (error != noErr)
        return error;

    AEDisposeDesc(&eventReply);
    return error;
}
