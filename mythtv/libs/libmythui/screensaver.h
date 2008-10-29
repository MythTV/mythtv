#ifndef MYTH_SCREENSAVER_H
#define MYTH_SCREENSAVER_H

#include <QEvent>

class ScreenSaverEvent : public QEvent
{
public:
    enum ScreenSaverEventKind {ssetDisable, ssetRestore, ssetReset};

    ScreenSaverEvent(ScreenSaverEventKind type) :
        QEvent((QEvent::Type)ScreenSaverEventType), sset(type)
    {
    }

    ScreenSaverEventKind getSSEventType()
    {
        return sset;
    }

    enum EventID { ScreenSaverEventType = 23425 };

protected:
    ScreenSaverEventKind sset;
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
