#ifndef XJ_H_
#define XJ_H_

struct XvData;

class XvVideoOutput
{
  public:
    XvVideoOutput();
   ~XvVideoOutput();

    bool Init(int width, int height, char *window_name, 
              char *icon_name, int num_buffers, 
              unsigned char **out_buffers, unsigned int wid = 0);
    void PrepareFrame(unsigned char *buffer, int width, int height);
    void Show();
    int CheckEvents(void);

    void ToggleFullScreen();

    void EmbedInWidget(unsigned long wid, int x, int y, int w, int h);
    void StopEmbedding(void);
    void ResizeVideo(int x, int y, int w, int h);
    void MoveResize(void);

    void GetDrawSize(int &xoff, int &yoff, int &width, int &height);

  private:
    void sizehint(int x, int y, int width, int height, int max);
    void Exit(void);

    void decorate(int dec); 
    void hide_cursor(void);
    void show_cursor(void);

    XvData *data;

    int XJ_screen_num;
    unsigned long XJ_white,XJ_black;
    int XJ_started;
    int XJ_depth;
    int XJ_caught_error;
    int XJ_width, XJ_height;
    int XJ_screenx, XJ_screeny;
    int XJ_screenwidth, XJ_screenheight;
    int XJ_fullscreen;
    int XJ_aspect;

    int oldx, oldy, oldw, oldh;
    int curx, cury, curw, curh;
    int img_xoff, img_yoff; 
    int imgx, imgy, imgw, imgh, dispxoff, dispyoff, dispwoff, disphoff;
    float img_hscanf, img_vscanf;

    int dispx, dispy, dispw, disph;
    int olddispx, olddispy, olddispw, olddisph;
    bool embedding;
    bool created_win;

    int xv_port;
    int colorid;

    int use_shm;

    unsigned char *scratchspace;

    pthread_mutex_t lock;
};

#endif
