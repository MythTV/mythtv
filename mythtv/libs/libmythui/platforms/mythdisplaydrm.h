#ifndef MYTHDISPLAYDRM_H
#define MYTHDISPLAYDRM_H

// Qt
#include <QObject>

// MythTV
#include "mythdisplay.h"

class MythDRMDevice;

class MythDisplayDRM final : public MythDisplay
{
    Q_OBJECT

  public:
    MythDisplayDRM();
   ~MythDisplayDRM() override;

    DisplayInfo GetDisplayInfo  (int VideoRate = 0) override;

  public slots:
    void ScreenChanged(QScreen *qScreen) override;

  private:
    MythDRMDevice* m_device;
};

#endif // MYTHDISPLAYDRM_H
