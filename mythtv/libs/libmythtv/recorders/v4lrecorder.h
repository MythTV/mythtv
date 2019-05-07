// -*- Mode: c++ -*-
#ifndef _V4L_RECORDER_H_
#define _V4L_RECORDER_H_

#include "dtvrecorder.h"
#include "cc608decoder.h"
#include "vbitext/vt.h"
#include "mthread.h"
#include "tv.h" // for VBIMode

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
    explicit V4LRecorder(TVRec *rec) : DTVRecorder(rec) {}
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
    QString          m_audiodevice;
    QString          m_vbidevice;
    int              m_vbimode                {VBIMode::None};
    struct VBIData  *m_pal_vbi_cb             {nullptr};
    struct vbi      *m_pal_vbi_tt             {nullptr};
    uint             m_ntsc_vbi_width         {0};
    uint             m_ntsc_vbi_start_line    {0};
    uint             m_ntsc_vbi_line_count    {0};
    VBI608Extractor *m_vbi608                 {nullptr};
    VBIThread       *m_vbi_thread             {nullptr};
    QList<struct txtbuffertype*> m_textbuffer;
    int              m_vbi_fd                 {-1};
    volatile bool    m_request_helper         {false};
};

class VBIThread : public MThread
{
  public:
    explicit VBIThread(V4LRecorder *_parent) :
        MThread("VBIThread"), m_parent(_parent)
    {
        start();
    }

    virtual ~VBIThread()
    {
        while (isRunning())
        {
            m_parent->StopRecording();
            wait(1000);
        }
    }

    void run(void) override // MThread
    {
        RunProlog();
        m_parent->RunVBIDevice();
        RunEpilog();
    }

  private:
    V4LRecorder *m_parent {nullptr};
};

#endif // _V4L_RECORDER_H_
