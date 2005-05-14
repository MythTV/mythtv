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
 *  currently XRandR and OS X Carbon implementation so this
 *  works for X (Linux/BSD/UNIX) and Mac OS X.
 */

typedef enum { GUI = 0, VIDEO = 1, CUSTOM_GUI = 2, CUSTOM_VIDEO = 3 } tmode;

class DisplayRes {
  public:
    /** \brief Factory method that returns a DisplayRes singleton */
    static DisplayRes *GetDisplayRes(void);

    /** \brief Initialize DisplayRes, normally called automatically.
     *
     *   While Initialize is called automatically by GetDisplayRes(void)
     *   it should be called again if the database settings for DisplayRes
     *   have changed.
     */
    bool Initialize(void);

    /** \name Resolution/Refresh Rate Changing methods.
     *   These return true if mode is changed
     * @{
     */

    /** \fn SwitchToVideo(int iwidth, int iheight, short irate = 0)
     *  \brief Switches to the resolution and refresh rate defined in the
     *         database for the specified video resolution and frame rate.
     */
    bool SwitchToVideo(int iwidth, int iheight, short irate = 0);
    /** \brief Switches to the GUI resolution specified.
     *
     *   If which_gui is GUI then this switches to the resolution
     *   and refresh rate set in the database for the GUI. If 
     *   which_gui is set to CUSTOM_GUI then we switch to the
     *   resolution and refresh rate specified in the last
     *   call to SwitchToCustomGUI(int, int, short).
     *
     *  \param which_gui either regular GUI or CUSTOM_GUI
     *  \sa SwitchToCustomGUI(int, int, short)
     */
    bool SwitchToGUI(tmode which_gui=GUI);
    /** \brief Switches to the custom GUI resolution specified.
     *
     *   This switches to the specified resolution, and refresh
     *   rate if specified. It also makes saves this resolution
     *   as the CUSTOM_GUI resolution, so that it can be 
     *   recalled with SwitchToGUI(CUSTOM_GUI) in later 
     *   DisplayRes calls.
     *  \sa SwitchToGUI(tmode)
     */
    bool SwitchToCustomGUI(int width, int height, short rate = 0);
    /** @} */


    /** \name Queries of current video mode
     * @{
     */

    /** \brief Returns current screen width in pixels. */
    int GetWidth(void)          const { return last.Width(); }

    /** \brief Returns current screen width in pixels. */
    int GetHeight(void)         const { return last.Height(); }

    /** \brief Returns current screen width in millimeters. */
    int GetPhysicalWidth(void)  const { return last.Width_mm(); }

    /** \brief Returns current screen height in millimeters. */
    int GetPhysicalHeight(void) const { return last.Height_mm(); }

    /** \brief Returns current screen refresh rate. */
    int GetRefreshRate(void)    const { return last.RefreshRate(); }
    /** \brief Returns current screen aspect ratio.
     *
     *  If there is an aspect overide in the database that aspect
     *  ratio is returned instead of the actual screen aspect ratio.
     */
    double GetAspectRatio(void) const { return last.AspectRatio(); }
    /** @} */


    /** \name These methods are global for all video modes
     * @{
     */

    /// \brief Returns maximum width in pixels supported by display.
    int GetMaxWidth(void)    const { return max_width; }
    /// \brief Returns maximum height in pixels supported by display.
    int GetMaxHeight(void)   const { return max_height; }
    /// \brief Returns all video modes supported by the display.
    virtual const vector<DisplayResScreen>& GetVideoModes() const = 0;
    /// \brief Returns refresh rates available at a specific screen resolution.
    const vector<short> GetRefreshRates(int width, int height) const;
    /** @} */

  protected:
    /// \brief DisplayRes is an abstract class, instanciate with GetDisplayRes(void)
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

    /// maps input video parameters to output video modes
    DisplayResMap in_size_to_output_mode;

    int max_width, max_height;

    static DisplayRes *instance;
};

/** \fn GetVideoModes(void)
 *  \relates DisplayRes
 *  \brief Returns all video modes available.
 * 
 *   This is a conveniance class that instanciates a DisplayRes
 *   class if needed, and returns a copy of vector returned by
 *   GetVideoModes(void).
 */
const vector<DisplayResScreen> GetVideoModes(void);

#endif
