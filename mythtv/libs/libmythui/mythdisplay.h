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

// Std
#include <cmath>

#define VALID_RATE(rate) ((rate) > 20.0F && (rate) < 200.0F)

class DisplayInfo
{
  public:
    DisplayInfo(void) = default;
    explicit DisplayInfo(int Rate)
      : m_rate(Rate)
    {
    }

    int Rate(void) const
    {
        return static_cast<int>(lroundf(m_rate));
    }

    QSize m_size { 0, 0};
    QSize m_res  { 0, 0};
    float m_rate { -1 };
};

class MUI_PUBLIC MythDisplay : public QObject, public ReferenceCounter
{
    Q_OBJECT

    friend class MythMainWindow;

  public:
    static MythDisplay* AcquireRelease(bool Acquire = true);

    typedef enum
    {
        GUI          = 0,
        VIDEO        = 1,
        CUSTOM_GUI   = 2,
        CUSTOM_VIDEO = 3,
        DESKTOP      = 4,
        MAX_MODES    = 5,
    } Mode;

    QScreen*   GetCurrentScreen (void);
    static int GetScreenCount   (void);
    double     GetPixelAspectRatio(void);

    virtual DisplayInfo GetDisplayInfo(int VideoRate = 0);
    static bool         SpanAllScreens(void);
    static QString      GetExtraScreenInfo(QScreen *qScreen);

    virtual bool UsingVideoModes   (void) { return false; }
    virtual const DisplayModeVector& GetVideoModes(void);
    bool         NextModeIsLarger  (Mode NextMode);
    bool         NextModeIsLarger  (int Width, int Height);
    void         SwitchToDesktop   (void);
    bool         SwitchToGUI       (Mode NextMode = GUI, bool Wait = false);
    bool         SwitchToVideo     (int Width, int Height, double Rate = 0.0);
    QSize        GetResolution     (void);
    QSize        GetPhysicalSize   (void);
    double       GetRefreshRate    (void);
    double       GetAspectRatio    (void);
    double       EstimateVirtualAspectRatio(void);
    std::vector<double> GetRefreshRates(int Width, int Height);

  public slots:
    virtual void ScreenChanged(QScreen *qScreen);
    static void PrimaryScreenChanged (QScreen *qScreen);
    void ScreenAdded          (QScreen *qScreen);
    void ScreenRemoved        (QScreen *qScreen);
    void GeometryChanged      (const QRect &Geometry);

  signals:
    void CurrentScreenChanged (QScreen *Screen);
    void ScreenCountChanged   (int Screens);

  protected:
    MythDisplay();
    virtual ~MythDisplay();

    void            DebugModes         (void) const;
    void            SetWidget          (QWidget *MainWindow);
    static QScreen* GetDesiredScreen   (void);
    static void     DebugScreen        (QScreen *qScreen, const QString &Message);
    static float    SanitiseRefreshRate(int Rate);

    void         InitialiseModes    (void);
    virtual bool SwitchToVideoMode  (int Width, int Height, double Framerate);
    void         WaitForScreenChange(void);

    QWidget* m_widget { nullptr };
    QScreen* m_screen { nullptr };
    mutable std::vector<MythDisplayMode> m_videoModes { };

  private:
    Q_DISABLE_COPY(MythDisplay)
    static void PauseForModeSwitch(void);

    Mode            m_curMode            { GUI };
    MythDisplayMode m_mode[MAX_MODES]    { };
    MythDisplayMode m_last               { }; // mirror of mode[current_mode]
    DisplayModeMap  m_inSizeToOutputMode { };
};

#endif // MYTHDISPLAY_H
