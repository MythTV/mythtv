#ifndef MYTHDISPLAY_H
#define MYTHDISPLAY_H

// Qt
#include <QSize>
#include <QScreen>
#include <QMutex>

// MythTV
#include "mythuiexp.h"
#include "referencecounter.h"
#include "mythdisplaymode.h"
#include "mythedid.h"

// Std
#include <cmath>

class MUI_PUBLIC MythDisplay : public QObject, public ReferenceCounter
{
    Q_OBJECT

    friend class MythMainWindow;

  public:
    virtual bool  VideoModesAvailable  () { return false; }
    virtual bool  UsingVideoModes      () { return false; }
    virtual const std::vector<MythDisplayMode>& GetVideoModes();

    static void  ConfigureQtGUI        (int SwapInterval = 1, const QString& Display = QString());
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
    int          GetRefreshInterval    (int Fallback) const;
    double       GetAspectRatio        (QString &Source, bool IgnoreModeOverride = false);
    double       EstimateVirtualAspectRatio();
    MythEDID&    GetEDID               ();
    std::vector<double> GetRefreshRates(QSize Size);

  public slots:
    virtual void ScreenChanged         (QScreen *qScreen);
    static void  PrimaryScreenChanged  (QScreen *qScreen);
    void         ScreenAdded           (QScreen *qScreen);
    void         ScreenRemoved         (QScreen *qScreen);
    void         PhysicalDPIChanged    (qreal    DPI);
    static void  GeometryChanged       (const QRect &Geometry);

  signals:
    void         CurrentScreenChanged  (QScreen *qScreen);
    void         ScreenCountChanged    (int      Screens);
    void         CurrentDPIChanged     (qreal    DPI);

  protected:
    static MythDisplay* Create();
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

    bool            m_waitForModeChanges { true };
    bool            m_modeComplete     { false };
    double          m_refreshRate      { 0.0  };
    double          m_aspectRatioOverride { 0.0  };
    QSize           m_resolution       { 0, 0 };
    QSize           m_physicalSize     { 0, 0 };
    MythEDID        m_edid             { };
    QWidget*        m_widget           { nullptr };
    QWindow*        m_window           { nullptr };
    QScreen*        m_screen           { nullptr };
    std::vector<MythDisplayMode> m_videoModes { };

  private:
    Q_DISABLE_COPY(MythDisplay)
    static void PauseForModeSwitch();

    bool            m_initialised      { false };
    bool            m_firstScreenChange{ true };
    QRect           m_screenBounds     { };
    MythDisplayMode m_desktopMode      { };
    MythDisplayMode m_guiMode          { };
    MythDisplayMode m_videoMode        { };
    DisplayModeMap  m_overrideVideoModes { };
};

#endif
