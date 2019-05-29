#ifndef MYTHHWCONTEXT_H
#define MYTHHWCONTEXT_H

// MythTV
#include "mythcodeccontext.h"

typedef int (*CreateHWDecoder)(AVCodecContext *Context);

class MythOpenGLInterop;

class MythHWContext
{
  public:
    static int  GetBuffer              (struct AVCodecContext *Context, AVFrame *Frame, int Flags);
    static int  GetBuffer2             (struct AVCodecContext *Context, AVFrame *Frame, int Flags);
    static int  GetBuffer3             (struct AVCodecContext *Context, AVFrame *Frame, int Flags);
    static int  InitialiseDecoder      (AVCodecContext *Context, CreateHWDecoder Callback, const QString &Debug);
    static int  InitialiseDecoder2     (AVCodecContext *Context, CreateHWDecoder Callback, const QString &Debug);
    static void ReleaseBuffer          (void *Opaque, uint8_t *Data);
    static void FramesContextFinished  (AVHWFramesContext *Context);
    static void DeviceContextFinished  (AVHWDeviceContext *Context);
    static void DestroyInteropCallback (void *Wait, void *Interop, void*);
    static void CreateDecoderCallback  (void *Wait, void *Context, void *Callback);
    static AVBufferRef* CreateDevice   (AVHWDeviceType Type, const QString &Device = QString());

  protected:
    static void DestroyInterop         (MythOpenGLInterop *Interop);
};

#endif // MYTHHWCONTEXT_H
