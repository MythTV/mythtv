#ifndef MYTH_SCREENSAVER_H
#define MYTH_SCREENSAVER_H

#include <qevent.h>

class ScreenSaverEvent : public QCustomEvent
{
public:
    enum ScreenSaverEventType {ssetDisable, ssetRestore, ssetReset};

    ScreenSaverEvent(ScreenSaverEventType type) :
        QCustomEvent(kScreenSaverEventType), sset(type)
    {
    }

    ScreenSaverEventType getSSEventType()
    {
        return sset;
    }

    static const int kScreenSaverEventType = 23425;

protected:
    ScreenSaverEventType sset;
};

class ScreenSaverControl 
{
  public:
    // creates one of the concrete subsclasses
    static ScreenSaverControl* get(void);
    
    ScreenSaverControl() { };
    virtual ~ScreenSaverControl() { };

    virtual void Disable(void) = 0;
    virtual void Restore(void) = 0;
    virtual void Reset(void) = 0;

    virtual bool Asleep(void) = 0;
};

#endif // MYTH_SCREENSAVER_H

