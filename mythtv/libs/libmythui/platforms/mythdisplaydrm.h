#ifndef MYTHDISPLAYDRM_H
#define MYTHDISPLAYDRM_H

// Qt
#include <QObject>

// MythTV
#include "platforms/mythdrmdevice.h"
#include "mythdisplay.h"

class MythDisplayDRM : public MythDisplay
{
    Q_OBJECT

  public:
    MythDisplayDRM();
   ~MythDisplayDRM() override;

    void UpdateCurrentMode() override;
    MythDRMPtr GetDevice();

  signals:
    void screenChanged();

  public slots:
    void ScreenChanged(QScreen *qScreen) override;

  private:
    MythDRMPtr m_device;
};

#endif
