#include "screensaver-osx.h"

#include <CoreServices/CoreServices.h>

class ScreenSaverOSXPrivate 
{
public:
    static void timerCallback(CFRunLoopTimerRef timer, void *info)
    {
        (void)timer;
        (void)info;
        UpdateSystemActivity(OverallAct);
    };
    
    CFRunLoopTimerRef m_timer;

    friend class ScreenSaverOSX;
};

ScreenSaverOSX::ScreenSaverOSX() 
{
    d = new ScreenSaverOSXPrivate();
    d->m_timer = NULL;
}

ScreenSaverOSX::~ScreenSaverOSX() 
{
    Restore();
    delete d;
}

void ScreenSaverOSX::Disable(void) 
{
    CFRunLoopTimerContext context = { 0, NULL, NULL, NULL, NULL };
    if (!d->m_timer)
    {
        d->m_timer = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent(),
                                          30, 0, 0,
                                          ScreenSaverOSXPrivate::timerCallback,
                                          &context);
        if (d->m_timer)
            CFRunLoopAddTimer(CFRunLoopGetCurrent(),
                              d->m_timer,
                              kCFRunLoopCommonModes);
    }
}

void ScreenSaverOSX::Restore(void) 
{
    if (d->m_timer)
    {
        CFRunLoopTimerInvalidate(d->m_timer);
        CFRelease(d->m_timer);
        d->m_timer = NULL;
    }
}

void ScreenSaverOSX::Reset(void)
{
    // Wake up the screen saver now.
    ScreenSaverOSXPrivate::timerCallback(NULL, NULL);
}

bool ScreenSaverOSX::Asleep(void)
{
    return 0;
}

