#ifndef MYTH_SCREENSAVER_H
#define MYTH_SCREENSAVER_H

// Qt
#include <QObject>

// Std
#include <vector>

class MythDisplay;

/// Base Class for screensavers
class MythScreenSaver
{
  public:
    MythScreenSaver() = default;
    virtual ~MythScreenSaver() = default;

    virtual void Disable() = 0;
    virtual void Restore() = 0;
    virtual void Reset() = 0;
    virtual bool Asleep() = 0;

  private:
    Q_DISABLE_COPY(MythScreenSaver)
};

/// Controls all instances of the screensaver
class MythScreenSaverControl : public QObject
{
    Q_OBJECT

  public:
    MythScreenSaverControl(MythDisplay* mDisplay);
    ~MythScreenSaverControl() override;

  public slots:
    void Disable();
    void Restore();
    void Reset();
    bool Asleep();

  private:
    Q_DISABLE_COPY(MythScreenSaverControl)
    std::vector<MythScreenSaver*> m_screenSavers;
};

#endif
