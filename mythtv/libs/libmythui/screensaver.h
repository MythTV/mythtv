#ifndef MYTH_SCREENSAVER_H
#define MYTH_SCREENSAVER_H

#include <QEvent>
#include <QList>

class ScreenSaverEvent : public QEvent
{
public:
    enum ScreenSaverEventKind {ssetDisable, ssetRestore, ssetReset};

    ScreenSaverEvent(ScreenSaverEventKind type) :
        QEvent(kEventType), sset(type)
    {
    }

    ScreenSaverEventKind getSSEventType()
    {
        return sset;
    }

    static Type kEventType;

protected:
    ScreenSaverEventKind sset;
};

/// Base Class for screensavers
class ScreenSaver
{
  public:
    ScreenSaver() { };
    virtual ~ScreenSaver() { };

    virtual void Disable(void) = 0;
    virtual void Restore(void) = 0;
    virtual void Reset(void) = 0;
    virtual bool Asleep(void) = 0;
};

/// Controls all instances of the screensaver
class ScreenSaverControl
{
  public:
    ScreenSaverControl();
    ~ScreenSaverControl();

    void Disable(void);
    void Restore(void);
    void Reset(void);
    bool Asleep(void);
  private:
    QList<ScreenSaver *> m_screenSavers;
};

#endif // MYTH_SCREENSAVER_H
