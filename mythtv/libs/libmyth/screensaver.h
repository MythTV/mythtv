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
    ScreenSaverControl();
    ~ScreenSaverControl();

    void Disable(void);
    void Restore(void);
    void Reset(void);

  protected:
    class ScreenSaverPrivate *d;
};

#endif // MYTH_SCREENSAVER_H

