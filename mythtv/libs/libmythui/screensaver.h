#ifndef MYTH_SCREENSAVER_H
#define MYTH_SCREENSAVER_H

// Qt
#include <QEvent>
#include <QList>

class ScreenSaverEvent : public QEvent
{
public:
    enum ScreenSaverEventKind {ssetDisable, ssetRestore, ssetReset};

    explicit ScreenSaverEvent(ScreenSaverEventKind type) :
        QEvent(kEventType), m_sset(type)
    {
    }

    ScreenSaverEventKind getSSEventType()
    {
        return m_sset;
    }

    static Type kEventType;

protected:
    ScreenSaverEventKind m_sset;
};

/// Base Class for screensavers
class ScreenSaver
{
  public:
    ScreenSaver() = default;
    virtual ~ScreenSaver() = default;

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
