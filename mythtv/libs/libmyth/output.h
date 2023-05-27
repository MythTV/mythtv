// -*- Mode: c++ -*-
// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#ifndef   OUTPUT_H
#define   OUTPUT_H

class OutputListeners;
class OutputEvent;

#include <vector>

#include <QMutex>
#include <QList>

#include "libmyth/mythexp.h"
#include "libmythbase/mythobservable.h"

using namespace std::chrono_literals;

class QObject;
class Buffer;

namespace MythTV {
class Visual;
}

class MPUBLIC OutputEvent : public MythEvent
{
  public:
    explicit OutputEvent(Type type) :
        MythEvent(type) {}
    OutputEvent(std::chrono::seconds s, unsigned long w, int b, int f, int p, int c) :
        MythEvent(kInfo), m_elaspedSeconds(s), m_writtenBytes(w),
        m_brate(b), m_freq(f), m_prec(p), m_chan(c) {}
    explicit OutputEvent(const QString &e) :
        MythEvent(kError)
    {
        QByteArray tmp = e.toUtf8();
        m_errorMsg = new QString(tmp.constData());
    }

    ~OutputEvent() override
    {
        delete m_errorMsg;
    }

    const QString *errorMessage() const { return m_errorMsg; }

    const std::chrono::seconds &elapsedSeconds() const { return m_elaspedSeconds; }
    const unsigned long &writtenBytes() const { return m_writtenBytes; }
    const int &bitrate() const { return m_brate; }
    const int &frequency() const { return m_freq; }
    const int &precision() const { return m_prec; }
    const int &channels() const { return m_chan; }

    MythEvent *clone(void) const override // MythEvent
        { return new OutputEvent(*this); }

    static const Type kPlaying;
    static const Type kBuffering;
    static const Type kInfo;
    static const Type kPaused;
    static const Type kStopped;
    static const Type kError;

  private:
    OutputEvent(const OutputEvent &o) : MythEvent(o),
        m_elaspedSeconds(o.m_elaspedSeconds),
        m_writtenBytes(o.m_writtenBytes),
        m_brate(o.m_brate), m_freq(o.m_freq),
        m_prec(o.m_prec), m_chan(o.m_chan)
    {
        if (o.m_errorMsg)
        {
            m_errorMsg = new QString(*o.m_errorMsg);
        }
    }

  // No implicit copying.
  public:
    OutputEvent &operator=(const OutputEvent &other) = delete;
    OutputEvent(OutputEvent &&) = delete;
    OutputEvent &operator=(OutputEvent &&) = delete;

  private:
    QString       *m_errorMsg        {nullptr};

    std::chrono::seconds m_elaspedSeconds {0s};
    unsigned long  m_writtenBytes    {0};
    int            m_brate           {0};
    int            m_freq            {0};
    int            m_prec            {0};
    int            m_chan            {0};
};

using Visuals = std::vector<MythTV::Visual*>;

class MPUBLIC OutputListeners : public MythObservable
{
public:
    OutputListeners() = default;
    ~OutputListeners() override = default;

    bool hasVisual(void) { return !m_visuals.empty(); }
    void addVisual(MythTV::Visual *v);
    void removeVisual(MythTV::Visual *v);

    QMutex *mutex() { return &m_mtx; }

    void setBufferSize(unsigned int sz) { m_bufsize = sz; }
    unsigned int bufferSize() const { return m_bufsize; }

protected:
    void error(const QString &e);
    void dispatchVisual(uchar *b, unsigned long b_len,
                        std::chrono::milliseconds timecode, int chan, int prec);
    void prepareVisuals();

private:
    Q_DISABLE_COPY(OutputListeners)

    QMutex       m_mtx;
    Visuals      m_visuals;

    unsigned int m_bufsize {0};
};


#endif // OUTPUT_H
