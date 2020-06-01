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
    static MythDisplay* AcquireRelease(bool Acquire = true);
    static QStringList  GetDescription(void);

    virtual bool  VideoModesAvailable(void) { return false; }
    virtual bool  UsingVideoModes(void) { return false; }
    virtual const vector<MythDisplayMode>& GetVideoModes(void);

    static void  ConfigureQtGUI        (int SwapInterval = 1);
    static bool  SpanAllScreens        (void);
    static QString GetExtraScreenInfo  (QScreen *qScreen);

    QRect        GetScreenBounds       (void);
    QScreen*     GetCurrentScreen      (void);
    static int   GetScreenCount        (void);
    double       GetPixelAspectRatio   (void);
    QSize        GetGUIResolution      (void);
    bool         NextModeIsLarger      (QSize Size);
    void         SwitchToDesktop       (void);
    bool         SwitchToGUI           (bool Wait = false);
    bool         SwitchToVideo         (QSize Size, double Rate = 0.0);
    QSize        GetResolution         (void);
    QSize        GetPhysicalSize       (void);
    double       GetRefreshRate        (void);
    int          GetRefreshInterval    (int Fallback);
    double       GetAspectRatio        (QString &Source, bool IgnoreModeOverride = false);
    double       EstimateVirtualAspectRatio(void);
    MythEDID&    GetEDID               (void);
    std::vector<double> GetRefreshRates(QSize Size);

  public slots:
    virtual void ScreenChanged         (QScreen *qScreen);
    static void  PrimaryScreenChanged  (QScreen *qScreen);
    void         ScreenAdded           (QScreen *qScreen);
    void         ScreenRemoved         (QScreen *qScreen);
    void         GeometryChanged       (const QRect &Geometry);
    void         PhysicalDPIChanged    (qreal    DPI);

  signals:
    void         CurrentScreenChanged  (QScreen *qScreen);
    void         ScreenCountChanged    (int Screens);
    void         CurrentDPIChanged     (qreal    DPI);

  protected:
    MythDisplay();
    ~MythDisplay() override;

    virtual void    UpdateCurrentMode  (void);
    virtual bool    SwitchToVideoMode  (QSize Size, double Framerate);

    void            DebugModes         (void) const;
    void            SetWidget          (QWidget *MainWindow);
    static QScreen* GetDesiredScreen   (void);
    static void     DebugScreen        (QScreen *qScreen, const QString &Message);
    void            Initialise         (void);
    void            InitScreenBounds   (void);
    void            WaitForScreenChange(void);
    void            WaitForNewScreen   (void);


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
    vector<MythDisplayMode> m_videoModes { };

  private:
    Q_DISABLE_COPY(MythDisplay)
    static void PauseForModeSwitch(void);

    bool            m_initialised      { false };
    bool            m_firstScreenChange{ true };
    QRect           m_screenBounds     { };
    MythDisplayMode m_desktopMode      { };
    MythDisplayMode m_guiMode          { };
    MythDisplayMode m_videoMode        { };
    DisplayModeMap  m_overrideVideoModes { };
};

#endif // MYTHDISPLAY_H
