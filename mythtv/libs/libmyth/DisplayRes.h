#ifndef _Display_Res_H_
#define _Display_Res_H_

using namespace std;

#include <fstream>
#include <vector>
#include <map>
#include "DisplayResScreen.h"

/** \class DisplayRes
 *  \brief The DisplayRes module allows for the display resolution
 *         and refresh rateto be changed "on the fly".
 *
 *  Resolution and refresh rate can be changed using an
 *  explicit call or based on the video resolution.
 *
 *  The SwitchToVideoMode routine takes care of the actual
 *  work involved in changing the display mode. There are
 *  currently XRandR and OSX Carbon implementation so this
 *  works for X11 and OSX.
 */

typedef enum { GUI = 0, VIDEO = 1, CUSTOM_GUI = 2, CUSTOM_VIDEO = 3 } tmode;

class DisplayRes {
  public:
    /// Factory method that returns DisplayRes singleton
    static DisplayRes *GetDisplayRes(void);

    /// Initialize is called automatically on creation, but 
    /// should be called again if the configuration is changed.
    bool Initialize(void);

    // These return true if mode is changed
    bool SwitchToVideo(int iwidth, int iheight, short irate = 0);
    bool SwitchToGUI(tmode which_gui=GUI);
    bool SwitchToCustomGUI(int width, int height, short rate = 0);

    // These return the current display attributes
    int GetWidth(void)       const { return last.Width(); }
    int GetHeight(void)      const { return last.Height(); }
    int GetPhysicalWidth(void)    const { return last.Width_mm(); }
    int GetPhysicalHeight(void)   const { return last.Height_mm(); }
    int GetRefreshRate(void) const { return last.RefreshRate(); }
    double GetAspectRatio(void) const { return last.AspectRatio(); }

    // These are global for all video modes
    int GetMaxWidth(void)    const { return max_width; }
    int GetMaxHeight(void)   const { return max_height; }
    virtual const vector<DisplayResScreen>& GetVideoModes() const = 0;
    const vector<short> GetRefreshRates(int width, int height) const;

    // These are for backward compatability
    static DisplayRes *getDisplayRes(void);
    bool switchToGUI(void) { return SwitchToGUI(); }
    int vwidth, vheight;
    bool switchToVid(int width, int height)
    {
        vwidth=width;
        vheight=height;
        return SwitchToVideo(width, height, 0);
    }
    bool switchToCustom(int width, int height)
    {
        return SwitchToCustomGUI(width, height, 0);
    }
    int Width(void)       const { return GetWidth(); }
    int Height(void)      const { return GetHeight(); }
    int Width_mm(void)    const { return GetPhysicalWidth(); }
    int Height_mm(void)   const { return GetPhysicalHeight(); }
    int vidWidth(void)    const { return vwidth; }
    int vidHeight(void)    const { return vheight; }

  protected:
    DisplayRes(void) : max_width(0), max_height(0) {;}
    virtual ~DisplayRes(void) {;}
    
    // These methods are implemented by the subclasses
    virtual bool GetDisplaySize(int &width_mm, int &height_mm) const = 0;
    virtual bool SwitchToVideoMode(int width, int height, short framerate) = 0;

  private:
    DisplayRes(const DisplayRes & rhs); // disable copy constructor;

    tmode cur_mode;           // current mode
    DisplayResScreen mode[4]; // GUI, default video, custom GUI, custom video
    DisplayResScreen last;    // mirror of mode[current_mode]

    // maps input video parameters to output video modes
    DisplayResMap in_size_to_output_mode;

    int max_width, max_height;

    static DisplayRes *instance;
};

// Helper function
const vector<DisplayResScreen> GetVideoModes();

#endif
