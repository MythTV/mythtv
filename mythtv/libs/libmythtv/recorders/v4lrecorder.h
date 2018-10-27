// -*- Mode: c++ -*-
#ifndef _V4L_RECORDER_H_
#define _V4L_RECORDER_H_

#include "dtvrecorder.h"
#include "cc608decoder.h"
#include "vbitext/vt.h"
#include "mthread.h"

class VBI608Extractor;
class VBIThread;
class TVRec;

struct vbi;
struct VBIData
{
    RecorderBase *nvr;
    vt_page teletextpage;
    bool foundteletextpage;
};

/// Abstract base class for Video4Linux based recorders.
class MTV_PUBLIC V4LRecorder : public DTVRecorder
{
    friend class VBIThread;
  public:
    explicit V4LRecorder(TVRec *rec);
    virtual ~V4LRecorder();

    void StopRecording(void) override; // RecorderBase
    void SetOption(const QString &name, const QString &value) override; // DTVRecorder
    void SetOption(const QString &name, int value) override // DTVRecorder
        { DTVRecorder::SetOption(name, value); }

  protected:
    int  OpenVBIDevice(void);
    void CloseVBIDevice(void);
    void RunVBIDevice(void);

    virtual bool IsHelperRequested(void) const;
    virtual void FormatTT(struct VBIData */*vbidata*/) {}
    virtual void FormatCC(uint /*code1*/, uint /*code2*/) {}

  protected:
    QString          audiodevice;
    QString          vbidevice;
    int              vbimode;
    struct VBIData  *pal_vbi_cb;
    struct vbi      *pal_vbi_tt;
    uint             ntsc_vbi_width;
    uint             ntsc_vbi_start_line;
    uint             ntsc_vbi_line_count;
    VBI608Extractor *vbi608;
    VBIThread       *vbi_thread;
    QList<struct txtbuffertype*> textbuffer;
    int              vbi_fd;
    volatile bool    request_helper;
};

class VBIThread : public MThread
{
  public:
    explicit VBIThread(V4LRecorder *_parent) :
        MThread("VBIThread"), parent(_parent)
    {
        start();
    }

    virtual ~VBIThread()
    {
        while (isRunning())
        {
            parent->StopRecording();
            wait(1000);
        }
    }

    void run(void) override // MThread
    {
        RunProlog();
        parent->RunVBIDevice();
        RunEpilog();
    }

  private:
    V4LRecorder *parent;
};

#endif // _V4L_RECORDER_H_
