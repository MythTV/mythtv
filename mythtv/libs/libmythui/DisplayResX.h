#ifndef DISPLAYRESX_H_
#define DISPLAYRESX_H_

// MythTV
#include "DisplayRes.h"

class DisplayResX : public DisplayRes
{
  public:
    DisplayResX(void);
   ~DisplayResX(void) override;

    static bool IsAvailable(void);
    const std::vector<DisplayResScreen>& GetVideoModes(void) const override;

  protected:
    bool GetDisplayInfo(int &WidthPixels, int &HeightPixels,
                        int &WidthMM, int &HeightMM,
                        double &RefreshRate, double &PixelAspectRatio) const override;
    bool SwitchToVideoMode(int width, int height, double desired_rate) override;

  private:
    void DebugModes(const QString& Message) const;
    mutable std::vector<DisplayResScreen> m_videoModes;
    mutable std::vector<DisplayResScreen> m_videoModesUnsorted;
};

#endif // DISPLAYRESX_H_
