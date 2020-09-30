#ifndef MYTH_SCREENSAVER_H
#define MYTH_SCREENSAVER_H

// Qt
#include <QEvent>
#include <QList>

/// Base Class for screensavers
class ScreenSaver
{
  public:
    ScreenSaver() = default;
    virtual ~ScreenSaver() = default;

    virtual void Disable() = 0;
    virtual void Restore() = 0;
    virtual void Reset() = 0;
    virtual bool Asleep() = 0;
};

/// Controls all instances of the screensaver
class ScreenSaverControl : public QObject
{
    Q_OBJECT

  public:
    ScreenSaverControl();
    ~ScreenSaverControl();

  public slots:
    void Disable();
    void Restore();
    void Reset();
    bool Asleep();

  private:
    QList<ScreenSaver *> m_screenSavers;
};

#endif
