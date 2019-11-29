#ifndef Display_Res_H_
#define Display_Res_H_

// MythTV
#include "referencecounter.h"
#include "DisplayResScreen.h"
#include "mythuiexp.h"

// Std
#include <vector>

class MUI_PUBLIC DisplayRes : public ReferenceCounter
{
  public:
    enum Mode
    {
        GUI          = 0,
        VIDEO        = 1,
        CUSTOM_GUI   = 2,
        CUSTOM_VIDEO = 3,
        DESKTOP      = 4,
        MAX_MODES    = 5,
    };

    static DisplayRes* AcquireRelease(bool Acquire = true);
    static std::vector<DisplayResScreen> GetModes(void);

    void   SwitchToDesktop(void);
    bool   Initialize(void);
    bool   SwitchToVideo(int Width, int Height, double Rate = 0.0);
    bool   SwitchToGUI(Mode NextMode = GUI);
    bool   SwitchToCustomGUI(int Width, int Height, short Rate = 0);
    int    GetWidth(void) const;
    int    GetHeight(void) const;
    int    GetPhysicalWidth(void) const;
    int    GetPhysicalHeight(void) const;
    double GetRefreshRate(void) const;
    double GetAspectRatio(void) const;
    double GetPixelAspectRatio(void) const;
    std::vector<double> GetRefreshRates(int Width, int Height) const;

    virtual const DisplayResVector& GetVideoModes() const = 0;

  protected:
    DisplayRes(void);
    virtual ~DisplayRes(void) = default;

    virtual bool GetDisplayInfo(int &WidthPixels, int &HeightPixels,
                                int &WidthMM,     int &HeightMM,
                                double &RefreshRate, double &PixelAspectRatio) const = 0;
    virtual bool SwitchToVideoMode(int Width, int Height, double Framerate) = 0;

  private:
    Q_DISABLE_COPY(DisplayRes)
    static void PauseForModeSwitch(void);

    Mode m_curMode                        { GUI };
    DisplayResScreen m_mode[MAX_MODES]    { };
    DisplayResScreen m_last               { }; // mirror of mode[current_mode]
    DisplayResMap    m_inSizeToOutputMode { };
    double m_pixelAspectRatio             { 1.0 };
};

#endif
