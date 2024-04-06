#ifndef MYTHDISPLAYDRM_H
#define MYTHDISPLAYDRM_H

// Qt
#include <QObject>

// MythTV
#include "libmythui/mythdisplay.h"
#include "libmythui/platforms/mythdrmdevice.h"

class MUI_PUBLIC MythDisplayDRM : public MythDisplay
{
    Q_OBJECT

  public:
    MythDisplayDRM(MythMainWindow* MainWindow);
   ~MythDisplayDRM() override;

  public slots:
    void MainWindowReady();
    static bool DirectRenderingAvailable();

  public:
    bool IsPlanar() override;
    bool VideoModesAvailable() override;
    bool UsingVideoModes() override;
    void UpdateCurrentMode() override;
    bool SwitchToVideoMode (QSize Size, double DesiredRate) override;
    const MythDisplayModes& GetVideoModes() override;
    MythDRMPtr GetDevice();

  signals:
    void screenChanged();

  public slots:
    void ScreenChanged(QScreen *qScreen) override;

  private:
    MythDRMPtr m_device;
    QMap<uint64_t, int> m_modeMap;
};

#endif
