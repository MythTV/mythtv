/*
	webcam.h

	(c) 2003 Paul Volkaerts
	
    header for the main interface screen
*/

#ifndef WEBCAM_H_
#define WEBCAM_H_

#include <qsqldatabase.h>
#include <qregexp.h>
#include <qtimer.h>
#include <qptrlist.h>
#include <qthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <linux/videodev.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/dialogbox.h>


#define RGB24_LEN(w,h)      ((w) * (h) * 3)
#define RGB32_LEN(w,h)      ((w) * (h) * 4)
#define YUV420P_LEN(w,h)    (((w) * (h) * 3) / 2)
#define YUV422P_LEN(w,h)    ((w) * (h) * 2)

// YUV --> RGB Conversion macros
#define _S(a)		(a)>255 ? 255 : (a)<0 ? 0 : (a)
#define _R(y,u,v) (0x2568*(y)  			       + 0x3343*(u)) /0x2000
#define _G(y,u,v) (0x2568*(y) - 0x0c92*(v) - 0x1a1e*(u)) /0x2000
#define _B(y,u,v) (0x2568*(y) + 0x40cf*(v))					     /0x2000


#define WC_CLIENT_BUFFERS   2

struct wcClient
{
    QObject *eventWindow; // Window to receive frame-ready events
    int format;
    int frameSize;
    int fps;
    int actualFps;
    int interframeTime;
    QPtrList<unsigned char> BufferList;
    QPtrList<unsigned char> FullBufferList;
    QTime timeLastCapture;

};


class WebcamEvent : public QCustomEvent
{
public:
    enum Type { FrameReady = (QEvent::User + 200)  };

    WebcamEvent(Type t, wcClient *c)
        : QCustomEvent(t)
    { client=c; }

    ~WebcamEvent()
    {
    }

    wcClient *getClient() { return client; }

private:
    wcClient *client;
};


class Webcam : public QObject
{

  Q_OBJECT

  public:

    Webcam(QObject *parent = 0, const char * = 0);
    virtual ~Webcam(void);
    static QString devName(QString WebcamName);
    bool camOpen(QString WebcamName, int width, int height);
    void camClose(void);
    bool SetPalette(int palette);
    int  GetPalette(void) { return (vPic.palette); };
    int  SetBrightness(int v);
    int  SetContrast(int v);
    int  SetColour(int v);
    int  SetHue(int v);
    int  SetTargetFps(wcClient *client, int fps);
    int  GetBrightness(void) { return (vPic.brightness);};
    int  GetColour(void) { return (vPic.colour);};
    int  GetContrast(void) { return (vPic.contrast);};
    int  GetHue(void) { return (vPic.hue);};
    int  GetActualFps();
    QString GetName(void) { return vCaps.name; };
    void GetMaxSize(int *x, int *y) { *y=vCaps.maxheight; *x=vCaps.maxwidth; };
    void GetMinSize(int *x, int *y) { *y=vCaps.minheight; *x=vCaps.minwidth; };
    void GetCurSize(int *x, int *y) { *y=vWin.height; *x=vWin.width; };
    int isGreyscale(void) { return ((vCaps.type & VID_TYPE_MONOCHROME) ? true : false); };

    wcClient *RegisterClient(int format, int fps, QObject *eventWin);
    void UnregisterClient(wcClient *client);
    unsigned char *GetVideoFrame(wcClient *client);
    void FreeVideoBuffer(wcClient *client, unsigned char *buffer);


  private:
    void StartThread();
    void KillThread();
    static void *WebcamThread(void *p);
    void WebcamThreadWorker();


    pthread_t webcamthread;
    QPtrList<wcClient> wcClientList;
    QMutex WebcamLock;

    void SetSize(int width, int height);

    void readCaps(void);

    int hDev;
    QString DevName;
    unsigned char *picbuff1;
    int imageLen;
    int frameSize;
    int fps;
    int actualFps;
    bool killWebcamThread;

    // Camera-reported capabilities
    struct video_capability vCaps;
    struct video_window vWin;
    struct video_picture vPic;
    struct video_clip vClips;


};


#endif
