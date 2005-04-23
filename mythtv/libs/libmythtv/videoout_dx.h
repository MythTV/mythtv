#ifndef VIDEOOUT_DX_H_
#define VIDEOOUT_DX_H_

#include "videooutbase.h"

#include <windows.h>
#include <ddraw.h>

class VideoOutputDX : public VideoOutput
{
  public:
    VideoOutputDX();
   ~VideoOutputDX();

    bool Init(int width, int height, float aspect, WId winid,
              int winx, int winy, int winw, int winh, WId embedid = 0);
    void PrepareFrame(VideoFrame *buffer, FrameScanType);
    void Show(FrameScanType );

    void InputChanged(int width, int height, float aspect);
    void AspectChanged(float aspect);
    void Zoom(int direction);

    void EmbedInWidget(WId wid, int x, int y, int w, int h);
    void StopEmbedding(void);

    int GetRefreshRate(void);

    void DrawUnusedRects(bool sync = true);

    void UpdatePauseFrame(void);
    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      NuppelVideoPlayer *pipPlayer);

    void MoveResize(void);
    int ChangePictureAttribute(int attribute, int newValue);
 
    float GetDisplayAspect(void) { return ((float) dispw)/disph; }
    
    void WaitForVSync(void);

  private:
    void Exit(void);

    bool XJ_started;

    VideoFrame pauseFrame;
    
    HWND wnd;
    
    LPDIRECTDRAW2        ddobject;                    /* DirectDraw object */
    LPDIRECTDRAWSURFACE2 display;                        /* Display device */
//    LPDIRECTDRAWSURFACE2 current_surface;   /* surface currently displayed */
    LPDIRECTDRAWCLIPPER  clipper;             /* clipper used for blitting */
    LPDIRECTDRAWCOLORCONTROL ccontrol;        /* colour controls object */
    HINSTANCE            ddraw_dll;       /* handle of the opened ddraw dll */

    /* Multi-monitor support */
    HMONITOR             monitor;          /* handle of the current monitor */
    GUID                 *display_driver;
    HMONITOR             (WINAPI* MonitorFromWindow)(HWND, DWORD);
    BOOL                 (WINAPI* GetMonitorInfo)(HMONITOR, LPMONITORINFO);

    RECT         rect_display;

    LPDIRECTDRAWSURFACE2 front_surface;
    LPDIRECTDRAWSURFACE2 back_surface;

    DWORD chroma;

    int colorkey;
    int rgb_colorkey;

    bool using_overlay;
    bool overlay_3buf;
//    bool hw_yuv;
    bool use_sysmem;
    
    int outputpictures;
    
    void MakeSurface();
    
    int DirectXInitDDraw();
    int DirectXCreateDisplay();
    int DirectXCreateClipper();
    int DirectXCreateSurface(LPDIRECTDRAWSURFACE2 *pp_surface_final,
                                 DWORD i_chroma, bool b_overlay, int i_backbuffers);
    void DirectXCloseDDraw();
    void DirectXCloseDisplay();
    void DirectXCloseSurface();
    void DirectXGetDDrawCaps();
    DWORD DirectXFindColorkey(uint32_t i_color);
    int DirectXUpdateOverlay();
    int DirectXLockSurface(void **picbuf, int *stride);
    int DirectXUnlockSurface();
    
    int NewPicture();
};

#endif
