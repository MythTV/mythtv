#ifndef MYTHDISPLAY_H
#define MYTHDISPLAY_H

// Qt
#include <QSize>
#include <QScreen>
#include <QMutex>

// MythTV
#include "libmythbase/mythcommandlineparser.h"
#include "libmythui/mythdisplaymode.h"
#include "libmythui/mythedid.h"
#include "libmythui/mythhdr.h"
#include "libmythui/mythuiexp.h"
#include "libmythui/mythvrr.h"

// Std
#include <cmath>

class MythMainWindow;

class MUI_PUBLIC MythDisplay : public QObject
{
    Q_OBJECT

    friend class MythMainWindow;

  public:
    virtual bool  VideoModesAvailable  () { return false; }
    virtual bool  UsingVideoModes      () { return false; }
    virtual bool  IsPlanar             () { return false; }
    virtual const MythDisplayModes& GetVideoModes();

    static void  ConfigureQtGUI        (int SwapInterval, const MythCommandLineParser& CmdLine);
    static bool  SpanAllScreens        ();
    static QString GetExtraScreenInfo  (QScreen *qScreen);
    QStringList  GetDescription        ();
    QRect        GetScreenBounds       ();
    QScreen*     GetCurrentScreen      ();
    static int   GetScreenCount        ();
    double       GetPixelAspectRatio   ();
    QSize        GetGUIResolution      ();
    bool         NextModeIsLarger      (QSize Size);
    void         SwitchToDesktop       ();
    bool         SwitchToGUI           (bool Wait = false);
    bool         SwitchToVideo         (QSize Size, double Rate = 0.0);
    QSize        GetResolution         ();
    QSize        GetPhysicalSize       ();
    double       GetRefreshRate        () const;
    std::chrono::microseconds GetRefreshInterval (std::chrono::microseconds Fallback) const;
    double       GetAspectRatio        (QString &Source, bool IgnoreModeOverride = false);
    double       EstimateVirtualAspectRatio();
    MythEDID&    GetEDID               ();
    MythDisplayRates GetRefreshRates   (QSize Size);
    MythHDRPtr   GetHDRState           ();

  public slots:
    virtual void ScreenChanged         (QScreen *qScreen);
    static void  PrimaryScreenChanged  (QScreen *qScreen);
    void         ScreenAdded           (QScreen *qScreen);
    void         ScreenRemoved         (QScreen *qScreen);
    void         PhysicalDPIChanged    (qreal    DPI);
    static void  GeometryChanged       (QRect Geometry);

  signals:
    void         CurrentScreenChanged  (QScreen *qScreen);
    void         ScreenCountChanged    (int      Screens);
    void         CurrentDPIChanged     (qreal    DPI);

  protected:
    static MythDisplay* Create(MythMainWindow* MainWindow);
    MythDisplay();
   ~MythDisplay() override;

    virtual void    UpdateCurrentMode  ();
    virtual bool    SwitchToVideoMode  (QSize Size, double Framerate);

    void            DebugModes         () const;
    void            SetWidget          (QWidget *MainWindow);
    static QScreen* GetDesiredScreen   ();
    static void     DebugScreen        (QScreen *qScreen, const QString &Message);
    void            Initialise         ();
    void            InitScreenBounds   ();
    void            WaitForScreenChange();
    void            WaitForNewScreen   ();
    void            InitHDR            ();

    bool            m_waitForModeChanges { true };
    bool            m_modeComplete     { false };
    double          m_refreshRate      { 0.0  };
    double          m_aspectRatioOverride { 0.0  };
    QSize           m_resolution       { 0, 0 };
    QSize           m_physicalSize     { 0, 0 };
    MythEDID        m_edid;
    QWidget*        m_widget           { nullptr };
    QWindow*        m_window           { nullptr };
    QScreen*        m_screen           { nullptr };
    MythDisplayModes m_videoModes;
    MythHDRPtr      m_hdrState         { nullptr };
    MythVRRPtr      m_vrrState         { nullptr };

  private:
    Q_DISABLE_COPY(MythDisplay)
    static void PauseForModeSwitch();

    bool            m_initialised      { false };
    bool            m_firstScreenChange{ true };
    QRect           m_screenBounds;
    MythDisplayMode m_desktopMode;
    MythDisplayMode m_guiMode;
    MythDisplayMode m_videoMode;
    DisplayModeMap  m_overrideVideoModes;
};

#endif
