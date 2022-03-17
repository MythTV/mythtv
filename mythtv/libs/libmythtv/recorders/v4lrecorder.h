// -*- Mode: c++ -*-
#ifndef V4L_RECORDER_H
#define V4L_RECORDER_H

#include "libmythbase/mthread.h"

#include "libmythtv/captions/cc608decoder.h"
#include "libmythtv/recorders/dtvrecorder.h"
#include "libmythtv/recorders/vbitext/vt.h"
#include "libmythtv/tv.h" // for VBIMode

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
    ~V4LRecorder() override;

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
    QString          m_audioDeviceName;
    QString          m_vbiDeviceName;
    int              m_vbiMode                {VBIMode::None};
    struct VBIData  *m_palVbiCb               {nullptr};
    struct vbi      *m_palVbiTt               {nullptr};
    uint             m_ntscVbiWidth           {0};
    uint             m_ntscVbiStartLine       {0};
    uint             m_ntscVbiLineCount       {0};
    VBI608Extractor *m_vbi608                 {nullptr};
    VBIThread       *m_vbiThread              {nullptr};
    QList<struct txtbuffertype*> m_textBuffer;
    int              m_vbiFd                  {-1};
    volatile bool    m_requestHelper          {false};
};

class VBIThread : public MThread
{
  public:
    explicit VBIThread(V4LRecorder *_parent) :
        MThread("VBIThread"), m_parent(_parent)
    {
        start();
    }

    ~VBIThread() override
    {
        while (isRunning())
        {
            m_parent->StopRecording();
            wait(1s);
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

#endif // V4L_RECORDER_H
