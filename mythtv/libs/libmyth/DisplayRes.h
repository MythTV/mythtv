#ifndef _Display_Res_H_
#define _Display_Res_H_

//using namespace std;

#include <fstream>
#include <vector>

/**********************************************************
 *  The DisplayRes module allows for the display resolution to be
 *  changed "on the fly", based on the video resolution.  It also
 *  allows for the GUI to have a specific resolution.
 *
 *  The switch_res routine takes care of the actual work involved in
 *  changing the display resolution.  It is currently implemented by
 *  calling Xrandr(3x) and is therefore limited to Xv and XvMC.
 **********************************************************/

class DisplayRes {
  private:
    class dim_t {
      public:
        int             vid_width;
        int             vid_height;
        int             width;
        int             height;
        int             width_mm;
        int             height_mm;

      public:
        dim_t(void);
        ~dim_t(void) {;}
    };

    dim_t      gui;
    dim_t      last;
    dim_t      default_res;

    std::vector<dim_t>   disp;

    bool       mode_video;

    int        x_width_mm;
    int        x_height_mm;
    int        alt_height_mm;
    int        vid_width;
    int        vid_height;

    static DisplayRes * instance;

    DisplayRes(const DisplayRes & rhs)
        { abort(); }

  protected:
    DisplayRes(void) {;}
    virtual ~DisplayRes(void) {;}
    
    // These methods are implemented by the subclasses
    virtual bool get_display_size(int & width_mm, int & height_mm) = 0;
    virtual bool switch_res(int width, int height) = 0;

  public:
    static DisplayRes * getDisplayRes(void);

    bool Initialize(void);
    bool getScreenSize(int vid_width, int vid_height,
                       int & width_mm, int & height_mm);
    bool switchToVid(int vid_width, int vid_height);
    bool switchToGUI(void);
    bool switchToCustom(int width, int height);

    int Width(void)     { return last.width; }
    int Height(void)    { return last.height; }
    int Width_mm(void)  { return last.width_mm; }
    int Height_mm(void) { return last.height_mm; }
    int vidWidth(void)  { return vid_width; }
    int vidHeight(void) { return vid_height; }
};

#endif
