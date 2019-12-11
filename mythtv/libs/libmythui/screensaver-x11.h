#ifndef MYTH_SCREENSAVER_X11_H
#define MYTH_SCREENSAVER_X11_H

#include <QObject>

#include "screensaver.h"

class ScreenSaverX11 : public QObject, public ScreenSaver
{
    Q_OBJECT

  public:
    ScreenSaverX11();
    ~ScreenSaverX11();

    void Disable(void) override; // ScreenSaver
    void Restore(void) override; // ScreenSaver
    void Reset(void) override; // ScreenSaver

    bool Asleep(void) override; // ScreenSaver

  public slots:
    void resetSlot();

  protected:
    class ScreenSaverX11Private *d {nullptr}; // NOLINT(readability-identifier-naming)
};

#endif // MYTH_SCREENSAVER_X11_H

